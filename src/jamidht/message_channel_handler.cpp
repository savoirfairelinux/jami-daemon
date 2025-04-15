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
#include "jamidht/message_channel_handler.h"

static constexpr const char MESSAGE_SCHEME[] {"msg:"};

namespace jami {

using Key = std::pair<std::string, DeviceId>;

struct MessageChannelHandler::Impl : public std::enable_shared_from_this<Impl>
{
    dhtnet::ConnectionManager& connectionManager_;
    OnMessage onMessage_;
    OnPeerStateChanged onPeerStateChanged_;
    std::recursive_mutex connectionsMtx_;
    std::map<std::string, std::map<DeviceId, std::vector<std::shared_ptr<dhtnet::ChannelSocket>>>> connections_;

    Impl(dhtnet::ConnectionManager& cm, OnMessage onMessage, OnPeerStateChanged onPeer)
        : connectionManager_(cm)
        , onMessage_(std::move(onMessage))
        , onPeerStateChanged_(std::move(onPeer))
    {}

    void onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                           const std::string& peerId,
                           const DeviceId& device);
};

MessageChannelHandler::MessageChannelHandler(dhtnet::ConnectionManager& cm,
                                             OnMessage onMessage, OnPeerStateChanged onPeer)
    : ChannelHandlerInterface()
    , pimpl_(std::make_shared<Impl>(cm, std::move(onMessage), std::move(onPeer)))
{}

MessageChannelHandler::~MessageChannelHandler() {}

void
MessageChannelHandler::connect(const DeviceId& deviceId,
                               const std::string&,
                               ConnectCb&& cb,
                               const std::string& connectionType,
                               bool forceNewConnection)
{
    auto channelName = MESSAGE_SCHEME + deviceId.toString();
    if (pimpl_->connectionManager_.isConnecting(deviceId, channelName)) {
        JAMI_LOG("Already connecting to {}", deviceId);
        return;
    }
    pimpl_->connectionManager_.connectDevice(deviceId,
                                             channelName,
                                             std::move(cb),
                                             false,
                                             forceNewConnection,
                                             connectionType);
}

void
MessageChannelHandler::Impl::onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                                               const std::string& peerId,
                                               const DeviceId& device)
{
    std::lock_guard lk(connectionsMtx_);
    auto peerIt = connections_.find(peerId);
    if (peerIt == connections_.end())
        return;
    auto connectionsIt = peerIt->second.find(device);
    if (connectionsIt == peerIt->second.end())
        return;
    auto& connections = connectionsIt->second;
    auto conn = std::find(connections.begin(), connections.end(), socket);
    if (conn != connections.end())
        connections.erase(conn);
    if (connections.empty()) {
        peerIt->second.erase(connectionsIt);
    }
    if (peerIt->second.empty()) {
        connections_.erase(peerIt);
        onPeerStateChanged_(peerId, false);
    }
}

std::shared_ptr<dhtnet::ChannelSocket>
MessageChannelHandler::getChannel(const std::string& peer, const DeviceId& deviceId) const
{
    std::lock_guard lk(pimpl_->connectionsMtx_);
    auto it = pimpl_->connections_.find(peer);
    if (it == pimpl_->connections_.end())
        return nullptr;
    auto deviceIt = it->second.find(deviceId);
    if (deviceIt == it->second.end())
        return nullptr;
    if (deviceIt->second.empty())
        return nullptr;
    return deviceIt->second.back();
}

std::vector<std::shared_ptr<dhtnet::ChannelSocket>>
MessageChannelHandler::getChannels(const std::string& peer) const
{
    std::vector<std::shared_ptr<dhtnet::ChannelSocket>> sockets;
    std::lock_guard lk(pimpl_->connectionsMtx_);
    auto it = pimpl_->connections_.find(peer);
    if (it == pimpl_->connections_.end())
        return sockets;
    sockets.reserve(it->second.size());
    for (auto& [deviceId, channels] : it->second) {
        for (auto& channel : channels) {
            sockets.push_back(channel);
        }
    }
    return sockets;
}

bool
MessageChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                 const std::string& /* name */)
{
    if (!cert || !cert->issuer)
        return false;
    return true;
}

void
MessageChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                               const std::string&,
                               std::shared_ptr<dhtnet::ChannelSocket> socket)
{
    if (!cert || !cert->issuer)
        return;
    auto peerId = cert->issuer->getId().toString();
    auto device = cert->getLongId();
    std::lock_guard lk(pimpl_->connectionsMtx_);
    auto& connections = pimpl_->connections_[peerId];
    bool newPeerConnection = connections.empty();
    auto& deviceConnections = connections[device];
    deviceConnections.insert(deviceConnections.begin(), socket);
    if (newPeerConnection)
        pimpl_->onPeerStateChanged_(peerId, true);

    socket->onShutdown([w = pimpl_->weak_from_this(), peerId, device, s = std::weak_ptr(socket)]() {
        if (auto shared = w.lock())
            shared->onChannelShutdown(s.lock(), peerId, device);
    });

    struct DecodingContext
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               1500};
    };

    socket->setOnRecv([onMessage = pimpl_->onMessage_,
                       peerId,
                       cert,
                       ctx = std::make_shared<DecodingContext>()](const uint8_t* buf, size_t len) {
        if (!buf)
            return len;

        ctx->pac.reserve_buffer(len);
        std::copy_n(buf, len, ctx->pac.buffer());
        ctx->pac.buffer_consumed(len);

        msgpack::object_handle oh;
        try {
            while (ctx->pac.next(oh)) {
                Message msg;
                oh.get().convert(msg);
                onMessage(cert, msg.t, msg.c);
            }
        } catch (const std::exception& e) {
            JAMI_WARNING("[convInfo] error on sync: {:s}", e.what());
        }
        return len;
    });
}

bool
MessageChannelHandler::sendMessage(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                                   const Message& message)
{
    if (!socket)
        return false;
    msgpack::sbuffer buffer(UINT16_MAX); // Use max
    msgpack::pack(buffer, message);
    std::error_code ec;
    auto sent = socket->write(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), ec);
    if (ec) {
        JAMI_WARNING("Error sending message: {:s}", ec.message());
    }
    return !ec && sent == buffer.size();
}

} // namespace jami
