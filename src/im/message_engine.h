/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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
#pragma once

#include <string>
#include <map>
#include <set>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <filesystem>

#include <msgpack.hpp>
#include <asio/steady_timer.hpp>

namespace jami {

class SIPAccountBase;

namespace im {

using MessageToken = uint64_t;

enum class MessageStatus { UNKNOWN=0, IDLE, SENDING, SENT, FAILURE };

class MessageEngine
{
public:
    MessageEngine(SIPAccountBase&, const std::filesystem::path& path);

    /**
     * Add a message to the engine and try to send it
     * @param to            Uri of the peer
     * @param deviceId      (Optional) if we want to send to a specific device
     * @param payloads      The message
     * @param refreshToken  The token of the message
     */
    MessageToken sendMessage(const std::string& to,
                             const std::string& deviceId,
                             const std::map<std::string, std::string>& payloads,
                             uint64_t refreshToken);

    MessageStatus getStatus(MessageToken t) const;

    void onMessageSent(const std::string& peer,
                       MessageToken t,
                       bool success,
                       const std::string& deviceId = {});

    /**
     * @NOTE retryOnTimeout is used for failing SIP messages (jamiAccount::sendTextMessage)
     */
    void onPeerOnline(const std::string& peer,
                      const std::string& deviceId = {},
                      bool retryOnTimeout = true);

    /**
     * Load persisted messages
     */
    void load();

    /**
     * Persist messages
     */
    void save() const;

private:
    static const constexpr unsigned MAX_RETRIES = 20;
    using clock = std::chrono::system_clock;

    void retrySend(const std::string& peer,
                   const std::string& deviceId,
                   bool retryOnTimeout);

    void save_() const;
    void scheduleSave();

    struct Message
    {
        MessageToken token {};
        std::string to {};
        std::map<std::string, std::string> payloads {};
        MessageStatus status {MessageStatus::IDLE};
        unsigned retried {0};
        clock::time_point last_op {};

        MSGPACK_DEFINE_MAP(token, to, payloads, status, retried, last_op)
    };

    SIPAccountBase& account_;
    const std::filesystem::path savePath_;
    std::shared_ptr<asio::io_context> ioContext_;
    asio::steady_timer saveTimer_;

    std::map<std::string, std::list<Message>> messages_;
    std::map<std::string, std::list<Message>> messagesDevices_;

    mutable std::mutex messagesMutex_ {};
};

} // namespace im
} // namespace jami

MSGPACK_ADD_ENUM(jami::im::MessageStatus);
