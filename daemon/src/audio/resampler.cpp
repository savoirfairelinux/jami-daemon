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

Resampler::Resampler(AudioFormat format) : floatBufferIn_(),
    floatBufferOut_(), scratchBuffer_(), samples_(0), format_(format), src_state_(nullptr)
{
    setFormat(format);
}

Resampler::Resampler(unsigned sample_rate, unsigned channels) : floatBufferIn_(),
    floatBufferOut_(), scratchBuffer_(), samples_(0), format_(sample_rate, channels), src_state_(nullptr)
{
    setFormat(format_);
}

Resampler::~Resampler()
{
    src_delete(src_state_);
}

void
Resampler::setFormat(AudioFormat format)
{
    format_ = format;
    samples_ = (format.nb_channels * format.sample_rate * 20) / 1000; // start with 20 ms buffers
    floatBufferIn_.resize(samples_);
    floatBufferOut_.resize(samples_);
    scratchBuffer_.resize(samples_);

    if (src_state_ != nullptr)
        src_delete(src_state_);

    int err;
    src_state_ = src_new(SRC_LINEAR, format.nb_channels, &err);
}

void
Resampler::Short2FloatArray(const SFLAudioSample *in, float *out, int len)
{
    // factor is 1/(2^15), used to rescale the short int range to the
    // [-1.0 - 1.0] float range.
    static const float FACTOR = 1.0f / (1 << 15);

    while (len--)
        out[len] = (float) in[len] * FACTOR;
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
        int err;
        src_delete(src_state_);
        src_state_ = src_new(SRC_LINEAR, nbChans, &err);
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

    src_process(src_state_, &src_data);

    /*
    TODO: one-shot deinterleave and float-to-short conversion
    */
    src_float_to_short_array(floatBufferOut_.data(), scratchBuffer_.data(), outSamples);
    dataOut.deinterleave(scratchBuffer_.data(), src_data.output_frames, nbChans);
}
