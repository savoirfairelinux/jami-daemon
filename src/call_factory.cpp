/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include <stdexcept>

#include "call_factory.h"
#include "sip/sipcall.h"
#include "sip/sipaccountbase.h"
#include "string_utils.h"

namespace jami {

// generate something like 7ea037947eb9fb2f
std::string
CallFactory::getNewCallID() const
{
    std::string random_id;
    do {
        random_id = std::to_string(
            std::uniform_int_distribution<uint64_t>(1, JAMI_ID_MAX_VAL)(rand_));
    } while (hasCall(random_id));
    return random_id;
}

std::shared_ptr<SIPCall>
CallFactory::newSipCall(const std::shared_ptr<SIPAccountBase>& account,
                        Call::CallType type,
                        const std::vector<libjami::MediaMap>& mediaList)
{
    if (not allowNewCall_) {
        JAMI_WARN("Creation of new calls is not allowed");
        return {};
    }

    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    auto id = getNewCallID();
    auto call = std::make_shared<SIPCall>(account, id, type, mediaList);
    callMaps_[call->getLinkType()].emplace(id, call);
    account->attach(call);
    return call;
}

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
    auto& map = callMaps_.at(call.getLinkType());
    map.erase(id);
    JAMI_DBG("Remaining %zu call", map.size());
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

bool
CallFactory::empty() const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    for (const auto& item : callMaps_) {
        if (not item.second.empty())
            return false;
    }

    return true;
}

void
CallFactory::clear()
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    callMaps_.clear();
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
CallFactory::callCount() const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::size_t count = 0;

    for (const auto& itemmap : callMaps_)
        count += itemmap.second.size();

    return count;
}

bool
CallFactory::hasCall(const std::string& id, Call::LinkType link) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    auto const map = getMap_(link);
    return map and map->find(id) != map->cend();
}

bool
CallFactory::empty(Call::LinkType link) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    const auto map = getMap_(link);
    return !map or map->empty();
}

std::shared_ptr<Call>
CallFactory::getCall(const std::string& id, Call::LinkType link) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    const auto map = getMap_(link);
    if (!map)
        return nullptr;

    const auto& it = map->find(id);
    if (it == map->cend())
        return nullptr;

    return it->second;
}

std::vector<std::shared_ptr<Call>>
CallFactory::getAllCalls(Call::LinkType link) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::vector<std::shared_ptr<Call>> v;

    const auto map = getMap_(link);
    if (map) {
        for (const auto& it : *map)
            v.push_back(it.second);
    }

    v.shrink_to_fit();
    return v;
}

std::vector<std::string>
CallFactory::getCallIDs(Call::LinkType link) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);
    std::vector<std::string> v;

    const auto map = getMap_(link);
    if (map) {
        for (const auto& it : *map)
            v.push_back(it.first);
    }

    v.shrink_to_fit();
    return v;
}

std::size_t
CallFactory::callCount(Call::LinkType link) const
{
    std::lock_guard<std::recursive_mutex> lk(callMapsMutex_);

    const auto map = getMap_(link);
    if (!map)
        return 0;

    return map->size();
}

} // namespace jami
