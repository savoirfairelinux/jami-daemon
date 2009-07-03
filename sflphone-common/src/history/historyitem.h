/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
}CallType; 


class HistoryItem {

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
