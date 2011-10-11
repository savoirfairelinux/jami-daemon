/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "samplerateconverter.h"
#include "manager.h"
#include <cassert>

SamplerateConverter::SamplerateConverter(int freq) : _maxFreq(freq)
{
    int err;
    _src_state = src_new(SRC_LINEAR, 1, &err);

    _samples = (freq * 20) / 1000; // start with 20 ms buffers

    _floatBufferIn = new float[_samples];
    _floatBufferOut = new float[_samples];
}

SamplerateConverter::~SamplerateConverter(void)
{
    delete [] _floatBufferIn;
    delete [] _floatBufferOut;

    src_delete(_src_state);
}

void
SamplerateConverter::Short2FloatArray(const short *in, float *out, int len)
{
    // factor is 1/(2^15), used to rescale the short int range to the
    // [-1.0 - 1.0] float range.

    while (len--)
        out[len] = (float) in[len] * .000030517578125f;
}

void SamplerateConverter::resample(SFLDataFormat* dataIn , SFLDataFormat* dataOut , int inputFreq , int outputFreq , int nbSamples)
{
    double sampleFactor = (double) outputFreq / inputFreq;

    if (sampleFactor == 1.0)
        return;

    unsigned int outSamples = nbSamples * sampleFactor;
    unsigned int maxSamples = outSamples;

    if (maxSamples < (unsigned int)nbSamples)
        maxSamples = nbSamples;

    if (maxSamples > _samples) {
        /* grow buffer if needed */
        _samples = maxSamples;
        delete [] _floatBufferIn;
        delete [] _floatBufferOut;
        _floatBufferIn = new float[_samples];
        _floatBufferOut = new float[_samples];
    }

    SRC_DATA src_data;
    src_data.data_in = _floatBufferIn;
    src_data.data_out = _floatBufferOut;
    src_data.input_frames = nbSamples;
    src_data.output_frames = outSamples;
    src_data.src_ratio = sampleFactor;
    src_data.end_of_input = 0; // More data will come

    Short2FloatArray(dataIn , _floatBufferIn, nbSamples);
    src_process(_src_state, &src_data);

    src_float_to_short_array(_floatBufferOut, dataOut , outSamples);
}
