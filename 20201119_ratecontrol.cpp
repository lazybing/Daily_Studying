#include "common.h"
#include "param.h"
#include "frame.h"
#include "framedata.h"
#include "picyuv.h"
#include "encoder.h"
#include "slicetype.h"
#include "ratecontrol.h"
#include "sei.h"

#define BR_SHIFT  6
#define CPB_SHIFT 4

using namespace X265_NS;

int RateControl::rateControlStart(Frame *curFrame, RateControlEntry *rce, Encoder *enc)
{
    int orderValue   = m_startEndOrder.get();
    int startOrdinal = rce->encodeOrder * 2;

    while (orderValue < startOrdinal && !m_bTerminated)
        orderValue = m_startEndOrder.waitForChange(orderValue);

    if (!curFrame)
    {
        //faked rateControlStart calls when the encoder is flushing 
        m_startEndOrder.incr();
        return 0;
    }

    FrameData &curEncData = *curFrame->m_encData;
    m_curSlice      = curEncData.m_slice;
    m_sliceType     = m_curSlice->m_sliceType;
    rce->sliceType  = m_sliceType;
    if (!m_2pass)
        rce->keptAsRef = IS_REFERENCED(curFrame);
    m_predType = getPredictorType(curFrame->m_lowres.sliceType, m_sliceType);
    rce->poc = m_curSlice->m_poc;

    if (!m_param->bResetZoneConfig && (rce->encodeOrder % m_param->reconfigWindowSize == 0))
    {
        int index = m_zoneBufferIdx % m_param->rc.zonefileCount;
        int read  = m_top->zoneReadCount[index].get();
        int write = m_top->zoneWriteCount[index].get();
        if (write <= read)
            write = m_top->zoneWriteCount[index].waitForChange(write);
        m_zoneBufferIdx++;

        for (int i = 0; i < m_param->rc.zonefileCount; i++)
        {
            if (m_param->rc.zones[i].startFrame == rce->encodeOrder)
            {
                m_param->rc.bitrate = m_param->rc.zones[i].zoneParam->rc.bitrate;
                m_param->rc.vbvmaxBitrate = m_param->rc.zones[i].zoneParam->rc.vbvmaxBitrate;
                memcpy(m_relativeComplexity, m_param->rc.zones[i].relativeComplexity, sizeof(double) * m_param->reconfigWindowSize);
                reconfigureRC();
                m_isCbr = 1;    //Always vbvmaxtrate == bitrate here
                m_top->zoneReadCount[i].incr();
            }
        }
    }

    if (m_param->bResetZoneConfig)
    {
        //change ratecontrol stats for next zone if specified
        for (int i = 0; i < m_param->rc.zonefileCount; i++)
        {
            if (m_param->rc.zones[i].startFrame == curFrame->m_encoderOrder)
            {
                m_param = m_param->rc.zones[i].zoneParam;
                reconfigureRC();
                init(*m_curSlice->m_sps);
            }
        }
    }
}
