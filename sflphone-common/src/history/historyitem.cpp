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

HistoryItem::HistoryItem (int timestamp, CallType call_type, std::string to, std::string from, std::string caller_id, std::string account_id)
    : _timestamp (timestamp), _call_type (call_type), _to (to), _from (from), _caller_id (caller_id), _account_id (account_id)
{
}

HistoryItem::~HistoryItem ()
{
    // TODO
}

bool HistoryItem::save (Conf::ConfigTree **history){

    std::stringstream section, call_type, timestamp;
    bool res;

    // The section is : "[" + timestamp = "]"
    section << _timestamp ;
    call_type << _call_type;
    timestamp << _timestamp;
     
    res = ( (*history)->setConfigTreeItem(section.str(), "type", call_type.str())
            && (*history)->setConfigTreeItem(section.str(), "timestamp", timestamp.str())
            && (*history)->setConfigTreeItem(section.str(), "to", _to)
            && (*history)->setConfigTreeItem(section.str(), "from", _from)
            && (*history)->setConfigTreeItem(section.str(), "id", _caller_id) );

    return res;
}























