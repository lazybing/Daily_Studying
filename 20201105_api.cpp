#include "common.h"
#include "bitstream.h"
#include "param.h"

#include "encoder.h"
#include "entropy.h"
#include "level.h"
#include "nal.h"
#include "bitcost.h"
#include "svt.h"

#if ENABLE_LIBVMAF
#include "libvmaf/libvmaf.h"
#endif

//multilib namesapec reflectors
#if LINKED_8BIT
namespace x265_8bit {
const x265_api *x265_api_get(int bitDepth);
const x265_api *x265_api_query(int bitDepth, int apiVersion, int *err);
}
#endif

#if LINKED_10BIT
namespace x265_10bit {
const x265_api *x265_api_get(int bitDepth);
const x265_api *x265_api_query(int bitDepth, int apiVersion, int *err);
}
#endif

#if LINKED_12BIT
namespace x265_12bit {
const x265_api *x265_api_get(int bitDepth);
const x265_api *x265_api_query(int bitDepth, int apiVersion, int *err);
}
#endif

#if EXPORT_C_API
//these functions are exported as C functions
using namespace X265_NS;
extern "C" {
#else
//these functions exist within privae namespace(multilib)
namespace X265_NS {
#endif

x265_encoder *x265_encoder_open(x265_param *p)
{
    if (!p)
        return NULL;
    Encoder *encoder = NULL;
    x265_param *param       = PARAM_NS::x265_param_alloc();
    x265_param *latestParam = PARAM_NS::x265_param_alloc();
    x265_param *zoneParam   = PARAM_NS::x265_param_alloc();

    if (param) PARAM_NS::x265_param_default(param);
    if (latestParam) PARAM_NS::x265_param_default(latestParam);
    if (zoneParam) PARAM_NS::x265_param_default(zoneParam);

    if (!param || !latestPram || !zoneParam)
        goto fail;
    if (p->rc.zoneCount || p->rc.zonefileCount)
    {
        int zoneCount           = p->rc.zonefileCount ? p->rc.zonefileCount : p->rc.zoneCount;
        param->rc.zones         = x265_zone_alloc(zoneCount, !!p->rc.zonefileCount);
        latestParam->rc.zones   = x265_zone_alloc(zoneCount, !!p->rc.zonefileCount);
        zoneParam->rc.zones     = x265_zone_alloc(zoneCount, !!p->rc.zonefileCount);
    }

    x265_copy_params(param, p);
    x265_copy_params(latestParam, p);
    x265_copy_params(zoneParam, p);
    x265_log(param, X265_LOG_INFO, "HEVC encoder version %s\n", PFX(version_str));
    x265_log(param, X265_LOG_INFO, "build info %s\n", PFX(build_info_str));

    encoder = new Encoder;

    x265_seetup_primitives(param);

    if (x265_check_params(param))
        goto fail;

    if (!param->rc.bEnableSlowFirstPass)
        PARAM_NS::x265_param_apply_fastfirstpass(param);

    //may change params for auto-detect, etc
    encoder->configure(param);
    if (encoder->m_aborted)
        goto fail;
    //may change rate control and CPB params
    if (!enforceLevel(*param, encoder->m_vps))
        goto fail;

    //will detect and set profile/tier/level in VPS
    determineLevel(*param, encoder->m_vps);

    if (!param->bAllowNonConformance && encoder->m_vps.ptl.profileIdc == Profile::NONE)
    {
        x265_log(param, X265_LOG_INFO, "non-conformant bitstreams not allowed(--allow-no)");
        goto fail;
    }

    encoder->create();
    p->frameNumThreads = encoder->m_param->frameNumThreads;

    if (!param->bResetZoneConfig)
    {
        param->rc.zones = X265_MALLOC(x265_zone, param->rc.zonefileCount);
        for (int i = 0; i < param->rc.zonefileCount; i++)
        {
            param->rc.zones[i].zoneParam = X265_MALLOC(x265_param, 1);
            memcpy(param->rc.zones[i].zoneParam, param, sizeof(x265_param));
            param->rc.zones[i].relativeComplexity = X265_MALLOC(double, param->reconfigWindowSize);
        }
    }

    memcpy(zoneParam, param, sizeof(x265_param));
    for (int i = 0; i < param->rc.zonefileCount; i++)
    {
        param->rc.zones[i].startFrame = -1;
        encoder->configureZone(zoneParam, param->rc.zones[i].zoneParam);
    }

    //Try to open CSV file handle
    if (encoder->m_param->csvfn)
    {
        encoder->m_param->csvfpt = x265_csvlog_open(encoder->m_param);
        if (!encoder->m_param->csvfpt)
        {
            x265_log(encoder->m_param, X265_LOG_ERROR, "Unable to open CSV log file\n");
            encoder->m_aborted = true;
        }
    }

    encoder->m_latestParam = latestParam;
    x265_copy_params(latestParam, param);
    if (encoder->m_aborted)
        goto fail;

    x265_print_params(param);
    return encoder;

fail:
    delete encoder;
    PARAM_NS::x265_param_free(param);
    PARAM_NS::x265_param_free(latestParam);
    PARAM_NS::x265_param_free(zoneParam);
    return NULL;
}

int x265_encoder_headers(x265_encoder *enc, x265_nal **pp_nal, uint32_t *pi_nal)
{
    if (pp_nal && enc)
    {
        Encoder *encoder = static_cast<Encoder *>(enc);
        Entropy sbacCoder;
        Bitstream bs;
        if (encoder->m_param->rc.bStatRead && encoder->m_param->bMultiPassOptRPS)
        {
            if (!encoder->computeSPSRPSIndex())
            {
                encoder->m_aborted = true;
                return -1;
            }
        }
        encoder->getStreamHeaders(encoder->m_nalList, sbacCoder, bs);
        *pp_nal = &encoder->m_nalList.m_nal[0];
        if (pi_nal) *pi_nal = encoder->m_nalList.m_numNal;
        return encoder->m_nalList.m_occupancy;
    }

    if (enc)
    {
        Encoder *encoder = static_cast<Encoder *>(enc);
        encoder->m_aborted = true;
    }
    return -1;
}


void x265_encoder_parameters(x265_encoder *enc, x265_param *out)
{
    if (enc && out)
    {
        Encoder *encoder = static_cast<Encoder *>(enc);
        x265_copy_params(out, encoder->m_param);
    }
}

int x265_encoder_reconfig(x265_encoder *enc, x265_param *param_in)
{
    if (!enc || !param_in)
        return -1;
    x265_param save;
    Encoder *encoder = static_cast<Encoder *>(enc);
    if (encoder->m_param->csvfn == NULL && param_in->csvfpt != NULL)
        encoder->m_param->csvfpt = param_in->csvfpt;
    if (encoder->m_latestParam->forceFlush != param_in->forceFlush)
        return encoder->reconfigureParam(encoder->m_latestParam, param_in);
    bool isReconfigureRc = encoder->isReconfigureRc(encoder->m_latestParam, param_in);
    if ((encoder->m_reconfigure && !isReconfigureRc) || (encoder->m_reconfigureRc && isReconfigureRc))
        return 1;
    if (encoder->m_latestParam->rc.zoneCount || encoder->m_latestParam->rc.zonefileCount)
    {
        int zoneCount = encoder->m_latestParam->rc.zonefileCount ? encoder->m_latestParam->rc.zonefileCount : encoder->m_latestParam->rc.zoneCount;
        save.rc.zones = x265_zone_alloc(zoneCount, !!encoder->m_latestParam->rc.zonefileCount);
    }
    x265_copy_params(&save, encoder->m_latestParam);
    int ret = encoder->reconfigureParam(encoder->m_latestParam, param_in);
    if (ret)
    {
        //reconfigure failed, recover saved param set
        x265_copy_params(encoder-.m_latestParam, &save);
        return -1;
    }
    else
    {
        encoder->configure(encoder->m_latestParam);
        if (encoder->m_latestParam->scalingLists && encoder->m_latestParam->scalingLists != encoder->m_param->scalingLists)
        {
            if (encoder->m_param->bRepeatHeaders)
            {
                if (encoder->m_scalingList.parseScalingList(encoder->m_latestParam->scalingLists))
                {
                    x265_copy_params(encoder->m_latestParam, &save);
                    return -1;
                }
                encoder->m_scalingList.setupQuantMatrices(encoder->m_param->internalCsp);
            }
            else
            {
                x265_log(encoder->m_param, X265_LOG_ERROR, "Repeat headers is turned OFF, cannot reconfigure scalinglists\n");
                x265_copy_params(encoder->m_latestParam, &save);
                return -1;
            }
        }
        if (!isReconfigureRc)
            encoder->m_reconfigure = true;
        else if (encoder->m_reconfigureRc)
        {
            VPS saveVPS;
            memcpy(&saveVPS.ptl, &encoder->m_vps.ptl, sizeof(saveVPS.ptl));
            determineLevel(*encoder->m_latestParam, encoder->m_vps);
            if (saveVPS.ptl.profileIdc != encoder->m_vps.ptl.profileIdc || saveVPS.ptl.levelIdc != encoder->m_vps.ptl.levelIdc
                || saveVPS.ptl.tierFlag != encoder->m_vps.ptl.tierFlag)
            {
                x265_copy_params(encoder->m_latestParam, &save);
                memcpy(&encoder->m_vps.ptl, &saveVPS.ptl, sizeof(saveVPS.ptl));
                encoder->m_reconfigureRc = false;
            }
        }
        encoder->printReconfigureParams();
    }
    //zones support modifying num of Refs.
    //Requires determinig level at each zone start
    if (encoder->m_param->rc.zonefileCount)
        determineLevel(*encoder->m_latestParam, encoder->m_vps);
    return ret;
}

int x265_encoder_reconfig_zone(x265_encoder* enc, x265_zone *zone_in)
{
    if (!enc || !zone_in)
        return;

    Encoder *encoder = static_cast<Encoder *>(enc);
    int read    = encoder->zoneReadCount[encoder->m_zoneIndeencoder->m_zoneIndex].get();
    int write   = encoder->zoneWriteCount[encoder->m_zoneIndeencoder->m_zoneIndex].get();

    x265_zone *zone         = &(encoder->m_param->rc).zones[encoder->m_zoneIndex];
    x265_param *zoneParam   = zone->zoneParam;

    if (write && (read < write))
    {
        read = encoder->zoneReadCount[encoder->m_zoneIndex].waitForChange(read);
    }

    zone->startFrame = zone_in->startframe;
    zoneParam->rc.bitrate = zone_in->zoneParam->rc.bitrate;
    zoneParam->rc.vbvMaxBitrate = zone_in->zoneParam->rc.vbvmaxBitrate;
    mempcy(zone->relativeComplexity, zone_in->relativeComplexity, sizeof(double) * encoder->m_param->reconfigWindowSize);

    encoder->zoneWriteCount[encoder->m_zoneIndex].incr();
    encoder->m_zoneIndex++;
    encoder->m_zoneIndex %= encoder->m_param->rc.zonefileCount;

    return 0;
}

int x265_encoder_encode(x265_encoder *enc, x265_nal **pp_nal, uint32_t *pi_nal, x265_picture *pin_in, x265_picture *pic_out)
{
    if (!enc)
        return -1;

    Encoder *encoder = static_cast<Encoder*>(enc);
    int numEncoded;

    //While flushing, we cannot return 0 until the entir stream if flushed
    do {
        numEncoded = encoder->encode(pic_in, pic_out);
    }
    while((numEncoded == 0 && !pic_in && encoder->m_numDelayedPic && !encoder->m_latestParam->forceFlush) && !encoder->m_externalFlush);
    if (numEncoded)
        encoder->m_externalFlush = false;

    //do not allow reuse of these buffers for more than one picture.
    //The encoder now owns these analysisData buffers.
    if (pic_in)
    {
        pic_in->analysisData.wt             = NULL;
        pic_in->analysisData.intraData      = NULL;
        pic_in->analysisData.interData      = NULL;
        pic_in->analysisData.distortionData = NULL;
    }

    if (pp_nal && numEncoded > 0 && encoder->m_outputCount >= encoder->m_latestParam->chunkStart)
    {
        *pp_nal = &encoder->m_nalList.m_nal[0];
        if (pi_nal) *pi_nal = encoder->m_nalList.m_numNal;
    }
    else if (pi_nal)
        *pi_nal = 0;

    if (numEncoded && encoder->m_param->csvLogLevel && encoder->m_outputCount >= encoder->m_latestParam->chunkStart)
        x265_csvlog_frame(encoder->m_param, pic_out);

    if (numEncoded < 0)
        encoder->m_aborted = true;

    return numEncoded;
}

void x265_encoder_get_stats(x265_encoder *enc, x265_stats *outputStats, uint32_t statsSizeBytes)
{
    if (enc && outputStats)
    {
        Encoder *encoder = static_cast<Encoder*>(enc);
        encoder->fetchStats(outputStats, statsSizeBytes);
    }
}

void x265_encoder_log(x265_encoder *enc, int argc, char **argv)
{
    if (enc)
    {
        Encoder *encoder = static_cast<Encoder*>(enc);
        x265_stats stats;
        encoder->fetchStats(&stats, sizeof(stats));
        int padx = encoder->m_sps.conformanceWindow.rightOffset;
        int pady = encoder->m_sps.conformanceWindow.bottomOffset;
        x265_csvlog_encode(encoder->m_param, &stats, padx, pady, argc, argv);
    }
}

void x265_encoder_close(x265_encoder *enc)
{
    if (enc)
    {
        Encoder *encoder = static_cast<Encoder*>(enc);
        encoder->stopJobs();
        encoder->printSummary();
        encoder->destroy();
        delete encoder;
    }
}

int x265_encoder_intra_refresh(x265_encoder *enc)
{
    if (!enc)
        return -1;

    Encoder *encoder = static_cast<Encoder*>(enc);
    encoder->m_bQueuedIntraRefresh = 1;
    return 0;
}

int x265_encoder_ctu_info(x265_encoder *enc, int poc, x265_ctu_info_t **ctu)
{
    if (!ctu || !enc)
        return -1;
    Encoder *encoder = static_cast<Encoder*>(enc);
    encoder->copyCutInfo(ctu, poc);
    return 0;
}

int x265_get_slicetype_poc_and_scenecut(x265_encoder *enc, int *slicetype, int *poc, int *sceneCut)
{
    if (!enc)
        return -1;
    Encoder *encoder = static_cast<Encoder *>(enc);
    if (!encoder->copySlicetypePocAndSceneCut(slicetype, poc, sceneCut))
        return 0;
    return -1;
}

int x265_get_ref_frame_list(x265_encoder *enc, x265_picyuv **l0, x265_pivyuv **l1, int sliceType, int poc, int *pocL0, int *pocL1)
{
    if (!enc)
        return -1;
    Encoder *encoder = static_cast<Encoder *>(enc);
    return encoder->getRefFrameList((PicYuv**)l0, (PicYuv **)l1, slicetype, poc, pocL0, pocL1);
}

int x265_set_analysis_data(x265_encoder *enc, x265_analysis_data *analysis_data, int poc, uint32_t cuBytes)
{
    
}

}

