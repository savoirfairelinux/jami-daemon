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

#ifndef HISTORY_
#define HISTORY_

#include "historyitem.h"
#include <vector>

class History {

    public:
        /*
         * Constructor
         */
        History();

        /**
         * Load history from file
         */
        void load(int limit);

        /**
         *@return bool True if the history has been successfully saved in the file
         */
        bool save();

        /**
         *@return bool  True if the history file has been successfully read
         */
        bool isLoaded() const {
            return loaded_;
        }

        /*
         *@return int   The number of items found in the history file
         */
        size_t numberOfItems() const {
            return items_.size();
        }

        bool empty() const {
            return items_.empty();
        }

        std::vector<std::map<std::string, std::string> > getSerialized() const;

        // FIXME:tmatth:get rid of this
        void setSerialized(const std::vector<std::map<std::string, std::string> > &history, int limit);

    private:
        void setPath(const std::string &path);
        void createPath(const std::string &path = "");
        /*
         * Add a new history item in the data structure
         */
        void addNewEntry(const HistoryItem &new_item, int limit);

        /*
         * Vector containing the history items
         */
        std::vector<HistoryItem> items_;

        /*
         * History has been loaded
         */
        bool loaded_;

        /*
         * The path to the history file
         */
        std::string path_;

        friend class HistoryTest;
};

#endif // HISTORY_
