/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "dcblocker.h"

DcBlocker::DcBlocker() : _y (0), _x (0), _xm1 (0), _ym1 (0) {}

DcBlocker::~DcBlocker() {}

void DcBlocker::reset()
{
    _y = 0;
    _x = 0;
    _xm1 = 0;
    _ym1 = 0;
}

void DcBlocker::putData (SFLDataFormat *inputData, int nbBytes) {}

int DcBlocker::getData (SFLDataFormat *outputData)
{
    return 0;
}

void DcBlocker::process (SFLDataFormat *data, int nbBytes)
{
    // y(n) = x(n) - x(n-1) + R y(n-1) , R = 0.9999

    int nbSamples = nbBytes / sizeof (SFLDataFormat);

    for (int i = 0; i < nbSamples; i++) {
        _x = data[i];

        _y = (SFLDataFormat) ( (float) _x - (float) _xm1 + 0.995 * (float) _ym1);
        _xm1 = _x;
        _ym1 = _y;

        data[i] = _y;

    }
}

int DcBlocker::process (SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes)
{

    int nbSamples = nbBytes / sizeof (SFLDataFormat);

    for (int i = 0; i < nbSamples; i++) {

        _x = inputData[i];

        _y = (SFLDataFormat) ( (float) _x - (float) _xm1 + 0.9999 * (float) _ym1);
        _xm1 = _x;
        _ym1 = _y;

        outputData[i] = _y;
    }

    return 0;

}

void DcBlocker::process (SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes) {}
