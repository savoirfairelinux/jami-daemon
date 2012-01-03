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
    int get_unix_timestamp_equivalent(int days)
    {
        // Number of seconds in one day: 60 x 60 x 24
        static const int DAY_UNIX_TIMESTAMP = 86400;
        return days * DAY_UNIX_TIMESTAMP;
    }
}

HistoryManager::HistoryManager() :
    history_items_(), history_loaded_(false), history_path_("")
{}

int HistoryManager::loadHistory(int limit, const std::string &path)
{
    Conf::ConfigTree historyList;
    createHistoryPath(path);
    loadHistoryFromFile(historyList);
    return loadHistoryItemsMap(historyList, limit);
}

bool HistoryManager::saveHistory()
{
    Conf::ConfigTree historyList;
    saveHistoryItemsVector(historyList);
    return saveHistoryToFile(historyList);
}

bool HistoryManager::loadHistoryFromFile(Conf::ConfigTree &historyList)
{
    int exist = historyList.populateFromFile(history_path_.c_str());
    history_loaded_ = (exist == 2) ? false : true;

    return history_loaded_;
}

int HistoryManager::loadHistoryItemsMap(Conf::ConfigTree &historyList, int limit)
{
    using std::string;

    // We want to save only the items recent enough (ie compared to CONFIG_HISTORY_LIMIT)
    time_t currentTimestamp;
    time(&currentTimestamp);
    int historyLimit = get_unix_timestamp_equivalent(limit);
    int oldestEntryTime =  static_cast<int>(currentTimestamp) - historyLimit;

    std::list<std::string> sections(historyList.getSections());
    int nb_items = 0;
    for (std::list<std::string>::const_iterator iter = sections.begin(); iter != sections.end(); ++iter) {
        HistoryItem item(*iter, historyList);
        // Make a check on the start timestamp to know it is loadable according to CONFIG_HISTORY_LIMIT
        if (item.youngerThan(oldestEntryTime)) {
            addNewHistoryEntry(item);
            ++nb_items;
        }
    }

    return nb_items;
}

bool HistoryManager::saveHistoryToFile(const Conf::ConfigTree &historyList) const
{
    DEBUG("HistoryManager: Saving history in XDG directory: %s", history_path_.c_str());
    return historyList.saveConfigTree(history_path_.data());
}

void HistoryManager::saveHistoryItemsVector(Conf::ConfigTree &historyList) const
{
    for (std::vector<HistoryItem>::const_iterator iter = history_items_.begin(); iter != history_items_.end(); ++iter)
        iter->save(historyList);
}

void HistoryManager::addNewHistoryEntry(const HistoryItem &new_item)
{
    history_items_.push_back(new_item);
}

void HistoryManager::createHistoryPath(const std::string &path)
{
    std::string xdg_data = std::string(HOMEDIR) + DIR_SEPARATOR_STR + ".local/share/sflphone";

    if (path.empty()) {
        std::string userdata;
        // If the environment variable is set (not null and not empty), we'll use it to save the history
        // Else we 'll the standard one, ie: XDG_DATA_HOME = $HOMEDIR/.local/share/sflphone
        if (XDG_DATA_HOME != NULL) {
            std::string xdg_env(XDG_DATA_HOME);
            (!xdg_env.empty()) ? userdata = xdg_env : userdata = xdg_data;
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
        setHistoryPath(userdata + DIR_SEPARATOR_STR + "history");
    } else
        setHistoryPath(path);
}


std::vector<std::map<std::string, std::string> > HistoryManager::getSerialized() const
{
    using std::map;
    using std::string;
    using std::vector;

    vector<map<string, string> > result;
    for (vector<HistoryItem>::const_iterator iter = history_items_.begin();
         iter != history_items_.end(); ++iter)
        result.push_back(iter->toMap());

    return result;
}

int HistoryManager::setHistorySerialized(const std::vector<std::map<std::string, std::string> > &history, int limit)
{
    history_items_.clear();
    using std::map;
    using std::string;
    using std::vector;

    // We want to save only the items recent enough (ie compared to CONFIG_HISTORY_LIMIT)
    // Get the current timestamp
    time_t current_timestamp;
    time(&current_timestamp);
    int history_limit = get_unix_timestamp_equivalent(limit);

    int items_added = 0;
    for (vector<map<string, string> >::const_iterator iter = history.begin(); iter != history.end(); ++iter) {
        HistoryItem new_item(*iter);
        int item_timestamp = atol(new_item.getTimestampStart().c_str());

        if (item_timestamp >= ((int) current_timestamp - history_limit)) {
            addNewHistoryEntry(new_item);
            ++items_added;
        }
    }

    return items_added;
}

