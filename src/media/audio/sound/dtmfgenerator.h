/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  Portions (c) 2003 iptel.org
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

#ifndef DTMFGENERATOR_H
#define DTMFGENERATOR_H

#include <stdexcept>
#include <string>
#include <vector>
#include "noncopyable.h"
#include "tone.h"

#define NUM_TONES 16

/*
 * @file dtmfgenerator.h
 * @brief DMTF Generator Exception
 */

namespace jami {

class DTMFException : public std::runtime_error {
    public:
        DTMFException(const std::string& str) : std::runtime_error(str) {};
};

/*
 * @file dtmfgenerator.h
 * @brief DTMF Tone Generator
 */
class DTMFGenerator {
    private:
        /** Struct to handle a DTMF */
        struct DTMFTone {
            unsigned char code; /** Code of the tone */
            int lower;          /** Lower frequency */
            int higher;         /** Higher frequency */
        };

        /** State of the DTMF generator */
        struct DTMFState {
            unsigned int offset;   /** Offset in the sample currently being played */
            AudioSample* sample;         /** Currently generated code */
        };

        /** State of the DTMF generator */
        DTMFState state;

        /** The different kind of tones */
        static const DTMFTone tones_[NUM_TONES];

        /** Generated samples for each tone */
        AudioSample* toneBuffers_[NUM_TONES];

        /** Sampling rate of generated dtmf */
        int sampleRate_;

        /** A tone object */
        Tone tone_;

    public:
        /**
         * DTMF Generator contains frequency of each keys
         * and can build one DTMF.
         * @param sampleRate frequency of the sample (ex: 8000 hz)
         */
        DTMFGenerator(unsigned int sampleRate);

        ~DTMFGenerator();

        NON_COPYABLE(DTMFGenerator);

        /*
         * Get n samples of the signal of code code
         * @param buffer a AudioSample vector
         * @param code   dtmf code to get sound
         */
        void getSamples(std::vector<AudioSample> &buffer, unsigned char code);

        /*
         * Get next n samples (continues where previous call to
         * genSample or genNextSamples stopped
         * @param buffer a AudioSample vector
         */
        void getNextSamples(std::vector<AudioSample> &buffer);

    private:

        /**
         * Fill tone buffer for a given index of the array of tones.
         * @param index of the tone in the array tones_
         * @return AudioSample* The generated data
         */
        AudioSample* fillToneBuffer(int index);
};

} // namespace jami

#endif // DTMFGENERATOR_H
