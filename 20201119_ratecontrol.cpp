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

//Return the zone for the current frame
x265_zone *RateControl::getZone()
{
    for (int i = m_param->rc.zoneCount - 1; i >= 0; i--)
    {
        x265_zone *z = &m_param->rc.zones[i];
        if (m_framesDone + 1 >= z->startFrame && m_framesDone < z->endFrame)
            return z;
    }
    return NULL;
}

RateControl::RateControl(x265_param &p, Encoder *top)
{
    m_param = &p;
    m_top   = top;
    int lowresCuWidth  = ((m_param->sourceWidth  / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS;
    int lowresCuHeight = ((m_param->sourceHeight / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS;
    m_ncu = lowresCuWidth * lowresCuHeight;

    m_qCompress = (m_param->rc.cuTree && !m_param->rc.hevcAq) ? 1 : m_param->rc.qCompress;

    m_zoneBufferIdx             = 0;
    m_residualFrames            = 0;
    m_partialResidualFrames     = 0;
    m_residualCost              = 0;
    m_partialResidualCost       = 0;
    m_rateFactorMaxIncrement    = 0;
    m_rateFactorMaxDecrement    = 0;
    m_fps = (double)m_param->fpsNum / m_param->fpsDenom;
    m_startEndOrder.set(0);
    m_bTerminated               = false;
    m_finalFrameCount           = 0;
    m_numEntries                = 0;
    m_isSceneTransition         = false;
    m_lastPredictorReset        = 0;
    m_avgPFrameQp               = 0;
    m_isFirstMiniGop            = false;
    m_lastPredictorReset        = 0;
    m_avgPFrameQp               = 0;
    m_isFirstMiniGop            = false;
    m_lastScenecut              = -1;
    m_lastScenecutAwareIFrame   = -1;

    if (m_param->rc.rateControlMode == X265_RC_CRF)
    {
        m_param->rc.qp = (int)m_param->rc.rfConstant;
        m_param->rc.bitrate = 0;
        
        double baseCplx = m_ncu * (m_param->bframes ? 120 : 80);
        double mbtree_offset = m_param->rc.cuTree ? (1.0 - m_param->rc.qCompress) * 13.5 : 0;
        m_rateFactorConstant = pow(baseCplx, 1 - m_qCompress) / 
            x265_qp2qScale(m_param->rc.rfConstant + mbtree_offset);
        if (m_param->rc.rfConstantMax)
        {
            m_rateFactorMaxIncrement = m_param->rc.rfConstantMax - m_param->rc.rfConstant;
            if (m_rateFactorMaxIncrement <= 0)
            {
                x265_log(m_param, X265_LOG_WARNING, "CRF max must be greater than CRF\n");
                m_rateFactorMaxIncrement = 0;
            }
        }
        if (m_param->rc.rfConstantMin)
            m_rateFactorMaxDecrement = m_param->rc.rfConstant - m_param->rc.rfConstantMin;
    }
    m_isAbr = m_param->rc.rateControlMode != X265_RC_CQP && !m_param->rc.bStatRead;
    m_2pass = m_param->rc.rateControlMode != X265_RC_CQP && m_param->rc.bStatRead;
    m_bitrate = m_param->rc.bitrate * 1000;
    m_frameDuration = (double)m_param->fpsDenom / m_param->fpsNum;
    m_qp = m_param->rc.qp;
    m_lastRceq = 1; //handles the cmplxrsum when the previous frame cost is zero
    m_shortTermCplxSum      = 0;
    m_shortTermCplxCount    = 0;
    m_lastNonBPictType      = I_SLICE;
    m_isAbrReset            = false;
    m_lastAbrResetPoc       = -1;
    m_statFileOut           = NULL;
    m_cutreeStatFileOut     = m_cutreeStatFileIn = NULL;
    m_rce2Pass              = NULL;
    m_encOrder              = NULL;
    m_lastBsliceSatdCost    = 0;
    m_movingAvgSum          = 0.0;
    m_isNextGop             = false;
    m_relativeComplexity    = NULL;

    //vbv initialization
    m_param->rc.vbvBufferSize = x265_clip3(0, 2000000, m_param->rc.vbvBufferSize);
    m_param->rc.vbvMaxBitrate = x265_clip3(0, 2000000, m_param->rc.vbvMaxBitrate);
    m_param->rc.vbvBufferInit = x265_clip3(0.0, 2000000.0, m_param->rc.vbvBufferInit);
    m_param->vbvBufferEnd     = x265_clipe(0.0, 2000000.0, m_param->vbvBufferEnd);
    m_initVbv = false;
    m_singleFrameVbv = 0;
    m_rateTolerance  = 1.0;

    if (m_param->rc.vbvBufferSize)
    {
        if (m_param->rc.ratecontrolMode == X265_RC_CQP)
        {
            x265_log(m_param, X265_LOG_WARNING, "VBV is incompatible with constant QP, ignored.\n");
            m_param->rc.vbvBufferSize = 0;
            m_param->rc.vbvMaxBitrate = 0;
        }
        else if (m_param->rc.vbvMaxBitrate == 0)
        {
            if (m_param->rc.rateControlMode == X265_RC_ABR)
            {
                x265_log(m_param, X265_LOG_WARNING, "VBV maxrate unspecified, assuming CBR\n");
                m_param->rc.vbvMaxBitrate = m_param->rc.bitrate;
            }
            else
            {
                x265_log(m_param, X265_LOG_WARNING, "VBV maxrate set but maxrate unspecified, ignored\n");
                m_param->rc.vbvBufferSize = 0;
            }
        }
        else if (m_param->rc.vbvmaxBitrate < m_param->rc.bitrate &&
                 m_param->rc.rateControlMode == X265_RC_ABR)
        {
            x265_log(m_param, X265_LOG_WARNING, "max bitrate less than average bitrate, assuming CBR\n");
            m_param->rc.bitrate = m_param->rc.vbvMaxBitrate;
        }
    }
    else if (m_param->rc.vbvMaxBitrate)
    {
       x265_log(m_param, X265_LOG_WARNING, "VBV maxrate specified, but no bufsize, ignored\n"); 
       m_param->rc.vbvMaxBitrate = 0;
    }
    m_isVbv = m_param->rc.vbvMaxBitrate > 0 && m_param->rc.vbvBufferSize > 0;
    if (m_param->vbvBufferEnd && !m_isVbv)
    {
        x265_log(m_param, X265_LOG_WARNING, "vbv-end requires VBV parameters, ignored\n");
        m_param->vbvBufferEnd = 0;
    }
    if (m_param->bEmitHRDSEI && !m_isVbv)
    {
        x265_log(m_param, X265_LOG_WARNING, "NAL HRD parameters require VBV parameters, ignored\n");
        m_param->bEmitHRDSEI = 0;
    }
    m_isCbr = m_param->rc.rateControlMode == X265_RC_ABR && m_isVbv && m_param->rc.vbvMaxBitrate <= m_param->rc.bitrate;
    if (m_param->rc.bStrictCbr && !m_isCbr)
    {
        x265_log(m_param, X265_LOG_WARNING, "strict CBR set without CBR mode, ignored\n");
        m_param->rc.bStrictCbr = 0;
    }
    if (m_param->rc.bStrictCbr)
        m_rateTolerance = 0.7;

    m_bframeBits        = 0;
    m_leadingNoBSatd    = 0;
    m_ipOffset          = 6.0 * X265_LOG2(m_param->rc.ipFactor);
    m_pbOffset          = 6.0 * X265_LOG2(m_param->rc.pbFactor);

    for (int i = 0; i < QP_MAX_MAX; i++)
        m_qpToEncodedBits[i] = 0;

    //Adjust the first frame in order to stabilize the quality level compared to the rest
#define ABR_INIT_QP_MIN (24)
#define ABR_INIT_QP_MAX (37)
#define ABR_INIT_QP_GRAIN_MAX (33)
#define ABR_SCENECUT_INIT_QP_MIN  (12)
#define CRF_INIT_QP (int)m_param->rc.rfConstant
    for (int i = 0; i < 3; i++)
    {
        m_lastQScaleFor[i] = x265_qp2qScale(m_param->rc.rateControlMode == X265_RC_CRF ? CRF_INIT_QP : ABR_INIT_QP_MIN);
        m_lmin[i]   = x265_qp2qScale(m_param->rc.qpMin);
        m_lmax[i]   = x265_qp2qScale(m_param->rc.qpMax);
    }

    if (m_param->rc.rateControlMode == X265_RC_CQP)
    {
        if (m_qp && !m_param-.bLossless)
        {
            m_qpConstant[P_SLICE] = m_qp;
            m_qpConstant[I_SLICE] = x265_clip3(QP_MIN, QP_MAX_MAX, (int)(m_qp - m_ipOffset + 0.5));
            m_qpConstant[B_SLICE] = x265_clip3(QP_MIN, QP_MAX_MAX, (int)(m_qp - m_pbOffset + 0.5));
        }
        else
        {
            m_qpConstant[P_SLICE] = m_qpConstant[I_SLICE] = m_qpConstant[B_SLICE] = m_qp;
        }
    }
    //qpstep - value set as encoder specific
    m_lstep = pow(2, m_param->rc.qpStep / 6.0);

    for (int i = 0; i < 2; i++)
        m_cuTreeStats.qpBuffer[i] = NULL;
}


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
