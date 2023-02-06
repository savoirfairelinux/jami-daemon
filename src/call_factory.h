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

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <utility>

#include "call.h"
#include "account.h"

namespace jami {

class SIPAccountBase;
class SIPCall;

class CallFactory
{
public:
    CallFactory(std::mt19937_64& rand)
        : rand_(rand)
    {}

    std::string getNewCallID() const;

    /**
     * Create a new call instance.
     * @param account Account used to create this call
     * @param type Set definitely this call as incoming/outgoing
     * @param mediaList The list of media to include
     * @return A shared pointer to the created call
     */
    std::shared_ptr<SIPCall> newSipCall(const std::shared_ptr<SIPAccountBase>& account,
                                        Call::CallType type,
                                        const std::vector<libjami::MediaMap>& mediaList);

    /**
     * Forbid creation of new calls.
     */
    void forbid();

    /**
     * Remove given call instance from call list.
     */
    void removeCall(Call& call);

    /**
     * Accessor on removeCall with callID than instance.
     */
    void removeCall(const std::string& id);

    /**
     * Return call pointer associated to given ID.Type can optionally be specified.
     */
    std::shared_ptr<Call> getCall(const std::string& id) const;
    std::shared_ptr<Call> getCall(const std::string& id, Call::LinkType link) const;

    template<class C>
    std::shared_ptr<C> getCall(const std::string& id)
    {
        return std::dynamic_pointer_cast<C>(getCall(id, C::LINK_TYPE));
    }

    /**
     * Return if given call exists. Type can optionally be specified.
     */
    bool hasCall(const std::string& id) const;
    bool hasCall(const std::string& id, Call::LinkType link) const;

    /**
     * Return if calls exist. Type can optionally be specified.
     */
    bool empty() const;
    bool empty(Call::LinkType link) const;

    /**
     * Erase all calls.
     */
    void clear();

    /**
     * Return all calls. Type can optionally be specified.
     */
    std::vector<std::shared_ptr<Call>> getAllCalls() const;
    std::vector<std::shared_ptr<Call>> getAllCalls(Call::LinkType link) const;

    /**
     * Return all call's IDs. Type can optionally be specified.
     */
    std::vector<std::string> getCallIDs() const;
    std::vector<std::string> getCallIDs(Call::LinkType link) const;

    /**
     * Return number of calls. Type can optionally be specified.
     */
    std::size_t callCount() const;
    std::size_t callCount(Call::LinkType link) const;

private:
    /**
     * @brief Get the calls map
     * @param link The call type
     * @return A pointer to the calls map instance
     * @warning Concurrency protection must done by the caller.
     */
    const CallMap* getMap_(Call::LinkType link) const
    {
        auto const& itermap = callMaps_.find(link);
        if (itermap != callMaps_.cend())
            return &itermap->second;
        return nullptr;
    }

    std::mt19937_64& rand_;

    mutable std::recursive_mutex callMapsMutex_ {};

    std::atomic_bool allowNewCall_ {true};

    std::map<Call::LinkType, CallMap> callMaps_ {};
};

} // namespace jami
