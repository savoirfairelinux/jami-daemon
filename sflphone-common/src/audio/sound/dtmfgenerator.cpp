/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
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

#include <math.h>

#include "dtmfgenerator.h"
#include "global.h"


/*
 * Tone frequencies
 */
const DTMFGenerator::DTMFTone DTMFGenerator::tones[NUM_TONES] = {
    {'0', 941, 1336},
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
    {'#', 941, 1477}
};


DTMFException::DTMFException (const char* _reason) throw() : reason (_reason)
{
}


DTMFException::~DTMFException() throw()
{
}

const char* DTMFException::what() const throw()
{
    return reason;
}



/*
 * Initialize the generator
 */
DTMFGenerator::DTMFGenerator (unsigned int sampleRate) : state(), _sampleRate (sampleRate), tone ("", sampleRate)
{
    state.offset = 0;
    state.sample = 0;

    for (int i = 0; i < NUM_TONES; i++) {
        samples[i] = generateSample (i);
    }
}


DTMFGenerator::~DTMFGenerator()
{
    for (int i = 0; i < NUM_TONES; i++) {
        delete[] samples[i];
        samples[i] = NULL;
    }
}

/*
 * Get n samples of the signal of code code
 */
void DTMFGenerator::getSamples (SFLDataFormat* buffer, size_t n, unsigned char code) throw (DTMFException)
{
    size_t i;

    if (!buffer) {
        throw DTMFException ("Invalid parameter value");
    }

    switch (code) {

        case '0':
            state.sample = samples[0];
            break;

        case '1':
            state.sample = samples[1];
            break;

        case '2':
            state.sample = samples[2];
            break;

        case '3':
            state.sample = samples[3];
            break;

        case '4':
            state.sample = samples[4];
            break;

        case '5':
            state.sample = samples[5];
            break;

        case '6':
            state.sample = samples[6];
            break;

        case '7':
            state.sample = samples[7];
            break;

        case '8':
            state.sample = samples[8];
            break;

        case '9':
            state.sample = samples[9];
            break;

        case 'A':

        case 'a':
            state.sample = samples[10];
            break;

        case 'B':

        case 'b':
            state.sample = samples[11];
            break;

        case 'C':

        case 'c':
            state.sample = samples[12];
            break;

        case 'D':

        case 'd':
            state.sample = samples[13];
            break;

        case '*':
            state.sample = samples[14];
            break;

        case '#':
            state.sample = samples[15];
            break;

        default:
            throw DTMFException ("Invalid code");
            return;
            break;
    }

    for (i = 0; i < n; i++) {
        buffer[i] = state.sample[i % _sampleRate];
    }

    state.offset = i % _sampleRate;
}


/*
 * Get next n samples (continues where previous call to
 * genSample or genNextSamples stopped
 */
void DTMFGenerator::getNextSamples (SFLDataFormat* buffer, size_t n) throw (DTMFException)
{
    size_t i;

    if (!buffer) {
        throw DTMFException ("Invalid parameter");
    }

    if (state.sample == 0) {
        throw DTMFException ("DTMF generator not initialized");
    }

    for (i = 0; i < n; i++) {
        buffer[i] = state.sample[ (state.offset + i) % _sampleRate];
    }

    state.offset = (state.offset + i) % _sampleRate;
}


/*
 * Generate a tone sample
 */
SFLDataFormat* DTMFGenerator::generateSample (unsigned char code) throw (DTMFException)
{
    SFLDataFormat* ptr;

    try {
        ptr = new SFLDataFormat[_sampleRate];

        if (!ptr) {
            throw new DTMFException ("No memory left");
            return 0;
        }

        tone.genSin (ptr, tones[code].higher, tones[code].lower,  _sampleRate);

        return ptr;
    } catch (...) {
        throw new DTMFException ("No memory left");
        return 0;
    }

}
