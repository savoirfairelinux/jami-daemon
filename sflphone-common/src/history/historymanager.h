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
#include <global.h>
#include <user_cfg.h>

#define DAY_UNIX_TIMESTAMP      86400   // Number of seconds in one day: 60 x 60 x 24

typedef std::map <std::string, HistoryItem*> HistoryItemMap; 

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

        /**
         *@param path  A specific file to use; if empty, use the global one
         *
         *@return int The number of history items succesfully loaded
         */
        int load_history (int limit, std::string path="");

        /**
         *@return bool True if the history has been successfully saved in the file
         */
        bool save_history (void);

        /*
         * Load the history from a file to the dedicated data structure
         */
        bool load_history_from_file (Conf::ConfigTree *history_list);

        /*
         * @return int The number of history items loaded
         */
        int load_history_items_map (Conf::ConfigTree *history_list, int limit);

        /*
         * Inverse method, ie save a data structure containing the history into a file
         */
        bool save_history_to_file (Conf::ConfigTree *history_list);

        /**
         * @return int The number of history entries successfully saved
         */
        int save_history_items_map (Conf::ConfigTree *history_list);

        /**
         *@return bool  True if the history file has been successfully read
         */
        inline bool is_loaded (void) {
            return _history_loaded;
        }

        inline void set_history_path (std::string filename) {
            _history_path = filename;
        }
    
        /*
         *@return int   The number of items found in the history file
         */
        inline int get_history_size (void) {
            return _history_items.size ();
        }

        std::map <std::string, std::string> get_history_serialized (void);

        int set_serialized_history (std::map <std::string, std::string> history, int limit);

    private:
        inline int get_unix_timestamp_equivalent (int days)
        {
            return days * DAY_UNIX_TIMESTAMP;
        }
        
        int getConfigInt(const std::string& section, const std::string& name, Conf::ConfigTree *history_list);
        std::string getConfigString(const std::string& section, const std::string& name, Conf::ConfigTree *history_list);

        /*
         * Set the path to the history file
         *
         * @param path  A specific file to use; if empty, use the global one
         */
        int create_history_path (std::string path="");
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

        friend class HistoryTest;
};

#endif //_HISTORY_MANAGER
