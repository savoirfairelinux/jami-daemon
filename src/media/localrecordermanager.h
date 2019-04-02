/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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
 */

#pragma once

#include <memory>
#include <localrecorder.h>

namespace jami {

class LocalRecorderManager {
    public:
        static LocalRecorderManager &instance();

        /**
         * Remove given local recorder instance from the map.
         */
        void removeRecorderByPath(const std::string& path);

        /**
         * Insert passed local recorder into the map. Path is used as key.
         */
        void insertRecorder(const std::string& path, std::unique_ptr<LocalRecorder> rec);

        /**
         * Get local recorder instance with passed path as key.
         */
        LocalRecorder *getRecorderByPath(const std::string& path);

        /**
         * Return true if the manager owns at least one currently running
         * local recorder.
         */
        bool hasRunningRecorders();

    private:
        std::map<std::string, std::unique_ptr<LocalRecorder>> recorderMap_;
        std::mutex recorderMapMutex_;
};

} // namespace jami
