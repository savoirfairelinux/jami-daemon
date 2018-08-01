/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "libav_deps.h"
#include "logger.h"
#include "media_buffer.h"
#include "media_filter.h"
#include "media_stream.h"
#include "resampler.h"
#include "ring_types.h"

extern "C" {
#include <libswresample/swresample.h>
}

namespace ring {

Resampler::Resampler()
    : swrCtx_(swr_alloc())
{}

Resampler::~Resampler()
{
    swr_free(&swrCtx_);
}

void
Resampler::reinit(const AudioFormat& in, const int inSampleFmt,
                  const AudioFormat& out, const int outSampleFmt)
{
    av_opt_set_int(swrCtx_, "ich", 0, 0);
    av_opt_set_int(swrCtx_, "icl", av_get_default_channel_layout(in.nb_channels), 0);
    av_opt_set_int(swrCtx_, "isr", in.sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx_, "isf", static_cast<AVSampleFormat>(inSampleFmt), 0);

    av_opt_set_int(swrCtx_, "och", 0, 0);
    av_opt_set_int(swrCtx_, "ocl", av_get_default_channel_layout(out.nb_channels), 0);
    av_opt_set_int(swrCtx_, "osr", out.sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx_, "osf", static_cast<AVSampleFormat>(outSampleFmt), 0);

    swr_init(swrCtx_);
}

int
Resampler::resample(const AVFrame* input, AVFrame* output)
{
    int ret = swr_convert_frame(swrCtx_, output, input);
    if (ret & AVERROR_INPUT_CHANGED || ret & AVERROR_OUTPUT_CHANGED) {
        reinit(AudioFormat{(unsigned)input->sample_rate, (unsigned)input->channels}, input->format,
               AudioFormat{(unsigned)output->sample_rate, (unsigned)output->channels}, output->format);
        return resample(input, output);
    } else if (ret < 0) {
        RING_ERR() << "Failed to resample frame";
        return -1;
    }

    return 0;
}

void
Resampler::resample(const AudioBuffer& dataIn, AudioBuffer& dataOut)
{
    auto input = dataIn.toAVFrame();
    AudioFrame resampled;
    auto output = resampled.pointer();
    output->sample_rate = dataOut.getSampleRate();
    output->channel_layout = av_get_default_channel_layout(dataOut.channels());
    output->format = AV_SAMPLE_FMT_S16;

    if (resample(input, output) < 0) {
        av_frame_free(&input);
        return;
    }

    dataOut.resize(output->nb_samples);
    dataOut.deinterleave(reinterpret_cast<const AudioSample*>(output->extended_data[0]),
        output->nb_samples, output->channels);
    av_frame_free(&input);
}

} // namespace ring
