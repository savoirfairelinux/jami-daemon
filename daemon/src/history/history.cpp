/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
#include "scoped_lock.h"
#include "fileutils.h"
#include "logger.h"
#include "call.h"
#include <jni.h>

namespace sfl {


using std::map;
using std::string;
using std::vector;

History::History() : historyItemsMutex_(), items_(), path_("")
{
    pthread_mutex_init(&historyItemsMutex_, NULL);
}

History::~History()
{
    pthread_mutex_destroy(&historyItemsMutex_);
}

JNIEXPORT jstring JNICALL Java_com_savoirfairelinux_sflphone_client_ManagerImpl_getJniString(JNIEnv* env, jclass obj){

    jstring jstr = env->NewStringUTF("This comes from jni.");
    jclass clazz;
    jmethodID getSipLogLevel;

	DEBUG("Java_com_savoirfairelinux_sflphone_client_ManagerImpl_getJniString");
	/* could be GetObjectClass instead of FindClass */
	clazz = env->FindClass("com/savoirfairelinux/sflphone/client/ManagerImpl");
	if (!getSipLogLevel) {
        DEBUG("whoops, class does not exist");
		return NULL;
	}
	getSipLogLevel = env->GetMethodID(clazz, "getSipLogLevel", "()Ljava/lang/String;");
	if (!getSipLogLevel) {
        DEBUG("whoops, method does not exist");
		return NULL;
	}

    jobject result = env->CallObjectMethod(obj, getSipLogLevel);

	// should be released but what a heck, it's a tutorial :)
    const char* str = env->GetStringUTFChars((jstring) result, NULL);
	DEBUG("%s", str);

    return env->NewStringUTF(str);
}

bool History::load(int limit)
{
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
    sfl::ScopedLock lock(historyItemsMutex_);
    DEBUG("Saving history in XDG directory: %s", path_.c_str());
    ensurePath();
    std::sort(items_.begin(), items_.end());
    std::ofstream outfile(path_.c_str());
    if (outfile.fail())
        return false;
    for (vector<HistoryItem>::const_iterator iter = items_.begin();
         iter != items_.end(); ++iter)
        outfile << *iter << std::endl;
    return true;
}

void History::addEntry(const HistoryItem &item, int oldest)
{
    sfl::ScopedLock lock(historyItemsMutex_);
    if (item.hasPeerNumber() and item.youngerThan(oldest))
        items_.push_back(item);
}

void History::ensurePath()
{
    if (path_.empty()) {
        const string xdg_data = fileutils::get_home_dir() + DIR_SEPARATOR_STR +
                                ".local/share/sflphone";
        // If the environment variable is set (not null and not empty), we'll use it to save the history
        // Else we 'll the standard one, ie: XDG_DATA_HOME = $HOME/.local/share/sflphone
        string xdg_env(XDG_DATA_HOME);
        const string userdata = not xdg_env.empty() ? xdg_env : xdg_data;

        if (mkdir(userdata.data(), 0755) != 0) {
            // If directory	creation failed
            if (errno != EEXIST) {
                DEBUG("Cannot create directory: %s!", userdata.c_str());
                return;
            }
        }
        // Load user's history
        path_ = userdata + DIR_SEPARATOR_STR + "history";
        DEBUG("path_: %s!", path_.c_str());
    }
}

vector<map<string, string> > History::getSerialized()
{
    sfl::ScopedLock lock(historyItemsMutex_);
    vector<map<string, string> > result;
    for (vector<HistoryItem>::const_iterator iter = items_.begin();
         iter != items_.end(); ++iter)
        result.push_back(iter->toMap());

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
    sfl::ScopedLock lock(historyItemsMutex_);
    items_.clear();
}

bool History::empty()
{
    sfl::ScopedLock lock(historyItemsMutex_);
    return items_.empty();
}


size_t History::numberOfItems()
{
    sfl::ScopedLock lock(historyItemsMutex_);
    return items_.size();
}

}
