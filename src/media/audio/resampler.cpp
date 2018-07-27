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

#include "logger.h"
#include "media_filter.h"
#include "resampler.h"
#include "ring_types.h"

namespace ring {

Resampler::Resampler(AudioFormat format, bool quality)
    : format_(format)
{
    setFormat(format, quality);
}

Resampler::Resampler(unsigned sample_rate, unsigned channels, bool quality)
    : format_(sample_rate, channels)
{
    setFormat(format_, quality);
}

Resampler::~Resampler() = default;

void
Resampler::reinitFilter(const MediaStream& inputParams)
{
    filter_.reset(new MediaFilter());
    std::stringstream aformat;
    aformat << "aformat=sample_fmts=s16:channel_layouts="
        << av_get_default_channel_layout(format_.nb_channels)
        << ":sample_rates=" << format_.sample_rate;
    if (filter_->initialize(aformat.str(), inputParams) < 0) {
        RING_ERR() << "Failed to initialize resampler";
        filter_.reset();
    }
}

void
Resampler::setFormat(AudioFormat format, bool /* quality */)
{
    format_ = format;
    if (filter_)
        reinitFilter(filter_->getInputParams());
}

void
Resampler::resample(const AudioBuffer& dataIn, AudioBuffer& dataOut)
{
    auto input = dataIn.toAVFrame();
    MediaStream currentParams("resampler", static_cast<AVSampleFormat>(input->format),
        0, input->sample_rate, input->channels);
    if (filter_) {
        const auto& ms = filter_->getInputParams();
        if (ms.sampleRate != input->sample_rate || ms.nbChannels != input->channels) {
            RING_WARN() << "Resampler settings changed, reinitializing";
            reinitFilter(currentParams);
        }
    } else {
        reinitFilter(currentParams);
    }

    auto frame = filter_->apply(input);
    av_frame_free(&input);
    if (!frame) {
        RING_ERR() << "Resampling failed, this may produce a glitch in the audio";
        return;
    }

    dataOut.setFormat(format_);
    dataOut.resize(frame->nb_samples);
    if (static_cast<AVSampleFormat>(frame->format) == AV_SAMPLE_FMT_FLTP)
        dataOut.convertFloatPlanarToSigned16(frame->extended_data,
            frame->nb_samples, frame->channels);
    else if (static_cast<AVSampleFormat>(frame->format) == AV_SAMPLE_FMT_S16)
        dataOut.deinterleave(reinterpret_cast<const AudioSample*>(frame->extended_data[0]),
            frame->nb_samples, frame->channels);
    av_frame_free(&frame);
}

} // namespace ring
