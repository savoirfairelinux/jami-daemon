/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "noncopyable.h"

#include <memory>
#include <string>
#include <map>
#include <cstdint>

namespace ring {

class SIPAccountBase;

namespace im {

using MessageToken = uint64_t;

enum class MessageStatus {
    UNKNOWN = 0,
    CREATED,
    SENDING,
    SENT,
    READ,
    FAILURE
};

class MessageEngine
{
public:
    MessageEngine(SIPAccountBase& account);
    ~MessageEngine();

    MessageToken sendMessage(const std::string& to, const std::map<std::string, std::string>& payloads);

    MessageStatus getStatus(MessageToken token) const;

    inline bool isSent(MessageToken token) const {
        return getStatus(token) == MessageStatus::SENT;
    }

    /// Account im sending code must call this method when a message is known to be carried to its recipient.
    void onMessageSent(MessageToken token, bool success);

private:
    NON_COPYABLE(MessageEngine);

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}} // namespace ring::im
