/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author : Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 * 	Portions Copyright (c) 2000 Billy Biggs <bbiggs@div8.net>
 *  Portions Copyright (c) 2004 Wirlab <kphone@wirlab.net>
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

#include "dtmf.h"

DTMF::DTMF (unsigned int sampleRate)
        : currentTone (0), newTone (0), dtmfgenerator (sampleRate)
{
}

DTMF::~DTMF (void)
{
}

void
DTMF::startTone (char code)
{
    newTone = code;
}

bool
DTMF::generateDTMF (SFLDataFormat* buffer, size_t n)
{
    if (!buffer) return false;

    try {
        if (currentTone != 0) {
            // Currently generating a DTMF tone
            if (currentTone == newTone) {
                // Continue generating the same tone
                dtmfgenerator.getNextSamples (buffer, n);
                return true;
            } else if (newTone != 0) {
                // New tone requested
                dtmfgenerator.getSamples (buffer, n, newTone);
                currentTone = newTone;
                return true;
            } else {
                // Stop requested
                currentTone = newTone;
                return false;
            }
        } else {
            // Not generating any DTMF tone
            if (newTone) {
                // Requested to generate a DTMF tone
                dtmfgenerator.getSamples (buffer, n, newTone);
                currentTone = newTone;
                return true;
            }

            return false;
        }
    } catch (DTMFException e) {
        // invalid key
        return false;
    }
}

