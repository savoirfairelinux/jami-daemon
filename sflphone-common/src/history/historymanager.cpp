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

HistoryManager::HistoryManager () : _history_loaded (false), _history_path (""){

}

HistoryManager::~HistoryManager () {

    // Clear the history map
    _history_items.clear ();
}

bool HistoryManager::init (void)
{
    Conf::ConfigTree history_list;

    create_history_path ();
    load_history_from_file (&history_list);
    load_history_items_map (&history_list);
}

bool HistoryManager::load_history_from_file (Conf::ConfigTree *history_list)
{
    bool exist;

    exist = history_list->populateFromFile (_history_path);
    _history_loaded = (exist == 2 ) ? false : true;

    return exist;
}

int HistoryManager::load_history_items_map (Conf::ConfigTree *history_list)
{

    short nb_items = 0;
    Conf::TokenList sections; 
    HistoryItem *item;
    Conf::TokenList::iterator iter;
    std::string to, from, caller_id, accountID;
    int timestamp;
    CallType type; 

    sections = history_list->getSections();
    iter = sections.begin();

    while(iter != sections.end()) {

        type = (CallType) getConfigInt (*iter, "type", history_list);
        timestamp = getConfigInt (*iter, "timestamp", history_list);
        to = getConfigString (*iter, "to", history_list);
        from = getConfigString (*iter, "from", history_list);
        caller_id = getConfigString (*iter, "id", history_list);

        item = new HistoryItem (timestamp, type, to, from, caller_id);
        add_new_history_entry (item);
        nb_items ++;

        iter ++;

    }

    return nb_items;
}

bool HistoryManager::save_history_to_file (Conf::ConfigTree *history_list)
{
    return  history_list->saveConfigTree(_history_path.data());
}


int HistoryManager::save_history_items_map (Conf::ConfigTree *history_list)
{
    HistoryItemMap::iterator iter;
    HistoryItem *item;    
    int items_saved = 0;

    iter = _history_items.begin ();

    while (iter != _history_items.end ())
    {
        item = iter->second;  
        if (item) {
            if (item->save (&history_list))
                items_saved ++;
        } else {
            std::cout << "[DEBUG]: can't save NULL history item." << std::endl;
        }
        iter ++;
    }

    return items_saved;
}

void HistoryManager::add_new_history_entry (HistoryItem *new_item)
{
    // Add it in the map
    _history_items [new_item->get_timestamp ()] = new_item;
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

// throw an Conf::ConfigTreeItemException if not found
    int
HistoryManager::getConfigInt(const std::string& section, const std::string& name, Conf::ConfigTree *history_list)
{
    try {
        return history_list->getConfigTreeItemIntValue(section, name);
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }
    return 0;
}

    std::string
HistoryManager::getConfigString(const std::string& section, const std::string& name, Conf::ConfigTree *history_list)
{
    try {
        return history_list->getConfigTreeItemValue(section, name);
    } catch (Conf::ConfigTreeItemException& e) {
        throw e;
    }
    return "";
}

