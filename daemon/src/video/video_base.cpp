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

#include "libav_deps.h"
#include "video_base.h"


namespace sfl_video {

VideoPacket::VideoPacket() : packet_(0)
{
    packet_ = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));
}

VideoPacket::~VideoPacket() { av_free_packet(packet_); }

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
    //ctx_->max_packet_size = buffer_size;
}

VideoIOHandle::~VideoIOHandle() { av_free(ctx_); av_free(buf_); }

VideoCodec::VideoCodec() : options_(0) {}

void VideoCodec::setOption(const char *name, const char *value)
{
    av_dict_set(&options_, name, value, 0);
}

VideoFrame::VideoFrame() : frame_(avcodec_alloc_frame()) {}

VideoFrame::~VideoFrame() { avcodec_free_frame(&frame_); }

void VideoFrame::setGeometry(int width, int height, int pix_fmt)
{
    frame_->format = libav_utils::libav_pixel_format(pix_fmt);
    frame_->width = width;
    frame_->height = height;
}

void VideoFrame::setDestination(void *data)
{
    avpicture_fill((AVPicture *) frame_, (uint8_t *) data,
                   (PixelFormat) frame_->format, frame_->width,
                   frame_->height);
}

size_t VideoFrame::getSize()
{
    return avpicture_get_size((PixelFormat) frame_->format,
                              frame_->width,
                              frame_->height);
}

}
