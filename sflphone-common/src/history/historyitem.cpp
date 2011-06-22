/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include <historyitem.h>
#include <sstream>
#include "stdlib.h"
#include <manager.h>

#define ITEM_SEPARATOR      "|"
#define EMPTY_STRING        "empty"

HistoryItem::HistoryItem (std::string timestamp_start, CallType call_type, std::string timestamp_stop, std::string name, std::string number, std::string account_id, std::string recording)
    :	_timestamp_start (timestamp_start),
        _timestamp_stop (timestamp_stop),
        _call_type (call_type),
        _name (name),
        _number (number),
        _account_id (account_id),
	_recording_file(recording)
{
}


HistoryItem::HistoryItem (std::string timestamp, std::string serialized_form)
    : _timestamp_start (timestamp)
{
    size_t pos;
    std::string tmp, id, name, number, start, stop, account, recordFile;
    int indice=0;

    while (serialized_form.find (ITEM_SEPARATOR, 0) != std::string::npos) {
        pos = serialized_form.find (ITEM_SEPARATOR, 0);
        tmp = serialized_form.substr (0, pos);
        serialized_form.erase (0, pos + 1);

        switch (indice) {
            case 0: // The call type
                id = tmp;
		_error("Unserialized id: %s", tmp.c_str());
                break;
            case 1: // The number field
                number = tmp;
		_error("Unserialized number: %s", tmp.c_str());
                break;
            case 2: // The name field
                name = tmp;
		_error("Unserialized name: %s", tmp.c_str());
                break;
            case 3: // The start timestamp
		_error("Unserialized time start: %s", tmp.c_str());
                start = tmp;
                break;
	    case 4: // The end timestamp
		_error("Unserialized time stop: %s", tmp.c_str());
		stop = tmp;
		break;
            case 5: // The account ID
		_error("Unserialized account: %s", tmp.c_str());
                account = tmp;
                break;
            case 6: // The recorded file name
		_error("Unserialized recordfile: %s", tmp.c_str());
		recordFile = tmp;
		break;
            default: // error
                std::cout <<"[ERROR] unserialized form not recognized."<<std::endl;
                break;
        }

        indice ++;
    }

    _call_type = (CallType) atoi (id.c_str());

    _number = number;
    (name == EMPTY_STRING) ? _name = "" : _name = name;
    _timestamp_stop = stop;
    (serialized_form == EMPTY_STRING) ? _account_id = "" : _account_id=tmp;
}

HistoryItem::~HistoryItem ()
{
    // TODO
}

bool HistoryItem::save (Conf::ConfigTree **history)
{

    std::string section, timestamp;
    std::stringstream call_type;
    bool res;

    // The section is : "[" + timestamp = "]"
    section = get_timestamp ();
    call_type << _call_type;

    res = ( (*history)->setConfigTreeItem (section, "type", call_type.str())
	    && (*history)->setConfigTreeItem (section, "timestamp_start", _timestamp_start)
            && (*history)->setConfigTreeItem (section, "timestamp_stop", _timestamp_stop)
            && (*history)->setConfigTreeItem (section, "number", _number)
            && (*history)->setConfigTreeItem (section, "accountid", _account_id)
            && (*history)->setConfigTreeItem (section, "name", _name)
	    && (*history)->setConfigTreeItem (section, "recordfile", _recording_file));

    return res;
}

std::string HistoryItem::serialize (void)
{
    std::stringstream res;
    std::string separator = ITEM_SEPARATOR;
    std::string name, accountID;

    // Replace empty string with a valid standard string value
    (_name == "") ? name = EMPTY_STRING : name = _name;
    // For the account ID, check also if the accountID corresponds to an existing account
    // ie the account may have been removed
    (_account_id == "" || non_valid_account (_account_id)) ? accountID = "empty" : accountID = _account_id;

    // Serialize it
    res << _call_type << separator << _number << separator << name << separator << _timestamp_start << separator << _timestamp_stop << separator << accountID
		<< separator << _recording_file;

    return res.str();
}


bool HistoryItem::non_valid_account (std::string id)
{
    return !Manager::instance().accountExists (id);
}
