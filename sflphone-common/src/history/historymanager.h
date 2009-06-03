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

#ifndef _HISTORY_MANAGER
#define _HISTORY_MANAGER

#include "historyitem.h" 
#include <config/config.h>
#include <global.h>
#include <user_cfg.h>

typedef std::map <int, HistoryItem*> HistoryItemMap; 

class HistoryManager {

    public:
        /*
         * Constructor
         */
        HistoryManager ();

        /*
         * Destructor
         */
        ~HistoryManager ();

        bool init (void);

        /*
         * Load the history from a file to the dedicated data structure
         */
        bool load_history_from_file (void);

        /*
         * @return int The number of history items loaded
         */
        int load_history_items_map (void);

        /*
         * Inverse method, ie save a data structure containing the history into a file
         */
        int save_history_to_file (void);

        inline bool is_loaded (void) {
            return _history_loaded;
        }

        inline void set_history_path (std::string filename) {
            _history_path = filename;
        }
    
        inline int get_history_size (void) {
            return _history_items.size ();
        }

    private:

        
        int getConfigInt(const std::string& section, const std::string& name);
        std::string getConfigString(const std::string& section, const std::string& name);

        /*
         * Set the path to the history file
         */
        int create_history_path (void);
        /*
         * Add a new history item in the data structure
         */
        void add_new_history_entry (HistoryItem *new_item);

        /*
         * Map containing the history items
         */
        HistoryItemMap _history_items;

        /*
         * The path to the history file
         */ 

        std::string _history_path;

        /*
         * History has been loaded
         */
        bool _history_loaded;

        /* 
         * The history tree. It contains the call history 
         */
        Conf::ConfigTree _history_list;

        friend class HistoryTest;
};

#endif //_HISTORY_MANAGER
