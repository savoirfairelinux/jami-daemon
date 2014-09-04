/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include <algorithm>
#include <fstream>
#include <sys/stat.h> // for mkdir
#include <ctime>
#include <cstring>
#include "fileutils.h"
#include "logger.h"
#include "call.h"

namespace sfl {

History::History() : historyItemsMutex_(), items_(), path_() {}


using std::map;
using std::string;
using std::vector;

bool History::load(int limit)
{
    // load only once
    if (!items_.empty())
        return true;

    ensurePath();
    std::ifstream infile(path_.c_str());
    if (!infile) {
        DEBUG("No history file to load");
        return false;
    }

    while (!infile.eof()) {
        HistoryItem item(infile);
        addEntry(item, limit);
    }

    return true;
}

bool History::save()
{
    std::lock_guard<std::mutex> lock(historyItemsMutex_);
    DEBUG("Saving history in XDG directory: %s", path_.c_str());
    ensurePath();
    std::sort(items_.begin(), items_.end());
    std::ofstream outfile(path_.c_str());
    if (outfile.fail())
        return false;
    for (const auto &item : items_)
        outfile << item << std::endl;
    return true;
}

void History::addEntry(const HistoryItem &item, int oldest)
{
    std::lock_guard<std::mutex> lock(historyItemsMutex_);
    if (item.hasPeerNumber() and item.youngerThan(oldest))
        items_.push_back(item);
}

void History::ensurePath()
{
    if (path_.empty()) {
#ifdef __ANDROID__
		path_ = fileutils::get_home_dir() + DIR_SEPARATOR_STR  + "history";
#else
        const string userdata = fileutils::get_data_dir();

        if (mkdir(userdata.data(), 0755) != 0) {
            // If directory	creation failed
            if (errno != EEXIST) {
                DEBUG("Cannot create directory: %s", userdata.c_str());
                strErr();
                return;
            }
        }
        // Load user's history
        path_ = userdata + DIR_SEPARATOR_STR + "history";
#endif
    }
}

vector<map<string, string> > History::getSerialized()
{
    std::lock_guard<std::mutex> lock(historyItemsMutex_);
    vector<map<string, string> > result;
    for (const auto &item : items_)
        result.push_back(item.toMap());

    return result;
}

void History::setPath(const std::string &path)
{
    path_ = path;
}

void History::addCall(Call *call, int limit)
{
    if (!call) {
        ERROR("Call is NULL, ignoring");
        return;
    }
    call->time_stop();
    HistoryItem item(call->createHistoryEntry());
    addEntry(item, limit);
}

void History::clear()
{
    std::lock_guard<std::mutex> lock(historyItemsMutex_);
    items_.clear();
}

bool History::empty()
{
    std::lock_guard<std::mutex> lock(historyItemsMutex_);
    return items_.empty();
}


size_t History::numberOfItems()
{
    std::lock_guard<std::mutex> lock(historyItemsMutex_);
    return items_.size();
}

}
