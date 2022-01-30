/****************************************************************************
 *    Copyright (C) 2018-2022 Savoir-faire Linux Inc.                       *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "video_frame_buffer.h"
#include "media/libav_utils.h"
extern "C" {
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
}

namespace DRing {

GenericVideoFrameBuffer::GenericVideoFrameBuffer(std::size_t size)
    : owner_(true)
    , bufferSize_(size)
{
    assert(bufferSize_ != 0);
}

GenericVideoFrameBuffer::GenericVideoFrameBuffer(uint8_t* buf, size_t size)
    : owner_(false)
    , videoBuffer_(buf)
    , bufferSize_(size)
{
    // Must provide a valid buffer.
    assert(videoBuffer_ != nullptr);
    assert(bufferSize_ != 0);
}

GenericVideoFrameBuffer::~GenericVideoFrameBuffer()
{
    if (owner_) {
        assert(bufferSize_ != 0);
        if (videoBuffer_ != nullptr) {
            std::free(videoBuffer_);
        }
    }
}

void
GenericVideoFrameBuffer::allocateMemory(int format, int width, int height, int align)
{
    format_ = format;
    width_ = width;
    height_ = height;
    assert(bufferSize_ > 0);
    videoBuffer_ = reinterpret_cast<uint8_t*>(std::aligned_alloc(align, bufferSize_));
    assert(videoBuffer_ != nullptr);
}

std::size_t
GenericVideoFrameBuffer::size() const
{
    return bufferSize_;
}

int
GenericVideoFrameBuffer::width() const
{
    return width_;
}

int
GenericVideoFrameBuffer::height() const
{
    return height_;
}

int
GenericVideoFrameBuffer::planes() const
{
    return strides_.size();
}

int
GenericVideoFrameBuffer::stride(int plane) const
{
    assert(height_ > 0);
    assert(bufferSize_ > 0);
    return bufferSize_ / height_;
}

uint8_t*
GenericVideoFrameBuffer::ptr(int plane)
{
    if (videoBuffer_)
        return videoBuffer_;

    return {};
}

int
GenericVideoFrameBuffer::format() const
{
    return format_;
}

AVFrame*
GenericVideoFrameBuffer::avframe()
{
    return {};
}

// ***********************************************************************
// *****   AV Frame Buffer
// ***********************************************************************

AVVideoFrameBuffer::AVVideoFrameBuffer(std::size_t size)
    : bufferSize_(size)
{
    assert(bufferSize_ != 0);
    avframe_.reset(av_frame_alloc());
    assert(avframe_ != nullptr);
}

AVVideoFrameBuffer::~AVVideoFrameBuffer() {}

void
AVVideoFrameBuffer::allocateMemory(int format, int width, int height, int align)
{
    // For AVFrame, strides should not be set, but rather determined internally
    // using ffmpeg helpers.
    assert(avframe_);
    avframe_->format = format;
    avframe_->width = width;
    avframe_->height = height;

    int linesizes[4];
    av_image_fill_linesizes(linesizes, (AVPixelFormat) format, width);

    // TODO. Seems weird, but it seems there is no direct method to get
    // the number of planes ??!!
    planes_ = 0;
    int i = 0;
    while (i < 4 and linesizes[i] > 0) {
        planes_++;
        i++;
    }

    if (av_frame_get_buffer(avframe_.get(), align) < 0) {
        bufferSize_ = 0;
        // TODO. Replace by a trace.
        assert(false);
        return;
    }

    // Just validate that the provided size matches the frame properties.
    assert(bufferSize_
           == (size_t) av_image_get_buffer_size((AVPixelFormat) format, width, height, align));
}

std::size_t
AVVideoFrameBuffer::size() const
{
    return bufferSize_;
}

int
AVVideoFrameBuffer::width() const
{
    return avframe_->width;
}

int
AVVideoFrameBuffer::height() const
{
    return avframe_->height;
}

int
AVVideoFrameBuffer::planes() const
{
    return planes_;
}

int
AVVideoFrameBuffer::stride(int plane) const
{
    if (plane >= planes_)
        return 0;
    return avframe_->linesize[plane];
}

uint8_t*
AVVideoFrameBuffer::ptr(int plane)
{
    if (plane >= planes_)
        return {};
    return avframe_->data[plane];
}

int
AVVideoFrameBuffer::format() const
{
    return avframe_->format;
}

AVFrame*
AVVideoFrameBuffer::avframe()
{
    return avframe_.get();
}

SinkTarget::VideoFrameBufferIfPtr
createVideoFrameBufferInstance(uint8_t* buf, size_t size)
{
    if (buf != nullptr)
        return std::make_unique<GenericVideoFrameBuffer>(buf, size);

    return std::make_unique<GenericVideoFrameBuffer>(size);
}

} // namespace DRing