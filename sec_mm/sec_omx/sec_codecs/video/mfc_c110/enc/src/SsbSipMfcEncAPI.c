/*
 * Copyright 2010 Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <utils/Log.h>

#include "SsbSipMfcApi.h"
#include "mfc_interface.h"

#define _MFCLIB_MAGIC_NUMBER     0x92241001

void *SsbSipMfcEncOpen(void)
{
    int hMFCOpen;
    unsigned int mmap_size;
    unsigned int mapped_addr;
    struct mfc_common_args EncArg;
    _MFCLIB *pCTX;

    hMFCOpen = open(SAMSUNG_MFC_DEV_NAME, O_RDWR | O_NDELAY);
    if (hMFCOpen < 0) {
        LOGE("SsbSipMfcEncOpen: MFC Open failure\n");
        return NULL;
    }

    pCTX = (_MFCLIB *)malloc(sizeof(_MFCLIB));
    if (pCTX == NULL) {
        LOGE("SsbSipMfcEncOpen: malloc failed.\n");
        close(hMFCOpen);
        return NULL;
    }
    memset(pCTX, 0, sizeof(_MFCLIB));

    /* 07-15-2011 codeworkx: added ioctl for getting memsize from kernel cause it's defined in kernel config on smdkv310 */
    pCTX->hMFC = hMFCOpen;

    mmap_size = ioctl(pCTX->hMFC, IOCTL_MFC_GET_MMAP_SIZE, &EncArg);
    if (EncArg.ret_code != MFC_RET_OK) {
        LOGE("SsbSipMfcEncOpen: IOCTL_MFC_GET_MMAP_SIZE failed(ret : %d)\n", EncArg.ret_code);
        return NULL;
    }

    mapped_addr = (unsigned int)mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, hMFCOpen, 0);
    if (!mapped_addr) {
        LOGE("SsbSipMfcEncOpen: FIMV5.0 driver address mapping failed\n");
        return NULL;
    }
    
    pCTX->magic = _MFCLIB_MAGIC_NUMBER;
    pCTX->mapped_addr = mapped_addr;
    pCTX->mapped_size = mmap_size;
    pCTX->inter_buff_status = MFC_USE_NONE;

    return (void *)pCTX;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncInit(void *openHandle, void *param)
{
    int ret_code;
    int dpbBufSize;

    _MFCLIB *pCTX;
    struct mfc_common_args EncArg;
    struct mfc_common_args user_addr_arg, phys_addr_arg;
    SSBSIP_MFC_ENC_H264_PARAM *h264_arg;
    SSBSIP_MFC_ENC_MPEG4_PARAM *mpeg4_arg;
    SSBSIP_MFC_ENC_H263_PARAM *h263_arg;
    SSBSIP_MFC_CODEC_TYPE codecType;

    pCTX = (_MFCLIB *)openHandle;
    memset(&EncArg, 0, sizeof(struct mfc_common_args));

    LOGV("SsbSipMfcEncInit: Encode Init start\n");

    mpeg4_arg = (SSBSIP_MFC_ENC_MPEG4_PARAM *)param;
    codecType = mpeg4_arg->codecType;

    if ((codecType != MPEG4_ENC) &&
        (codecType != H264_ENC)  &&
        (codecType != H263_ENC)) {
        LOGE("SsbSipMfcEncOpen: Undefined codec type.\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX->codecType = codecType;

    switch (pCTX->codecType) {
    case MPEG4_ENC:
        LOGV("SsbSipMfcEncInit: MPEG4 Encode\n");
        mpeg4_arg = (SSBSIP_MFC_ENC_MPEG4_PARAM *)param;

        pCTX->width = mpeg4_arg->SourceWidth;
        pCTX->height = mpeg4_arg->SourceHeight;
        break;

    case H263_ENC:
        LOGV("SsbSipMfcEncInit: H263 Encode\n");
        h263_arg = (SSBSIP_MFC_ENC_H263_PARAM *)param;

        pCTX->width = h263_arg->SourceWidth;
        pCTX->height = h263_arg->SourceHeight;
        break;

    case H264_ENC:
        LOGV("SsbSipMfcEncInit: H264 Encode\n");
        h264_arg = (SSBSIP_MFC_ENC_H264_PARAM *)param;

        pCTX->width = h264_arg->SourceWidth;
        pCTX->height = h264_arg->SourceHeight;
        break;

    default:
        break;
    }

    switch (pCTX->codecType) {
    case MPEG4_ENC:
        mpeg4_arg = (SSBSIP_MFC_ENC_MPEG4_PARAM*)param;

        EncArg.args.enc_init.cmn.in_codec_type = pCTX->codecType;
        EncArg.args.enc_init.codec.mpeg4.in_profile = mpeg4_arg->ProfileIDC;
        EncArg.args.enc_init.codec.mpeg4.in_level = mpeg4_arg->LevelIDC;

        EncArg.args.enc_init.cmn.in_width = mpeg4_arg->SourceWidth;
        EncArg.args.enc_init.cmn.in_height = mpeg4_arg->SourceHeight;
        EncArg.args.enc_init.cmn.in_gop_num = mpeg4_arg->IDRPeriod;
        if (mpeg4_arg->DisableQpelME)
            EncArg.args.enc_init.codec.mpeg4.in_quart_pixel = 0;
        else
            EncArg.args.enc_init.codec.mpeg4.in_quart_pixel = 1;

        EncArg.args.enc_init.cmn.in_ms_mode = mpeg4_arg->SliceMode;
        EncArg.args.enc_init.cmn.in_ms_arg = mpeg4_arg->SliceArgument;

        if (mpeg4_arg->NumberBFrames > 2) {
            LOGE("SsbSipMfcEncInit: No such BframeNum is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.codec.mpeg4.in_bframenum = mpeg4_arg->NumberBFrames;
        EncArg.args.enc_init.cmn.in_mb_refresh = mpeg4_arg->RandomIntraMBRefresh;

        /* rate control*/
        EncArg.args.enc_init.cmn.in_rc_fr_en = mpeg4_arg->EnableFRMRateControl;
        if ((mpeg4_arg->QSCodeMin > 51) || (mpeg4_arg->QSCodeMax > 51)) {
            LOGE("SsbSipMfcEncInit: No such Min/Max QP is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_rc_qbound_min = mpeg4_arg->QSCodeMin;
        EncArg.args.enc_init.cmn.in_rc_qbound_max = mpeg4_arg->QSCodeMax;
        EncArg.args.enc_init.cmn.in_rc_rpara = mpeg4_arg->CBRPeriodRf;

        /* pad control */
        EncArg.args.enc_init.cmn.in_pad_ctrl_on = mpeg4_arg->PadControlOn;
        if ((mpeg4_arg->LumaPadVal > 255) || (mpeg4_arg->CbPadVal > 255) || (mpeg4_arg->CrPadVal > 255)) {
            LOGE("SsbSipMfcEncInit: No such Pad value is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_y_pad_val = mpeg4_arg->LumaPadVal;
        EncArg.args.enc_init.cmn.in_cb_pad_val = mpeg4_arg->CbPadVal;
        EncArg.args.enc_init.cmn.in_cr_pad_val = mpeg4_arg->CrPadVal;

        EncArg.args.enc_init.codec.mpeg4.in_TimeIncreamentRes = mpeg4_arg->TimeIncreamentRes;
        EncArg.args.enc_init.codec.mpeg4.in_VopTimeIncreament = mpeg4_arg->VopTimeIncreament;
	/* Not in mpeg4 args 
        EncArg.args.enc_init_mpeg4.in_RC_framerate = (mpeg4_arg->TimeIncreamentRes / mpeg4_arg->VopTimeIncreament);
	*/
        EncArg.args.enc_init.cmn.in_rc_bitrate = mpeg4_arg->Bitrate;
        if ((mpeg4_arg->FrameQp > 51) || (mpeg4_arg->FrameQp_P) > 51 || (mpeg4_arg->FrameQp_B > 51)) {
            LOGE("SsbSipMfcEncInit: No such FrameQp is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_vop_quant = mpeg4_arg->FrameQp;
        if (mpeg4_arg->FrameQp_P)
            EncArg.args.enc_init.cmn.in_vop_quant_p = mpeg4_arg->FrameQp_P;
        else
            EncArg.args.enc_init.cmn.in_vop_quant_p = mpeg4_arg->FrameQp;
        if (mpeg4_arg->FrameQp_B)
            EncArg.args.enc_init.codec.mpeg4.in_vop_quant_b = mpeg4_arg->FrameQp_B;
        else
            EncArg.args.enc_init.codec.mpeg4.in_vop_quant_b = mpeg4_arg->FrameQp;

            break;

    case H263_ENC:
        h263_arg = (SSBSIP_MFC_ENC_H263_PARAM *)param;

        EncArg.args.enc_init.cmn.in_codec_type = pCTX->codecType;
	/* Not in h263 args
        EncArg.args.enc_init_mpeg4.in_profile_level = ENC_PROFILE_LEVEL(66, 40);
	*/
        EncArg.args.enc_init.cmn.in_width = h263_arg->SourceWidth;
        EncArg.args.enc_init.cmn.in_height = h263_arg->SourceHeight;
        EncArg.args.enc_init.cmn.in_gop_num = h263_arg->IDRPeriod;
        EncArg.args.enc_init.cmn.in_mb_refresh = h263_arg->RandomIntraMBRefresh;
        EncArg.args.enc_init.cmn.in_ms_mode = h263_arg->SliceMode;
        EncArg.args.enc_init.cmn.in_ms_arg = 0;

        /* rate control*/
        EncArg.args.enc_init.cmn.in_rc_fr_en = h263_arg->EnableFRMRateControl;
        if ((h263_arg->QSCodeMin > 51) || (h263_arg->QSCodeMax > 51)) {
            LOGE("SsbSipMfcEncInit: No such Min/Max QP is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_rc_qbound_min = h263_arg->QSCodeMin;
        EncArg.args.enc_init.cmn.in_rc_qbound_max = h263_arg->QSCodeMax;
        EncArg.args.enc_init.cmn.in_rc_rpara = h263_arg->CBRPeriodRf;

        /* pad control */
        EncArg.args.enc_init.cmn.in_pad_ctrl_on = h263_arg->PadControlOn;
        if ((h263_arg->LumaPadVal > 255) || (h263_arg->CbPadVal > 255) || (h263_arg->CrPadVal > 255)) {
            LOGE("SsbSipMfcEncInit: No such Pad value is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_y_pad_val = h263_arg->LumaPadVal;
        EncArg.args.enc_init.cmn.in_cb_pad_val = h263_arg->CbPadVal;
        EncArg.args.enc_init.cmn.in_cr_pad_val = h263_arg->CrPadVal;

        EncArg.args.enc_init.codec.h263.in_rc_framerate = h263_arg->FrameRate;
        EncArg.args.enc_init.cmn.in_rc_bitrate = h263_arg->Bitrate;
        if (h263_arg->FrameQp > 51) {
            LOGE("SsbSipMfcEncInit: No such FrameQp is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_vop_quant = h263_arg->FrameQp;
        if (h263_arg->FrameQp_P)
            EncArg.args.enc_init.cmn.in_vop_quant_p = h263_arg->FrameQp_P;
        else
            EncArg.args.enc_init.cmn.in_vop_quant_p = h263_arg->FrameQp;

        break;

    case H264_ENC:
        h264_arg = (SSBSIP_MFC_ENC_H264_PARAM *)param;

        EncArg.args.enc_init.cmn.in_codec_type = H264_ENC;
        EncArg.args.enc_init.codec.h264.in_profile = h264_arg->ProfileIDC;
        EncArg.args.enc_init.codec.h264.in_level = h264_arg->LevelIDC;

        EncArg.args.enc_init.cmn.in_width = h264_arg->SourceWidth;
        EncArg.args.enc_init.cmn.in_height = h264_arg->SourceHeight;
        EncArg.args.enc_init.cmn.in_gop_num = h264_arg->IDRPeriod;

        if ((h264_arg->NumberRefForPframes > 2) || (h264_arg->NumberReferenceFrames > 2)) {
            LOGE("SsbSipMfcEncInit: No such ref Num is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.codec.h264.in_reference_num = h264_arg->NumberReferenceFrames;
        EncArg.args.enc_init.codec.h264.in_ref_num_p = h264_arg->NumberRefForPframes;

        if ((h264_arg->SliceMode == 0) || (h264_arg->SliceMode == 1) ||
            (h264_arg->SliceMode == 2) || (h264_arg->SliceMode == 4)) {
            EncArg.args.enc_init.cmn.in_ms_mode = h264_arg->SliceMode;
        } else {
            LOGE("SsbSipMfcEncInit: No such slice mode is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_ms_arg = h264_arg->SliceArgument;

        if (h264_arg->NumberBFrames > 2) {
            LOGE("SsbSipMfcEncInit: No such BframeNum is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.codec.h264.in_bframenum = h264_arg->NumberBFrames;

        EncArg.args.enc_init.codec.h264.in_deblock_dis = h264_arg->LoopFilterDisable;
        if ((abs(h264_arg->LoopFilterAlphaC0Offset) > 6) || (abs(h264_arg->LoopFilterBetaOffset) > 6)) {
            LOGE("SsbSipMfcEncInit: No such AlphaC0Offset or BetaOffset is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.codec.h264.in_deblock_alpha_c0 = h264_arg->LoopFilterAlphaC0Offset;
        EncArg.args.enc_init.codec.h264.in_deblock_beta = h264_arg->LoopFilterBetaOffset;

        EncArg.args.enc_init.codec.h264.in_symbolmode = h264_arg->SymbolMode;
        EncArg.args.enc_init.codec.h264.in_interlace_mode = h264_arg->PictureInterlace;
        EncArg.args.enc_init.codec.h264.in_transform8x8_mode = h264_arg->Transform8x8Mode;

        EncArg.args.enc_init.cmn.in_mb_refresh = h264_arg->RandomIntraMBRefresh;

        /* pad control */
        EncArg.args.enc_init.cmn.in_pad_ctrl_on = h264_arg->PadControlOn;
        if ((h264_arg->LumaPadVal > 255) || (h264_arg->CbPadVal > 255) || (h264_arg->CrPadVal > 255)) {
            LOGE("SsbSipMfcEncInit: No such Pad value is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_y_pad_val = h264_arg->LumaPadVal;
        EncArg.args.enc_init.cmn.in_cb_pad_val = h264_arg->CbPadVal;
        EncArg.args.enc_init.cmn.in_cr_pad_val = h264_arg->CrPadVal;

        /* rate control*/
        EncArg.args.enc_init.cmn.in_rc_fr_en = h264_arg->EnableFRMRateControl;
        EncArg.args.enc_init.codec.h264.in_rc_mb_en = h264_arg->EnableMBRateControl;
        EncArg.args.enc_init.codec.h264.in_rc_framerate = h264_arg->FrameRate;
        EncArg.args.enc_init.cmn.in_rc_bitrate = h264_arg->Bitrate;
        if (h264_arg->FrameQp > 51) {
            LOGE("SsbSipMfcEncInit: No such FrameQp is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_vop_quant = h264_arg->FrameQp;
        if (h264_arg->FrameQp_P)
            EncArg.args.enc_init.cmn.in_vop_quant_p = h264_arg->FrameQp_P;
        else
            EncArg.args.enc_init.cmn.in_vop_quant_p = h264_arg->FrameQp;
        if (h264_arg->FrameQp_B)
            EncArg.args.enc_init.codec.h264.in_vop_quant_b = h264_arg->FrameQp_B;
        else
            EncArg.args.enc_init.codec.h264.in_vop_quant_b = h264_arg->FrameQp;

        if ((h264_arg->QSCodeMin > 51) || (h264_arg->QSCodeMax > 51)) {
            LOGE("SsbSipMfcEncInit: No such Min/Max QP is supported.\n");
            return MFC_RET_INVALID_PARAM;
        }
        EncArg.args.enc_init.cmn.in_rc_qbound_min = h264_arg->QSCodeMin;
        EncArg.args.enc_init.cmn.in_rc_qbound_max = h264_arg->QSCodeMax;
        EncArg.args.enc_init.cmn.in_rc_rpara = h264_arg->CBRPeriodRf;
        EncArg.args.enc_init.codec.h264.in_rc_mb_dark_dis = h264_arg->DarkDisable;
        EncArg.args.enc_init.codec.h264.in_rc_mb_smooth_dis = h264_arg->SmoothDisable;
        EncArg.args.enc_init.codec.h264.in_rc_mb_static_dis = h264_arg->StaticDisable;
        EncArg.args.enc_init.codec.h264.in_rc_mb_activity_dis = h264_arg->ActivityDisable;

        /* default setting */
        EncArg.args.enc_init.codec.h264.in_md_interweight_pps = 0;
        EncArg.args.enc_init.codec.h264.in_md_intraweight_pps = 0;
        break;

    default:
        break;
    }

    EncArg.args.enc_init.cmn.in_mapped_addr = pCTX->mapped_addr;

    ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_ENC_INIT, &EncArg);
    if (EncArg.ret_code != MFC_RET_OK) {
        LOGE("SsbSipMfcEncInit: IOCTL_MFC_ENC_INIT (%d) failed\n", EncArg.ret_code);
        return MFC_RET_ENC_INIT_FAIL;
    }

    pCTX->virStrmBuf = EncArg.args.enc_init.cmn.out_u_addr.strm_ref_y;
    pCTX->phyStrmBuf = EncArg.args.enc_init.cmn.out_p_addr.strm_ref_y;
    pCTX->sizeStrmBuf = MAX_ENCODER_OUTPUT_BUFFER_SIZE;
    pCTX->encodedHeaderSize = EncArg.args.enc_init.cmn.out_header_size;

    pCTX->virMvRefYC = EncArg.args.enc_init.cmn.out_u_addr.mv_ref_yc;

    pCTX->inter_buff_status |= MFC_USE_STRM_BUFF;

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncExe(void *openHandle)
{
    int ret_code;
    _MFCLIB *pCTX;
    struct mfc_common_args EncArg;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncExe: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;

    memset(&EncArg, 0x00, sizeof(struct mfc_common_args));

    EncArg.args.enc_exe.in_codec_type = pCTX->codecType;
    EncArg.args.enc_exe.in_Y_addr = (unsigned int)pCTX->phyFrmBuf.luma;
    EncArg.args.enc_exe.in_CbCr_addr = (unsigned int)pCTX->phyFrmBuf.chroma;
    EncArg.args.enc_exe.in_Y_addr_vir = (unsigned int)pCTX->virFrmBuf.luma;
    EncArg.args.enc_exe.in_CbCr_addr_vir = (unsigned int)pCTX->virFrmBuf.chroma;
    EncArg.args.enc_exe.in_strm_st = (unsigned int)pCTX->phyStrmBuf;
    EncArg.args.enc_exe.in_strm_end = (unsigned int)pCTX->phyStrmBuf + pCTX->sizeStrmBuf;
    EncArg.args.enc_exe.in_frametag = pCTX->inframetag;

    ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_ENC_EXE, &EncArg);
    if (EncArg.ret_code != MFC_RET_OK) {
        LOGE("SsbSipMfcDecExe: IOCTL_MFC_ENC_EXE failed(ret : %d)\n", EncArg.ret_code);
        return MFC_RET_ENC_EXE_ERR;
    }

    pCTX->encodedDataSize = EncArg.args.enc_exe.out_encoded_size;
    pCTX->encodedframeType = EncArg.args.enc_exe.out_frame_type;
    pCTX->encodedphyFrmBuf.luma = EncArg.args.enc_exe.out_Y_addr;
    pCTX->encodedphyFrmBuf.chroma = EncArg.args.enc_exe.out_CbCr_addr;
    pCTX->outframetagtop = EncArg.args.enc_exe.out_frametag_top;
    pCTX->outframetagbottom = EncArg.args.enc_exe.out_frametag_bottom;

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncClose(void *openHandle)
{
    int ret_code;
    _MFCLIB *pCTX;
    struct mfc_common_args free_arg;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncClose: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;

    if (pCTX->inter_buff_status & MFC_USE_YUV_BUFF) {
        free_arg.args.mem_free.key = pCTX->virFrmBuf.luma - pCTX->mapped_addr;
        ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_FREE_BUF, &free_arg);
    }

    if (pCTX->inter_buff_status & MFC_USE_STRM_BUFF) {
        free_arg.args.mem_free.key = pCTX->virStrmBuf - pCTX->mapped_addr;
        ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_FREE_BUF, &free_arg);
        free_arg.args.mem_free.key = pCTX->virMvRefYC - pCTX->mapped_addr;
        ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_FREE_BUF, &free_arg);
    }

    pCTX->inter_buff_status = MFC_USE_NONE;

    munmap((void *)pCTX->mapped_addr, pCTX->mapped_size);
    close(pCTX->hMFC);
    free(pCTX);

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncGetInBuf(void *openHandle, SSBSIP_MFC_ENC_INPUT_INFO *input_info)
{
    int ret_code;
    _MFCLIB *pCTX;
    struct mfc_common_args user_addr_arg, phys_addr_arg;
    int y_size, c_size;
    int aligned_y_size, aligned_c_size;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncGetInBuf: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;

    user_addr_arg.args.mem_alloc.type = ENCODER;

    y_size = pCTX->width * pCTX->height;
    c_size = (pCTX->width * pCTX->height) >> 1;

    aligned_y_size = ALIGN_TO_8KB(ALIGN_TO_128B(pCTX->width) * ALIGN_TO_32B(pCTX->height));
    aligned_c_size = ALIGN_TO_8KB(ALIGN_TO_128B(pCTX->width) * ALIGN_TO_32B(pCTX->height/2));

    /* Allocate luma & chroma buf */
    user_addr_arg.args.mem_alloc.buff_size = aligned_y_size + aligned_c_size;
    user_addr_arg.args.mem_alloc.mapped_addr = pCTX->mapped_addr;
    ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_GET_IN_BUF, &user_addr_arg);
    if (ret_code < 0) {
        LOGE("SsbSipMfcEncGetInBuf: IOCTL_MFC_GET_IN_BUF failed\n");
        return MFC_RET_ENC_GET_INBUF_FAIL;
    }

    /* Get physical address information */
    phys_addr_arg.args.real_addr.key = user_addr_arg.args.mem_alloc.offset;
    ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_GET_REAL_ADDR, &phys_addr_arg);
    if (ret_code < 0) {
        LOGE("SsbSipMfcEncGetInBuf: IOCTL_MFC_GET_REAL_ADDR failed\n");
        return MFC_RET_ENC_GET_INBUF_FAIL;
    }
    
    pCTX->virFrmBuf.luma = user_addr_arg.args.mem_alloc.offset + pCTX->mapped_addr;
    pCTX->virFrmBuf.chroma = user_addr_arg.args.mem_alloc.offset + pCTX->mapped_addr + (unsigned int)aligned_y_size;
    pCTX->phyFrmBuf.luma = phys_addr_arg.args.real_addr.addr;
    pCTX->phyFrmBuf.chroma = phys_addr_arg.args.real_addr.addr + (unsigned int)aligned_y_size;

    pCTX->sizeFrmBuf.luma = (unsigned int)y_size;
    pCTX->sizeFrmBuf.chroma = (unsigned int)c_size;
    pCTX->inter_buff_status |= MFC_USE_YUV_BUFF;

    input_info->YPhyAddr = (void*)pCTX->phyFrmBuf.luma;
    input_info->CPhyAddr = (void*)pCTX->phyFrmBuf.chroma;
    input_info->YVirAddr = (void*)pCTX->virFrmBuf.luma;
    input_info->CVirAddr = (void*)pCTX->virFrmBuf.chroma;

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncSetInBuf(void *openHandle, SSBSIP_MFC_ENC_INPUT_INFO *input_info)
{
    _MFCLIB *pCTX;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncSetInBuf: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    LOGV("SsbSipMfcEncSetInBuf: input_info->YPhyAddr & input_info->CPhyAddr should be 64KB aligned\n");

    pCTX = (_MFCLIB *)openHandle;

    pCTX->phyFrmBuf.luma = (unsigned int)input_info->YPhyAddr;
    pCTX->phyFrmBuf.chroma = (unsigned int)input_info->CPhyAddr;
    pCTX->virFrmBuf.luma = (unsigned int)input_info->YVirAddr;
    pCTX->virFrmBuf.chroma = (unsigned int)input_info->CVirAddr;

    pCTX->sizeFrmBuf.luma = (unsigned int)input_info->YSize;
    pCTX->sizeFrmBuf.chroma = (unsigned int)input_info->CSize;

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncGetOutBuf(void *openHandle, SSBSIP_MFC_ENC_OUTPUT_INFO *output_info)
{
    _MFCLIB *pCTX;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncGetOutBuf: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;

    output_info->headerSize = pCTX->encodedHeaderSize;
    output_info->dataSize = pCTX->encodedDataSize;
    output_info->StrmPhyAddr = (void *)pCTX->phyStrmBuf;
    output_info->StrmVirAddr = (void *)pCTX->virStrmBuf;

    if (pCTX->encodedframeType == 0)
        output_info->frameType = MFC_FRAME_TYPE_NOT_CODED;
    else if (pCTX->encodedframeType == 1)
        output_info->frameType = MFC_FRAME_TYPE_I_FRAME;
    else if (pCTX->encodedframeType == 2)
        output_info->frameType = MFC_FRAME_TYPE_P_FRAME;
    else if (pCTX->encodedframeType == 3)
        output_info->frameType = MFC_FRAME_TYPE_B_FRAME;
    else if (pCTX->encodedframeType == 4)
        output_info->frameType = MFC_FRAME_TYPE_OTHERS;
    else {
        LOGE("Strange encoded frame type = %d\n", pCTX->encodedframeType);
        return MFC_RET_INVALID_PARAM;
    }

    output_info->encodedYPhyAddr = (void *)pCTX->encodedphyFrmBuf.luma;
    output_info->encodedCPhyAddr = (void *)pCTX->encodedphyFrmBuf.chroma;

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncSetOutBuf(void *openHandle, void *phyOutbuf, void *virOutbuf, int outputBufferSize)
{
    _MFCLIB *pCTX;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncSetOutBuf: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;

    pCTX->phyStrmBuf = (int)phyOutbuf;
    pCTX->virStrmBuf = (int)virOutbuf;
    pCTX->sizeStrmBuf = outputBufferSize;

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncSetConfig(void *openHandle, SSBSIP_MFC_ENC_CONF conf_type, void *value)
{
    int ret_code;
    _MFCLIB *pCTX;
    struct mfc_common_args EncArg;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncSetConfig: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    if (value == NULL) {
        LOGE("SsbSipMfcEncSetConfig: value is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;
    memset(&EncArg, 0x00, sizeof(struct mfc_common_args));

    switch (conf_type) {
    case MFC_ENC_SETCONF_FRAME_TYPE:
    case MFC_ENC_SETCONF_CHANGE_FRAME_RATE:
    case MFC_ENC_SETCONF_CHANGE_BIT_RATE:
    case MFC_ENC_SETCONF_ALLOW_FRAME_SKIP:
        EncArg.args.set_config.in_config_param = conf_type;
        EncArg.args.set_config.in_config_value[0] = *((unsigned int *) value);
        EncArg.args.set_config.in_config_value[1] = 0;
        break;

    case MFC_ENC_SETCONF_FRAME_TAG:
        pCTX->inframetag = *((int *)value);
        return MFC_RET_OK;

    default:
        LOGE("SsbSipMfcEncSetConfig: No such conf_type is supported.\n");
        return MFC_RET_INVALID_PARAM;
    }

    ret_code = ioctl(pCTX->hMFC, IOCTL_MFC_SET_CONFIG, &EncArg);
    if (EncArg.ret_code != MFC_RET_OK) {
        LOGE("SsbSipMfcEncSetConfig: IOCTL_MFC_SET_CONFIG failed(ret : %d)\n", EncArg.ret_code);
        return MFC_RET_ENC_SET_CONF_FAIL;
    }

    return MFC_RET_OK;
}

SSBSIP_MFC_ERROR_CODE SsbSipMfcEncGetConfig(void *openHandle, SSBSIP_MFC_ENC_CONF conf_type, void *value)
{
    int ret_code;
    _MFCLIB *pCTX;
    struct mfc_common_args EncArg;

    pCTX = (_MFCLIB *)openHandle;

    if (openHandle == NULL) {
        LOGE("SsbSipMfcEncGetConfig: openHandle is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }
    if (value == NULL) {
        LOGE("SsbSipMfcEncGetConfig: value is NULL\n");
        return MFC_RET_INVALID_PARAM;
    }

    pCTX = (_MFCLIB *)openHandle;
    memset(&EncArg, 0x00, sizeof(struct mfc_common_args));

    switch (conf_type) {
    case MFC_ENC_GETCONF_FRAME_TAG:
        *((unsigned int *)value) = pCTX->outframetagtop;
        break;

    default:
        LOGE("SsbSipMfcEncGetConfig: No such conf_type is supported.\n");
        return MFC_RET_INVALID_PARAM;
    }

    return MFC_RET_OK;
}
