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

VideoCodec::VideoCodec() : options_(0) {}

void VideoCodec::setOption(const char *name, const char *value)
{
    av_dict_set(&options_, name, value, 0);
}

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


static int flipPlanarHorizontal(AVFrame *frame)
{
    uint8_t *inrow, *outrow;
    int step, hsub, vsub;
    const AVPixFmtDescriptor *pix_desc = av_pix_fmt_desc_get((AVPixelFormat) libav_utils::libav_pixel_format(frame->format));
    if (!pix_desc) {
        ERROR("Could not get pixel descriptor");
        return -1;
    }
    int max_step[4];    ///< max pixel step for each plane, expressed as a number of bytes
    av_image_fill_max_pixsteps(max_step, NULL, pix_desc);

    for (int plane = 0; plane < 4 && frame->data[plane]; plane++) {
        step = max_step[plane];
        hsub = (plane == 1 || plane == 2) ? pix_desc->log2_chroma_w : 0;
        vsub = (plane == 1 || plane == 2) ? pix_desc->log2_chroma_h : 0;

        outrow = frame->data[plane];
        inrow  = frame->data[plane] + ((frame->width >> hsub) - 1) * step;
        for (int i = 0; i < frame->height >> vsub; i++) {
            switch (step) {
            case 1:
                for (int j = 0; j < ((frame->width >> hsub) / 2); j++)
                    std::swap(outrow[j], inrow[-j]);
            break;

            case 2:
            {
                uint16_t *outrow16 = (uint16_t *) outrow;
                uint16_t * inrow16 = (uint16_t *) inrow;
                for (int j = 0; j < (frame->width >> hsub) / 2; j++)
                    std::swap(outrow16[j], inrow16[-j]);
            }
            break;

            case 3:
            {
                uint8_t *in  =  inrow;
                uint8_t *out = outrow;
                for (int j = 0; j < (frame->width >> hsub) / 2; j++, out += 3, in -= 3) {
                    int32_t vl = AV_RB24(in);
                    int32_t vr = AV_RB24(out);
                    AV_WB24(out, vl);
                    AV_WB24(in, vr);
                }
            }
            break;

            case 4:
            {
                uint32_t *outrow32 = (uint32_t *)outrow;
                uint32_t * inrow32 = (uint32_t *) inrow;
                for (int j = 0; j < (frame->width >> hsub) / 2; j++)
                    std::swap(outrow32[j], inrow32[-j]);
            }
            break;

            default:
                for (int j = 0; j < (frame->width >> hsub) / 2; j++) {
                    uint8_t tmp[step];
                    memcpy(tmp, outrow + j * step, step);
                    memcpy(outrow + j * step, inrow - j * step, step);
                    memcpy(inrow - j * step, tmp, step);
                }
            }

            inrow  += frame->linesize[plane];
            outrow += frame->linesize[plane];
        }
    }
    return 0;
}

static int
flipPackedHorizontal(AVFrame *frame, size_t offset)
{
    uint16_t *inpixel, *outpixel;
    inpixel = outpixel = (uint16_t *) frame->data[0];
    const unsigned pixelsPerRow = frame->linesize[0] / sizeof(*inpixel);

    for (int i = 0; i < frame->height; ++i) {
        // swap pixels first (luma AND chroma)
        for (int j = 0; j < frame->width / 2; ++j)
            std::swap(outpixel[j], inpixel[frame->width - 1 - j]);

        // swap Cb with Cr for each pixel
        uint8_t *inchroma, *outchroma;
        inchroma = outchroma = ((uint8_t *) inpixel) + offset;
        for (int j = 0; j < frame->width * 2; j += 4)
            std::swap(outchroma[j], inchroma[j + 2]);

        inpixel += pixelsPerRow;
        outpixel += pixelsPerRow;
    }

    return 0;
}

int VideoFrame::mirror()
{
    switch (frame_->format) {
        case PIXEL_FORMAT(YUYV422):
            return flipPackedHorizontal(frame_, 1);
        case PIXEL_FORMAT(UYVY422):
            return flipPackedHorizontal(frame_, 0);
        case PIXEL_FORMAT(RGB48BE):
        case PIXEL_FORMAT(RGB48LE):
        case PIXEL_FORMAT(BGR48BE):
        case PIXEL_FORMAT(BGR48LE):
        case PIXEL_FORMAT(ARGB):
        case PIXEL_FORMAT(RGBA):
        case PIXEL_FORMAT(ABGR):
        case PIXEL_FORMAT(BGRA):
        case PIXEL_FORMAT(RGB24):
        case PIXEL_FORMAT(BGR24):
        case PIXEL_FORMAT(RGB565BE):
        case PIXEL_FORMAT(RGB565LE):
        case PIXEL_FORMAT(RGB555BE):
        case PIXEL_FORMAT(RGB555LE):
        case PIXEL_FORMAT(BGR565BE):
        case PIXEL_FORMAT(BGR565LE):
        case PIXEL_FORMAT(BGR555BE):
        case PIXEL_FORMAT(BGR555LE):
        case PIXEL_FORMAT(GRAY16BE):
        case PIXEL_FORMAT(GRAY16LE):
        case PIXEL_FORMAT(YUV420P16LE):
        case PIXEL_FORMAT(YUV420P16BE):
        case PIXEL_FORMAT(YUV422P16LE):
        case PIXEL_FORMAT(YUV422P16BE):
        case PIXEL_FORMAT(YUV444P16LE):
        case PIXEL_FORMAT(YUV444P16BE):
        case PIXEL_FORMAT(YUV444P):
        case PIXEL_FORMAT(YUV422P):
        case PIXEL_FORMAT(YUV420P):
        case PIXEL_FORMAT(YUV411P):
        case PIXEL_FORMAT(YUV410P):
        case PIXEL_FORMAT(YUV440P):
        case PIXEL_FORMAT(YUVJ444P):
        case PIXEL_FORMAT(YUVJ422P):
        case PIXEL_FORMAT(YUVJ420P):
        case PIXEL_FORMAT(YUVJ440P):
        case PIXEL_FORMAT(YUVA420P):
        case PIXEL_FORMAT(RGB8):
        case PIXEL_FORMAT(BGR8):
        case PIXEL_FORMAT(RGB4_BYTE):
        case PIXEL_FORMAT(BGR4_BYTE):
        case PIXEL_FORMAT(PAL8):
        case PIXEL_FORMAT(GRAY8):
        case PIXEL_FORMAT(NONE):
            break;
        default:
            ERROR("Unsupported pixel format %s",
                  av_get_pix_fmt_name((AVPixelFormat) frame_->format));
            return -1;
    }
    return flipPlanarHorizontal(frame_);
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
