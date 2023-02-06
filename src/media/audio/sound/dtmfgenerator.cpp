/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <cmath>
#include <cassert>
#include <ciso646> // fix windows compiler bug

#include "dtmfgenerator.h"

namespace jami {

/*
 * Tone frequencies
 */
const DTMFGenerator::DTMFTone DTMFGenerator::tones_[] = {{'0', 941, 1336},
                                                         {'1', 697, 1209},
                                                         {'2', 697, 1336},
                                                         {'3', 697, 1477},
                                                         {'4', 770, 1209},
                                                         {'5', 770, 1336},
                                                         {'6', 770, 1477},
                                                         {'7', 852, 1209},
                                                         {'8', 852, 1336},
                                                         {'9', 852, 1477},
                                                         {'A', 697, 1633},
                                                         {'B', 770, 1633},
                                                         {'C', 852, 1633},
                                                         {'D', 941, 1633},
                                                         {'*', 941, 1209},
                                                         {'#', 941, 1477}};

/*
 * Initialize the generator
 */
DTMFGenerator::DTMFGenerator(unsigned int sampleRate)
    : state()
    , sampleRate_(sampleRate)
    , tone_("", sampleRate)
{
    state.offset = 0;
    state.sample = 0;

    for (int i = 0; i < NUM_TONES; i++)
        toneBuffers_[i] = fillToneBuffer(i);
}

DTMFGenerator::~DTMFGenerator()
{
    for (int i = 0; i < NUM_TONES; i++)
        delete[] toneBuffers_[i];
}

using std::vector;

/*
 * Get n samples of the signal of code code
 */
void
DTMFGenerator::getSamples(vector<AudioSample>& buffer, unsigned char code)
{
    code = toupper(code);

    if (code >= '0' and code <= '9')
        state.sample = toneBuffers_[code - '0'];
    else if (code >= 'A' and code <= 'D')
        state.sample = toneBuffers_[code - 'A' + 10];
    else {
        switch (code) {
        case '*':
            state.sample = toneBuffers_[NUM_TONES - 2];
            break;

        case '#':
            state.sample = toneBuffers_[NUM_TONES - 1];
            break;

        default:
            throw DTMFException("Invalid code");
            break;
        }
    }

    size_t i;
    const size_t n = buffer.size();

    for (i = 0; i < n; ++i)
        buffer[i] = state.sample[i % sampleRate_];

    state.offset = i % sampleRate_;
}

/*
 * Get next n samples (continues where previous call to
 * genSample or genNextSamples stopped
 */
void
DTMFGenerator::getNextSamples(vector<AudioSample>& buffer)
{
    if (state.sample == 0)
        throw DTMFException("DTMF generator not initialized");

    size_t i;
    const size_t n = buffer.size();

    for (i = 0; i < n; i++)
        buffer[i] = state.sample[(state.offset + i) % sampleRate_];

    state.offset = (state.offset + i) % sampleRate_;
}

AudioSample*
DTMFGenerator::fillToneBuffer(int index)
{
    assert(index >= 0 and index < NUM_TONES);
    AudioSample* ptr = new AudioSample[sampleRate_];
    tone_.genSin(ptr, tones_[index].higher, tones_[index].lower, sampleRate_);
    return ptr;
}

} // namespace jami
