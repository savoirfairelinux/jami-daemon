/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#include <math.h>
#include <cstdlib>
#include <strings.h>

#define TABLE_LENGTH 4096
double TWOPI = 2 * M_PI;

Tone::Tone (const std::string& definition, unsigned int sampleRate) : AudioLoop(), _sampleRate (sampleRate), _xhigher(0.0), _xlower(0.0)
{
	fillWavetable();
    genBuffer (definition); // allocate memory with definition parameter
}

Tone::~Tone()
{
}

void
Tone::genBuffer (const std::string& definition)
{
    if (definition.empty()) {
        return;
    }

    _size = 0;

    SFLDataFormat* buffer = new SFLDataFormat[SIZEBUF]; //1kb
    SFLDataFormat* bufferPos = buffer;

    // Number of format sections
    std::string::size_type posStart = 0; // position of precedent comma
    std::string::size_type posEnd = 0; // position of the next comma

    std::string s; // portion of frequency
    int count; // number of int for one sequence

    std::string::size_type deflen = definition.length();

    do {
        posEnd = definition.find (',', posStart);

        if (posEnd == std::string::npos) {
            posEnd = deflen;
        }

        {
            // Sample string: "350+440" or "350+440/2000,244+655/2000"
            int freq1, freq2, time;
            s = definition.substr (posStart, posEnd-posStart);

            // The 1st frequency is before the first + or the /
            std::string::size_type pos_plus = s.find ('+');
            std::string::size_type pos_slash = s.find ('/');
            std::string::size_type len = s.length();
            std::string::size_type endfrequency = 0;

            if (pos_slash == std::string::npos) {
                time = 0;
                endfrequency = len;
            } else {
                time = atoi ( (s.substr (pos_slash+1,len-pos_slash-1)).data());
                endfrequency = pos_slash;
            }

            // without a plus = 1 frequency
            if (pos_plus == std::string::npos) {
                freq1 = atoi ( (s.substr (0,endfrequency)).data());
                freq2 = 0;
            } else {
                freq1 = atoi ( (s.substr (0,pos_plus)).data());
                freq2 = atoi ( (s.substr (pos_plus+1, endfrequency-pos_plus-1)).data());
            }

            // If there is time or if it's unlimited
            if (time == 0) {
                count = _sampleRate;
            } else {
                count = (_sampleRate * time) / 1000;
            }

            // Generate SAMPLING_RATE samples of sinus, buffer is the result
            _debug("genSin(%d, %d)", freq1, freq2);
            genSin (bufferPos, freq1, freq2, count);

            // To concatenate the different buffers for each section.
            _size += (count);

            bufferPos += (count);
        }

        posStart = posEnd+1;
    } while (posStart < deflen);

    _buffer = new SFLDataFormat[_size];

    // src, dest, tocopy
    bcopy (buffer, _buffer, _size*sizeof (SFLDataFormat)); // copy char, not SFLDataFormat.

    delete[] buffer;

    buffer=0;

    bufferPos=0;
}

void
Tone::fillWavetable()
{
	double tableSize = (double)TABLE_LENGTH;

	for(int i = 0; i < TABLE_LENGTH; i++) {
		_wavetable[i] = sin( ((double)i / (tableSize - 1.0)) * TWOPI );
	}
}

double
Tone::interpolate(double x)
{
	int xi_0, xi_1;
	double yi_0, yi_1, A, B;

	xi_0 = (int)x;
	xi_1 = xi_0+1;

	yi_0  =_wavetable[xi_0];
	yi_1 = _wavetable[xi_1];

	A = (x - xi_0);
	B = 1.0 - A;

	return A*yi_0 + B*yi_1;
}

void
Tone::genSin (SFLDataFormat* buffer, int frequency1, int frequency2, int nb)
{
	_xhigher = 0.0;
	_xlower = 0.0;

	double sr = (double)_sampleRate;
	double tableSize = (double)TABLE_LENGTH;

	 double N_h = sr / (double) (frequency1);
	 double N_l = sr / (double)  (frequency2);

	 double dx_h = tableSize / N_h;
	 double dx_l = tableSize / N_l;

	 double x_h = _xhigher;
	 double x_l = _xlower;

	 double amp = (double)SFLDataAmplitude;

	 for (int t = 0; t < nb; t ++) {
		 buffer[t] = (int16)(amp*(interpolate(x_h) + interpolate(x_l)));
		 x_h += dx_h;
		 x_l += dx_l;

		 if(x_h > tableSize) {
			 x_h -= tableSize;
		}

		 if(x_l > tableSize) {
			 x_l -= tableSize;
		}
	 }

	 _xhigher = x_h;
	 _xlower = x_l;

}

