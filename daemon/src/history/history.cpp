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

#include "history.h"
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
    
    using std::map;
    using std::string;
    using std::list;
    using std::vector;
}

History::History() :
    items_(), loaded_(false), path_("")
{}

void History::load(int limit, const string &path)
{
    Conf::ConfigTree historyList;
    createPath(path);
    loadFromFile(historyList);
    loadItems(historyList, limit);
}

bool History::save()
{
    Conf::ConfigTree historyList;
    saveItems(historyList);
    return saveToFile(historyList);
}

bool History::loadFromFile(Conf::ConfigTree &historyList)
{
    loaded_ = historyList.populateFromFile(path_.c_str());
    return loaded_;
}

void History::loadItems(Conf::ConfigTree &historyList, int limit)
{
    // We want to save only the items recent enough (ie compared to CONFIG_HISTORY_LIMIT)
    time_t currentTimestamp;
    time(&currentTimestamp);
    int historyLimit = get_unix_timestamp_equivalent(limit);
    int oldestEntryTime =  static_cast<int>(currentTimestamp) - historyLimit;

    list<string> sections(historyList.getSections());
    for (list<string>::const_iterator iter = sections.begin(); iter != sections.end(); ++iter) {
        HistoryItem item(*iter, historyList);
        // Make a check on the start timestamp to know it is loadable according to CONFIG_HISTORY_LIMIT
        if (item.youngerThan(oldestEntryTime))
            addNewEntry(item);
    }
}

bool History::saveToFile(const Conf::ConfigTree &historyList) const
{
    DEBUG("History: Saving history in XDG directory: %s", path_.c_str());
    return historyList.saveConfigTree(path_.data());
}

void History::saveItems(Conf::ConfigTree &historyList) const
{
    for (vector<HistoryItem>::const_iterator iter = items_.begin(); iter != items_.end(); ++iter)
        iter->save(historyList);
}

void History::addNewEntry(const HistoryItem &new_item)
{
    items_.push_back(new_item);
}

void History::createPath(const string &path)
{
    string xdg_data = string(HOMEDIR) + DIR_SEPARATOR_STR + ".local/share/sflphone";

    if (path.empty()) {
        string userdata;
        // If the environment variable is set (not null and not empty), we'll use it to save the history
        // Else we 'll the standard one, ie: XDG_DATA_HOME = $HOMEDIR/.local/share/sflphone
        if (XDG_DATA_HOME != NULL) {
            string xdg_env(XDG_DATA_HOME);
            (!xdg_env.empty()) ? userdata = xdg_env : userdata = xdg_data;
        } else
            userdata = xdg_data;

        if (mkdir(userdata.data(), 0755) != 0) {
            // If directory	creation failed
            if (errno != EEXIST) {
                DEBUG("History: Cannot create directory: %m");
                return;
            }
        }
        // Load user's history
        setPath(userdata + DIR_SEPARATOR_STR + "history");
    } else
        setPath(path);
}


vector<map<string, string> > History::getSerialized() const
{
    vector<map<string, string> > result;
    for (vector<HistoryItem>::const_iterator iter = items_.begin();
         iter != items_.end(); ++iter)
        result.push_back(iter->toMap());

    return result;
}

void History::setSerialized(const vector<map<string, string> > &history, int limit)
{
    items_.clear();

    // We want to save only the items recent enough (ie compared to CONFIG_HISTORY_LIMIT)
    // Get the current timestamp
    time_t current_timestamp;
    time(&current_timestamp);
    int history_limit = get_unix_timestamp_equivalent(limit);

    for (vector<map<string, string> >::const_iterator iter = history.begin(); iter != history.end(); ++iter) {
        HistoryItem new_item(*iter);

        if (new_item.hasPeerNumber() and new_item.youngerThan((int) current_timestamp - history_limit))
            addNewEntry(new_item);
    }
}

