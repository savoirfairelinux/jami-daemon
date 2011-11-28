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

const char * const HistoryItem::ACCOUNT_ID_KEY = "accountid";
const char * const HistoryItem::CALLID_KEY = "callid";
const char * const HistoryItem::CONFID_KEY = "confid";
const char * const HistoryItem::NAME_KEY = "name";
const char * const HistoryItem::NUMBER_KEY = "number";
const char * const HistoryItem::RECORDING_PATH_KEY = "recordfile";
const char * const HistoryItem::TIME_ADDED_KEY = "timeadded";
const char * const HistoryItem::TIMESTAMP_START_KEY = "timestamp_start";
const char * const HistoryItem::TIMESTAMP_STOP_KEY = "timestamp_stop";
const char * const HistoryItem::STATE_KEY = "state";

HistoryItem::HistoryItem(const std::string &timestampStart,
                         HistoryState state, const std::string &timestampStop,
                         const std::string &name, const std::string &number,
                         const std::string &callID, const std::string &accountID,
                         const std::string &recording,
                         const std::string &confID,
                         const std::string &timeAdded)
    :	accountID_(accountID),
        confID_(confID),
        callID_(callID),
        name_(name),
        number_(number),
        recordingPath_(recording),
        timeAdded_(timeAdded),
        timestampStart_(timestampStart),
        timestampStop_(timestampStop),
        state_(state)
{}


HistoryItem::HistoryItem(std::string serialized_form) :
    accountID_(), confID_(), callID_(), name_(), number_(), recordingPath_(),
    timeAdded_(), timestampStart_(), timestampStop_(),
    state_(MISSED)
{
    for (int index = 0; serialized_form.find(ITEM_SEPARATOR, 0) != std::string::npos; ++index) {
        size_t pos = serialized_form.find(ITEM_SEPARATOR, 0);
        std::string tmp(serialized_form.substr(0, pos));
        serialized_form.erase(0, pos + 1);

        switch (index) {
            case 0: // The call state
                state_ = (HistoryState) atoi(tmp.c_str());
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
                timestampStart_ = tmp;
                break;
            case 4: // The end timestamp
                timestampStop_ = tmp;
                break;
            case 5: // The call ID
                callID_ = tmp;
                break;
            case 6: // The account ID
                accountID_ = tmp;
                break;
            case 7: // The recorded file name
                recordingPath_ = tmp;
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
    std::stringstream state;

    // The section is : "[" + timestamp = "]"
    section << rand();
    std::string sectionstr = section.str();
    state << state_;

    return (*history)->setConfigTreeItem(sectionstr, STATE_KEY, state.str())
           && (*history)->setConfigTreeItem(sectionstr, TIMESTAMP_START_KEY, timestampStart_)
           && (*history)->setConfigTreeItem(sectionstr, TIMESTAMP_STOP_KEY, timestampStop_)
           && (*history)->setConfigTreeItem(sectionstr, NUMBER_KEY, number_)
           && (*history)->setConfigTreeItem(sectionstr, CALLID_KEY, callID_)
           && (*history)->setConfigTreeItem(sectionstr, ACCOUNT_ID_KEY, accountID_)
           && (*history)->setConfigTreeItem(sectionstr, NAME_KEY, name_)
           && (*history)->setConfigTreeItem(sectionstr, RECORDING_PATH_KEY, recordingPath_)
           && (*history)->setConfigTreeItem(sectionstr, CONFID_KEY, confID_)
           && (*history)->setConfigTreeItem(sectionstr, TIME_ADDED_KEY, timeAdded_);
}

std::string HistoryItem::serialize() const
{
    // Replace empty string with a valid standard string value
    std::string name(name_);

    if (name.empty())
        name = "empty";

    // For the account ID, check also if the accountID corresponds to an existing account
    // ie the account may have been removed
    std::string accountID(accountID_);

    if (accountID_.empty() or not valid_account(accountID_))
        accountID = "empty";

    std::stringstream res;
    // Serialize it
    res << state_ << ITEM_SEPARATOR << number_ << ITEM_SEPARATOR << name << ITEM_SEPARATOR << timestampStart_ << ITEM_SEPARATOR << timestampStop_
        << ITEM_SEPARATOR << callID_ << ITEM_SEPARATOR << accountID << ITEM_SEPARATOR << recordingPath_ << ITEM_SEPARATOR << confID_ << ITEM_SEPARATOR << timeAdded_;

    return res.str();
}

std::map<std::string, std::string> HistoryItem::toMap() const
{
    std::map<std::string, std::string> result;

    // Replace empty string with a valid standard string value
    if (name_.empty())
        result[NAME_KEY] = "empty";
    else
        result[NAME_KEY] = name_;

    // For the account ID, check also if the accountID corresponds to an existing account
    // ie the account may have been removed
    std::string accountID(accountID_);

    if (accountID_.empty() or not valid_account(accountID_))
        result[ACCOUNT_ID_KEY] = "empty";
    else
        result[ACCOUNT_ID_KEY] = accountID_;

    result[STATE_KEY] = state_;
    result[NUMBER_KEY] = number_;
    result[TIMESTAMP_START_KEY] = timestampStart_;
    result[TIMESTAMP_STOP_KEY] = timestampStop_;
    result[CALLID_KEY] = callID_;
    result[RECORDING_PATH_KEY] = recordingPath_;
    result[CONFID_KEY] = confID_;
    result[TIME_ADDED_KEY] = timeAdded_;

    return result;
}


bool HistoryItem::valid_account(const std::string &id) const
{
    return Manager::instance().accountExists(id);
}
