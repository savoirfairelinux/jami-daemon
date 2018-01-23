/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include <string>
#include <map>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace ring {

class SIPAccountBase;

namespace im {

using MessageToken = uint64_t;

enum class MessageStatus {
    UNKNOWN = 0,
    IDLE,
    SENDING,
    SENT,
    READ,
    FAILURE
};

class MessageEngine
{
public:

    MessageEngine(SIPAccountBase&, const std::string& path);

    MessageToken sendMessage(const std::string& to, const std::map<std::string, std::string>& payloads);

    MessageStatus getStatus(MessageToken t) const;

    bool isSent(MessageToken t) const {
        return getStatus(t) == MessageStatus::SENT;
    }

    void onMessageSent(MessageToken t, bool success);

    /**
     * Load persisted messages
     */
    void load();

    /**
     * Persist messages
     */
    void save() const;

private:

    static const constexpr unsigned MAX_RETRIES = 3;
    static const std::chrono::minutes RETRY_PERIOD;
    using clock = std::chrono::steady_clock;

    clock::time_point nextEvent() const;
    void retrySend();
    void reschedule();

    struct Message {
        std::string to;
        std::map<std::string, std::string> payloads;
        MessageStatus status {MessageStatus::UNKNOWN};
        unsigned retried {0};
        clock::time_point last_op;
    };

    SIPAccountBase& account_;
    const std::string savePath_;

    std::map<MessageToken, Message> messages_;
    mutable std::mutex messagesMutex_ {};
};

}} // namespace ring::im
