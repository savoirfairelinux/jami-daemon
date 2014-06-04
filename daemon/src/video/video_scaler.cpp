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
#include "video_scaler.h"
#include "logger.h"

namespace sfl_video {

VideoScaler::VideoScaler() : ctx_(0), mode_(SWS_FAST_BILINEAR) {}

VideoScaler::~VideoScaler() { sws_freeContext(ctx_); }

void VideoScaler::scale(const VideoFrame &input, VideoFrame &output)
{
    const AVFrame *input_frame = input.get();
    AVFrame *output_frame = output.get();

    ctx_ = sws_getCachedContext(ctx_,
                                input_frame->width,
                                input_frame->height,
                                (AVPixelFormat) input_frame->format,
                                output_frame->width,
                                output_frame->height,
                                (AVPixelFormat) output_frame->format,
                                mode_,
                                NULL, NULL, NULL);
    if (!ctx_) {
        ERROR("Unable to create a scaler context");
        return;
    }

    sws_scale(ctx_, input_frame->data, input_frame->linesize, 0,
              input_frame->height, output_frame->data, output_frame->linesize);
}

/* from ffmpeg/imgconvert.c */
static inline int is_yuv_planar(const AVPixFmtDescriptor *desc)
{
    int i;
    int planes[4] = { 0 };

    if (     desc->flags & PIX_FMT_RGB
             || !(desc->flags & PIX_FMT_PLANAR))
        return 0;

    /* set the used planes */
    for (i = 0; i < desc->nb_components; i++)
        planes[desc->comp[i].plane] = 1;

    /* if there is an unused plane, the format is not planar */
    for (i = 0; i < desc->nb_components; i++)
        if (!planes[i])
            return 0;
    return 1;
}

void VideoScaler::scale_and_pad(const VideoFrame &input, VideoFrame &output,
                                unsigned xoff, unsigned yoff,
                                unsigned dest_width, unsigned dest_height)
{
    const AVFrame *input_frame = input.get();
    AVFrame *output_frame = output.get();

    ctx_ = sws_getCachedContext(ctx_,
                                input_frame->width,
                                input_frame->height,
                                (AVPixelFormat) input_frame->format,
                                dest_width,
                                dest_height,
                                (AVPixelFormat) output_frame->format,
                                mode_,
                                NULL, NULL, NULL);
    if (!ctx_) {
        ERROR("Unable to create a scaler context");
        return;
    }

    // Make an offset'ed copy of output data from xoff and yoff
    // WARN: no buffer overflow checks, must be done by the caller
    const AVPixFmtDescriptor *out_desc = av_pix_fmt_desc_get((AVPixelFormat)output_frame->format);
    unsigned nb_comp = out_desc->nb_components;
    if (nb_comp != 1 and nb_comp != 3) {
        ERROR("unsupported number of components: %u", nb_comp);
        return;
    }

    if (is_yuv_planar(out_desc)) {
        unsigned x_shift = out_desc->log2_chroma_w;
        unsigned y_shift = out_desc->log2_chroma_h;

        tmp_data_[0] = output_frame->data[0] + yoff * output_frame->linesize[0] + xoff;
        tmp_data_[1] = output_frame->data[1] + (yoff >> y_shift) * output_frame->linesize[1] + (xoff >> x_shift);
        tmp_data_[2] = output_frame->data[2] + (yoff >> y_shift) * output_frame->linesize[2] + (xoff >> x_shift);
    } else {
        tmp_data_[0] = output_frame->data[0] + yoff * output_frame->linesize[0] + xoff;
        tmp_data_[1] = tmp_data_[2] = nullptr;
    }

    sws_scale(ctx_, input_frame->data, input_frame->linesize, 0,
              input_frame->height, tmp_data_, output_frame->linesize);
}

void VideoScaler::reset()
{
    if (ctx_) {
        sws_freeContext(ctx_);
        ctx_ = nullptr;
    }
}

}
