/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include "resampler.h"
#include "logger.h"
#include "sfl_types.h"

#include <samplerate.h>

namespace sfl {

class SrcState {
    public:
        SrcState(int nb_channels)
        {
            int err;
            state_ = src_new(SRC_LINEAR, nb_channels, &err);
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

Resampler::Resampler(AudioFormat format) : floatBufferIn_(),
    floatBufferOut_(), scratchBuffer_(), samples_(0), format_(format), src_state_()
{
    setFormat(format);
}

Resampler::Resampler(unsigned sample_rate, unsigned channels) : floatBufferIn_(),
    floatBufferOut_(), scratchBuffer_(), samples_(0), format_(sample_rate, channels), src_state_()
{
    setFormat(format_);
}

Resampler::~Resampler() = default;

void
Resampler::setFormat(AudioFormat format)
{
    format_ = format;
    samples_ = (format.nb_channels * format.sample_rate * 20) / 1000; // start with 20 ms buffers
    floatBufferIn_.resize(samples_);
    floatBufferOut_.resize(samples_);
    scratchBuffer_.resize(samples_);

    src_state_.reset(new SrcState(format.nb_channels));
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
        src_state_.reset(new SrcState(nbChans));
        format_.nb_channels = nbChans;
        DEBUG("SRC channel number changed.");
    }
    if (nbChans != dataOut.channels()) {
        DEBUG("Output buffer had the wrong number of channels (in: %d, out: %d).", nbChans, dataOut.channels());
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

}
