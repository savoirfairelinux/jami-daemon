/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
 * Manages the "ydoc://" channels carrying a collaborative document's real-time
 * traffic -- Y-CRDT updates and awareness states -- between the devices that
 * currently have it open.
 *
 * A channel exists per (document, peer device) pair and only while both ends
 * are editing: a request is accepted when this device has the document open and
 * the peer is a member of the document per the local replica. Everything else
 * -- who may hold the document at all, its durable state -- is the document
 * swarm's business, not this channel's.
 */
class YdocChannelHandler : public ChannelHandlerInterface
{
public:
    YdocChannelHandler(const std::shared_ptr<JamiAccount>& acc, dhtnet::ConnectionManager& cm);
    ~YdocChannelHandler();

    /**
     * Ask for a new ydoc channel
     * @param deviceId      The device to connect
     * @param name          The document (repository) id
     * @param cb            The callback to call when connected (can be immediate if already connected)
     */
    void connect(const DeviceId& deviceId,
                 const std::string& name,
                 ConnectCb&& cb,
                 const std::string& connectionType = "",
                 bool forceNewConnection = false) override;

    /**
     * Determine if we accept or not the request. Accepted only when the
     * document is currently open here and the peer is one of its members.
     * @param peer          the requesting peer's certificate
     * @param name          "ydoc://<device>/<documentId>"
     */
    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer, const std::string& name) override;

    /**
     * Hand the channel to CollaborativeEditing, which owns it from here on.
     * Called for both directions: the requesting side and the accepting one.
     */
    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<dhtnet::ChannelSocket> channel) override;

private:
    std::weak_ptr<JamiAccount> account_;
    dhtnet::ConnectionManager& connectionManager_;
};

} // namespace jami
