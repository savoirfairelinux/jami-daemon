/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <cmath>
#include <cstdlib>
#include <cstring>

static const double TWOPI = 2.0 * M_PI;

Tone::Tone(const std::string& definition, unsigned int sampleRate) :
    sampleRate_(sampleRate), xhigher_(0.0), xlower_(0.0)
{
    fillWavetable();
    genBuffer(definition); // allocate memory with definition parameter
}

void
Tone::genBuffer(const std::string& definition)
{
    if (definition.empty())
        return;

    size_ = 0;

    SFLDataFormat* buffer = new SFLDataFormat[SIZEBUF]; //1kb
    SFLDataFormat* bufferPos = buffer;

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
            int freq1, freq2, time;
            s = definition.substr(posStart, posEnd-posStart);

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
                freq1 = atoi(s.substr(0, endfrequency).c_str());
                freq2 = 0;
            } else {
                freq1 = atoi(s.substr(0, pos_plus).c_str());
                freq2 = atoi(s.substr(pos_plus + 1, endfrequency - pos_plus - 1).c_str());
            }

            // If there is time or if it's unlimited
            if (time == 0)
                count = sampleRate_;
            else
                count = (sampleRate_ * time) / 1000;

            // Generate SAMPLING_RATE samples of sinus, buffer is the result
            DEBUG("genSin(%d, %d)", freq1, freq2);
            genSin(bufferPos, freq1, freq2, count);

            // To concatenate the different buffers for each section.
            size_ += count;
            bufferPos += count;
        } /* end scope */

        posStart = posEnd + 1;
    } while (posStart < deflen);

    buffer_ = new SFLDataFormat[size_];

    memcpy(buffer_, buffer, size_ * sizeof(SFLDataFormat)); // copy char, not SFLDataFormat.

    delete [] buffer;
}

void
Tone::fillWavetable()
{
    double tableSize = (double) TABLE_LENGTH;

    for (int i = 0; i < TABLE_LENGTH; ++i)
        wavetable_[i] = sin((static_cast<double>(i) / (tableSize - 1.0)) * TWOPI);
}

double
Tone::interpolate(double x)
{
    int xi_0, xi_1;
    double yi_0, yi_1, A, B;

    xi_0 = (int) x;
    xi_1 = xi_0 + 1;

    yi_0 = wavetable_[xi_0];
    yi_1 =  wavetable_[xi_1];

    A = (x - xi_0);
    B = 1.0 - A;

    return (A * yi_0) + (B * yi_1);
}

void
Tone::genSin(SFLDataFormat* buffer, int frequency1, int frequency2, int nb)
{
    xhigher_ = 0.0;
    xlower_ = 0.0;

    double sr = (double) sampleRate_;
    double tableSize = (double) TABLE_LENGTH;

    double N_h = sr / (double)(frequency1);
    double N_l = sr / (double)(frequency2);

    double dx_h = tableSize / N_h;
    double dx_l = tableSize / N_l;

    double x_h = xhigher_;
    double x_l = xlower_;

    static const double DATA_AMPLITUDE = 2047;
    double amp =  DATA_AMPLITUDE;

    for (int t = 0; t < nb; t ++) {
        buffer[t] = static_cast<SFLDataFormat>(amp * (interpolate(x_h) + interpolate(x_l)));
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

