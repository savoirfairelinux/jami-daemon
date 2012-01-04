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

#ifndef _HISTORY_MANAGER
#define _HISTORY_MANAGER

#include "historyitem.h"
#include "global.h"

class HistoryManager {

    public:
        /*
         * Constructor
         */
        HistoryManager();

        /**
         *@param path  A specific file to use; if empty, use the global one
         *
         *@return int The number of history items successfully loaded
         */
        int loadHistory(int limit, const std::string &path="");

        /**
         *@return bool True if the history has been successfully saved in the file
         */
        bool saveHistory();

        /*
         * Load the history from a file to the dedicated data structure
         */
        bool loadHistoryFromFile(Conf::ConfigTree &history_list);

        /*
         * @return int The number of history items loaded
         */
        int loadHistoryItemsMap(Conf::ConfigTree &history_list, int limit);

        /*
         * Inverse method, ie save a data structure containing the history into a file
         */
        bool saveHistoryToFile(const Conf::ConfigTree &history_list) const;

        void saveHistoryItemsVector(Conf::ConfigTree &history_list) const;

        /**
         *@return bool  True if the history file has been successfully read
         */
        bool isLoaded() const {
            return history_loaded_;
        }

        void setHistoryPath(const std::string &filename) {
            history_path_ = filename;
        }

        /*
         *@return int   The number of items found in the history file
         */
        size_t numberOfItems() const {
            return history_items_.size();
        }

        bool empty() const {
            return history_items_.empty();
        }

        std::vector<std::string> getHistorySerialized() const;
        std::vector<std::map<std::string, std::string> > getSerialized() const;

        int setHistorySerialized(const std::vector<std::map<std::string, std::string> > &history, int limit);

    private:
        /*
         * Set the path to the history file
         *
         * @param path  A specific file to use; if empty, use the global one
         */
        void createHistoryPath(const std::string &path="");
        /*
         * Add a new history item in the data structure
         */
        void addNewHistoryEntry(const HistoryItem &new_item);

        /*
         * Vector containing the history items
         */
        std::vector<HistoryItem> history_items_;

        /*
         * History has been loaded
         */
        bool history_loaded_;

        /*
         * The path to the history file
         */

        std::string history_path_;

        friend class HistoryTest;
};

#endif //_HISTORY_MANAGER
