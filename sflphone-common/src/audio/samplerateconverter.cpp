/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

SamplerateConverter::SamplerateConverter (void)
        : _frequence (Manager::instance().getConfigInt (AUDIO , AUDIO_SAMPLE_RATE)) //44100
        , _framesize (Manager::instance().getConfigInt (AUDIO , ALSA_FRAME_SIZE))
        , _floatBufferDownMic (NULL)
        , _floatBufferUpMic (NULL)
        , _src_state_mic (NULL)
        , _floatBufferDownSpkr (NULL)
        , _floatBufferUpSpkr (NULL)
        , _src_state_spkr (NULL)
        , _src_err (0)
{
    init();
}

SamplerateConverter::SamplerateConverter (int freq , int fs)
        : _frequence (freq)
        , _framesize (fs)
        , _floatBufferDownMic (NULL)
        , _floatBufferUpMic (NULL)
        , _src_state_mic (NULL)
        , _floatBufferDownSpkr (NULL)
        , _floatBufferUpSpkr (NULL)
        , _src_state_spkr (NULL)
        , _src_err (0)
{
    init();
}

SamplerateConverter::~SamplerateConverter (void)
{

    delete [] _floatBufferUpMic;
    _floatBufferUpMic = NULL;
    delete [] _floatBufferDownMic;
    _floatBufferDownMic = NULL;

    delete [] _floatBufferUpSpkr;
    _floatBufferUpSpkr = NULL;
    delete [] _floatBufferDownSpkr;
    _floatBufferDownSpkr = NULL;

    // libSamplerateConverter-related
    _src_state_mic  = src_delete (_src_state_mic);
    _src_state_spkr = src_delete (_src_state_spkr);
}

void SamplerateConverter::init (void)
{

    // libSamplerateConverter-related
    // Set the converter type for the upsampling and the downsampling
    // interpolator SRC_SINC_BEST_QUALITY
    // interpolator SRC_SINC_FASTEST
    // interpolator SRC_LINEAR
    _src_state_mic  = src_new (SRC_LINEAR, 1, &_src_err);
    _src_state_spkr = src_new (SRC_LINEAR, 1, &_src_err);

    int nbSamplesMax = (int) (getFrequence() * getFramesize() / 1000);

    _floatBufferDownMic  = new float32[nbSamplesMax];
    _floatBufferUpMic = new float32[nbSamplesMax];
    _floatBufferDownSpkr  = new float32[nbSamplesMax];
    _floatBufferUpSpkr = new float32[nbSamplesMax];
}

void
SamplerateConverter::Short2FloatArray (const short *in, float *out, int len)
{
    // factor is 1/(2^15), used to rescale the short int range to the
    // [-1.0 - 1.0] float range.
#define S2F_FACTOR .000030517578125f;

    while (len) {
        len--;
        out[len] = (float) in[len] * S2F_FACTOR;
    }
}


//TODO Add ifdef for int16 or float32 type
int SamplerateConverter::upsampleData (SFLDataFormat* dataIn , SFLDataFormat* dataOut, int samplerate1 , int samplerate2 , int nbSamples)
{

    double upsampleFactor = (double) samplerate2 / samplerate1 ;

    int nbSamplesMax = (int) (samplerate2 * getFramesize() / 1000);

    if (upsampleFactor != 1 && dataIn != NULL) {
        SRC_DATA src_data;
        src_data.data_in = _floatBufferDownSpkr;
        src_data.data_out = _floatBufferUpSpkr;
        src_data.input_frames = nbSamples;
        src_data.output_frames = (int) floor (upsampleFactor * nbSamples);
        src_data.src_ratio = upsampleFactor;
        src_data.end_of_input = 0; // More data will come
        // _debug("    upsample %d %d %f %d" , src_data.input_frames , src_data.output_frames, src_data.src_ratio , nbSamples);
        // Override libsamplerate conversion function
        Short2FloatArray (dataIn , _floatBufferDownSpkr, nbSamples);
        //src_short_to_float_array (dataIn , _floatBufferDownSpkr, nbSamples);
        //_debug("upsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
        src_process (_src_state_spkr, &src_data);
        // _debug("    upsample %d %d %d" , samplerate1, samplerate2 , nbSamples);
        nbSamples  = (src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;
        src_float_to_short_array (_floatBufferUpSpkr, dataOut, nbSamples);
        //_debug("upsample %d %d %d" , samplerate1, samplerate2 , nbSamples);
    }

    return nbSamples;
}

//TODO Add ifdef for int16 or float32 type
int SamplerateConverter::downsampleData (SFLDataFormat* dataIn , SFLDataFormat* dataOut , int samplerate1 , int samplerate2 , int nbSamples)
{

    double downsampleFactor = (double) samplerate1 / samplerate2;

    int nbSamplesMax = (int) (samplerate1 * getFramesize() / 1000);

    if (downsampleFactor != 1) {
        SRC_DATA src_data;
        src_data.data_in = _floatBufferUpMic;
        src_data.data_out = _floatBufferDownMic;
        src_data.input_frames = nbSamples;
        src_data.output_frames = (int) floor (downsampleFactor * nbSamples);
        src_data.src_ratio = downsampleFactor;
        src_data.end_of_input = 0; // More data will come
        //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
        // Override libsamplerate conversion function
        Short2FloatArray (dataIn , _floatBufferUpMic, nbSamples);
        //src_short_to_float_array (dataIn, _floatBufferUpMic, nbSamples);
        //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
        src_process (_src_state_mic, &src_data);
        //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
        nbSamples  = (src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;
        //_debug("downsample %d %f %d" ,  src_data.output_frames, src_data.src_ratio , nbSamples);
        src_float_to_short_array (_floatBufferDownMic , dataOut , nbSamples);
    }

    return nbSamples;
}
