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

static const char * const ITEM_SEPARATOR = "|";

HistoryItem::HistoryItem(const std::string &timestamp_start,
                         CallType call_type, const std::string &timestamp_stop,
                         const std::string &name, const std::string &number,
                         const std::string &id, const std::string &account_id,
                         const std::string &recording,
                         const std::string &confID,
                         const std::string &timeAdded)
    :	timestamp_start_(timestamp_start),
        timestamp_stop_(timestamp_stop),
        call_type_(call_type),
        name_(name),
        number_(number),
        id_(id),
        account_id_(account_id),
        recording_file_(recording),
        confID_(confID),
        timeAdded_(timeAdded)
{}


HistoryItem::HistoryItem(std::string serialized_form) :
    timestamp_start_(), timestamp_stop_(), call_type_(CALL_MISSED), name_(),
    number_(), id_(), account_id_(), recording_file_(), confID_(), timeAdded_() 
{
    for (int index = 0; serialized_form.find(ITEM_SEPARATOR, 0) != std::string::npos; ++index) {
        size_t pos = serialized_form.find(ITEM_SEPARATOR, 0);
        std::string tmp(serialized_form.substr(0, pos));
        serialized_form.erase(0, pos + 1);

        switch (index) {
            case 0: // The call type
                call_type_ = (CallType) atoi(tmp.c_str());
                break;
            case 1: // The number field
                number_ = tmp;
                break;
            case 2: // The name field
                name_ = tmp;

                if (name_ == "empty")
                    name_ = "";
                break;
            case 3: // The start timestamp
                timestamp_start_ = tmp;
                break;
            case 4: // The end timestamp
                timestamp_stop_ = tmp;
                break;
            case 5: // The ID
                id_ = tmp;
                break;
            case 6: // The account ID
                account_id_ = tmp;
                break;
            case 7: // The recorded file name
                recording_file_ = tmp;
                break;
            case 8: // The conference ID
                confID_ = tmp;
                break;
            case 9: // the time
                timeAdded_ = tmp;
                break;
            default: // error
                ERROR("Unserialized form %d not recognized\n", index);
                break;
        }
    }
}

bool HistoryItem::save(Conf::ConfigTree **history)
{
    std::stringstream section;
    std::stringstream call_type;

    // The section is : "[" + timestamp = "]"
    section << rand();
    std::string sectionstr = section.str();
    call_type << call_type_;

    return (*history)->setConfigTreeItem(sectionstr, "type", call_type.str())
           && (*history)->setConfigTreeItem(sectionstr, "timestamp_start", timestamp_start_)
           && (*history)->setConfigTreeItem(sectionstr, "timestamp_stop", timestamp_stop_)
           && (*history)->setConfigTreeItem(sectionstr, "number", number_)
           && (*history)->setConfigTreeItem(sectionstr, "id", id_)
           && (*history)->setConfigTreeItem(sectionstr, "accountid", account_id_)
           && (*history)->setConfigTreeItem(sectionstr, "name", name_)
           && (*history)->setConfigTreeItem(sectionstr, "recordfile", recording_file_)
           && (*history)->setConfigTreeItem(sectionstr, "confid", confID_)
           && (*history)->setConfigTreeItem(sectionstr, "timeadded", timeAdded_);
}

std::string HistoryItem::serialize() const
{
    // Replace empty string with a valid standard string value
    std::string name(name_);

    if (name.empty())
        name = "empty";

    // For the account ID, check also if the accountID corresponds to an existing account
    // ie the account may have been removed
    std::string accountID(account_id_);

    if (account_id_.empty() or not valid_account(account_id_))
        accountID = "empty";

    std::stringstream res;
    // Serialize it
    res << call_type_ << ITEM_SEPARATOR << number_ << ITEM_SEPARATOR << name << ITEM_SEPARATOR << timestamp_start_ << ITEM_SEPARATOR << timestamp_stop_
        << ITEM_SEPARATOR << id_ << ITEM_SEPARATOR << accountID << ITEM_SEPARATOR << recording_file_ << ITEM_SEPARATOR << confID_ << ITEM_SEPARATOR << timeAdded_;

    return res.str();
}


bool HistoryItem::valid_account(const std::string &id) const
{
    return Manager::instance().accountExists(id);
}
