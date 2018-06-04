/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

#include "localrecordermanager.h"

namespace ring {

LocalRecorderManager&
LocalRecorderManager::instance()
{
    static LocalRecorderManager instance;
    return instance;
}

size_t
LocalRecorderManager::getNextId()
{
    return nextId_++;
}

void
LocalRecorderManager::removeRecorderById(size_t id)
{
    std::lock_guard<std::mutex> lock(recorderMapMutex_);
    recorderMap_.erase(id);
}

size_t
LocalRecorderManager::insertRecorder(std::unique_ptr<LocalRecorder> rec)
{
    if (!rec) {
        throw std::invalid_argument("can't insert NULL recorder pointer into local recorder manager");
    }

    size_t id = getNextId();
    std::lock_guard<std::mutex> lock(recorderMapMutex_);
    recorderMap_.insert(std::make_pair(id, std::move(rec)));
    return id;
}

LocalRecorder*
LocalRecorderManager::getRecorderById(size_t id)
{
    auto rec = recorderMap_.find(id);
    if (rec == recorderMap_.end())
        return nullptr;
    else
        return rec->second.get();
}

} // namespace ring
