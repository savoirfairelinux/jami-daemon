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

#include <historymanager.h>
#include <errno.h>
#include <cc++/file.h>

HistoryManager::HistoryManager () {

    bool exist;

    // Load the path to the file
    if (create_history_path () ==1){
        exist = _history_config.populateFromFile (_history_path);
    }

    _history_loaded = (exist == 2 ) ? false : true;
}

HistoryManager::~HistoryManager () {
    // TODO
}

int HistoryManager::load_history_from_file (void)
{
    //  
    return 0;
}

int HistoryManager::save_history_to_file (void)
{
    // TODO
    return 0;
}

void HistoryManager::add_new_history_entry (HistoryItem new_item)
{
    // TODO
}

int HistoryManager::create_history_path (void) {
    
    std::string path;

    path = std::string(HOMEDIR) + DIR_SEPARATOR_STR + "." + PROGDIR;
    
    if (mkdir (path.data(), 0755) != 0) {
        // If directory	creation failed
        if (errno != EEXIST) {
            _debug("Cannot create directory: %s\n", strerror(errno));
            return -1;
        }
    }

    // Load user's history
    _history_path = path + DIR_SEPARATOR_STR + "history";
    return 0;
}
