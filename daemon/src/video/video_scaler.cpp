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

void VideoScaler::scale_and_pad(const VideoFrame &input, VideoFrame &output,
                                unsigned xoff, unsigned yoff,
                                unsigned dest_width, unsigned dest_height)
{
    const AVFrame *input_frame = input.get();
    AVFrame *output_frame = output.get();
    uint8_t *data[AV_NUM_DATA_POINTERS] = {nullptr};

    for (unsigned i = 0; i < AV_NUM_DATA_POINTERS and output_frame->data[i]; i++) {
        const unsigned divisor = i == 0 ? 1 : 2;
        unsigned offset = (yoff * output_frame->linesize[i] + xoff) / divisor;
        data[i] = output_frame->data[i] + offset;
    }

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

    sws_scale(ctx_, input_frame->data, input_frame->linesize, 0,
              input_frame->height, data, output_frame->linesize);
}

void VideoScaler::reset()
{
    if (ctx_) {
        sws_freeContext(ctx_);
        ctx_ = nullptr;
    }
}

}
