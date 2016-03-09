/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
#include "libav_utils.h"
#include "media_buffer.h"
#include "dring/videomanager_interface.h"

#include <new> // std::bad_alloc
#include <cstdlib>
#include <cstring> // std::memset

namespace ring {

MediaFrame::MediaFrame()
    : frame_ {av_frame_alloc(), [](AVFrame* frame){ av_frame_free(&frame); }}
{
    if (not frame_)
        throw std::bad_alloc();
}

void
MediaFrame::reset() noexcept
{
    av_frame_unref(frame_.get());
}

#ifdef RING_VIDEO

VideoFrame::~VideoFrame()
{
    if (releaseBufferCb_)
        releaseBufferCb_(ptr_);
}

void
VideoFrame::reset() noexcept
{
    MediaFrame::reset();
#if !USE_OLD_AVU
    allocated_ = false;
#endif
    releaseBufferCb_ = {};
}

size_t
VideoFrame::size() const noexcept
{
    return videoFrameSize(format(), width(), height());
}

int
VideoFrame::format() const noexcept
{
    return libav_utils::ring_pixel_format(frame_->format);
}

int
VideoFrame::width() const noexcept
{
    return frame_->width;
}

int
VideoFrame::height() const noexcept
{
    return frame_->height;
}

void
VideoFrame::setGeometry(int format, int width, int height) noexcept
{
    frame_->format = libav_utils::libav_pixel_format(format);
    frame_->width = width;
    frame_->height = height;
}

void
VideoFrame::reserve(int format, int width, int height)
{
    auto libav_format = (AVPixelFormat)libav_utils::libav_pixel_format(format);
    auto libav_frame = frame_.get();

    if (allocated_) {
        // nothing to do if same properties
        if (width == libav_frame->width
            and height == libav_frame->height
            and libav_format == libav_frame->format)
#if USE_OLD_AVU
        avpicture_free((AVPicture *) libav_frame);
#else
        av_frame_unref(libav_frame);
#endif
    }

    setGeometry(format, width, height);
    if (av_frame_get_buffer(libav_frame, 32))
        throw std::bad_alloc();
    allocated_ = true;
    releaseBufferCb_ = {};
}

void
VideoFrame::setFromMemory(uint8_t* ptr, int format, int width, int height) noexcept
{
    reset();
    setGeometry(format, width, height);
    if (not ptr)
        return;
    avpicture_fill((AVPicture*)frame_.get(), (uint8_t*)ptr,
                   (AVPixelFormat)frame_->format, width, height);
}

void
VideoFrame::setFromMemory(uint8_t* ptr, int format, int width, int height,
                          std::function<void(uint8_t*)> cb) noexcept
{
    setFromMemory(ptr, format, width, height);
    if (cb) {
        releaseBufferCb_ = cb;
        ptr_ = ptr;
    }
}

void
VideoFrame::noise()
{
    auto f = frame_.get();
    if (f->data[0] == nullptr)
        return;
    for (std::size_t i=0 ; i < size(); ++i) {
        f->data[0][i] = std::rand() & 255;
    }
}

VideoFrame&
VideoFrame::operator =(const VideoFrame& src)
{
    reserve(src.format(), src.width(), src.height());
    av_picture_copy((AVPicture *)frame_.get(), (AVPicture *)src.pointer(),
                    (AVPixelFormat)frame_->format,
                    frame_->width, frame_->height);
    return *this;
}

//=== HELPERS ==================================================================

std::size_t
videoFrameSize(int format, int width, int height)
{
    return avpicture_get_size((AVPixelFormat)libav_utils::libav_pixel_format(format),
                              width, height);
}

void
yuv422_clear_to_black(VideoFrame& frame)
{
    const auto libav_frame = frame.pointer();
    const auto desc = av_pix_fmt_desc_get((AVPixelFormat)libav_frame->format);
    if (not desc)
        return;

    if (not libav_utils::is_yuv_planar(*desc)) {
        // not planar
        auto stride = libav_frame->linesize[0];
        if (libav_frame->width % 2) {
            // non-even width (16bits write x-loop)
            for (int y = 0; y < libav_frame->height; ++y) {
                auto src = &libav_frame->data[0][y * stride];
                for (int x = 0; x < libav_frame->width; ++x) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    *((uint16_t*)src) = 0x8000;
#else
                    *((uint16_t*)src) = 0x0080;
#endif
                    src += 2;
                }
            }
        } else if (libav_frame->width % 4) {
            // non-quad width (32bits write x-loop)
            for (int y = 0; y < libav_frame->height; ++y) {
                auto src = &libav_frame->data[0][y * stride];
                for (int x = 0; x < libav_frame->width / 2; ++x) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    *((uint32_t*)src) = 0x80008000;
#else
                    *((uint32_t*)src) = 0x00800080;
#endif
                    src += 4;
                }
            }
        } else {
            // quad width (64bits write x-loop)
            for (int y = 0; y < libav_frame->height; ++y) {
                auto src = &libav_frame->data[0][y * stride];
                for (int x = 0; x < libav_frame->width / 4; ++x) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    *((uint64_t*)src) = 0x8000800080008000;
#else
                    *((uint64_t*)src) = 0x0080008000800080;
#endif
                    src += 8;
                }
            }
        }
    } else {
        // planar
        std::memset(libav_frame->data[0], 0, libav_frame->linesize[0] * libav_frame->height);
        // 128 is the black level for U/V channels
        std::memset(libav_frame->data[1], 128, libav_frame->linesize[1] * (libav_frame->height >> desc->log2_chroma_w));
        std::memset(libav_frame->data[2], 128, libav_frame->linesize[2] * (libav_frame->height >> desc->log2_chroma_h));
    }
}

#endif // RING_VIDEO

} // namespace ring
