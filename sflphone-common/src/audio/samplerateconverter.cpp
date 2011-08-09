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

SamplerateConverter::SamplerateConverter (int freq , int fs) :
    _floatBufferDownMic (NULL)
    , _floatBufferUpMic (NULL)
    , _src_state_mic (NULL)
    , _floatBufferDownSpkr (NULL)
    , _floatBufferUpSpkr (NULL)
    , _src_state_spkr (NULL)
{
    int err;
    _src_state_mic  = src_new (SRC_LINEAR, 1, &err);
    _src_state_spkr = src_new (SRC_LINEAR, 1, &err);

    int nbSamplesMax = (int) ( (freq * fs) / 1000);

    _floatBufferDownMic  = new float32[nbSamplesMax];
    _floatBufferUpMic = new float32[nbSamplesMax];
    _floatBufferDownSpkr  = new float32[nbSamplesMax];
    _floatBufferUpSpkr = new float32[nbSamplesMax];
}

SamplerateConverter::~SamplerateConverter (void)
{
	delete [] _floatBufferUpMic;
	delete [] _floatBufferDownMic;
	delete [] _floatBufferUpSpkr;
	delete [] _floatBufferDownSpkr;

	src_delete (_src_state_mic);
	src_delete (_src_state_spkr);
}

void
SamplerateConverter::Short2FloatArray (const short *in, float *out, int len)
{
    // factor is 1/(2^15), used to rescale the short int range to the
    // [-1.0 - 1.0] float range.

    while (len--)
        out[len] = (float) in[len] * .000030517578125f;
}


//TODO Add ifdef for int16 or float32 type
void SamplerateConverter::upsampleData (SFLDataFormat* dataIn , SFLDataFormat* dataOut, int samplerate1 , int samplerate2 , int nbSamples)
{
    double upsampleFactor = (double) samplerate2 / samplerate1 ;

    if (upsampleFactor == 1)
    	return;

    SRC_DATA src_data;
	src_data.data_in = _floatBufferDownSpkr;
	src_data.data_out = _floatBufferUpSpkr;
	src_data.input_frames = nbSamples;
	src_data.output_frames = nbSamples;
	src_data.src_ratio = upsampleFactor;
	src_data.end_of_input = 0; // More data will come

	Short2FloatArray (dataIn , _floatBufferDownSpkr, nbSamples);
	src_process (_src_state_spkr, &src_data);

	assert(nbSamples == src_data.output_frames_gen);
	src_float_to_short_array (_floatBufferUpSpkr, dataOut, nbSamples);
}

//TODO Add ifdef for int16 or float32 type
void SamplerateConverter::downsampleData (SFLDataFormat* dataIn , SFLDataFormat* dataOut , int samplerate1 , int samplerate2 , int nbSamples)
{
    double downsampleFactor = (double) samplerate1 / samplerate2;

    if (downsampleFactor == 1)
		return;

	SRC_DATA src_data;
	src_data.data_in = _floatBufferUpMic;
	src_data.data_out = _floatBufferDownMic;
	src_data.input_frames = nbSamples;
	src_data.output_frames = nbSamples;
	src_data.src_ratio = downsampleFactor;
	src_data.end_of_input = 0; // More data will come

	Short2FloatArray (dataIn , _floatBufferUpMic, nbSamples);
	src_process (_src_state_mic, &src_data);

	assert(nbSamples == src_data.output_frames_gen);

	src_float_to_short_array (_floatBufferDownMic , dataOut , nbSamples);
}
