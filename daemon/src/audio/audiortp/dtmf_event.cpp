/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#include "dtmf_event.h"
#include "logger.h"

namespace sfl {

DTMFEvent::DTMFEvent(char digit) : payload(), newevent(true), length(1000)
{
    /*
       From RFC2833:

       Event  encoding (decimal)
       _________________________
       0--9                0--9
       *                     10
       #                     11
       A--D              12--15
       Flash                 16
    */

    switch (digit) {
        case '!':
            digit = 16;
            break;

        case '*':
            digit = 10;
            break;

        case '#':
            digit = 11;
            break;

        case 'A' ... 'D':
            digit = digit - 'A' + 12;
            break;

        case '0' ... '9':
            digit = digit - '0';
            break;

        default:
            ERROR("Unexpected DTMF %c", digit);
    }

    payload.event = digit;
    payload.ebit = false; // end of event bit
    payload.rbit = false; // reserved bit
    payload.duration = 1; // duration for this event
    payload.vol = 10;
}

}
