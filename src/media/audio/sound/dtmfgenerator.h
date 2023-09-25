/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#pragma once

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

class DTMFException : public std::runtime_error
{
public:
    DTMFException(const std::string& str)
        : std::runtime_error(str) {};
};

/*
 * @file dtmfgenerator.h
 * @brief DTMF Tone Generator
 */
class DTMFGenerator
{
private:
    /** Struct to handle a DTMF */
    struct DTMFTone
    {
        unsigned char code; /** Code of the tone */
        unsigned lower;          /** Lower frequency */
        unsigned higher;         /** Higher frequency */
    };

    /** State of the DTMF generator */
    struct DTMFState
    {
        unsigned int offset; /** Offset in the sample currently being played */
        AVFrame* sample; /** Currently generated code */
    };

    /** State of the DTMF generator */
    DTMFState state;

    /** The different kind of tones */
    static const DTMFTone tones_[NUM_TONES];

    /** Generated samples for each tone */
    std::array<libjami::FrameBuffer, NUM_TONES> toneBuffers_;

    /** Sampling rate of generated dtmf */
    unsigned sampleRate_;

    /** A tone object */
    Tone tone_;

public:
    /**
     * DTMF Generator contains frequency of each keys
     * and can build one DTMF.
     * @param sampleRate frequency of the sample (ex: 8000 hz)
     */
    DTMFGenerator(unsigned int sampleRate, AVSampleFormat sampleFormat);

    ~DTMFGenerator();

    NON_COPYABLE(DTMFGenerator);

    /*
     * Get n samples of the signal of code code
     * @param frame  the output AVFrame to fill
     * @param code   dtmf code to get sound
     */
    void getSamples(AVFrame* frame, unsigned char code);

    /*
     * Get next n samples (continues where previous call to
     * genSample or genNextSamples stopped
     * @param buffer a AudioSample vector
     */
    void getNextSamples(AVFrame* frame);

private:
    /**
     * Fill tone buffer for a given index of the array of tones.
     * @param index of the tone in the array tones_
     * @return AudioSample* The generated data
     */
    libjami::FrameBuffer fillToneBuffer(int index);
};

} // namespace jami
