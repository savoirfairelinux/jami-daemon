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

#define ITEM_SEPARATOR      "|"

HistoryItem::HistoryItem (std::string timestamp, CallType call_type, std::string to, std::string from, std::string caller_id, std::string account_id)
    : _timestamp (timestamp), _call_type (call_type), _to (to), _from (from), _caller_id (caller_id), _account_id (account_id)
{
}


HistoryItem::HistoryItem (std::string timestamp, std::string serialized_form)
    : _timestamp (timestamp)
{
    size_t pos;
    std::string tmp, id, to, from, callerid;
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
            case 1: // The to field
                to = tmp;
                break;
            case 2: // The from field
                from = tmp;
                break;
            case 3: // The calller id information
                callerid = tmp;
                break;
            default: // error
                std::cout <<"[ERROR] unserialized form not recognized."<<std::endl;
                break;
        }   
        indice ++;
    }

    _call_type = (CallType)atoi (id.c_str());
    _to = to;
    _from = from;
    _caller_id = serialized_form;
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
    timestamp = get_timestamp ();
    call_type << _call_type;

    res = ( (*history)->setConfigTreeItem(section, "type", call_type.str())
            && (*history)->setConfigTreeItem(section, "timestamp", timestamp)
            && (*history)->setConfigTreeItem(section, "to", _to)
            && (*history)->setConfigTreeItem(section, "from", _from)
            && (*history)->setConfigTreeItem(section, "id", _caller_id) );

    return res;
}

std::string HistoryItem::serialize (void)
{
    std::stringstream res;
    std::string separator = ITEM_SEPARATOR;

    res << _call_type << separator << _to << separator << _from << separator << _caller_id;
    return res.str();
}





















