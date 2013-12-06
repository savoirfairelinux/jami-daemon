/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexamdre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef HISTORY_ITEM_H_
#define HISTORY_ITEM_H_

#include <string>
#include <map>

namespace sfl {

class HistoryItem {
    public:
        static const char * const ACCOUNT_ID_KEY;
        static const char * const CONFID_KEY;
        static const char * const CALLID_KEY;
        static const char * const DISPLAY_NAME_KEY;
        static const char * const PEER_NUMBER_KEY;
        static const char * const RECORDING_PATH_KEY;
        static const char * const TIMESTAMP_START_KEY;
        static const char * const TIMESTAMP_STOP_KEY;
        static const char * const AUDIO_CODEC_KEY;
        static const char * const VIDEO_CODEC_KEY;
        static const char * const STATE_KEY;
        static const char * const MISSED_KEY;
        static const char * const DIRECTION_KEY;

        static const char * const MISSED_STRING;
        static const char * const INCOMING_STRING;
        static const char * const OUTGOING_STRING;
        HistoryItem(const std::map<std::string, std::string> &args);
        HistoryItem(std::istream &stream);

        bool hasPeerNumber() const;

        bool youngerThan(unsigned long otherTime) const;

        std::map<std::string, std::string> toMap() const;
        void print(std::ostream &o) const;
        bool operator< (const HistoryItem &other) const {
            return timestampStart_ > other.timestampStart_;
        }

        bool operator> (const HistoryItem &other) const {
            return not (*this < other);
        }

    private:
        std::map<std::string, std::string> entryMap_;
        unsigned long timestampStart_; // cached as we use this a lot, avoids string ops
};

std::ostream& operator << (std::ostream& o, const HistoryItem& item);

}

#endif // HISTORY_ITEM
