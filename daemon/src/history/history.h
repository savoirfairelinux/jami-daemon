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

#ifndef HISTORY_
#define HISTORY_

#include "historyitem.h"
#include <mutex>
#include <vector>

class Call;

namespace sfl {

class History {

    public:
        History();
        /** Load history from file */
        bool load(int limit);

        /**
         *@return True if the history has been successfully saved in the file
         */
        bool save();

        /*
         *@return The number of items found in the history file
         */
        size_t numberOfItems();

        bool empty();

        std::vector<std::map<std::string, std::string> > getSerialized();

        void addCall(Call *call, int limit);
        void clear();
        void setPath(const std::string &path);
    private:
        /* Mutex to protect the history items */
        std::mutex historyItemsMutex_;

        /* If no path has been set, this will initialize path to a
         * system-dependent location */
        void ensurePath();
        /*
         * Add a new history item in the data structure
         */
        void addEntry(const HistoryItem &new_item, int limit);

        /*
         * Vector containing the history items
         */
        std::vector<HistoryItem> items_;

        /* The path to the history file */
        std::string path_;

        friend class HistoryTest;
};

}

#endif // HISTORY_
