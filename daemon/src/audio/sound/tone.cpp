/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
/*
 * YM: 2006-11-15: changes unsigned int to std::string::size_type, thanks to Pierre Pomes (AMD64 compilation)
 */
#include "tone.h"
#include "logger.h"
#include "sfl_types.h"
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

Tone::Tone(const std::string& definition, unsigned int sampleRate) :
    AudioLoop(sampleRate), xhigher_(0.0), xlower_(0.0)
{
    fillWavetable();
    genBuffer(definition); // allocate memory with definition parameter
}

void
Tone::genBuffer(const std::string& definition)
{
    if (definition.empty())
        return;

    size_t size = 0;
    const int sampleRate = buffer_->getSampleRate();

    std::vector<SFLAudioSample> buffer(SIZEBUF);
    size_t bufferPos(0);

    // Number of format sections
    std::string::size_type posStart = 0; // position of precedent comma
    std::string::size_type posEnd = 0; // position of the next comma

    std::string s; // portion of frequency
    int count; // number of int for one sequence

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
Tone::fillWavetable()
{
    static const double TABLE_SZ = TABLE_LENGTH - 1.0;
    static const double TWO_PI = 2.0 * M_PI;

    for (int i = 0; i < TABLE_LENGTH; ++i)
        wavetable_[i] = sin((i / TABLE_SZ) * TWO_PI);
}

double
Tone::interpolate(double x) const
{
    int xi_0 = x;
    int xi_1 = xi_0 + 1;

    double yi_0 = wavetable_[xi_0];
    double yi_1 =  wavetable_[xi_1];

    double A = (x - xi_0);
    double B = 1.0 - A;

    return (A * yi_0) + (B * yi_1);
}

void
Tone::genSin(SFLAudioSample* buffer, int lowFrequency, int highFrequency, int nb)
{
    xhigher_ = 0.0;
    xlower_ = 0.0;

    const double sr = (double)buffer_->getSampleRate();
    static const double tableSize = TABLE_LENGTH;

    const double dx_h = lowFrequency and sr ? tableSize / (sr / lowFrequency) : 0.0;
    const double dx_l = highFrequency and sr ? tableSize / (sr / highFrequency) : 0.0;

    double x_h = xhigher_;
    double x_l = xlower_;

    static const double DATA_AMPLITUDE = 2047;
    const double amp =  DATA_AMPLITUDE;

    for (int t = 0; t < nb; t ++) {
        buffer[t] = amp * (interpolate(x_h) + interpolate(x_l));
        x_h += dx_h;
        x_l += dx_l;

        while (x_h > tableSize)
            x_h -= tableSize;

        while (x_l > tableSize)
            x_l -= tableSize;
    }

    xhigher_ = x_h;
    xlower_ = x_l;
}

