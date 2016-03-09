/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include "resampler.h"
#include "logger.h"
#include "ring_types.h"

#include <samplerate.h>

namespace ring {

class SrcState {
    public:
        SrcState(int nb_channels, bool high_quality = false)
        {
            int err;
            state_ = src_new(high_quality ? SRC_SINC_BEST_QUALITY : SRC_LINEAR, nb_channels, &err);
        }

        ~SrcState()
        {
            src_delete(state_);
        }

        void process(SRC_DATA *src_data)
        {
            src_process(state_, src_data);
        }

    private:
        SRC_STATE *state_ {nullptr};
};

Resampler::Resampler(AudioFormat format, bool quality) : floatBufferIn_(),
    floatBufferOut_(), scratchBuffer_(), samples_(0), format_(format), high_quality_(quality), src_state_()
{
    setFormat(format, quality);
}

Resampler::Resampler(unsigned sample_rate, unsigned channels, bool quality) : floatBufferIn_(),
    floatBufferOut_(), scratchBuffer_(), samples_(0), format_(sample_rate, channels), high_quality_(quality), src_state_()
{
    setFormat(format_, quality);
}

Resampler::~Resampler() = default;

void
Resampler::setFormat(AudioFormat format, bool quality)
{
    format_ = format;
    samples_ = (format.nb_channels * format.sample_rate * 20) / 1000; // start with 20 ms buffers
    floatBufferIn_.resize(samples_);
    floatBufferOut_.resize(samples_);
    scratchBuffer_.resize(samples_);

    src_state_.reset(new SrcState(format.nb_channels, quality));
}

void Resampler::resample(const AudioBuffer &dataIn, AudioBuffer &dataOut)
{
    const double inputFreq = dataIn.getSampleRate();
    const double outputFreq = dataOut.getSampleRate();
    const double sampleFactor = outputFreq / inputFreq;

    if (sampleFactor == 1.0)
        return;

    const size_t nbFrames = dataIn.frames();
    const size_t nbChans = dataIn.channels();

    if (nbChans != format_.nb_channels) {
        // change channel num if needed
        src_state_.reset(new SrcState(nbChans, high_quality_));
        format_.nb_channels = nbChans;
        RING_DBG("SRC channel number changed.");
    }
    if (nbChans != dataOut.channels()) {
        RING_DBG("Output buffer had the wrong number of channels (in: %zu, out: %u).", nbChans, dataOut.channels());
        dataOut.setChannelNum(nbChans);
    }

    size_t inSamples = nbChans * nbFrames;
    size_t outSamples = inSamples * sampleFactor;

    // grow buffer if needed
    floatBufferIn_.resize(inSamples);
    floatBufferOut_.resize(outSamples);
    scratchBuffer_.resize(outSamples);

    SRC_DATA src_data;
    src_data.data_in = floatBufferIn_.data();
    src_data.data_out = floatBufferOut_.data();
    src_data.input_frames = nbFrames;
    src_data.output_frames = nbFrames * sampleFactor;
    src_data.src_ratio = sampleFactor;
    src_data.end_of_input = 0; // More data will come

    dataIn.interleaveFloat(floatBufferIn_.data());

    src_state_->process(&src_data);
    /*
    TODO: one-shot deinterleave and float-to-short conversion
    */
    src_float_to_short_array(floatBufferOut_.data(), scratchBuffer_.data(), outSamples);
    dataOut.deinterleave(scratchBuffer_.data(), src_data.output_frames, nbChans);
}

} // namespace ring
