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

#ifndef _SAMPLE_RATE_H
#define _SAMPLE_RATE_H

#include <samplerate.h>
#include <cmath>
#include <cstring>

#include "audiobuffer.h"
#include "sfl_types.h"
#include "noncopyable.h"

class Resampler {
    public:
        /**
         * Resampler is used for several situations:
        * streaming conversion (RTP, IAX), audiolayer conversion,
        * audio files conversion. Parameters are used to compute
        * internal buffer size. Resampler must be reinitialized
        * every time these parameters change
        */
        Resampler(AudioFormat outFormat);
        Resampler(unsigned sample_rate, unsigned channels=1);

        /** Destructor */
        ~Resampler();

        /**
         * Change the converter sample rate and channel number.
         * Internal state is lost.
         */
        void setFormat(AudioFormat format);

        /**
         * resample from the samplerate1 to the samplerate2
         * @param dataIn  Input buffer
         * @param dataOut Output buffer
         * @param nbSamples	  The number of samples to process
         */
        void resample(const AudioBuffer& dataIn, AudioBuffer& dataOut);

        /**
         * Convert short table to floats for audio processing
         * @param in the input (short) array
         * @param out The resulting (float) array
         * @param len The number of elements in both tables
         */
        void Short2FloatArray(const SFLAudioSample *in, float *out, int len);

    private:
        NON_COPYABLE(Resampler);

        /* temporary buffers */
        std::vector<float> floatBufferIn_;
        std::vector<float> floatBufferOut_;
        std::vector<SFLAudioSample> scratchBuffer_;

        size_t samples_; // size in samples of temporary buffers
        AudioFormat format_; // number of channels and max output frequency

        SRC_STATE* src_state_;
};

#endif //_SAMPLE_RATE_H
