/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef HISTORY_ITEM_H_
#define HISTORY_ITEM_H_

#include <string>
#include <map>

typedef enum HistoryState {
    MISSED,
    INCOMING,
    OUTGOING
} HistoryState;

namespace Conf {
class ConfigTree;
}

class HistoryItem {
    public:
        static const char * const ACCOUNT_ID_KEY;
        static const char * const CONFID_KEY;
        static const char * const CALLID_KEY;
        static const char * const PEER_NAME_KEY;
        static const char * const PEER_NUMBER_KEY;
        static const char * const RECORDING_PATH_KEY;
        static const char * const TIME_ADDED_KEY;
        static const char * const TIMESTAMP_START_KEY;
        static const char * const TIMESTAMP_STOP_KEY;
        static const char * const STATE_KEY;
        /*
         * Constructor
         *
         * @param Timestamp start
         * @param Call type
         * @param Timestamp stop
         * @param Call name
         * @param Call number
         * @param Call id
         * @param Call account id
         * @param Recording file name (if any recording were performed)
         * @param Configuration ID
         * @param time added
         */
        HistoryItem(const std::string&, HistoryState, const std::string&,
                    const std::string&, const std::string&, const std::string&,
                    const std::string&, const std::string&, const std::string&,
                    const std::string&);

        /*
         * Constructor from a serialized form
        	 * @string contaning serialized form
         */
        HistoryItem(std::string="");

        std::string get_timestamp() const {
            return timestampStart_;
        }

        bool youngerThan(int otherTime) const;

        bool save(Conf::ConfigTree &history) const;

        std::string serialize() const;
        std::map<std::string, std::string> toMap() const;

    private:
        /*
         * @return true if the account ID corresponds to a loaded account
         */
        bool valid_account(const std::string &id) const;

        /*
         * The account the call was made with
         */
        std::string accountID_;

        /**
         * The conference ID for this call (if any)
         */
        std::string confID_;

        /**
         * The identifier for this call
         */
        std::string callID_;

        /*
         * The information about the callee/caller, depending on the type of call.
         */
        std::string peerName_;
        std::string peerNumber_;

        /**
         * Path of recording for this call, if it exists
         */
        std::string recordingPath_;

        /**
         * Time added to conference
         */
        std::string timeAdded_;

        /*
         * Timestamp representing the date of the call
         */
        std::string timestampStart_;
        std::string timestampStop_;

        /*
         * Represents the type of call
         * Has be either CALL_MISSED, CALL_INCOMING or CALL_OUTGOING
         */
        HistoryState state_;
};


#endif // HISTORY_ITEM
