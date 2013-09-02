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
#include "logger.h"


namespace sfl_video {

/*=== VideoPacket  ===========================================================*/

VideoPacket::VideoPacket() : packet_(0)
{
    packet_ = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));
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

VideoCodec::VideoCodec() : options_(0) {}

void VideoCodec::setOption(const char *name, const char *value)
{
    av_dict_set(&options_, name, value, 0);
}

/*=== VideoFrame =============================================================*/

VideoFrame::VideoFrame() : frame_(avcodec_alloc_frame()), allocated_(false) {}

VideoFrame::~VideoFrame() { avcodec_free_frame(&frame_); }

int VideoFrame::getFormat() const { return libav_utils::sfl_pixel_format(frame_->format); }
int VideoFrame::getWidth() const { return frame_->width; }
int VideoFrame::getHeight() const { return frame_->height; }

bool VideoFrame::allocBuffer(int width, int height, int pix_fmt)
{
    AVPixelFormat libav_pix_fmt = (AVPixelFormat) libav_utils::libav_pixel_format(pix_fmt);
    if (allocated_ and (width != frame_->width ||
                        height != frame_->height ||
                        libav_pix_fmt != frame_->format))
        avpicture_free((AVPicture *) frame_);

    allocated_ = not avpicture_alloc((AVPicture *) frame_,
                                     libav_pix_fmt, width, height);
    if (allocated_)
        setGeometry(width, height, pix_fmt);

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
    if (allocated_)
        avpicture_free((AVPicture *) frame_);

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

int VideoFrame::blit(VideoFrame &src, int xoff, int yoff)
{
    const AVFrame *src_frame = src.get();

    if (src_frame->format != PIX_FMT_YUV420P
        || frame_->format != PIX_FMT_YUV420P) {
        ERROR("Unsupported pixel format");
        return -1;
    }

    uint8_t *src_data, *dst_data;
	ssize_t dst_stride;

    // Y
    dst_stride = frame_->linesize[0];
	src_data = src_frame->data[0];
	dst_data = frame_->data[0] + yoff * frame_->height * dst_stride + xoff;
	for (int i = 0; i < src_frame->height; i++) {
		memcpy(dst_data, src_data, src_frame->linesize[0]);
		src_data += src_frame->linesize[0];
		dst_data += dst_stride;
	}

    // U
	dst_stride = frame_->linesize[1];
	src_data = src_frame->data[1];
	dst_data = frame_->data[1] + yoff * frame_->height / 2 * dst_stride + xoff / 2;
	for (int i = 0; i < src_frame->height / 2; i++) {
		memcpy(dst_data, src_data, src_frame->linesize[1]);
		src_data += src_frame->linesize[1];
		dst_data += dst_stride;
	}

    // V
	dst_stride = frame_->linesize[2];
	src_data = src_frame->data[2];
	dst_data = frame_->data[2] + yoff * frame_->height / 2 * dst_stride + xoff / 2;
	for (int i = 0; i < src_frame->height / 2; i++) {
		memcpy(dst_data, src_data, src_frame->linesize[2]);
		src_data += src_frame->linesize[2];
		dst_data += dst_stride;
	}

    return 0;
}

void VideoFrame::copy(VideoFrame &src)
{
    const AVFrame *src_frame = src.get();
    av_picture_copy((AVPicture *)frame_, (AVPicture *)src_frame,
                    (AVPixelFormat)frame_->format, src_frame->width,
                    src_frame->height);
}

void VideoFrame::test()
{
    memset(frame_->data[0], 0xaa, frame_->linesize[0]*frame_->height/2);
}

/*=== VideoGenerator =========================================================*/

VideoGenerator::VideoGenerator() :
    VideoSource::VideoSource()
    , mutex_()
    , condition_()
    , writableFrame_()
    , lastFrame_()
{
    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&condition_, NULL);
}

VideoGenerator::~VideoGenerator()
{
    pthread_cond_destroy(&condition_);
    pthread_mutex_destroy(&mutex_);
}

void VideoGenerator::publishFrame()
{
    pthread_mutex_lock(&mutex_);
    {
        lastFrame_ = std::move(writableFrame_); // we owns it now
        pthread_cond_signal(&condition_);
    }
    pthread_mutex_unlock(&mutex_);
}

std::shared_ptr<VideoFrame> VideoGenerator::waitNewFrame()
{
    pthread_mutex_lock(&mutex_);
    pthread_cond_wait(&condition_, &mutex_);
    pthread_mutex_unlock(&mutex_);

    return obtainLastFrame();
}

std::shared_ptr<VideoFrame> VideoGenerator::obtainLastFrame()
{
    std::shared_ptr<VideoFrame> frame;

    pthread_mutex_lock(&mutex_);
    frame = lastFrame_;
    pthread_mutex_unlock(&mutex_);

    return frame;
}

VideoFrame& VideoGenerator::getNewFrame()
{
    VideoFrame* frame;

    pthread_mutex_lock(&mutex_);
    {
        if (writableFrame_) {
            frame = writableFrame_.get();
            frame->setdefaults();
        } else {
            frame = new VideoFrame();
            writableFrame_.reset(frame);
        }
    }
    pthread_mutex_unlock(&mutex_);

    return *frame;
}

}
