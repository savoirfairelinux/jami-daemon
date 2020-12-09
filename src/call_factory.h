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

class SIPCall;

class CallFactory
{
public:
    /**
     * Create a new call instance.
     * @param id Unique identifier of the call
     * @param type Set definitely this call as incoming/outgoing
     * @param account Account used to create this call
     * @param details Call details
     */
    std::shared_ptr<Call> CreateCall(const std::shared_ptr<Account>& account,
                                     const std::string& id,
                                     Call::CallType type,
                                     const std::map<std::string, std::string>& details = {});

    std::shared_ptr<Call> CreateCall(const std::shared_ptr<Account>& account,
                                     const std::string& id,
                                     Call::CallType type,
                                     const std::vector<MediaAttribute>& mediaList);

    template<class A>
    std::shared_ptr<Call> newCall(std::shared_ptr<A> account,
                                  const std::string& id,
                                  Call::CallType type,
                                  const std::map<std::string, std::string>& details = {})
    {
        return CreateCall(account, id, type, details);
    }

    template<class A>
    std::shared_ptr<Call> newCall(std::shared_ptr<A> account,
                                  const std::string& id,
                                  Call::CallType type,
                                  const std::vector<MediaAttribute>& mediaList)
    {
        return CreateCall(account, id, type, mediaList);
    }

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

    bool hasCall(const std::string& id) const;

    void clear();

    bool empty() const;

    std::shared_ptr<Call> getCall(const std::string& id) const;

    std::vector<std::shared_ptr<Call>> getAllCalls() const;

    std::vector<std::string> getCallIDs() const;

    std::size_t callCount();

private:
    mutable std::recursive_mutex callMapsMutex_ {};

    std::atomic_bool allowNewCall_ {true};

    std::map<Call::LinkType, CallMap> callMaps_ {};
};

} // namespace jami
