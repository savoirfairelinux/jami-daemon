/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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
#include "tone.h"
#include "logger.h"
#include "ring_types.h"

#include <vector>
#include <cmath>
#include <cstdlib>

namespace jami {

Tone::Tone(const std::string& definition, unsigned int sampleRate) :
    AudioLoop(sampleRate)
{
    genBuffer(definition); // allocate memory with definition parameter
}

void
Tone::genBuffer(const std::string& definition)
{
    if (definition.empty())
        return;

    size_t size = 0;
    const int sampleRate = buffer_->getSampleRate();

    std::vector<AudioSample> buffer(SIZEBUF);
    size_t bufferPos(0);

    // Number of format sections
    std::string::size_type posStart = 0; // position of precedent comma
    std::string::size_type posEnd = 0; // position of the next comma

    std::string s; // portion of frequency
    size_t count; // number of int for one sequence

    std::string::size_type deflen = definition.length();

    do {
        posEnd = definition.find(',', posStart);

        if (posEnd == std::string::npos)
            posEnd = deflen;

        /* begin scope */
        {
            // Sample string: "350+440" or "350+440/2000,244+655/2000"
            int low, high, time;
            s = definition.substr(posStart, posEnd - posStart);

            // The 1st frequency is before the first + or the /
            size_t pos_plus = s.find('+');
            size_t pos_slash = s.find('/');
            size_t len = s.length();
            size_t endfrequency = 0;

            if (pos_slash == std::string::npos) {
                time = 0;
                endfrequency = len;
            } else {
                time = atoi(s.substr(pos_slash + 1, len - pos_slash - 1).c_str());
                endfrequency = pos_slash;
            }

            // without a plus = 1 frequency
            if (pos_plus == std::string::npos) {
                low = atoi(s.substr(0, endfrequency).c_str());
                high = 0;
            } else {
                low = atoi(s.substr(0, pos_plus).c_str());
                high = atoi(s.substr(pos_plus + 1, endfrequency - pos_plus - 1).c_str());
            }

            // If there is time or if it's unlimited
            if (time == 0)
                count = sampleRate;
            else
                count = (sampleRate * time) / 1000;

            // Generate SAMPLING_RATE samples of sinus, buffer is the result
            buffer.resize(size+count);
            genSin(&(*(buffer.begin()+bufferPos)), low, high, count);

            // To concatenate the different buffers for each section.
            size += count;
            bufferPos += count;
        } /* end scope */

        posStart = posEnd + 1;
    } while (posStart < deflen);

    buffer_->copy(buffer.data(), size); // fill the buffer
}

void
Tone::genSin(AudioSample* buffer, int lowFrequency, int highFrequency, size_t nb)
{
    static constexpr auto PI = 3.141592653589793238462643383279502884L;
    const double sr = (double)buffer_->getSampleRate();
    const double dx_h = sr ? 2.0 * PI * lowFrequency / sr : 0.0;
    const double dx_l = sr ? 2.0 * PI * highFrequency / sr : 0.0;
    static constexpr double DATA_AMPLITUDE = 2048;
    for (size_t t = 0; t < nb; t ++) {
        buffer[t] = DATA_AMPLITUDE * (sin(t*dx_h) + sin(t*dx_l));
    }
}

} // namespace jami
