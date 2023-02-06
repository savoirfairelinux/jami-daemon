/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "video_scaler.h"
#include "media_buffer.h"
#include "logger.h"

#include <cassert>

namespace jami {
namespace video {

VideoScaler::VideoScaler()
    : ctx_(0)
    , mode_(SWS_FAST_BILINEAR)
    , tmp_data_()
{}

VideoScaler::~VideoScaler()
{
    sws_freeContext(ctx_);
}

void
VideoScaler::scale(const VideoFrame& input, VideoFrame& output){
    scale(input.pointer(), output.pointer());
}

void
VideoScaler::scale(const AVFrame* input_frame, AVFrame* output_frame)
{
    ctx_ = sws_getCachedContext(ctx_,
                                input_frame->width,
                                input_frame->height,
                                (AVPixelFormat) input_frame->format,
                                output_frame->width,
                                output_frame->height,
                                (AVPixelFormat) output_frame->format,
                                mode_,
                                NULL,
                                NULL,
                                NULL);
    if (!ctx_) {
        JAMI_ERR("Unable to create a scaler context");
        return;
    }

    sws_scale(ctx_,
              input_frame->data,
              input_frame->linesize,
              0,
              input_frame->height,
              output_frame->data,
              output_frame->linesize);
}

void
VideoScaler::scale_with_aspect(const VideoFrame& input, VideoFrame& output)
{
    if (input.width() == output.width() && input.height() == output.height()) {
        if (input.format() != output.format()) {
            auto outPtr = convertFormat(input, (AVPixelFormat) output.format());
            output.copyFrom(*outPtr);
        } else {
            output.copyFrom(input);
        }
    } else {
        auto output_frame = output.pointer();
        scale_and_pad(input, output, 0, 0, output_frame->width, output_frame->height, true);
    }
}

void
VideoScaler::scale_and_pad(const VideoFrame& input,
                           VideoFrame& output,
                           unsigned xoff,
                           unsigned yoff,
                           unsigned dest_width,
                           unsigned dest_height,
                           bool keep_aspect)
{
    const auto input_frame = input.pointer();
    auto output_frame = output.pointer();

    /* Correct destination width/height and offset if we need to keep input
     * frame aspect.
     */
    if (keep_aspect) {
        const float local_ratio = (float) dest_width / dest_height;
        const float input_ratio = (float) input_frame->width / input_frame->height;

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
    if ((xoff + dest_width > (unsigned) output_frame->width)
        || (yoff + dest_height > (unsigned) output_frame->height)) {
        JAMI_ERR("Unable to scale video");
        return;
    }

    ctx_ = sws_getCachedContext(ctx_,
                                input_frame->width,
                                input_frame->height,
                                (AVPixelFormat) input_frame->format,
                                dest_width,
                                dest_height,
                                (AVPixelFormat) output_frame->format,
                                mode_,
                                NULL,
                                NULL,
                                NULL);
    if (!ctx_) {
        JAMI_ERR("Unable to create a scaler context");
        return;
    }

    // Make an offset'ed copy of output data from xoff and yoff
    const auto out_desc = av_pix_fmt_desc_get((AVPixelFormat) output_frame->format);
    memset(tmp_data_, 0, sizeof(tmp_data_));
    for (int i = 0; i < 4 && output_frame->linesize[i]; i++) {
        signed x_shift = xoff, y_shift = yoff;
        if (i == 1 || i == 2) {
            x_shift = -((-x_shift) >> out_desc->log2_chroma_w);
            y_shift = -((-y_shift) >> out_desc->log2_chroma_h);
        }
        auto x_step = out_desc->comp[i].step;
        tmp_data_[i] = output_frame->data[i] + y_shift * output_frame->linesize[i]
                       + x_shift * x_step;
    }

    sws_scale(ctx_,
              input_frame->data,
              input_frame->linesize,
              0,
              input_frame->height,
              tmp_data_,
              output_frame->linesize);
}

std::unique_ptr<VideoFrame>
VideoScaler::convertFormat(const VideoFrame& input, AVPixelFormat pix)
{
    auto output = std::make_unique<VideoFrame>();
    output->reserve(pix, input.width(), input.height());
    scale(input, *output);
    av_frame_copy_props(output->pointer(), input.pointer());
    return output;
}

void
VideoScaler::reset()
{
    if (ctx_) {
        sws_freeContext(ctx_);
        ctx_ = nullptr;
    }
}

} // namespace video
} // namespace jami
