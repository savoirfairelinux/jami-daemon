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

#ifndef _SAMPLE_RATE_H
#define _SAMPLE_RATE_H

#include <cmath>
#include <cstring>
#include <memory>

#include "audiobuffer.h"
#include "ring_types.h"
#include "noncopyable.h"

namespace ring {

struct SrcState;

class Resampler {
    public:
        /**
         * Resampler is used for several situations:
        * streaming conversion (RTP, IAX), audiolayer conversion,
        * audio files conversion. Parameters are used to compute
        * internal buffer size. Resampler must be reinitialized
        * every time these parameters change
        */
        Resampler(AudioFormat outFormat, bool quality = false);
        Resampler(unsigned sample_rate, unsigned channels=1, bool quality = false);
        // empty dtor, needed for unique_ptr
        ~Resampler();

        /**
         * Change the converter sample rate and channel number.
         * Internal state is lost.
         */
        void setFormat(AudioFormat format, bool quality = false);

        /**
         * resample from the samplerate1 to the samplerate2
         * @param dataIn  Input buffer
         * @param dataOut Output buffer
         * @param nbSamples	  The number of samples to process
         */
        void resample(const AudioBuffer& dataIn, AudioBuffer& dataOut);

    private:
        NON_COPYABLE(Resampler);

        /* temporary buffers */
        std::vector<float> floatBufferIn_;
        std::vector<float> floatBufferOut_;
        std::vector<AudioSample> scratchBuffer_;

        size_t samples_; // size in samples of temporary buffers
        AudioFormat format_; // number of channels and max output frequency
        bool high_quality_;

        std::unique_ptr<SrcState> src_state_;
};

} // namespace ring

#endif //_SAMPLE_RATE_H
