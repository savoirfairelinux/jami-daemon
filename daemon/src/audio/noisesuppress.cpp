/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include <cassert>
#include "noisesuppress.h"

NoiseSuppress::NoiseSuppress(int smplPerFrame, int samplingRate) :
    smplPerFrame_(smplPerFrame),
    noiseState_(speex_preprocess_state_init(smplPerFrame_, samplingRate))
{
    int i = 1;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_DENOISE, &i);
    i = -20;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &i);
    i = 0;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_AGC, &i);
    i = 8000;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_AGC_TARGET, &i);
    i = 16000;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_AGC_LEVEL, &i);
    i = 0;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_DEREVERB, &i);
    float f = 0.0;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
    f = 0.0;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);
    i = 0;
    speex_preprocess_ctl(noiseState_, SPEEX_PREPROCESS_SET_VAD, &i);
}


NoiseSuppress::~NoiseSuppress()
{
    speex_preprocess_state_destroy(noiseState_);
}

void NoiseSuppress::process(SFLDataFormat *data, int samples)
{
    assert(smplPerFrame_ == samples);
    speex_preprocess_run(noiseState_, data);
}
