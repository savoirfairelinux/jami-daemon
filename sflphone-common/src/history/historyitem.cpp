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

#include <historyitem.h>
#include <sstream>
#include "stdlib.h"
#include <manager.h>

#define ITEM_SEPARATOR      "|"
#define EMPTY_STRING        "empty"

HistoryItem::HistoryItem (std::string timestamp_start, CallType call_type, std::string timestamp_stop, std::string name, std::string number, std::string account_id)
    : _timestamp_start (timestamp_start), _call_type (call_type), _timestamp_stop (timestamp_stop), _name (name), _number (number), _account_id (account_id)
{
}


HistoryItem::HistoryItem (std::string timestamp, std::string serialized_form)
    : _timestamp_start (timestamp)
{
    size_t pos;
    std::string tmp, id, name, number, stop, account;
    int indice=0;

    while (serialized_form.find(ITEM_SEPARATOR, 0) != std::string::npos)
    {
        pos = serialized_form.find (ITEM_SEPARATOR, 0);
        tmp = serialized_form.substr (0, pos);
        serialized_form.erase (0, pos + 1);
        switch (indice)
        {
            case 0: // The call type
                id = tmp;
                break;
            case 1: // The number field
                number = tmp;
                break;
            case 2: // The name field
                name = tmp;
                break;
            case 3: // The end timestamp
                stop = tmp;
                break;
            case 4: // The account ID
                account = tmp;
                break;
            default: // error
                std::cout <<"[ERROR] unserialized form not recognized."<<std::endl;
                break;
        }   
        indice ++;
    }

    _call_type = (CallType)atoi (id.c_str());
    _number = number;
    (name == EMPTY_STRING) ? _name = "" : _name = name;
    _timestamp_stop = stop;
    (serialized_form == EMPTY_STRING) ? _account_id = "" : _account_id=serialized_form ;
}

HistoryItem::~HistoryItem ()
{
    // TODO
}

bool HistoryItem::save (Conf::ConfigTree **history){

    std::string section, timestamp;
    std::stringstream call_type;
    bool res;

    // The section is : "[" + timestamp = "]"
    section = get_timestamp ();
    call_type << _call_type;

    res = ( (*history)->setConfigTreeItem(section, "type", call_type.str())
            && (*history)->setConfigTreeItem(section, "timestamp_stop", _timestamp_stop)
            && (*history)->setConfigTreeItem(section, "number", _number)
            && (*history)->setConfigTreeItem(section, "accountid", _account_id)
            && (*history)->setConfigTreeItem(section, "name", _name) );

    return res;
}

std::string HistoryItem::serialize (void)
{
    std::stringstream res;
    std::string separator = ITEM_SEPARATOR;
    std::string name, accountID;

    // Replace empty string with a valid standard string value
    (_name == "")? name = EMPTY_STRING : name = _name;
    // For the account ID, check also if the accountID corresponds to an existing account
    // ie the account may have been removed
    (_account_id == "" || non_valid_account (_account_id))? accountID = "empty" : accountID = _account_id;

    // Serialize it
    res << _call_type << separator << _number << separator << name << separator << _timestamp_stop << separator << accountID;

    return res.str();
}


bool HistoryItem::non_valid_account (std::string id)
{
    return !Manager::instance().accountExists (id);
}
