/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "historyitem.h"
#include <sstream>
#include <cstdlib>
#include "manager.h"

const char * const HistoryItem::ACCOUNT_ID_KEY =        "accountid";
const char * const HistoryItem::CALLID_KEY =            "callid";
const char * const HistoryItem::CONFID_KEY =            "confid";
const char * const HistoryItem::PEER_NAME_KEY =         "peer_name";
const char * const HistoryItem::PEER_NUMBER_KEY =       "peer_number";
const char * const HistoryItem::RECORDING_PATH_KEY =    "recordfile";
const char * const HistoryItem::TIME_ADDED_KEY =        "timeadded";
const char * const HistoryItem::TIMESTAMP_START_KEY =   "timestamp_start";
const char * const HistoryItem::TIMESTAMP_STOP_KEY =    "timestamp_stop";
const char * const HistoryItem::STATE_KEY =             "state";
const char * const HistoryItem::MISSED_STRING =         "missed";
const char * const HistoryItem::INCOMING_STRING =       "incoming";
const char * const HistoryItem::OUTGOING_STRING =       "outgoing";

HistoryItem::HistoryItem(const std::map<std::string, std::string> &args)
    : entryMap_(args)
{}

HistoryItem::HistoryItem(const std::string &item, Conf::ConfigTree &historyList)
    : entryMap_()
{
    const char *const KEYS [] = {
        ACCOUNT_ID_KEY,
        CALLID_KEY,
        CONFID_KEY,
        PEER_NAME_KEY,
        PEER_NUMBER_KEY,
        RECORDING_PATH_KEY,
        TIME_ADDED_KEY,
        TIMESTAMP_START_KEY,
        TIMESTAMP_STOP_KEY,
        STATE_KEY,
        NULL};
    for (int i = 0; KEYS[i]; ++i)
        entryMap_[KEYS[i]] = historyList.getConfigTreeItemValue(item, KEYS[i]);
}

void HistoryItem::save(Conf::ConfigTree &history) const
{
    // The section is : "[" + random integer = "]"
    std::stringstream section;
    section << rand();
    const std::string sectionstr = section.str();

    typedef std::map<std::string, std::string>::const_iterator EntryIter;
    for (EntryIter iter = entryMap_.begin(); iter != entryMap_.end(); ++iter)
        history.setConfigTreeItem(sectionstr, iter->first, iter->second);
}

std::map<std::string, std::string> HistoryItem::toMap() const
{
    return entryMap_;
}

bool HistoryItem::youngerThan(int otherTime) const
{
    return atol(getTimestampStart().c_str()) >= otherTime;
}

std::string HistoryItem::getTimestampStart() const {
    std::map<std::string, std::string>::const_iterator iter(entryMap_.find(TIMESTAMP_START_KEY));
    if (iter != entryMap_.end())
        return iter->second;
    else
        return "";
}
