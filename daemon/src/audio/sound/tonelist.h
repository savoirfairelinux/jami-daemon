/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *
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

#ifndef __TONELIST_H__
#define __TONELIST_H__

#include "tone.h"

namespace sfl {

class TelephoneTone {
    public:
        /** Countries */
        enum COUNTRYID {
            ZID_NORTH_AMERICA = 0,
            ZID_FRANCE,
            ZID_AUSTRALIA,
            ZID_UNITED_KINGDOM,
            ZID_SPAIN,
            ZID_ITALY,
            ZID_JAPAN,
            ZID_COUNTRIES,
        };

        TelephoneTone(const std::string& countryName, unsigned int sampleRate);
        ~TelephoneTone();

        void setCurrentTone(Tone::TONEID toneId);
        Tone* getCurrentTone();

    private:
        NON_COPYABLE(TelephoneTone);

        static COUNTRYID getCountryId(const std::string& countryName);

        Tone* tone_[Tone::TONE_NULL];
        Tone::TONEID currentTone_;
};

}

#endif
