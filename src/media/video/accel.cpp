/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

#ifdef RING_VAAPI
#include "v4l2/vaapi.h"
#endif

#ifdef RING_VDPAU
#include "v4l2/vdpau.h"
#endif

#if defined(RING_VIDEOTOOLBOX) || defined(RING_VDA)
#include "osxvideo/videotoolbox.h"
#endif

#include "string_utils.h"
#include "logger.h"

#include <sstream>
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
            accel->setCodecCtx(codecCtx);
            if (accel->init())
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
        if (!accel->hasFailed() && accel->allocateBuffer(frame, flags) == 0) {
            accel->succeedAllocation();
            return 0;
        }

        accel->failAllocation();
    }

    return avcodec_default_get_buffer2(codecCtx, frame, flags);
}

HardwareAccel::HardwareAccel(const std::string& name, const AVPixelFormat format)
    : name_(name)
    , format_(format)
{}

void
HardwareAccel::failAllocation()
{
    ++allocationFails_;
    fail(false);
}

void
HardwareAccel::failExtraction()
{
    ++extractionFails_;
    fail(false);
}

void
HardwareAccel::fail(bool forceFallback)
{
    if (allocationFails_ >= MAX_ACCEL_FAILURES || extractionFails_ >= MAX_ACCEL_FAILURES || forceFallback) {
        RING_ERR("Hardware acceleration failure");
        fallback_ = true;
        allocationFails_ = 0;
        extractionFails_ = 0;
        codecCtx_->get_format = avcodec_default_get_format;
        codecCtx_->get_buffer2 = avcodec_default_get_buffer2;
    }
}

bool
HardwareAccel::extractData(VideoFrame& input)
{
    try {
        auto inFrame = input.pointer();

        if (inFrame->format != format_) {
            std::stringstream buf;
            buf << "Frame format mismatch: expected " << av_get_pix_fmt_name(format_);
            buf << ", got " << av_get_pix_fmt_name((AVPixelFormat)inFrame->format);
            throw std::runtime_error(buf.str());
        }

        // FFmpeg requires a second frame in which to transfer the data
        // from the GPU buffer to the main memory
        auto output = std::unique_ptr<VideoFrame>(new VideoFrame());
        auto outFrame = output->pointer();
        outFrame->format = AV_PIX_FMT_YUV420P;

        extractData(input, *output);

        // move outFrame into inFrame so the caller receives extracted image data
        // but we have to delete inFrame first
        av_frame_unref(inFrame);
        av_frame_move_ref(inFrame, outFrame);
    } catch (const std::runtime_error& e) {
        failExtraction();
        RING_ERR("%s", e.what());
        return false;
    }

    succeedExtraction();
    return true;
}

template <class T>
static std::unique_ptr<HardwareAccel>
makeHardwareAccel(const std::string name, const AVPixelFormat format) {
    return std::unique_ptr<HardwareAccel>(new T(name, format));
}

std::unique_ptr<HardwareAccel>
makeHardwareAccel(AVCodecContext* codecCtx)
{
    enum class AccelID {
        NoAccel,
        Vdpau,
        Vaapi,
        VideoToolbox,
        Vda,
    };

    struct AccelInfo {
        AccelID type;
        std::string name;
        AVPixelFormat format;
        std::unique_ptr<HardwareAccel> (*create)(const std::string name, const AVPixelFormat format);
    };

    /* Each item in this array reprensents a fully implemented hardware acceleration in Ring.
     * Each item should be enclosed in an #ifdef to prevent its compilation on an
     * unsupported platform (VAAPI for Linux Intel won't compile on a Mac).
     * A new item should be added when support for an acceleration has been added to Ring,
     * which is also supported by FFmpeg.
     * Steps to add an acceleration (after its implementation):
     * - Create an AccelID and add it to the switch statement
     * - Give it a name (this is used for the daemon logs)
     * - Specify its AVPixelFormat (the one used by FFmpeg: check pixfmt.h)
     * - Add a function pointer that returns an instance (makeHardwareAccel<> does this already)
     * Note: the include of the acceleration's header file must be guarded by the same #ifdef as
     * in this array.
     */
    const AccelInfo accels[] = {
#ifdef RING_VAAPI
        { AccelID::Vaapi, "vaapi", AV_PIX_FMT_VAAPI, makeHardwareAccel<VaapiAccel> },
#endif
#ifdef RING_VDPAU
        { AccelID::Vdpau, "vdpau", AV_PIX_FMT_VDPAU, makeHardwareAccel<VdpauAccel> },
#endif
#ifdef RING_VIDEOTOOLBOX
        { AccelID::VideoToolbox, "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, makeHardwareAccel<VideoToolboxAccel> },
#endif
#ifdef RING_VDA
        { AccelID::Vda, "vda", AV_PIX_FMT_VDA, makeHardwareAccel<VideoToolboxAccel> },
#endif
        { AccelID::NoAccel, "none", AV_PIX_FMT_NONE, nullptr },
    };

    std::vector<AccelID> possibleAccels = {};
    switch (codecCtx->codec_id) {
        case AV_CODEC_ID_H264:
            possibleAccels.push_back(AccelID::Vdpau);
            possibleAccels.push_back(AccelID::Vaapi);
            possibleAccels.push_back(AccelID::VideoToolbox);
            possibleAccels.push_back(AccelID::Vda);
            break;
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_H263P:
            possibleAccels.push_back(AccelID::Vdpau);
            possibleAccels.push_back(AccelID::Vaapi);
            possibleAccels.push_back(AccelID::VideoToolbox);
            break;
        case AV_CODEC_ID_VP8:
            break;
        default:
            break;
    }

    for (auto& info : accels) {
        for (auto& pa : possibleAccels) {
            if (info.type == pa) {
                auto accel = info.create(info.name, info.format);
                // don't break if the check fails, we want to check every possibility
                if (accel->checkAvailability()) {
                    codecCtx->get_format = getFormatCb;
                    codecCtx->get_buffer2 = allocateBufferCb;
                    codecCtx->thread_safe_callbacks = 1;
                    RING_DBG("Attempting to use '%s' hardware acceleration", accel->name().c_str());
                    return accel;
                }
            }
        }
    }

    RING_WARN("Not using hardware acceleration");
    return nullptr;
}

}} // namespace ring::video
