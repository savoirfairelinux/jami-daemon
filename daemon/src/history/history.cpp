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
#include <algorithm>
#include <ctime>
#include "global.h"
#include "logger.h"

namespace {
    int oldestAllowed(int days)
    {
        time_t currentTimestamp;
        time(&currentTimestamp);
        // Number of seconds in one day: 60 sec/min x 60 min/hr x 24hr/day 
        static const int DAY_UNIX_TIMESTAMP = 60 * 60 * 24;
        return static_cast<int>(currentTimestamp) - (days * DAY_UNIX_TIMESTAMP);
    }
    
    using std::map;
    using std::string;
    using std::vector;
}

History::History() :
    items_(), loaded_(false), path_("")
{}

void History::load(int limit)
{
    ensurePath();
    std::ifstream infile(path_.c_str());
    if (!infile) {
        DEBUG("No history file to load");
        return;
    }
    while (!infile.eof()) {
        HistoryItem item(infile);
        addNewEntry(item, limit);
    }
    loaded_ = true;
}

bool History::save()
{
    DEBUG("History: Saving history in XDG directory: %s", path_.c_str());
    std::sort(items_.begin(), items_.end());
    std::ofstream outfile(path_.c_str());
    if (outfile.fail())
        return false;
    for (vector<HistoryItem>::const_iterator iter = items_.begin();
         iter != items_.end(); ++iter)
        outfile << *iter << std::endl;
    return true;
}

void History::addNewEntry(const HistoryItem &item, int oldest)
{
    if (item.hasPeerNumber() and item.youngerThan(oldest))
        items_.push_back(item);
}

void History::ensurePath()
{
    if (path_.empty()) {
        string xdg_data = string(HOMEDIR) + DIR_SEPARATOR_STR + ".local/share/sflphone";

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
        path_ = userdata + DIR_SEPARATOR_STR + "history";
    }
}


vector<map<string, string> > History::getSerialized() const
{
    vector<map<string, string> > result;
    for (vector<HistoryItem>::const_iterator iter = items_.begin();
         iter != items_.end(); ++iter)
        result.push_back(iter->toMap());

    return result;
}

void History::setSerialized(const vector<map<string, string> > &history,
                            int limit)
{
    items_.clear();
    const int oldest = oldestAllowed(limit);
    for (vector<map<string, string> >::const_iterator iter = history.begin();
         iter != history.end(); ++iter) {
        HistoryItem item(*iter);
        addNewEntry(item, oldest);
    }
}

void History::setPath(const std::string &path)
{
    path_ = path;
}
