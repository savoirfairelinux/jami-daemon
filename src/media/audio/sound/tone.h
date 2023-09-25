/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */

#pragma once

#include <string>
#include "audio/audioloop.h"

/**
 * @file tone.h
 * @brief Tone sample (dial, busy, ring, congestion)
 */

namespace jami {

class Tone : public AudioLoop
{
public:
    /**
     * Constructor
     * @param definition String that contain frequency/time of the tone
     * @param sampleRate SampleRating of audio tone
     */
    Tone(std::string_view definition, unsigned int sampleRate, AVSampleFormat sampleFormat);

    /** The different kind of tones */
    enum class ToneId { DIALTONE = 0, BUSY, RINGTONE, CONGESTION, TONE_NULL };

    /**
     * Add a simple or double sin to the buffer, it double the sin in stereo
     * @param buffer  The data
     * @param frequency1 The first frequency
     * @param frequency2	The second frequency
     * @param nb the number of samples to generate
     */
    static void genSin(AVFrame* buffer, unsigned outPos, unsigned frequency1, unsigned frequency2);

private:
    /**
     * allocate the memory with the definition
     * @param definition String that contain frequency/time of the tone.
     */
    void genBuffer(std::string_view definition);
};

} // namespace jami
