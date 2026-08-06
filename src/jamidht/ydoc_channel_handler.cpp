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
#include "jamidht/ydoc_channel_handler.h"

#include "jamidht/collaborative_editing.h"

namespace jami {

/// The document id a "ydoc://<device>/<documentId>" channel name carries.
static std::string_view
documentIdOf(std::string_view name)
{
    auto sep = name.find_last_of('/');
    return sep != std::string_view::npos ? name.substr(sep + 1) : std::string_view {};
}

YdocChannelHandler::YdocChannelHandler(const std::shared_ptr<JamiAccount>& acc, dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
{}

YdocChannelHandler::~YdocChannelHandler() {}

void
YdocChannelHandler::connect(const DeviceId& deviceId,
                            const std::string& name,
                            ConnectCb&& cb,
                            const std::string& /*connectionType*/,
                            bool /*forceNewConnection*/)
{
    auto channelName = "ydoc://" + deviceId.toString() + "/" + name;
    if (connectionManager_.isConnecting(deviceId, channelName)) {
        JAMI_LOG("Already connecting to {} for document {}", deviceId, name);
        return;
    }
    connectionManager_.connectDevice(deviceId, channelName, std::move(cb));
}

bool
YdocChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;
    const auto documentId = documentIdOf(name);
    if (documentId.empty())
        return false;
    // Real-time traffic is only for a document being edited on both ends:
    // accepted when this device has it open and the peer is one of its members.
    // Membership is the document's own, never the parent conversation's: a
    // joiner whose commits have not reached us yet is refused here, and called
    // back the moment they are merged.
    return acc->collaborativeEditing()->acceptsRealtimeChannel(std::string(documentId),
                                                               cert->issuer->getId().toString(),
                                                               cert->getLongId().toString());
}

void
YdocChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                            const std::string& name,
                            std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    auto acc = account_.lock();
    if (!cert || !cert->issuer || !acc || !channel)
        return;
    const auto documentId = documentIdOf(name);
    if (documentId.empty()) {
        channel->shutdown();
        return;
    }
    acc->collaborativeEditing()->onRealtimeChannel(std::string(documentId),
                                                   cert->issuer->getId().toString(),
                                                   cert->getLongId().toString(),
                                                   std::move(channel));
}

} // namespace jami
