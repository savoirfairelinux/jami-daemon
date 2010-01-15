/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */



#include "dcblocker.h"

DcBlocker::DcBlocker()
{

    y = 0;
    x = 0;
    xm1 = 0;
    ym1 = 0;

}

DcBlocker::~DcBlocker()
{


}

void DcBlocker::filter_signal (SFLDataFormat* audio_data, int length)
{
    // y(n) = x(n) - x(n-1) + R y(n-1) , R = 0.9999

    for (int i = 0; i < length; i++) {

        x = audio_data[i];

        y = (SFLDataFormat) ( (float) x - (float) xm1 + 0.9999 * (float) ym1);
        xm1 = x;
        ym1 = y;

        audio_data[i] = y;

    }

}
