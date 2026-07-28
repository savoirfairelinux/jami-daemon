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

#include "jamidht/jamiaccount.h"

namespace jami {

/**
 * Serves the git repository backing a collaborative document to the other
 * devices of the conversation that hosts it.
 *
 * Channel names are "collab://<targetDevice>/<conversationId>/<documentId>".
 * A dedicated scheme is used rather than "git://" because conversation URLs are
 * parsed as a two-part "<device>/<conversationId>", which leaves no room for a
 * document id.
 *
 * A document has no membership of its own: it is replicated by exactly the
 * devices allowed to read the conversation that announced it, so authorization
 * is delegated to that conversation.
 */
class CollabChannelHandler : public ChannelHandlerInterface
{
public:
    CollabChannelHandler(const std::shared_ptr<JamiAccount>& acc, dhtnet::ConnectionManager& cm);
    ~CollabChannelHandler();

    /// @param channelName "<conversationId>/<documentId>"
    void connect(const DeviceId& deviceId,
                 const std::string& channelName,
                 ConnectCb&& cb,
                 const std::string& connectionType = "",
                 bool forceNewConnection = false) override;

    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name) override;

    void onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                 const std::string& name,
                 std::shared_ptr<dhtnet::ChannelSocket> channel) override;

    /// Split "collab://<device>/<conversationId>/<documentId>" into its parts.
    /// Returns false when @p name is not a well-formed collaborative document URL.
    static bool parse(std::string_view name,
                      std::string& deviceId,
                      std::string& conversationId,
                      std::string& documentId);

private:
    std::weak_ptr<JamiAccount> account_;
    dhtnet::ConnectionManager& connectionManager_;
};

} // namespace jami
