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

#include "historymanager.h"
#include <cerrno>
#include <cc++/file.h>
#include <ctime>
#include "config/config.h"

namespace {
    int get_unix_timestamp_equivalent(int days) {
        // Number of seconds in one day: 60 x 60 x 24
        static const int DAY_UNIX_TIMESTAMP = 86400;
        return days * DAY_UNIX_TIMESTAMP;
    }
}

HistoryManager::HistoryManager() :
    history_items_(), history_items_simple_(), history_loaded_(false), history_path_("")
{}

int HistoryManager::load_history(int limit, const std::string &path)
{
    Conf::ConfigTree history_list;
    create_history_path(path);
    load_history_from_file(&history_list);
    return load_history_items_map(&history_list, limit);
}

bool HistoryManager::save_history()
{
    Conf::ConfigTree history_list;
    save_history_items_vector(&history_list);
    return save_history_to_file(&history_list);
}

bool HistoryManager::load_history_from_file(Conf::ConfigTree *history_list)
{
    DEBUG("HistoryManager: Load history from file %s", history_path_.c_str());

    int exist = history_list->populateFromFile(history_path_.c_str());
    history_loaded_ = (exist == 2) ? false : true;

    return history_loaded_;
}

int HistoryManager::load_history_items_map(Conf::ConfigTree *history_list, int limit)
{
    using std::string;

    // We want to save only the items recent enough (ie compared to CONFIG_HISTORY_LIMIT)
    // Get the current timestamp
    time_t current_timestamp;
    time(&current_timestamp);
    int history_limit = get_unix_timestamp_equivalent(limit);

    Conf::TokenList sections(history_list->getSections());
    int nb_items = 0;
    for (Conf::TokenList::iterator iter = sections.begin(); iter != sections.end(); ++iter) {
        HistoryState state = static_cast<HistoryState>(getConfigInt(*iter, HistoryItem::STATE_KEY, history_list));
        string timestamp_start(getConfigString(*iter, HistoryItem::TIMESTAMP_START_KEY, history_list));
        string timestamp_stop(getConfigString(*iter, HistoryItem::TIMESTAMP_STOP_KEY, history_list));
        string name(getConfigString(*iter, HistoryItem::NAME_KEY, history_list));
        string number(getConfigString(*iter, HistoryItem::NUMBER_KEY, history_list));
        string callID(getConfigString(*iter, HistoryItem::CALLID_KEY, history_list));
        string accountID(getConfigString(*iter, HistoryItem::ACCOUNT_ID_KEY, history_list));
        string recording_file(getConfigString(*iter, HistoryItem::RECORDING_PATH_KEY, history_list));
        string confID(getConfigString(*iter, HistoryItem::CONFID_KEY, history_list));
        string timeAdded(getConfigString(*iter, HistoryItem::TIME_ADDED_KEY, history_list));

        // Make a check on the start timestamp to know it is loadable according to CONFIG_HISTORY_LIMIT
        if (atoi(timestamp_start.c_str()) >= ((int) current_timestamp - history_limit)) {
            HistoryItem item(timestamp_start, state, timestamp_stop, name, number, callID, accountID, recording_file, confID, timeAdded);
            add_new_history_entry(item);
            ++nb_items;
        }
    }

    return nb_items;
}


bool HistoryManager::save_history_to_file(Conf::ConfigTree *history_list)
{
    DEBUG("HistoryManager: Saving history in XDG directory: %s", history_path_.c_str());
    return history_list->saveConfigTree(history_path_.data());
}

void HistoryManager::save_history_items_vector(Conf::ConfigTree *history_list)
{
    for (std::vector<HistoryItem>::iterator iter = history_items_.begin(); iter != history_items_.end(); ++iter)
        if (not iter->save(&history_list))
            DEBUG("can't save NULL history item\n");
}

void HistoryManager::add_new_history_entry(const HistoryItem &new_item)
{
    history_items_.push_back(new_item);
    history_items_simple_[rand()] = new_item;
}

void HistoryManager::create_history_path(const std::string &path)
{
    std::string xdg_data = std::string(HOMEDIR) + DIR_SEPARATOR_STR + ".local/share/sflphone";

    if (path.empty()) {
        std::string userdata;
        // If the environment variable is set (not null and not empty), we'll use it to save the history
        // Else we 'll the standard one, ie: XDG_DATA_HOME = $HOMEDIR/.local/share/sflphone
        if (XDG_DATA_HOME != NULL) {
            std::string xdg_env(XDG_DATA_HOME);
            (xdg_env.length() > 0) ? userdata = xdg_env : userdata = xdg_data;
        } else
            userdata = xdg_data;

        if (mkdir(userdata.data(), 0755) != 0) {
            // If directory	creation failed
            if (errno != EEXIST) {
                DEBUG("HistoryManager: Cannot create directory: %m");
                return;
            }
        }
        // Load user's history
        set_history_path(userdata + DIR_SEPARATOR_STR + "history");
    } else
        set_history_path(path);
}


int
HistoryManager::getConfigInt(const std::string& section, const std::string& name, Conf::ConfigTree *history_list)
{
    return history_list->getConfigTreeItemIntValue(section, name);
}

std::string
HistoryManager::getConfigString(const std::string& section, const std::string& name, Conf::ConfigTree *history_list)
{
    return history_list->getConfigTreeItemValue(section, name);
}

std::vector<std::string> HistoryManager::get_history_serialized() const
{
    std::vector<std::string> serialized;
    for (std::vector<HistoryItem>::const_iterator iter = history_items_.begin(); iter != history_items_.end(); ++iter)
        serialized.push_back(iter->serialize());

    return serialized;
}

// Convert our map of HistoryItems to a map<int, map<string, string> >
std::map<int, std::map<std::string, std::string> > HistoryManager::get_history_simple() const
{
    using std::map;
    using std::string;
    map<int, map<string, string> > result;
    for (map<int, HistoryItem>::const_iterator iter = history_items_simple_.begin(); iter != history_items_simple_.end(); ++iter)
        result[iter->first] = iter->second.toMap();

    return result;
}

int HistoryManager::set_serialized_history(const std::vector<std::string> &history, int limit)
{
    history_items_.clear();

    // We want to save only the items recent enough (ie compared to CONFIG_HISTORY_LIMIT)
    // Get the current timestamp
    time_t current_timestamp;
    time(&current_timestamp);
    int history_limit = get_unix_timestamp_equivalent(limit);

    int items_added = 0;
    for (std::vector<std::string>::const_iterator iter = history.begin(); iter != history.end(); ++iter) {
        HistoryItem new_item(*iter);
        int item_timestamp = atoi(new_item.get_timestamp().c_str());

        if (item_timestamp >= ((int) current_timestamp - history_limit)) {
            add_new_history_entry(new_item);
            ++items_added;
        }
    }

    return items_added;
}

