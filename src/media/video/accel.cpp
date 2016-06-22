/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_buffer.h"

#include "accel.h"

#if defined(HAVE_VAAPI_ACCEL_X11) || defined(HAVE_VAAPI_ACCEL_DRM)
#include "v4l2/vaapi.h"
#endif

#include "string_utils.h"
#include "logger.h"

namespace ring { namespace video {

HardwareAccel::AccelInfo
HardwareAccel::getAccel(const enum HardwareAccel::AccelID* codecAccels, int size)
{
    const AccelInfo accels[] = {
#ifdef HAVE_MOCK_ACCEL
        { AccelID::Mock, AV_PIX_FMT_YUV420P, "mock", make<MockAccel> },
#endif
// #ifdef HAVE_VDPAU_ACCEL
//     { HardwareAccel::AccelID::Vdpau, AV_PIX_FMT_VDPAU, "vdpau", HardwareAccel::make<VdpauAccel> },
// #endif
// #ifdef HAVE_VIDEOTOOLBOX_ACCEL
//     { HardwareAccel::AccelID::VideoToolbox, AV_PIX_FMT_VIDEOTOOLBOX, "videotoolbox", HardwareAccel::make<VideoToolboxAccel> },
// #endif
// #ifdef HAVE_DXVA2_ACCEL
//     { HardwareAccel::AccelID::Dxva2, AV_PIX_FMT_DXVA2_VLD, "dxva2", HardwareAccel::make<Dxva2Accel> },
// #endif
#if defined(HAVE_VAAPI_ACCEL_X11) || defined(HAVE_VAAPI_ACCEL_DRM)
        { AccelID::Vaapi, AV_PIX_FMT_VAAPI, "vaapi", make<VaapiAccel> },
#endif
// #ifdef HAVE_VDA_ACCEL
//     { HardwareAccel::AccelID::Vda, AV_PIX_FMT_VDA, "vda", HardwareAccel::make<VdaAccel> },
// #endif
        { AccelID::NoAccel, AV_PIX_FMT_NONE, "", nullptr } // sentinel value
    };

    for (int i = 0; accels[i].type != AccelID::NoAccel; i++) {
        for (int j = 0; j < size; j++) {
            if (accels[i].type == codecAccels[j]) {
                RING_DBG("Found '%s' hardware acceleration", accels[i].name.c_str());
                return accels[i];
            }
        }
    }

    RING_DBG("Did not find a matching hardware acceleration");
    return AccelInfo {};
}

void
HardwareAccel::setupAccel(AVCodecContext* codecCtx)
{
    RING_DBG("Checking if hardware acceleration is possible");
    HardwareAccel::AccelInfo info;
    switch (codecCtx->codec_id) {
        case AV_CODEC_ID_H264:
            {
                const HardwareAccel::AccelID supportedAccels[] = {
                    AccelID::Vdpau,
                    AccelID::VideoToolbox,
                    AccelID::Dxva2,
                    AccelID::Vaapi,
                    AccelID::Vda
                };
                info = getAccel(supportedAccels, 5);
            }
            break;
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            {
                const HardwareAccel::AccelID supportedAccels[] = {
                    AccelID::Vdpau,
                    AccelID::VideoToolbox,
                    AccelID::Vaapi
                };
                info = getAccel(supportedAccels, 3);
            }
            break;
        default:
            return;
    }

#ifdef HAVE_MOCK_ACCEL
    const HardwareAccel::AccelID mockAccel[] = { AccelID::Mock };
    info = getAccel(mockAccel, 1);
#endif

    if (info.type != HardwareAccel::AccelID::NoAccel && info.create) {
        if (auto accel = info.create()) {
            accel->preinit(codecCtx, info);
            codecCtx->get_format = getFormatCb;
            codecCtx->get_buffer2 = allocateBufferCb;
            codecCtx->thread_safe_callbacks = 1;
            codecCtx->thread_count = 1;
            codecCtx->opaque = accel;
            RING_DBG("Hardware acceleration setup has succeeded");
            return;
        }
    }

    RING_DBG("Not using hardware acceleration");
}

enum AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const enum AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);
    if (!accel) {
        // invalid state, try to recover
        return avcodec_default_get_format(codecCtx, formats);
    }

    RING_DBG("Acceleration supports these pixel formats:");
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; i++)
        RING_DBG("    %s", av_get_pix_fmt_name(formats[i]));

    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; i++) {
        if (formats[i] == accel->format()) {
            accel->setWidth(codecCtx->coded_width);
            accel->setHeight(codecCtx->coded_height);
            accel->setProfile(codecCtx->profile);
            try {
                accel->init(codecCtx);
                return accel->format();
            } catch (const std::runtime_error& e) {
                RING_ERR("%s", e.what());
            }
        }
    }

    accel->fail(true);
    RING_WARN("Falling back to software decoding");
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; i++) {
        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(formats[i]);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            return formats[i];
        }
    }

    return AV_PIX_FMT_NONE;
}

int
allocateBufferCb(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    int ret;
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);
    if (accel && !accel->hasFailed()) {
        ret = accel->allocateBuffer(codecCtx, frame, flags);
        if (!ret) {
            accel->reset();
            return ret;
        }
    }

    accel->fail();
    return avcodec_default_get_buffer2(codecCtx, frame, flags);
}

void
HardwareAccel::preinit(AVCodecContext* codecCtx, AccelInfo info)
{
    type_ = info.type;
    format_ = info.format;
    name_ = info.name;
    failCount_ = 0;
    fallback_ = false;
    int width_ = -1;
    int height_ = -1;
    int profile_ = -1;
}

void
HardwareAccel::fail(bool forceFallback)
{
    ++failCount_;
    if (failCount_ >= MAX_ACCEL_FAILURES || forceFallback) {
        fallback_ = true;
        failCount_ = 0;
        // force reinit of media decoder to correctly set thread count
    }
}

#ifdef HAVE_MOCK_ACCEL
MockAccel::MockAccel()
{
}

MockAccel::~MockAccel()
{
}

void
MockAccel::init(AVCodecContext* codecCtx)
{
    RING_DBG("MockAccel init");
}

int
MockAccel::allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    RING_DBG("MockAccel allocateBuffer");
    return avcodec_default_get_buffer2(codecCtx, frame, flags);
}

bool
MockAccel::extractData(AVCodecContext* codecCtx, VideoFrame& container)
{
    RING_DBG("MockAccel extractData");
    return true;
}
#endif

}} // namespace ring::video
