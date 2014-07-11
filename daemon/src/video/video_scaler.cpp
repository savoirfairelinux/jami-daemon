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

#include <cassert>

namespace sfl_video {

VideoScaler::VideoScaler() : ctx_(0), mode_(SWS_FAST_BILINEAR), tmp_data_() {}

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
              input_frame->height, output_frame->data,
              output_frame->linesize);
}

void VideoScaler::scale_with_aspect(const VideoFrame &input, VideoFrame &output)
{
    AVFrame *output_frame = output.get();
    scale_and_pad(input, output, 0, 0, output_frame->width,
                  output_frame->height, true);
}

static inline bool is_yuv_planar(const AVPixFmtDescriptor *desc)
{
    unsigned used_bit_mask = (1u << desc->nb_components) - 1;

    if (not (desc->flags & PIX_FMT_PLANAR)
        or desc->flags & PIX_FMT_RGB)
        return false;

    /* handle formats that do not use all planes */
    for (unsigned i = 0; i < desc->nb_components; ++i)
        used_bit_mask &= ~(1u << desc->comp[i].plane);

    return not used_bit_mask;
}

void VideoScaler::scale_and_pad(const VideoFrame &input, VideoFrame &output,
                                unsigned xoff, unsigned yoff,
                                unsigned dest_width, unsigned dest_height,
                                bool keep_aspect)
{
    const AVFrame *input_frame = input.get();
    AVFrame *output_frame = output.get();

    /* Correct destination width/height and offset if we need to keep input
     * frame aspect.
     */
    if (keep_aspect) {
        const float local_ratio = (float)dest_width / dest_height;
        const float input_ratio = (float)input_frame->width / input_frame->height;

        if (local_ratio > input_ratio) {
            auto old_dest_width = dest_width;
            dest_width = dest_height * input_ratio;
            xoff += (old_dest_width - dest_width) / 2;
        } else {
            auto old_dest_heigth = dest_height;
            dest_height = dest_width / input_ratio;
            yoff += (old_dest_heigth - dest_height) / 2;
        }
    }

    // buffer overflow checks
    assert(xoff + dest_width <= (unsigned)output_frame->width);
    assert(yoff + dest_height <= (unsigned)output_frame->height);

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
    const AVPixFmtDescriptor *out_desc = av_pix_fmt_desc_get((AVPixelFormat)output_frame->format);
    if (is_yuv_planar(out_desc)) {
        unsigned x_shift = out_desc->log2_chroma_w;
        unsigned y_shift = out_desc->log2_chroma_h;

        tmp_data_[0] = output_frame->data[0] + yoff * output_frame->linesize[0] + xoff;
        tmp_data_[1] = output_frame->data[1] + (yoff >> y_shift) * output_frame->linesize[1] + (xoff >> x_shift);
        tmp_data_[2] = output_frame->data[2] + (yoff >> y_shift) * output_frame->linesize[2] + (xoff >> x_shift);
        tmp_data_[3] = nullptr;
    } else {
        memcpy(tmp_data_, output_frame->data, sizeof(tmp_data_));
        tmp_data_[0] += yoff * output_frame->linesize[0] + xoff;
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
