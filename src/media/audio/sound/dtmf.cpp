/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  Portions Copyright (c) 2000 Billy Biggs <bbiggs@div8.net>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "dtmf.h"

namespace ring {

DTMF::DTMF(unsigned int sampleRate)
    : currentTone_(0), newTone_(0), dtmfgenerator_(sampleRate)
{}

void DTMF::startTone(char code)
{
    newTone_ = code;
}

using std::vector;

bool DTMF::generateDTMF(vector<AudioSample> &buffer)
{
    try {
        if (currentTone_ != 0) {
            // Currently generating a DTMF tone
            if (currentTone_ == newTone_) {
                // Continue generating the same tone
                dtmfgenerator_.getNextSamples(buffer);
                return true;
            } else if (newTone_ != 0) {
                // New tone requested
                dtmfgenerator_.getSamples(buffer, newTone_);
                currentTone_ = newTone_;
                return true;
            } else {
                // Stop requested
                currentTone_ = newTone_;
                return false;
            }
        } else {
            // Not generating any DTMF tone
            if (newTone_) {
                // Requested to generate a DTMF tone
                dtmfgenerator_.getSamples(buffer, newTone_);
                currentTone_ = newTone_;
                return true;
            } else
                return false;
        }
    } catch (const DTMFException &e) {
        // invalid key
        return false;
    }
}

} // namespace ring
