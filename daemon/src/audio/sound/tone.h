/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
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
#ifndef __TONE_H__
#define __TONE_H__

#include <string>
#include "audio/audioloop.h"

/**
 * @file tone.h
 * @brief Tone sample (dial, busy, ring, congestion)
 */
class Tone : public AudioLoop {
    public:
        /**
         * Constructor
         * @param definition String that contain frequency/time of the tone
         * @param sampleRate SampleRating of audio tone
         */
        Tone(const std::string& definition, unsigned int sampleRate);

        /** The different kind of tones */
        enum TONEID {
            TONE_DIALTONE = 0,
            TONE_BUSY,
            TONE_RINGTONE,
            TONE_CONGESTION,
            TONE_NULL
        };

        /**
         * Add a simple or double sin to the buffer, it double the sin in stereo
         * @param buffer  The data
         * @param frequency1 The first frequency
         * @param frequency2	The second frequency
         * @param nb are the number of int16 (mono) to generate
         * by example nb=5 generate 10 int16, 5 for the left, 5 for the right
         */
        void genSin(SFLAudioSample* buffer, int frequency1, int frequency2, int nb);

        /**
         *
         */
        void fillWavetable();

        double interpolate(double x) const;

    private:

        /**
         * allocate the memory with the definition
         * @param definition String that contain frequency/time of the tone.
         */
        void genBuffer(const std::string& definition);

        static const int TABLE_LENGTH = 4096;
        double wavetable_[TABLE_LENGTH];

        double xhigher_;
        double xlower_;
};

#endif // __TONE_H__

