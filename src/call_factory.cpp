/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "call_factory.h"

#include <stdexcept>

namespace jami {

void
CallFactory::forbid()
{
    allowNewCall_ = false;
}

void
CallFactory::removeCall(Call& call)
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    const auto& id = call.getCallId();
    JAMI_DBG("Removing call %s", id.c_str());
    auto& linkType = call.getLinkType();
    auto& map = callMaps_.at(linkType);
    map.erase(id);
    JAMI_DBG("Remaining %zu %s call(s)", map.size(), linkType.c_str());
}

void
CallFactory::removeCall(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    if (auto call = getCall(id)) {
        removeCall(*call);
    } else
        JAMI_ERR("No call with ID %s", id.c_str());
}

bool
CallFactory::hasCall(const std::string& id) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        if (map.find(id) != map.cend())
            return true;
    }

    return false;
}

void
CallFactory::clear()
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    callMaps_.clear();
}

bool
CallFactory::empty() const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        if (!map.empty())
            return false;
    }

    return true;
}

std::shared_ptr<Call>
CallFactory::getCall(const std::string& id) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        const auto& iter = map.find(id);
        if (iter != map.cend())
            return iter->second;
    }

    return nullptr;
}

std::vector<std::shared_ptr<Call>>
CallFactory::getAllCalls() const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::vector<std::shared_ptr<Call>> v;

    for (const auto& itemmap : callMaps_) {
        const auto& map = itemmap.second;
        v.reserve(v.size() + map.size());
        for (const auto& item : map)
            v.push_back(item.second);
    }

    return v;
}

std::vector<std::string>
CallFactory::getCallIDs() const
{
    std::vector<std::string> v;

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        for (const auto& it : map)
            v.push_back(it.first);
    }

    v.shrink_to_fit();
    return v;
}

std::size_t
CallFactory::callCount()
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::size_t count = 0;

    for (const auto& itemmap : callMaps_)
        count += itemmap.second.size();

    return count;
}

} // namespace jami
