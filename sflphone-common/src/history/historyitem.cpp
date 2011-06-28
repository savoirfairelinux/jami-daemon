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

HistoryItem::HistoryItem (std::string timestamp_start, CallType call_type, std::string timestamp_stop, std::string name, std::string number, std::string id, std::string account_id, std::string recording, std::string confID, std::string timeAdded)
    :	_timestamp_start (timestamp_start),
        _timestamp_stop (timestamp_stop),
        _call_type (call_type),
        _name (name),
        _number (number),
	_id(id),
        _account_id (account_id),
	_recording_file(recording),
	_confID(confID),
	_timeAdded(timeAdded)
{
}


HistoryItem::HistoryItem (std::string serialized_form)
{
    size_t pos;
    std::string tmp, type, name, number, start, stop, id, account, recordFile;
    std::string confID, timeAdded;
    int indice = 0;

    while (serialized_form.find (ITEM_SEPARATOR, 0) != std::string::npos) {
        pos = serialized_form.find (ITEM_SEPARATOR, 0);
        tmp = serialized_form.substr (0, pos);
        serialized_form.erase (0, pos + 1);

        switch (indice) {
            case 0: // The call type
                type = tmp;
		_error("Unserialized type: %s", tmp.c_str());
                break;
            case 1: // The number field
                number = tmp;
		_error("Serialized number: %s", tmp.c_str());
                break;
            case 2: // The name field
                name = tmp;
		_error("Serialized name: %s", tmp.c_str());
                break;
            case 3: // The start timestamp
		_error("Serialized time start: %s", tmp.c_str());
                start = tmp;
                break;
	    case 4: // The end timestamp
		_error("Serialized time stop: %s", tmp.c_str());
		stop = tmp;
		break;
	    case 5: // The ID
		_error("Serialized id: %s", tmp.c_str());
		id = tmp;
		break;
            case 6: // The account ID
		_error("Serialized account: %s", tmp.c_str());
                account = tmp;
                break;
            case 7: // The recorded file name
		_error("Serialized recordfile: %s", tmp.c_str());
		recordFile = tmp;
		break;
            case 8: // The conference ID
	        _error("Serialized conferenceID: %s", tmp.c_str());
		confID = tmp;
		break;
	    case 9: // The time
		_error("Serialized timeadded: %s", tmp.c_str());
		timeAdded = tmp;
		break;
            default: // error
                std::cout << "[ERROR] unserialized form not recognized." << std::endl;
                break;
        }

        indice ++;
    }

    _id = id;

    _call_type = (CallType) atoi (type.c_str());

    _number = number;
    (name == EMPTY_STRING) ? _name = "" : _name = name;
    _timestamp_start = start;
    _timestamp_stop = stop;
    // (serialized_form == EMPTY_STRING) ? _account_id = "" : _account_id = tmp;
    _account_id = account;

    _confID = confID;
    _timeAdded = timeAdded;

    _recording_file = recordFile;
}

HistoryItem::~HistoryItem ()
{
    // TODO
}

bool HistoryItem::save (Conf::ConfigTree **history)
{
    std::stringstream section;
    std::stringstream call_type;
    std::string sectionstr;
    bool res;

    // The section is : "[" + timestamp = "]"
    section << rand();
    call_type << _call_type;

    sectionstr = section.str();

    _error("-- Unserialized type: %s", call_type.str().c_str());
    _error("-- Unserialized time start: %s", _timestamp_start.c_str());
    _error("-- Unserialized time stop: %s", _timestamp_stop.c_str());
    _error("-- Unserialized number: %s", _number.c_str());
    _error("-- Unserialized id: %s", _id.c_str());
    _error("-- Unserialized account: %s", _account_id.c_str());
    _error("-- Unserialized name: %s", _name.c_str());
    _error("-- Unserialized record file: %s", _recording_file.c_str());
    _error("-- Unserialized conference id:%s", _confID.c_str());
    _error("-- Unserialized time added: %s", _timeAdded.c_str());


    res = ( (*history)->setConfigTreeItem (sectionstr, "type", call_type.str())
	    && (*history)->setConfigTreeItem (sectionstr, "timestamp_start", _timestamp_start)
            && (*history)->setConfigTreeItem (sectionstr, "timestamp_stop", _timestamp_stop)
            && (*history)->setConfigTreeItem (sectionstr, "number", _number)
	    && (*history)->setConfigTreeItem (sectionstr, "id", _id)
            && (*history)->setConfigTreeItem (sectionstr, "accountid", _account_id)
            && (*history)->setConfigTreeItem (sectionstr, "name", _name)
	    && (*history)->setConfigTreeItem (sectionstr, "recordfile", _recording_file)
	    && (*history)->setConfigTreeItem (sectionstr, "confid", _confID)
	    && (*history)->setConfigTreeItem (sectionstr, "timeadded", _timeAdded));
	   

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
    res << _call_type << separator << _number << separator << name << separator << _timestamp_start << separator << _timestamp_stop 
	<< separator << _id << separator << accountID << separator << _recording_file << separator << _confID << separator << _timeAdded;

    return res.str();
}


bool HistoryItem::non_valid_account (std::string id)
{
    return !Manager::instance().accountExists (id);
}
