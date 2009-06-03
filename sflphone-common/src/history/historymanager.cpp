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
    // TODO
}

bool HistoryManager::init (void)
{
    create_history_path ();
    load_history_from_file ();
}

bool HistoryManager::load_history_from_file (void)
{
    bool exist;
    
    exist = _history_list.populateFromFile (_history_path);
    _history_loaded = (exist == 2 ) ? false : true;

    return exist;
}

int HistoryManager::load_history_items_map (void)
{

    short nb_items = 0;
    Conf::TokenList sections; 
    HistoryItem *item;
    Conf::TokenList::iterator iter;
    std::string to, from, accountID;
    int timestamp;
    CallType type; 
 
    sections = _history_list.getSections();
    iter = sections.begin();

    while(iter != sections.end()) {

        type = (CallType) getConfigInt (*iter, "type");
        timestamp = getConfigInt (*iter, "timestamp");
        to = getConfigString (*iter, "to");
        from = getConfigString (*iter, "from");

        item = new HistoryItem (timestamp, type, to, from);
        add_new_history_entry (item);
        nb_items ++;

        iter ++;

    }

    return nb_items;
}

int HistoryManager::save_history_to_file (void)
{
    // TODO
    return 0;
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
HistoryManager::getConfigInt(const std::string& section, const std::string& name)
{
  try {
    return _history_list.getConfigTreeItemIntValue(section, name);
  } catch (Conf::ConfigTreeItemException& e) {
    throw e;
  }
  return 0;
}

std::string
HistoryManager::getConfigString(const std::string& section, const std::string&
    name)
{
  try {
    return _history_list.getConfigTreeItemValue(section, name);
  } catch (Conf::ConfigTreeItemException& e) {
    throw e;
  }
  return "";
}

