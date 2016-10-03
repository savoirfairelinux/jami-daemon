/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#ifdef HAVE_VIDEOTOOLBOX_ACCEL
#include "osxvideo/videotoolbox.h"
#endif

#include "string_utils.h"
#include "logger.h"

#include <initializer_list>
#include <algorithm>

namespace ring { namespace video {

static constexpr const unsigned MAX_ACCEL_FAILURES { 5 };

static AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);
    if (!accel) {
        // invalid state, try to recover
        return avcodec_default_get_format(codecCtx, formats);
    }

    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; i++) {
        if (formats[i] == accel->format()) {
            accel->setWidth(codecCtx->coded_width);
            accel->setHeight(codecCtx->coded_height);
            accel->setProfile(codecCtx->profile);
            if (accel->init(codecCtx))
                return accel->format();
            break;
        }
    }

    accel->fail(true);
    RING_WARN("Falling back to software decoding");
    codecCtx->get_format = avcodec_default_get_format;
    codecCtx->get_buffer2 = avcodec_default_get_buffer2;
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; i++) {
        auto desc = av_pix_fmt_desc_get(formats[i]);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            return formats[i];
        }
    }

    return AV_PIX_FMT_NONE;
}

static int
allocateBufferCb(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    if (auto accel = static_cast<HardwareAccel*>(codecCtx->opaque)) {
        if (!accel->hasFailed() && accel->allocateBuffer(codecCtx, frame, flags) == 0) {
            accel->succeed();
            return 0;
        }

        accel->fail();
    }

    return avcodec_default_get_buffer2(codecCtx, frame, flags);
}

template <class T>
static std::unique_ptr<HardwareAccel>
makeHardwareAccel(const AccelInfo& info) {
    return std::unique_ptr<HardwareAccel>(new T(info));
}

static const AccelInfo*
getAccelInfo(std::initializer_list<AccelID> codecAccels)
{
    /* Each item in this array reprensents a fully implemented hardware acceleration in Ring.
     * Each item should be enclosed in an #ifdef to prevent its compilation on an
     * unsupported platform (VAAPI for Linux Intel won't compile on a Mac).
     * A new item should be added when support for an acceleration has been added to Ring,
     * which is also supported by FFmpeg.
     * Steps to add an acceleration (after its implementation):
     * - If it doesn't yet exist, add a unique AccelID
     * - Specify its AVPixelFormat (the one used by FFmpeg)
     * - Give it a name (this is used for the daemon logs)
     * - Add a function pointer that returns an instance (makeHardwareAccel does this already)
     * Note: the acceleration's header file must be guarded by the same #ifdef as
     * in this array.
     */
    static const AccelInfo accels[] = {
#if defined(HAVE_VAAPI_ACCEL_X11) || defined(HAVE_VAAPI_ACCEL_DRM)
        { AccelID::Vaapi, AV_PIX_FMT_VAAPI, "vaapi", makeHardwareAccel<VaapiAccel> },
#endif
#ifdef HAVE_VIDEOTOOLBOX_ACCEL
        { AccelID::VideoToolbox, AV_PIX_FMT_VIDEOTOOLBOX, "videotoolbox", makeHardwareAccel<VideoToolboxAccel> },
#endif
    };

    for (auto& accel : accels) {
        for (auto& ca : codecAccels) {
            if (accel.type == ca) {
                RING_DBG("Found '%s' hardware acceleration", accel.name.c_str());
                return &accel;
            }
        }
    }

    RING_DBG("Did not find a matching hardware acceleration");
    return nullptr;
}

HardwareAccel::HardwareAccel(const AccelInfo& info)
    : type_(info.type)
    , format_(info.format)
    , name_(info.name)
{
    failCount_ = 0;
    fallback_ = false;
    width_ = -1;
    height_ = -1;
    profile_ = -1;
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

std::unique_ptr<HardwareAccel>
makeHardwareAccel(AVCodecContext* codecCtx)
{
    const AccelInfo* info = nullptr;

    switch (codecCtx->codec_id) {
        case AV_CODEC_ID_H264:
            info = getAccelInfo({
                AccelID::Vdpau,
                AccelID::VideoToolbox,
                AccelID::Dxva2,
                AccelID::Vaapi
            });
            break;
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            info = getAccelInfo({
                AccelID::Vdpau,
                AccelID::VideoToolbox,
                AccelID::Vaapi
            });
            break;
        default:
            break;
    }

    if (info && info->type != AccelID::NoAccel) {
        if (auto accel = info->create(*info)) {
            codecCtx->get_format = getFormatCb;
            codecCtx->get_buffer2 = allocateBufferCb;
            codecCtx->thread_safe_callbacks = 1;
            codecCtx->thread_count = 1;
            RING_DBG("Hardware acceleration setup has succeeded");
            return accel;
        } else
            RING_ERR("Failed to create %s hardware acceleration", info->name.c_str());
    }

    RING_WARN("Not using hardware acceleration");
    return nullptr;
}

}} // namespace ring::video
