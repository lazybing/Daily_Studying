#ifndef X265_RATECONTROL_H
#define X265_RATECONTROL_H

#include "common.h"
#include "sei.h"

namespace X265_NS {
// encoder namespace
class Encoder;
class Frame;
class SEIBufferingPeriod;
#define BASE_FRAME_DURATION 0.04

/*Arbitrary limitations as a sanity check.*/
#define MAX_FRAME_DURATION 1.00
#define MIN_FRAME_DURATION 0.01

#define MIN_AMORTIZE_FRAME 10
#define MIN_AMORTIZE_FRACTION 0.2
#define CLIP_DURATION(f) x265_clip3(MIN_FRAME_DURATION, MAX_FRAME_DURATION, f)

/*Scenecut Aware QP*/
#define WINDOW1_DELTA   0   /*The offset for the frame coming in the window-1*/
#define WINDOW2_DELTA   0   /*The offset for the frame coming in the window-2*/
#define WINDOW3_DELTA   0   /*The offset for the frame coming in the window-3*/

struct Predictor
{
    double conffMin;
    double coeff;
    double count;
    double decay;
    double offset;
};

struct HRDTiming
{
    double cpbInitialAT;
    double cpbFinalAT;
    double dpbOutputTime;
    double cpbRemovalTime;
};

struct RateControlEntry
{
    Predictor   rowPreds[3][2];
    Predictor*  rowPred[3];

    int64_t     lastSatd;   /*Contains the picture cost of the previous frame, required for resetAbr and VBV*/
    int64_t     leadingNoBSatd;
    int64_t     rowTotalBits;   /*update cplxrsum and totalbits at the end of 2 rows*/
    double      blurredComplexity;
    double      qpaRc;
    double      qpAq;
    double      aRceq;
    double      qpPrev;
    double      frameSizePlanned;   /*frame Size decided by RateControl before encoding the frame*/
    double      bufferRate;
    double      movingavgSum;
    double      rowCplxrSum;
    double      qpNoVbv;
    double      bufferFill;
    double      targetFill;
    bool        vbvEndAdj;      
    double      frameDuration;
    double      clippedDuration;
    double      frameSizeEstimated; /*hold frameSize, updated from cu level vbv rc*/
    double      frameSizeMaximum;   /*max frame Size according to minCR restrictions and level of the video*/
    int         sliceType;
    int         bframes;
    int         poc;
    int         encoderOrder;
    bool        bLastMiniGopBFrame;
    bool        isActive;
    double      amortizeFrames;
    double      amortizeFraction;
    /*Required in 2-pass rate control*/
    uint64_t    expectedBits;   /*total expected bits up to the current frame(current one exculed)*/
    double      iCuCount;
    double      pCuCount;
    double      skipCuCount;
    double      expectedVbv;
    double      qScale;
    double      newQScale;
    double      newQp;
    int         mvBits;
    int         miscBits;
    int         coeffBits;
    bool        keptAsRef;
    bool        scenecut;
    bool        isIdr;
    SEIPictureTiming    *picTimingSEI;
    HRDTiming           *hrdTiming;
    int         rpsIdx;
    RPS         rpsData;
    bool        isFadeEnd;
};

class RateControl
{
    public:
        x265_param* m_param;
        Slice*      m_curSlice; //all info about the current frame
        SliceType   m_sliceType;//Current frame type
        int         m_ncu;      //number of CUs in a frame
        int         m_qp;       //updated qp for current frame

        //Zone reconfiguration
        double*     m_relativeComplexity;
        int         m_zoneBufferIdx;

        bool        m_isAbr;
        bool        m_isVbv;
        bool        m_isCbr;
        bool        m_singleFrameVbv;
        bool        m_isGrainEnabled;
        bool        m_isAbrReset;
        bool        m_isNextGop;
        bool        m_initVbv;
        int         m_lastAbrResetPoc;

        int         m_lastScenecut;
        int         m_lastScenecutAwareIFrame;
        double      m_rateTolerance;
        double      m_frameDuration;    //current frame duration in seconds
        double      m_bitrate;
        double      m_rateFactorConstant;
        double      m_bufferSize;
        double      m_bufferFillFinal;  //real buffer as of the last finished frame
        double      m_unclippedBufferFillFinal; //real unclipped buffer as of the last finished frame used to log in CSV
        double      m_bufferFill;       //planned buffer, if all in-progress frames hit their bit budget
        double      m_bufferRate;       //#of bits added to buffer_fill after each frame
        double      m_vbvmaxRate;       //in kbps 
        double      m_rateFactorMaxIncrement;   //Don't allow RF above (CRF + this value)
        double      m_rateFactorMaxDecrement;   //don't allow RF below(this value)
        double      m_avgPFrameQp;
        double      m_bufferFillActual;
        double      m_bufferExcess;
        bool        m_isFirstminiGop;
        Predictor   m_pred[4];  //Slice predictors to predict bits for each Slice type - I, P, Bref and B
        int64_t     m_leadingNoBSatd;
        int         m_predType; //Type of slice predictors to be used -depends on the slice type
        double      m_ipOffset;
        double      m_pbOffset;
        int64_t     m_bframeBits;
        int64_t     m_currentSatd;


};
}

#endif
