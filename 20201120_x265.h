#ifndef X265_H
#define X265_H

#include <stdint.h>
#include <stdio.h>
#include "x265_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * x265_encoder:
 *      opaque handler for encoder
 */
typedef struct x265_encoder x265_encoder;

/*
 * x265_picyuv:
 *      opaque handler for PicYuv
 */
typedef struct x265_picyuv x265_picyuv;

#define X265_LOOKAHEAD_MAX 250

typedef struct x265_lookahead_data
{
    int64_t     plannedSatd[X265_LOOKAHEAD_MAX + 1];
    uint32_t    *vbvCost;
    uint32_t    *intraVbvCost;
    uint32_t    *satdForVbv;
    uint32_t    *intraSatdForVbv;
    int         keyframe;
    int         lastMiniGopBFrame;
    int         plannedType[X265_LOOKAHEAD_MAX + 1];
    int64_t     dts;
    int64_t     reorderedPts;
}x265_lookahead_data;

/*
 * x265 input parameters
 *
 * For version safety you may use x265_param_allo/free() to manage the
 * allocation of x265_param instances, and x265_param_parse() to assign values
 * by name. By never dereferencing param fields in your own code you can treat
 * x265_param as an opaque data structure
 */
typedef struct x265_param
{
    /*
     * x265_param_default() will auto-detect this cpu capability bitmap. it si
     * recommended to not change this value unless you know the cpu detection is
     * somehow flawed on your target hardware. The asm function tables are process global,
     * the first encoder configures them for all encoders
     */
    int cpuid;

    /*==Parallelism Features==*/
    /*
     * Number of concurrently encoded frames between 1 and X265_MAX_FRAME_THREADS
     * or 0 for auto-detection. By default x265 will use a number of frame
     * threads empirically determined to be optimal for your CPU core count,
     * between 2 and 6. Using more than one frame thread causes motion search
     * in the down direction to be clamped but otherwise encode behavior is
     * unaffected. With CQP rate control the output bitstream is deterministic
     * for all values of frameNumThreads greater than 1. All other forms of
     * frame threads because the extra concurrency adds uncertainty to the
     * bitrate estimations. Frame parallelism is generally limited by the the
     * number of CU rows
     *
     * When thread pools are used, each frame thread is assigned to a single
     * pool and the frame thread itself is given the node affinity of its pool.
     * But when no thread pools are used no node affinity is assigned.
     */
    int     frameNumThreads;

    const char *numaPools;

    /*
     * Enable wave front parallel processing, greatly increases parallelism for
     * less than 1% compression efficiency loss. Requires a thread pool, enabled
     * by default;
     */
    int bEnableWavefront;

    /*
     * Use multiple threads to measure CU mode costs. Recommended for many core CPUs.
     * On RD levels less than 5, it may not offload enough work to warrant the
     * overhead. It is useful with the slow preset since it has the 
     * rectangular predictions enabled. At RD level 5 and 6 (preset slower and below),
     * this feature should be an unambiguous win if you have CPU cores 
     * available for work. Default disabled.
     */
    int bDistributeModeAnalysis;

    /*
     * Use multiple threads to perform motion estimation to (ME to one reference
     * per thread). Recommended for many core CPUs. The more references the more
     * motion searches there will be to distribute. This option is often not a 
     * win, particularly in video sequences with low motion. Default disabled
     */
    int bDistributeMotionEstimation;

    /*==Logging Features==*/
    /*==Internal Picture Specification==*/
    /*==Profile/Tier/Level==*/
    /*==Bitstream Options==*/
    /*==GOP structure and slice type decisions(lookahead)==*/
    /*==Coding Unit(CU) definitions==*/
    /*==Residual Quadtree Transform Unit(TU) definitions==*/
    /*==Intra Coding Tools==*/
    /*==Inter Coding Tools==*/
    /*==Analysis tools==*/
    /*==Rate Control==*/

    /*
     * The lossless flag enable true lossless coding, bypassing scaling,
     * transform, quantization and in-loop filter processes. This is used for
     * ultra-high bitrates with zero loss of quality. It implies no rate control
     */
    int bLossless;

    /*
     * Generally a small signed integer which offsets the QP used to quantize
     * the Cb/Cr chroma residual (delta from luma QP specified by rate-control).
     * Default is 0, which is recommended.
     */
    int cbQpOffset;
    int crQpOffset;

    int preferedTransferCharacteristics;
    int pictureStructure;
    struct{
        /*
         * Explcit mode of rate-control, necessary for API users. It must 
         * be one of the X265_RC_METHODS enum values.
         */
        int rateCongtrolMode;

        /*
         * Base QP to use for Constant QP rate control. Adaptive QP may alter
         * the QP used for each block. If a QP is specified on the command line
         * CQP rate control is implied.Default:32
         */
        int qp;

        /*
         * target bitrate for Average Bitrate(ABR) rate control. If a non-zero
         * bitrate is specified on the command line, ABR is implied. Default 0.
         */
        int bitrate;

        /*
         * qComp sets the quantizer curve compression factor. It weights the frame
         * quantizer based on the complexity of residual (measured by lookahead).
         * Default value is 0.6. Increasing it to 1 will effectively generate GOP
         */
        double qCompress;

        /*
         * QP offset between I/P and P/B frames. Default ipfactor: 1.4
         * Default pbFactor: 1.3
         */
        double ipFactor;
        double pbFactor;

        /*
         * Ratefactor constant: targets a certain constant "quality"
         * Acceptable values between 0 and 51. Default value: 28
         */
        double rfConstant;

        /*
         * Max QP difference between frames. Default: 4
         */
        int qpStep;

        /*
         * Enable adaptive quantization. This mode distributes available bits between all
         * CTUs of a frame, assigning more bits to low complexity areas. Turning
         * this ON will ususally affect PSNR negative, however SSIM and visual quality
         * generally improves. Default: X265_AQ_AUTO_VARIANCE
         */
        int aqMode;

        /*
         * Enable adaptive quantizaion.
         * It scales the quantization step size according to the spatial activity of one
         * coding unit relative to frame average spatial activity. This AQ method utilizes
         * the minimum variance of sub-unit in each coding unit to represent the coding
         * unit spatial complexity.
         */
        int hevcAq;

        /*
         * Sets the strength of AQ bias towards low detail CTUs. Valid only if
         * AQ is enabled. Default value: 1.0. Acceptable values between 0.0 and 3.0
         */
        double aqStrength;

        /*
         * Delta QP range by QP adatation based on a psycho-visual model.
         * Acceptable values between 1.0 to 6.0
         */
        double qpAdaptationRange;

        /*
         * Sets the maximum rate the VBV buffer should be assumed to refill at
         * Default is zero
         */
        int vbvMaxBitrate;

        /*
         * Sets the size of the VBV buffer in kilobits. Default is zero
         */
        int vbvBufferSize;

        /*
         * Sets how full the VBV buffer must be before playback starts. If it si less than
         * 1, then the initial fill is vbv-init * vbvBufferSize. Otherwise, it is
         * interpreted as the initial fill in kbits. Default is 0.9
         */
        double vbvBufferInit;

        /*
         * Enable CUTree rate-control. This keeps track of the CUs that propagate temporally
         * across frames and assigns more bits to these CUs. Improves encode efficiency.
         * Default:enabled
         */
        int cuTree;

        /*
         * In CRF mode, maximum CRF as caused by VBV, 0 implies no limit
         */
        double rfConstantMax;

        /*
         * In CRF mode, minimum CRF as caused by VBV
         */
        double rfConstantMin;

        //Multi-pass encoding
        /*
         * Enable writing the stats in a multi-pass encode to the stat output file
         */
        int bStatWrite;

        /*
         * Enable loading data from the stat input file in a multi pass encode
         */
        int bStatRead;

        /*
         * Filename of the 2pass output/input stats file, if unspecified the
         * encoder will default to using x265_2pass.log
         */
        const char *statFilename;

        /*
         * temporally blur quants
         */
        double qblur;

        /*
         * temporally blur complexity
         */
        double complexityBlur;

        /*
         * Enable slow and a more detailed first pass encode in multi pass rate control
         */
        int bEnableSlowFirstPass;

        /*
         * rate-control overrides
         */
        int zoneCount;
        x265_zone *zones;

        /*
         * number of zones in zone-file
         */
        int zonefileCount;

        /*
         * Specify a text file which contains MAX_MAX_QP + 1 floating point
         * values to be copied into x265_lambda_tab and a second set of
         * MAX_MAX_QP + 1 floating point vlaues for x265_lambda2_tab. All values
         * are separated by comma, space or newline. Text after a hash(#) is
         * ignored. The lambda tables are process-global, so these new labmda
         * values will affect all encoders in the same process
         */
        const char *lambdaFileName;

        /*
         * Enable stricter conditions to check bitrate deviations in CBR mode. May compromise
         * quality to maintain bitrate adherence
         */
        int bStrictCbr;

        /*
         * Enable adaptive quantization at CU granularity. This parameter specifies
         * the minimum CU size at which QP can be adjusted, i.e. Quantization Group
         * (QP) size. Allowed values are 64, 32, 16, 8 provided it falls within the
         * inclusuve range[maxCUSize, minCUSize]. Experimental, default: maxCUSize
         */
        uint32_t qgSize;

        /*
         * internally enable if tune grain is set
         */
        int bEnableGrain;

        /*
         * sets a hard upper limit on QP
         */
        int qpMax;

        /*
         * sets a hard lower limit on QP
         */
        int qpMin;

        /*
         * internally enable if tune grain is set
         */
        int bEnableConstVbv;
    }rc;
    };
};

#ifdef __cplusplus
}
#endif


#endif
