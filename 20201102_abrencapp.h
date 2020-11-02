#ifndef ABR_ENCODE_H
#define ABR_ENCODE_H

#include "x265.h"
#include "scaler.h"
#include "threading.h"
#include "x265cli.h"

namespace X265_NS {
    class PassEncoder;
    class Scaler;
    class Reader;

    class AbrEncoder
    {
    public:
        uint8_t             m_numEncoders;
        PassEncoder         **m_passEnc;
        uint32_t            m_queueSize;
        ThreadSafeInteger   m_numActiveEncodes;

        x265_picture        ***m_inputPicBuffer;
        x265_analysis_data  **m_analysisBuffer;
        int                 **m_readFlag;

        ThreadSafeInteger   *m_picWriteCnt;
        ThreadSafeInteger   *m_picReadCnt;
        ThreadSafeInteger   **m_picIdxReadCnt;
        ThreadSafeInteger   *m_analysisWriteCnt;
        ThreadSafeInteger   *m_analysisReadCnt;
        ThreadSafeInteger   *m_analysisWrite;
        ThreadSafeInteger   *m_analysisRead;

        AbrEncoder(CLIOptions cliopt[], uint8_t numEncodes, int& ret);
        bool allocBuffers();
        void destroy();
    };

    class PassEncoder:public Thread
    {
    public:
        uint32_t            m_id;
        x265_param          *m_param;
        AbrEncoder          *m_parent;
        x265_encoder        *m_encoder;
        Reader              *m_reader;
        Scaler              *m_scaler;
        bool                m_inputOver;

        int                 m_threadActive;
        int                 m_lastIdx;
        uint32_t            m_outputNalsCount;

        x265_picture        **m_inputPicBuffer;
        x265_analysis_data  **m_analysisBuffer;
        x265_nal            **m_outputNals;
        x265_picture        **m_outputRecon;

        CLIOptions          m_cliopt;
        InputFile           *m_input;
        const char          *m_reconPlayCmd;
        FILE                *m_qpfile;
        FILE                *m_zoneFile;
        FILE                *m_dolbyVisionRpu;  //File containing Dolby Vision BL RPU metadata

        int                 m_ret;

        PassEncoder(uint32_t id, CLIOptions cliopt, AbrEncoder *parent);
        int init(int &result);
        void setReuseLevel();

        void startThreads();
        void copyInfo(x265_analysis_data *src);

        bool readPicture(x265_picture *);
        void destroy();

    private:
        void threadMain();
    };

    class Scaler:public Thread
    {
    public:
        PassEncoder         *m_parentEnc;
        int                 m_id;
        int                 m_scalePlanes[3];
        int                 m_scaleFrameSize;
        uint32_t            m_threadId;
        uint32_t            m_threadTotal;
        ThreadSafeInteger   m_scaledWriteCnt;
        VideoDesc           *m_srcFormat;
        VideoDesc           *m_dstFormat;
        int                 m_threadActive;
        ScalerFilterManager *m_filterManager;

        Scaler(int threadId, int threadNum, int id, VideoDesc *src, VideoDesc *dst, PassEncoder *parentEnc);
        bool scalePic(x265_picture *destination, x265_picture *source);
        void threadMain();
        void destroy()
        {
            if (m_filterManager)
            {
                delete m_filterManager;
                m_filterManager = NULL;
            }
        }
    };

    class Reader:public Thread
    {
    public:
        PassEncoder *m_parentEnc;
        int         m_id;
        InputFile   *m_input;
        int         m_threadActive;

        Reader(int id, PassEncoder *parentEnc);
        void threadmain();
    };
}

#endif
