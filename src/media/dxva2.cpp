/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

 /* File borrowed and adapted from FFmpeg*/

#include <windows.h>

#include "logger.h"

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#define DXVA2API_USE_BITFIELDS
#define COBJMACROS

#include <stdint.h>

#include <d3d9.h>
#include <dxva2api.h>

#include "libav_deps.h"

#include "libavcodec/dxva2.h"

 /* define all the GUIDs used directly here,
    to avoid problems with inconsistent dxva2api.h versions in mingw-w64 and different MSVC version */
#include <initguid.h>
DEFINE_GUID(IID_IDirectXVideoDecoderService, 0xfc51a551,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

DEFINE_GUID(DXVA2_ModeMPEG2_VLD,      0xee27417f, 0x5e28,0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID(DXVA2_ModeMPEG2and1_VLD,  0x86695f12, 0x340e,0x4f04,0x9f,0xd3,0x92,0x53,0xdd,0x32,0x74,0x60);
DEFINE_GUID(DXVA2_ModeH264_E,         0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_F,         0x1b81be69, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVADDI_Intel_ModeH264_E, 0x604F8E68, 0x4951,0x4C54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(DXVA2_ModeVC1_D,          0x1b81beA3, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D2010,      0x1b81beA4, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main,  0x5b11d51b, 0x2f4c,0x4452,0xbc,0xc3,0x09,0xf2,0xa1,0x16,0x0c,0xc0);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main10,0x107af0e0, 0xef1a,0x4d19,0xab,0xa8,0x67,0xa1,0x63,0x07,0x3d,0x13);
DEFINE_GUID(DXVA2_ModeVP9_VLD_Profile0, 0x463707f8, 0xa1d0,0x4585,0x87,0x6d,0x83,0xaa,0x6d,0x60,0xb8,0x9e);
DEFINE_GUID(DXVA2_NoEncrypt,          0x1b81beD0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(GUID_NULL,                0x00000000, 0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

typedef IDirect3D9* WINAPI pDirect3DCreate9(UINT);
typedef HRESULT WINAPI pCreateDeviceManager9(UINT *, IDirect3DDeviceManager9 **);

typedef struct dxva2_mode {
  const GUID     *guid;
  enum AVCodecID codec;
} dxva2_mode;

static const dxva2_mode dxva2_modes[] = {
    /* MPEG-2 */
    { &DXVA2_ModeMPEG2_VLD,      AV_CODEC_ID_MPEG2VIDEO },
    { &DXVA2_ModeMPEG2and1_VLD,  AV_CODEC_ID_MPEG2VIDEO },

    /* H.264 */
    { &DXVA2_ModeH264_F,         AV_CODEC_ID_H264 },
    { &DXVA2_ModeH264_E,         AV_CODEC_ID_H264 },
    /* Intel specific H.264 mode */
    { &DXVADDI_Intel_ModeH264_E, AV_CODEC_ID_H264 },

    /* VC-1 / WMV3 */
    { &DXVA2_ModeVC1_D2010,      AV_CODEC_ID_VC1  },
    { &DXVA2_ModeVC1_D2010,      AV_CODEC_ID_WMV3 },
    { &DXVA2_ModeVC1_D,          AV_CODEC_ID_VC1  },
    { &DXVA2_ModeVC1_D,          AV_CODEC_ID_WMV3 },

    /* HEVC/H.265 */
    { &DXVA2_ModeHEVC_VLD_Main,  AV_CODEC_ID_HEVC },
    { &DXVA2_ModeHEVC_VLD_Main10,AV_CODEC_ID_HEVC },

    /* VP8/9 */
    { &DXVA2_ModeVP9_VLD_Profile0, AV_CODEC_ID_VP9 },

    { NULL,                      AV_CODEC_ID_NONE },
};

typedef struct surface_info {
    int used;
    uint64_t age;
} surface_info;

typedef struct DXVA2Context {
    HMODULE d3dlib;
    HMODULE dxva2lib;

    HANDLE  deviceHandle;

    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d9device;
    IDirect3DDeviceManager9     *d3d9devmgr;
    IDirectXVideoDecoderService *decoder_service;
    IDirectXVideoDecoder        *decoder;

    GUID                        decoder_guid;
    DXVA2_ConfigPictureDecode   decoder_config;

    LPDIRECT3DSURFACE9          *surfaces;
    surface_info                *surface_infos;
    uint32_t                    num_surfaces;
    uint64_t                    surface_age;
    D3DFORMAT                   surface_format;

    AVFrame                     *tmp_frame;
} DXVA2Context;

typedef struct DXVA2SurfaceWrapper {
    DXVA2Context         *ctx;
    LPDIRECT3DSURFACE9   surface;
    IDirectXVideoDecoder *decoder;
} DXVA2SurfaceWrapper;

static void dxva2_uninit(AVCodecContext *s)
{
    (void) s;
    // InputStream  *ist = s->opaque;
    DXVA2Context *ctx = (DXVA2Context*)s->hwaccel_context;
    //
    // ist->hwaccel_uninit        = NULL;
    // ist->hwaccel_get_buffer    = NULL;
    // ist->hwaccel_retrieve_data = NULL;
    //
    // if (ctx->decoder)
    //     dxva2_destroy_decoder(s);
    //
    if (ctx->decoder_service)
        ctx->decoder_service->Release();

    if (ctx->d3d9devmgr && ctx->deviceHandle != INVALID_HANDLE_VALUE)
        ctx->d3d9devmgr->CloseDeviceHandle(ctx->deviceHandle);

    if (ctx->d3d9devmgr)
        ctx->d3d9devmgr->Release();

    if (ctx->d3d9device)
        ctx->d3d9device->Release();

    if (ctx->d3d9)
        ctx->d3d9->Release();

    if (ctx->d3dlib)
        FreeLibrary(ctx->d3dlib);

    if (ctx->dxva2lib)
        FreeLibrary(ctx->dxva2lib);

    av_frame_free(&ctx->tmp_frame);

    av_freep(&s->hwaccel_context);
}

static int dxva2_get_buffer(AVCodecContext *s, AVFrame *frame, int flags)
{
    // InputStream  *ist = s->opaque;
    // DXVA2Context *ctx = ist->hwaccel_ctx;
    // int i, old_unused = -1;
    // LPDIRECT3DSURFACE9 surface;
    // DXVA2SurfaceWrapper *w = NULL;
    //
    // av_assert0(frame->format == AV_PIX_FMT_DXVA2_VLD);
    //
    // for (i = 0; i < ctx->num_surfaces; i++) {
    //     surface_info *info = &ctx->surface_infos[i];
    //     if (!info->used && (old_unused == -1 || info->age < ctx->surface_infos[old_unused].age))
    //         old_unused = i;
    // }
    // if (old_unused == -1) {
    //     av_log(NULL, AV_LOG_ERROR, "No free DXVA2 surface!\n");
    //     return AVERROR(ENOMEM);
    // }
    // i = old_unused;
    //
    // surface = ctx->surfaces[i];
    //
    // w = av_mallocz(sizeof(*w));
    // if (!w)
    //     return AVERROR(ENOMEM);
    //
    // frame->buf[0] = av_buffer_create((uint8_t*)surface, 0,
    //                                  dxva2_release_buffer, w,
    //                                  AV_BUFFER_FLAG_READONLY);
    // if (!frame->buf[0]) {
    //     av_free(w);
    //     return AVERROR(ENOMEM);
    // }
    //
    // w->ctx     = ctx;
    // w->surface = surface;
    // IDirect3DSurface9_AddRef(w->surface);
    // w->decoder = ctx->decoder;
    // IDirectXVideoDecoder_AddRef(w->decoder);
    //
    // ctx->surface_infos[i].used = 1;
    // ctx->surface_infos[i].age  = ctx->surface_age++;
    //
    // frame->data[3] = (uint8_t *)surface;
    //
    // return 0;
}

static int dxva2_retrieve_data(AVCodecContext *s, AVFrame *frame)
{
//     LPDIRECT3DSURFACE9 surface =  (LPDIRECT3DSURFACE9)frame->data[3];
//     InputStream        *ist = s->opaque;
//     DXVA2Context       *ctx = ist->hwaccel_ctx;
//     D3DSURFACE_DESC    surfaceDesc;
//     D3DLOCKED_RECT     LockedRect;
//     HRESULT            hr;
//     int                ret, nbytes;
//
//     IDirect3DSurface9_GetDesc(surface, &surfaceDesc);
//
//     ctx->tmp_frame->width  = frame->width;
//     ctx->tmp_frame->height = frame->height;
//     switch (ctx->surface_format){
//     case MKTAG('N','V','1','2'):
//         ctx->tmp_frame->format = AV_PIX_FMT_NV12;
//         nbytes = 1;
//         break;
//     case MKTAG('P','0','1','0'):
//         ctx->tmp_frame->format = AV_PIX_FMT_P010;
//         nbytes = 2;
//         break;
//     default:
//         av_assert0(0);
//     }
//
//     ret = av_frame_get_buffer(ctx->tmp_frame, 32);
//     if (ret < 0)
//         return ret;
//
//     hr = IDirect3DSurface9_LockRect(surface, &LockedRect, NULL, D3DLOCK_READONLY);
//     if (FAILED(hr)) {
//         av_log(NULL, AV_LOG_ERROR, "Unable to lock DXVA2 surface\n");
//         return AVERROR_UNKNOWN;
//     }
//
//     av_image_copy_plane(ctx->tmp_frame->data[0], ctx->tmp_frame->linesize[0],
//                         (uint8_t*)LockedRect.pBits,
//                         LockedRect.Pitch, frame->width * nbytes, frame->height);
//
//     av_image_copy_plane(ctx->tmp_frame->data[1], ctx->tmp_frame->linesize[1],
//                         (uint8_t*)LockedRect.pBits + LockedRect.Pitch * surfaceDesc.Height,
//                         LockedRect.Pitch, frame->width * nbytes, frame->height / 2);
//
//     IDirect3DSurface9_UnlockRect(surface);
//
//     ret = av_frame_copy_props(ctx->tmp_frame, frame);
//     if (ret < 0)
//         goto fail;
//
//     av_frame_unref(frame);
//     av_frame_move_ref(frame, ctx->tmp_frame);
//
//     return 0;
// fail:
//     av_frame_unref(ctx->tmp_frame);
//     return ret;
}

static int dxva2_alloc(AVCodecContext *s)
{
    AVCodecContext  *ist = s;
    DXVA2Context *ctx;
    pDirect3DCreate9      *createD3D = NULL;
    pCreateDeviceManager9 *createDeviceManager = NULL;
    HRESULT hr;
    D3DPRESENT_PARAMETERS d3dpp = {};
    D3DDISPLAYMODE        d3ddm;
    unsigned resetToken = 0;
    UINT adapter = D3DADAPTER_DEFAULT;

    ctx = (DXVA2Context*)av_mallocz(sizeof(*ctx));
    if (!ctx) {
        RING_ERR("Failed to allocate DXVA2Context\n");
        return AVERROR(ENOMEM);
    }

    ctx->deviceHandle = INVALID_HANDLE_VALUE;

    ist->hwaccel_context           = ctx;
    //ist->hwaccel_uninit        = dxva2_uninit;
    //ist->hwaccel_get_buffer    = dxva2_get_buffer;
    //ist->hwaccel_retrieve_data = dxva2_retrieve_data;

    ctx->d3dlib = LoadLibrary("d3d9.dll");
    if (!ctx->d3dlib) {
        RING_ERR("Failed to load D3D9 library\n");
        goto fail;
    }
    ctx->dxva2lib = LoadLibrary("dxva2.dll");
    if (!ctx->dxva2lib) {
        RING_ERR("Failed to load DXVA2 library\n");
        goto fail;
    }

    createD3D = (pDirect3DCreate9 *)GetProcAddress(ctx->d3dlib, "Direct3DCreate9");
    if (!createD3D) {
        RING_ERR("Failed to locate Direct3DCreate9\n");
        goto fail;
    }
    createDeviceManager = (pCreateDeviceManager9 *)GetProcAddress(ctx->dxva2lib, "DXVA2CreateDirect3DDeviceManager9");
    if (!createDeviceManager) {
        RING_ERR("Failed to locate DXVA2CreateDirect3DDeviceManager9\n");
        goto fail;
    }

    ctx->d3d9 = createD3D(D3D_SDK_VERSION);
    if (!ctx->d3d9) {
        RING_ERR("Failed to create IDirect3D object\n");
        goto fail;
    }

/*    if (ist->hwaccel_device) {
        adapter = atoi(ist->hwaccel_device);
        RING_ERR("Using HWAccel device %d\n", adapter);
    }*/

    ctx->d3d9->GetAdapterDisplayMode(adapter, &d3ddm);
    d3dpp.Windowed         = TRUE;
    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount  = 0;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags            = D3DPRESENTFLAG_VIDEO;

    hr = ctx->d3d9->CreateDevice(adapter, D3DDEVTYPE_HAL, GetDesktopWindow(),
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                                &d3dpp, &ctx->d3d9device);
    if (FAILED(hr)) {
        RING_ERR("Failed to create Direct3D device\n");
        goto fail;
    }

    hr = createDeviceManager(&resetToken, &ctx->d3d9devmgr);
    if (FAILED(hr)) {
        RING_ERR("Failed to create Direct3D device manager\n");
        goto fail;
    }

    hr = ctx->d3d9devmgr->ResetDevice(ctx->d3d9device, resetToken);
    if (FAILED(hr)) {
        RING_ERR("Failed to bind Direct3D device to device manager\n");
        goto fail;
    }

    hr = ctx->d3d9devmgr->OpenDeviceHandle(&ctx->deviceHandle);
    if (FAILED(hr)) {
        RING_ERR("Failed to open device handle\n");
        goto fail;
    }

    hr = ctx->d3d9devmgr->GetVideoService(ctx->deviceHandle, IID_IDirectXVideoDecoderService, (void**)&ctx->decoder_service);
    if (FAILED(hr)) {
        RING_ERR("Failed to create IDirectXVideoDecoderService\n");
        goto fail;
    }

    ctx->tmp_frame = av_frame_alloc();
    if (!ctx->tmp_frame)
        goto fail;

    s->hwaccel_context = av_mallocz(sizeof(struct dxva_context));
    if (!s->hwaccel_context)
        goto fail;

    return 0;
fail:
    dxva2_uninit(s);
    return AVERROR(EINVAL);
}

 int dxva2_init(AVCodecContext *s)
 {
     //InputStream *ist = s->opaque;
     //int loglevel = (ist->hwaccel_id == HWACCEL_AUTO) ? AV_LOG_VERBOSE : AV_LOG_ERROR;
     DXVA2Context *ctx;
     int ret;

     if (!s->hwaccel_context) {
         ret = dxva2_alloc(s);
         if (ret < 0)
             return ret;
     }
     ctx = (DXVA2Context *)s->hwaccel_context;

     if (s->codec_id == AV_CODEC_ID_H264 &&
         (s->profile & ~FF_PROFILE_H264_CONSTRAINED) > FF_PROFILE_H264_HIGH) {
         RING_ERR("Unsupported H.264 profile for DXVA2 HWAccel: %d\n", s->profile);
         return AVERROR(EINVAL);
     }
     //
    //  if (s->codec_id == AV_CODEC_ID_HEVC &&
    //      s->profile != FF_PROFILE_HEVC_MAIN && s->profile != FF_PROFILE_HEVC_MAIN_10) {
    //      av_log(NULL, loglevel, "Unsupported HEVC profile for DXVA2 HWAccel: %d\n", s->profile);
    //      return AVERROR(EINVAL);
    //  }
     //
    //  if (ctx->decoder)
    //      dxva2_destroy_decoder(s);
     //
    //  ret = dxva2_create_decoder(s);
    //  if (ret < 0) {
    //      av_log(NULL, loglevel, "Error creating the DXVA2 decoder\n");
    //      return ret;
    //  }

     return 0;
 }
