/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef _HISTORY_ITEM
#define _HISTORY_ITEM

#include <string>
#include <config/config.h>
#include <iostream>

typedef enum CallType {
    CALL_MISSED,
    CALL_INCOMING,
    CALL_OUTGOING
} CallType;


class HistoryItem
{

    public:
        /*
         * Constructor
         */
        HistoryItem (std::string, CallType, std::string, std::string, std::string, std::string="");

        /*
         * Constructor from a serialized form
         */
        HistoryItem (std::string, std::string="");

        /*
         * Destructor
         */
        ~HistoryItem ();

        inline std::string get_timestamp () {
            return _timestamp_start;
        }

        bool save (Conf::ConfigTree **history);

        std::string serialize (void);

    private:

        /*
         * @return true if the account ID corresponds to a loaded account
         */
        bool non_valid_account (std::string);

        /*
         * Timestamp representing the date of the call
         */
        std::string _timestamp_start;
        std::string _timestamp_stop;

        /*
         * Represents the type of call
         * Has be either CALL_MISSED, CALL_INCOMING or CALL_OUTGOING
         */
        CallType _call_type;

        /*
         * The information about the callee/caller, depending on the type of call.
         */
        std::string _name;
        std::string _number;

        /*
         * The account the call was made with
         */
        std::string _account_id;
};


#endif // HISTORY_ITEM
