/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <cassert>
#include "libav_deps.h"
#include "video_base.h"
#include "logger.h"


namespace sfl_video {

/*=== VideoPacket  ===========================================================*/

VideoPacket::VideoPacket() : packet_(static_cast<AVPacket *>(av_mallocz(sizeof(AVPacket))))
{
    av_init_packet(packet_);
}

VideoPacket::~VideoPacket() { av_free_packet(packet_); av_free(packet_); }

/*=== VideoIOHandle  =========================================================*/

VideoIOHandle::VideoIOHandle(ssize_t buffer_size,
                             bool writeable,
                             io_readcallback read_cb,
                             io_writecallback write_cb,
                             io_seekcallback seek_cb,
                             void *opaque) : ctx_(0), buf_(0)

{
    buf_ = static_cast<unsigned char *>(av_malloc(buffer_size));
    ctx_ = avio_alloc_context(buf_, buffer_size, writeable, opaque, read_cb,
                              write_cb, seek_cb);
    ctx_->max_packet_size = buffer_size;
}

VideoIOHandle::~VideoIOHandle() { av_free(ctx_); av_free(buf_); }

/*=== VideoFrame =============================================================*/

VideoFrame::VideoFrame() : frame_(avcodec_alloc_frame()), allocated_(false) {}

VideoFrame::~VideoFrame()
{
    if (allocated_)
        avpicture_free((AVPicture *) frame_);
    avcodec_free_frame(&frame_);
}

int VideoFrame::getPixelFormat() const
{ return libav_utils::sfl_pixel_format(frame_->format); }

int VideoFrame::getWidth() const
{ return frame_->width; }

int VideoFrame::getHeight() const
{ return frame_->height; }

bool VideoFrame::allocBuffer(int width, int height, int pix_fmt)
{
    AVPixelFormat libav_pix_fmt = (AVPixelFormat) libav_utils::libav_pixel_format(pix_fmt);
    if (allocated_) {
        // nothing to do if same properties
        if (width == frame_->width
            and height == frame_->height
            and libav_pix_fmt == frame_->format)
            return true;
        avpicture_free((AVPicture *) frame_);
    }

    allocated_ = not avpicture_alloc((AVPicture *) frame_,
                                     libav_pix_fmt, width, height);
    if (allocated_) {
        setGeometry(width, height, pix_fmt);
        clear();
    }

    return allocated_;
}

void VideoFrame::setdefaults()
{
    avcodec_get_frame_defaults(frame_);
}

void VideoFrame::setGeometry(int width, int height, int pix_fmt)
{
    frame_->format = libav_utils::libav_pixel_format(pix_fmt);
    frame_->width = width;
    frame_->height = height;
}

void VideoFrame::setDestination(void *data)
{
    if (allocated_) {
        avpicture_free((AVPicture *) frame_);
        allocated_ = false;
    }

    avpicture_fill((AVPicture *) frame_, (uint8_t *) data,
                   (AVPixelFormat) frame_->format, frame_->width,
                   frame_->height);
}

size_t VideoFrame::getSize()
{
    return avpicture_get_size((AVPixelFormat) frame_->format,
                              frame_->width,
                              frame_->height);
}

size_t VideoFrame::getSize(int width, int height, int format)
{
    return avpicture_get_size(
        (AVPixelFormat) libav_utils::libav_pixel_format(format), width, height);
}

// Only supports YUV_420P input and output
int VideoFrame::blit(VideoFrame &src, int xoff, int yoff)
{
    const AVFrame *src_frame = src.get();

    assert(src_frame->format == PIXEL_FORMAT(YUV420P) and
           frame_->format == PIXEL_FORMAT(YUV420P));

    auto copy_plane = [&] (unsigned idx) {
        const unsigned divisor = idx == 0 ? 1 : 2;
        ssize_t dst_stride = frame_->linesize[idx];
        uint8_t *src_data = src_frame->data[idx];
        uint8_t *dst_data = frame_->data[idx] + yoff / divisor * dst_stride + xoff / divisor;
        for (unsigned i = 0; i < src_frame->height / divisor; i++) {
            memcpy(dst_data, src_data, src_frame->linesize[idx]);
            src_data += src_frame->linesize[idx];
            dst_data += dst_stride;
        }
    };

    for (unsigned plane = 0; plane < 3; ++plane)
        copy_plane(plane);

    return 0;
}

void VideoFrame::copy(VideoFrame &dst)
{
    const AVFrame *dst_frame = dst.get();
    av_picture_copy((AVPicture *)dst_frame, (AVPicture *)frame_,
                    (AVPixelFormat)frame_->format, frame_->width,
                    frame_->height);
}

void VideoFrame::clear()
{
    // FIXME: beurk!!!!

    memset(frame_->data[0], 0, frame_->linesize[0]*frame_->height);
    // 128 is the black level for U/V channels
    memset(frame_->data[1], 128, frame_->linesize[1]*frame_->height/2);
    memset(frame_->data[2], 128, frame_->linesize[2]*frame_->height/2);
}


void VideoFrame::test()
{
    memset(frame_->data[0], 0xaa, frame_->linesize[0]*frame_->height/2);
}

/*=== VideoGenerator =========================================================*/

VideoFrame& VideoGenerator::getNewFrame()
{
    std::unique_lock<std::mutex> lk(mutex_);
    if (writableFrame_)
        writableFrame_->setdefaults();
    else
        writableFrame_.reset(new VideoFrame());
    return *writableFrame_.get();
}

void VideoGenerator::publishFrame()
{
    std::unique_lock<std::mutex> lk(mutex_);
    lastFrame_ = std::move(writableFrame_);
    notify(lastFrame_);
}

std::shared_ptr<VideoFrame> VideoGenerator::obtainLastFrame()
{
    std::unique_lock<std::mutex> lk(mutex_);
    return lastFrame_;
}

}
