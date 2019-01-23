/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

namespace ring {

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
    RING_DBG("Removing call %s", id.c_str());
    const auto& linkType = call.getLinkType();
    auto& map = callMaps_.at(linkType);
    map.erase(id);
    RING_DBG("Remaining %zu %s call(s)", map.size(), linkType);
}

void
CallFactory::removeCall(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    if (auto call = getCall(id)) {
        removeCall(*call);
    } else
        RING_ERR("No call with ID %s", id.c_str());
}

//==============================================================================
// Template specializations (when T = Call)

template <> bool
CallFactory::hasCall<Call>(const std::string& id) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        if (map.find(id) != map.cend())
            return true;
    }

    return false;
}

template <> void
CallFactory::clear<Call>()
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    callMaps_.clear();
}

template <> bool
CallFactory::empty<Call>() const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        if (!map.empty())
            return false;
    }

    return true;
}

template <> std::shared_ptr<Call>
CallFactory::getCall<Call>(const std::string& id) const
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

template <> std::vector<std::shared_ptr<Call> >
CallFactory::getAllCalls<Call>() const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::vector<std::shared_ptr<Call> > v;

    for (const auto& itemmap : callMaps_) {
        const auto& map = itemmap.second;
        for (const auto item : map)
            v.push_back(item.second);
    }

    std::cout << "There are " << v.size() << " remaining calls \n";

    v.shrink_to_fit();

    if (v.empty())
        RING_WARN(">>>> Vector empty !!! <<<< \n");

    return v;
}

template <> std::vector<std::string>
CallFactory::getCallIDs<Call>() const {
    std::vector<std::string> v;

    for (const auto& item : callMaps_) {
        const auto& map = item.second;
        for (const auto& it : map)
            v.push_back(it.first);
    }

    v.shrink_to_fit();
    return v;
}

template <> std::size_t
CallFactory::callCount<Call>()
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::size_t count = 0;

    for (const auto& itemmap : callMaps_)
        count += itemmap.second.size();

    return count;
}

} // namespace ring
