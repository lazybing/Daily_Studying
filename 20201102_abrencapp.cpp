#include "abrEncApp.h"
#include "mv.h"
#include "slice.h"
#include "param.h"

#include <signal.h>
#include <errno.h>

#include <queue>

using namespace X265_NS;

static volatile sig_atomic_t b_ctrl_c;
static void sigint_handler(int)
{
    b_ctrl_c = 1;
}

namespace X265_NS {
    //private namespace
#define X265_INPUT_QUEUE_SIZE 250
    AbrEncoder::AbrEncoder(CLIOptions cliopt[], uint8_t numEncodes, int &ret)
    {
        m_numEncodes = numEncodes;
        m_numActiveEncodes.set(numEncodes);
        m_queueSize  = (numEncodes > 1) ? X265_INPUT_QUEUE_SIZE : 1;
        m_passEnc    = X265_MALLOC(PassEncoder *, m_numEncodes);

        for (uint8_t i = 0; i < m_numEncodes; i++)
        {
            m_passEnc[i] = new PassEncoder(i, cliopt[i], this);
            if (!m_passEnc[i])
            {
                x265_log(NULL, X265_LOG_ERROR, "Unable to allocate memory for passEncoder\n");
                ret = 4;
            }
            m_passEnc[i]->init(ret);
        }

        if (!allocBuffers())
        {
            x265_log(NULL, X265_LOG_ERROR, "Unable to allocate memory for buffers\n");
            ret = 4;
        }

        /*start passEncoder worker threads*/
        for (uint8_t pass = 0; pass < m_numEncodes; pass+)
            m_passEnc[pass]->startThread();
    }

    bool AbrEncoder::allocBuffers()
    {
        m_inputPicBuffer    = X265_MALLOC(x265_picture**, m_numEncodes);
        m_analysisBuffer    = X265_MALLOC(x265_analysis_data*, m_numEncodes);

        m_picWriteCnt       = new ThreadSafeInteger[m_numEncodes];
        m_picReadCnt        = new ThreadSafeInteger[m_numEncodes];
        m_analysisWriteCnt  = new ThreadSafeInteger[m_numEncodes];
        m_analysisReadCnt   = new ThreadSafeInteger[m_numEncodes];

        m_picIdxReadCnt     = X265_MALLOC(ThreadSafeInteger*, m_numEncodes);
        m_analysisWrite     = X265_MALLOC(ThreadSafeInteger*, m_numEncodes);
        m_analysisRead      = X265_MALLOC(ThreadSafeInteger*, m_numEncodes);
        m_readFlag          = X265_MALLOC(int*, m_numEncodes);

        for (uint8_t pass = 0; pass < m_numEncodes; pass+)
        {
            m_inputPicBuffer[pass] = X265_MALLOC(x265_picture*, m_queueSize);
            for (uint32_t idx = 0; idx < m_queueSize; idx++)
            {
                m_inputPicBuffer[pass][idx] = x265_picture_alloc();
                x265_picture_init(m_passEnc[pass]->m_param, m_inputPicBuffer[pass][idx]);
            }

            m_analysisBuffer[pass]      = X265_MALLOC(x265_analysis_data, m_queueSize);
            m_picIdxReadCnt[pass]       = new ThreadSafeInteger[m_queueSize];
            m_analysisWrite[pass]       = new ThreadSafeInteger[m_queueSize]; 
            m_analysisRead[pass]        = new ThreadSafeInteger[m_queueSize]; 
            m_readFlag[pass]            = X265_MALLOC(int, m_queueSize);
        }
        return true;
    }

    void AbrEncoder::destroy()
    {
        x265_cleanup(); /*Free library singletons*/
        for (uint8_t pass = 0; pass < m_numEncodes; pass++)
        {
            for (uint32_t index = 0; index < m_queueSize; index++)
            {
                X265_FREE(m_inputPicBuffer[pass][index]->planes[0]);
                x265_picture_free(m_inputPicBuffer[pass][index]);
            }

            X265_FREE(m_inputPicBuffer[pass]);
            X265_FREE(m_analysisBuffer[pass]);
            X265_FREE(m_readFlag[pass]);
            delete[] m_picIdxReadCnt[pass];
            delete[] m_analysisWrite[pass];
            delete[] m_analysisRead[pass];
            m_passEnc[pass]->destroy();
            delete m_passEnc[pass];
        }
        X265_FREE(m_inputPicBuffer);
        X265_FREE(m_analysisBuffer);
        X265_FREE(m_readFlag);

        delete[] m_picWriteCnt;
        delete[] m_picReadCnt;
        delete[] m_analysisWriteCnt;
        delete[] m_analysisReadCnt;

        X265_FREE(m_picIdxReadCnt);
        X265_FREE(m_analysisWrite);
        X265_FREE(m_analysisRead);

        X265_FREE(m_passEnc);
    }

    PassEncoder::PassEncoder(uint32_t id, CLIOptions cliopt, AbrEncoder *parent)
    {
        m_id        = id;
        m_cliopt    = cliopt;
        m_parent    = parent;
        if (!(m_cliopt.enableScaler && m_id))
            m_input = m_cliopt.input;
        m_param     = cliopt.param;
        m_inputOver = false;
        m_lastIdx   = -1;
        m_encoder   = NULL;
        m_scaler    = NULL;
        m_reader    = NULL;
        m_ret       = 0;
    }

    int PassEncoder::init(int &result)
    {
        if (m_parent->m_numEncodes > 1)
            setReuseLevel();

        if (!(m_cliopt.enableScaler && m_id))
            m_reader = new Reader(m_id, this);
        else
        {
            VideoDesc *src = NULL, *dst = NULL;
            dst = new VideoDesc(m_param->sourceWidth, m_param->sourceHeight, m_param->internalCsp, m_param->internalBitDepth);
            int dstW = m_parent->m_passEnc[m_id - 1]->m_param->sourceWidth;
            int dstH = m_parent->m_passEnc[m_id - 1]->m_param->sourceHeight;
            src = new VideoDesc(dstW, dstH, m_param->internalCsp, m_param->internalBitDepth);
            if (src != NULL && dst != NULL)
            {
                m_scaler = new Scaler(0, 1, m_id, src, dst, this);
                if (!m_scaler)
                {
                    x265_log(m_param, X265_LOG_ERROR, "\n MALLOC failure in Scaler");
                    result = 4;
                }
            }
            
        }

        //note: we could try to acquire a different libx265 API here based on
        //the profile found during option parsing, but it must be done before
        //opening an encoder
        if (m_param)
            m_encoder = m_cliopt.api->encoder_open(m_param);
        if (!m_encoder)
        {
            x265_log(NULL, X265_LOG_ERROR, "x265_encoder_open() failed for Enc,\n");
            m_ret = 2;
            return  -1;
        }

        //get the encoder parameters post-initialization
        m_cliopt.api->encoder_parameters(m_encoder, m_param);

        return 1;
    }

    void PassEncoder::setReuseLevel()
    {
        uint32_t r, path = 0, padw = 0;

        m_param->confWinBottomOffset    = m_param->confWinRightOffset = 0;

        m_param->analysisLoadReuseLevel = m_cliopt.loadLevel;
        m_param->analysisSaveReuseLevel = m_cliopt.saveLevel;
        m_param->analysisSave           = m_cliopt.saveLevel ? "save.data" : NULL;
        m_param->analysisLoad           = m_cliopt.loadLevel ? "load.data" : NULL;
        m_param->bUseAnalysisFile       = 0;

        if (m_cliopt.loadLevel)
        {
            x265_param *refParam = m_parent->m_passEnc[m_cliopt.refId]->m_param;

            if (m_param->sourceHeight == (refParam->sourceHeight - refParam->confWinBottomOffset) &&
                m_param->sourceWidth  == (refParam->sourceWidth  - refParam->confWinRightOffset))
            {
                m_parent->m_passEnc[m_id]->m_param->confWinBottomOffset = refParam->confWinBottomOffset;
                m_parent->m_passEnc[m_id]->m_param->confWinRightOffset  = refParam->confWinRightOffset;
            }
            else
            {
                int srcH = refParam->sourceHeight - refParam->confWinBottomOffset;
                int srcW = refParam->sourceWidth  - refParam->confWinRightOffset;

                double scaleFactorH = double(m_param->sourceHeight / srcH);
                double scaleFactorW = double(m_param->sourceWidth / srcW);

                int absScaleFactorH = (int)(10 * scaleFactorH + 0.5);
                int absScaleFactorW = (int)(10 * scaleFactorW + 0.5);

                if (absScaleFactorH == 20 && absScaleFactorW == 20)
                {
                    m_param->scaleFactor = 2;

                    m_param->m_passEnc[m_id]->m_param->confWinBottomOffset = refParam->confWinBottomOffset * 2;
                    m_param->m_passEnc[m_id]->m_param->confWinRightOffset = refParam->confWinRightOffset * 2;
                }
            }
        }

        int h = m_param->souceHeight + m_param->confWinBottomOffset;
        int w = m_param->souceWidth  + m_param->confWinRightOffset;
        if (h & (m_param->minCUSize - 1))
        {
            r = h & (m_param->minCUSize - 1);
            padh = m_param->minCUSize - r;
            m_param->confWinBottomOffset += padh;
        }

        if (w & (m_param->minCUSize - 1))
        {
            r = w & (m_param->minCUSize - 1);
            padw = m_param->minCUSize - r;
            m_param->confWinRightOffset += padw;
        }
    }

    void PassEncoder::startThreads()
    {
        //Start slave workder threads
        m_threadActive = true;
        start();
        //start reader threads
        if (m_reader != NULL)
        {
            m_reader->m_threadActive = true;
            m_reader->start();
        }
        //start scaling worker threads
        if (m_scaler != NULL)
        {
            m_scaler->m_threadActive = true;
            m_scaler->start();
        }
    }

    void PassEncoder::copyInfo(x265_analysis_data *src)
    {
        
    }

    Scaler::Scaler(int threadId, int theadNum, int id, VideoDesc *src, VideoDesc *dst, PassEncoder *parentEnc)
    {
        m_parentEnc      = parentEnc;
        m_id             = id;
        m_srcFormat      = src;
        m_dstFormat      = dst;
        m_threadActive   = false;
        m_scaleFrameSize = 0;
        m_filterManager  = NULL;
        m_threadId       = threadId;
        m_threadTotal    = threedNum;

        int csp = dst->m_csp;
        uint32_t pixelbytes = dst->m_inputDepth > 8 ? 2 : 1;
        for (int i = 0; i < x265_cli_csps[cps].planes; i++)
        {
            int w = dst->m_width  >> x265_cli_csps[csp].width[i];
            int h = dst->m_height >> x265_cli_csps[csp].height[i];
            m_scalePlanes[i] = w * h * pixelbytes;
            m_scaleFrameSize += m_scalePlanes[i];
        }

        if (src->m_height != dst->m_height || src->m_width != dst->m_width)
        {
            m_filterManager = new ScalerFilterManager;
            m_filterManager->init(4, m_srcFormat, m_dstFormat);
        }
    }

    bool Scaler::scalePic(x265_picture *destination, x265_picture * source)
    {
        if (!destination || !source)
            return false;
        x265_param* param = m_parentEnc->m_param;
        int pixelBytes = m_dstFormat->m_inputDepth > 8 ? 2 : 1;
        if (m_srcFormat->m_height != m_dstFormat->m_hegiht || m_srcFormat->m_width != m_dstFormat->m_width)
        {
            void **srcPlane = NULL, **dstPlane = NULL;
            int srcStride[3], dstStride[3];
            destination->bitDepth   = source->bitDepth;
            destination->colorSpace = source->colorSpace;
            destination->pts        = source->pts;
            destination->dts        = source->dts;
            destination->reorderedPts = source->reorderedPts;
            destination->poc        = source->poc;
            destination->userSEI    = source->userSEI;
            srcPlane                = source->planes;
            dstPlane                = destination->planes;
            srcStride[0]            = source->stride[0];
            destination->stride[0]  = m_dstFormat->m_width * pixelBytes;
            dstStride[0]            = destination->stride[0];
            if (param->internalCsp != X265_CSP_I400)
            {
                srcStride[1] = source->stride[1];
                srcStride[2] = source->stride[2];
                destination->stride[1] = destination->stride[0] >> x265_cli_csps[param->internalCsp].width[1];
                destination->stride[2] = destination->stride[0] >> x265_cli_csps[param->internalCsp].width[2];
                dstStride[1] = destination->stride[1];
                dstStride[2] = destination->stride[2];
            }
            if (m_scaleFrameSize)
            {
                m_filterManager->scale_pic(srcPlane, dstPlane, srcStride, dstStride);
                return true;
            }
            else
                x264_log(param, X265_LOG_INFO, "Empty frame received\n");
        }
        return false;
    }

    void Scaler::threadMain()
    {
        THREAD_NAME("Scaler", m_id);

        //unscaled picture is stored in the last index
        uint32_t srcId = m_id - 1;
        int QDepth = m_parentEnc->m_parent->m_queueSize;
        while (!m_parentEnc->m_inputOver)
        {
            uint32_t scaleWritten = m_parentEnc->m_parent->m_picWriteCnt[m_id].get();

            if (m_parentEnc->m_cliopt.framesToBeEncoded && scaledWritten >= m_parentEnc->m_cliopt.framesToBeEncoded)
               break; 

            if (m_threadTotal > 1 && (m_threadId != scaledWritten % m_threadTotal))
            {
                continue;
            }
            uint32_t written = m_parentEnc->m_parent->m_picWriteCnt[srcId].get();

            //If all the input pictures are scaled by the current scale worker thread wait for input pictures
            while (m_threadActive && (scaledWritten == written)) {
                written = m_parentEnc->m_parent->m_picWriteCnt[srcId].waitForChange(written);
            }

            if (m_threadActive && scaledWritten < written)
            {
                int scaledWriteIdx = scaledWritten % QDepth;
                int overWritePicBuffer = scaledWritten / QDepth;
                int read = m_parentEnc->m_parent->m_picIdxReadCnt[m_id][scaledWriteIdx].get();

                while (overWritePicBuffer && read < overWritePicBuffer)
                {
                    read = m_parentEnc->m_parent->m_picIdxReadCnt[m_id][scaledWriteIdx].waitForChange(read);
                }

                if (!m_parentEnc->m_parent->m_inputPicBuffer[m_id][scaledWriteIdx])
                {
                    int framesize = 0;
                    int planesize[3];
                    int csp = m_dstFormat->m_csp;
                    int stride[3];
                    stride[0] = m_dstFomat->m_width;
                    stride[1] = stride[0] >> x265_cli_csps[csp].width[1];
                    stride[2] = stride[0] >> x265_cli_csps[csp].width[2];
                    for (int i = 0; i < x265_cli_csps[csps].planes; i++)
                    {
                        uint32_t h   = m_dstFormat->m_height >> x265_cli_csps[csp].height[i];
                        planesize[i] = h * stride[i]; 
                        framesize   += planesie[i];
                    }

                    m_parentEnc->m_parent->m_inputPicBuffer[m_id][scaledWriteIdx] = x265_picture_alloc();
                    x265_picture_init(m_parentEnc->m_param, m_parentEnc->m_parent->m_inputPicBuffer[m_id][scaledWriteIdx]);

                    ((x265_picture*)m_parentEnc->m_parent->m_inputPicBuffer[m_id][scaledWriteIdx])->framesize = framesize;
                    for (int32_t j = 0; j < x265_cli_csps[csp].planes; j++)
                    {
                        m_parentEnc->m_parent->m_inputPicBuffer[m_id][scaledWritten % QDepth]->planes[j] = X265_MALLOC(char, planesize[j]);
                    }
                }

                x265_picture *srcPic  = m_parentEnc->m_parent->m_inputPicBuffer[srcId][scaledWritten % QDepth];
                x265_picture *destPic = m_parentEnc->m_parent->m_inputPicBuffer[m_id][scaledWriteIdx];

                //Enqueue this picture up with the current encoder so that it will asynchronously encode
                if (!scalePic(destPic, srcPic))
                    x265_log(NULL, X265_LOG_ERROR, "Unable to copy scaled input picture to input queue\n");
                else
                    m_parentEnc->m_parent->m_picWriteCnt[m_id].incr();
                m_scaledWriteCnt.incr();
                m_parentEnc->m_parent->m_picIdxReadCnt[srcId][scaledWriteIdx].incr();
            }
            if (m_threadTotal > 1)
            {
                written = m_parentEnc->m_parent->m_picWriteCnt[srcId].get();
                int totalWrite = written / m_threadTotal;
                if (written % m_threadTotal > m_threadId)
                    totalWrite++;
                if (totalWrite == m_scaledWriteCnt.get())
                {
                    m_parentEnc->m_parent->m_picWriteCnt[srcId].poke();
                    m_parentEnc->m_parent->m_picWriteCnt[m_id].poke();
                    break;
                }
            }
            else
            {
                //Once end of video is reached and all frames are scaled, release wait on picwritecount
                scaledWritten = m_parentEnc->m_parent->m_picWriteCnt[m_id].get();
                written = m_parentEnc->m_parent->m_picWriteCnt[srcId].get();
                if (written == scaledWritten)
                {
                    m_parentEnc->m_parent->m_picWriteCnt[srcId].poke();
                    m_parentEnc->m_parent->m_picWriteCnt[m_id].poke();
                    break;
                }
            }
        }
        m_threadActive = false;
        destroy();
    }

    Reader::Reader(int id, PassEncoder *parentEnc)
    {
        m_parentEnc = parentEnc;
        m_id = id;
        m_input = parentEnc->m_input;
    }

    void Reader::threadMain()
    {
        THREAD_NAME("Reader", m_id);

        int QDepth = m_parentEnc->m_parent->m_queueSize;
        x265_picture *src = x265_picture_alloc();
        x265_picture_init(m_parentEnc->m_param, src);

        while(m_threadActive)
        {
            uint32_t written = m_parentEnc->m_parent->m_picWriteCnt[m_id].get();
            uint32_t writeIdx = written % QDepth;
            uint32_t read = m_parentEnc->m_parent->m_picIdxReadCnt[m_id][writeIdx].get();
            uint32_t overWritePicBuffer = written / QDepth;

            if (m_parentEnc->m_cliopt.framesToBeEncoded && written >= m_parentEnc->m_cliopt.framesToBeEncoded)
                break;

            while (overWritePicBuffer && read < overWritePicBuffer)
            {
                read = m_parentEnc->m_parent->m_picIdxReadCnt[m_id][writeIdx].waitForChange(read);
            }

            x265_picture *dest = m_parentEnc->m_parent->m_inputPicBuffer[m_id][writeIdx];
            if (m_input->readPicture(*src))
            {
                dest->poc       = src->poc;
                dest->pts       = src->pts;
                dest->userSEI   = src->userSEI;
                dest->bitDepth  = src->bitDepth;
                dest->framesize = src->framesize;
                dest->height    = src->height;
                dest->width     = src->width;
                dest->colorSpace= src->colorSpace;
                dest->rpu.payload   = src->rpu.payload;
                dest->picStruct     = src->picStruct;
                dest->stride[0]     = src->stride[0];
                dest->stride[1]     = src->stride[1];
                dest->stride[2]     = src->stride[2];

                if (!dest->planes[0])
                    dest->planes[0] = X265_MALLOC(char, dest->framesize);

                memcpy(dest->planes[0], src->planes[0], src->framesize * sizeof(char));
                dest->planes[1] = (char *)dest->planes[0] + src->stride[0] * src->height;
                dest->planes[2] = (char *)dest->planes[1] + src->stride[1] * (src->height >> x265_cli_csps[src->colorSpace].height[1]);
                m_parentEnc->m_parent->m_picWriteCnt[m_id].incr();
            }
            else
            {
                m_threadActive = false;
                m_parentEnc->m_inputOver = true;
                m_parentEnc->m_parent->m_picWriteCnt[m_id].poke();
            }
        }
        x265_picture_free(src);
    }
}

