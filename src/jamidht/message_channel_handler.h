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

#include "jamidht/channel_handler.h"
#include "jamidht/jamiaccount.h"
#include <dhtnet/connectionmanager.h>

namespace jami {

/**
 * Manages channels for exchanging messages between peers
 */
class MessageChannelHandler : public ChannelHandlerInterface
{
public:
    using OnMessage = std::function<void(const std::shared_ptr<dht::crypto::Certificate>&, std::string&, const std::string&)>;
    using OnPeerStateChanged = std::function<void(const std::string&, bool)>;
    MessageChannelHandler(dhtnet::ConnectionManager& cm, OnMessage onMessage, OnPeerStateChanged onPeer);
    ~MessageChannelHandler();

    /**
     * Ask for a new message channel
     * @param deviceId      The device to connect
     * @param name          (Unused, generated from deviceId)
     * @param cb            The callback to call when connected (can be immediate if already connected)
     * @param connectionType for iOS notifications
     * @param forceNewConnection If we want a new SIP connection
     */
    void connect(const DeviceId& deviceId,
                 const std::string&,
                 ConnectCb&& cb,
                 const std::string& connectionType,
                 bool forceNewConnection = false) override;

    std::shared_ptr<dhtnet::ChannelSocket> getChannel(const std::string& peer,
                                                      const DeviceId& deviceId) const;
    std::vector<std::shared_ptr<dhtnet::ChannelSocket>> getChannels(const std::string& peer) const;

    /**
     * Determine if we accept or not the message request
     * @param deviceId      Device who asked
     * @param name          Name asked
     * @return if the channel is for a valid conversation and device not banned
     */
    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer,
                   const std::string& name) override;

    /**
     * Launch message process
     * @param deviceId      Device who asked
     * @param name          Name asked
     * @param channel       Channel used to message
     */
    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<dhtnet::ChannelSocket> channel) override;

    struct Message
    {
        uint64_t id {0};                          /* Message ID */
        std::string t;                            /* Message type */
        std::string c;                            /* Message content */
        std::unique_ptr<ConversationRequest> req; /* Conversation request */
        MSGPACK_DEFINE_MAP(id, t, c, req)
    };

    static bool sendMessage(const std::shared_ptr<dhtnet::ChannelSocket>&, const Message& message);

private:
    struct Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami
