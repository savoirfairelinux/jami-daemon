/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
 *
 *  Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>
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

#include "jamidht/channel_handler.h"
#include "connectivity/connectionmanager.h"
#include "jamidht/conversation_channel_handler.h"

#include "logger.h"

namespace jami {

using NodeId = dht::h256;

class JamiAccount;

/**
 * Manages Conversation's channels
 */
class SwarmChannelHandler : public ChannelHandlerInterface
{
public:
    SwarmChannelHandler(const std::shared_ptr<JamiAccount>& acc, ConnectionManager& cm);
    ~SwarmChannelHandler();

    /**
     * Ask for a new git channel
     * @param nodeId      The node to connect
     * @param name          The name of the channel
     * @param cb            The callback to call when connected (can be immediate if already connected)
     */
    void connect(const NodeId& nodeId, const std::string& name, ConnectCb&& cb) override;

    /**
     * Determine if we accept or not the git request
     * @param nodeId      node who asked
     * @param name          name asked
     * @return if the channel is for a valid conversation and node not banned
     */
    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer,
                   const std::string& name) override;

    /**
     * TODO, this needs to extract gitservers from JamiAccount
     */
    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<ChannelSocket> channel) override;

private:
    std::weak_ptr<JamiAccount> account_;
    ConnectionManager& connectionManager_;
};

} // namespace jami