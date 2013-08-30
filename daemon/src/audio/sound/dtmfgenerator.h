/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 * Author: Yan Morin <yan.morin@savoirfairelinux.com>
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 * Portions (c) 2003 iptel.org
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
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
            SFLAudioSample* sample;         /** Currently generated code */
        };

        /** State of the DTMF generator */
        DTMFState state;

        /** The different kind of tones */
        static const DTMFTone tones_[NUM_TONES];

        /** Generated samples for each tone */
        SFLAudioSample* toneBuffers_[NUM_TONES];

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
         * @param buffer a SFLAudioSample vector
         * @param code   dtmf code to get sound
         */
        void getSamples(std::vector<SFLAudioSample> &buffer, unsigned char code);

        /*
         * Get next n samples (continues where previous call to
         * genSample or genNextSamples stopped
         * @param buffer a SFLAudioSample vector
         */
        void getNextSamples(std::vector<SFLAudioSample> &buffer);

    private:

        /**
         * Fill tone buffer for a given index of the array of tones.
         * @param index of the tone in the array tones_
         * @return SFLAudioSample* The generated data
         */
        SFLAudioSample* fillToneBuffer(int index);
};

#endif // DTMFGENERATOR_H
