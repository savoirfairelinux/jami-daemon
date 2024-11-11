/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "localrecordermanager.h"

namespace jami {

LocalRecorderManager&
LocalRecorderManager::instance()
{
    static LocalRecorderManager instance;
    return instance;
}

void
LocalRecorderManager::removeRecorderByPath(const std::string& path)
{
    std::lock_guard lock(recorderMapMutex_);
    recorderMap_.erase(path);
}

void
LocalRecorderManager::insertRecorder(const std::string& path, std::unique_ptr<LocalRecorder> rec)
{
    if (!rec) {
        throw std::invalid_argument("Unable to insert null recorder");
    }

    std::lock_guard lock(recorderMapMutex_);
    auto ret = recorderMap_.emplace(path, std::move(rec));

    if (!ret.second) {
        throw std::invalid_argument(
            "Unable to insert recorder (passed path is already used as key)");
    }
}

LocalRecorder*
LocalRecorderManager::getRecorderByPath(const std::string& path)
{
    auto rec = recorderMap_.find(path);
    return (rec == recorderMap_.end()) ? nullptr : rec->second.get();
}

bool
LocalRecorderManager::hasRunningRecorders()
{
    for (auto it = recorderMap_.begin(); it != recorderMap_.end(); ++it) {
        if (it->second->isRecording())
            return true;
    }

    return false;
}

} // namespace jami
