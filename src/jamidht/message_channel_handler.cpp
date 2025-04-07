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
    std::weak_ptr<JamiAccount> account_;
    dhtnet::ConnectionManager& connectionManager_;
    std::recursive_mutex connectionsMtx_;
    std::map<Key, std::vector<std::shared_ptr<dhtnet::ChannelSocket>>> connections_;

    Impl(const std::shared_ptr<JamiAccount>& acc, dhtnet::ConnectionManager& cm)
        : account_(acc)
        , connectionManager_(cm)
    {}

    void onChannelShutdown(const std::shared_ptr<dhtnet::ChannelSocket>& socket,
                           const std::string& peerId,
                           const DeviceId& device);
};

MessageChannelHandler::MessageChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                             dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , pimpl_(std::make_shared<Impl>(acc, cm))
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
        JAMI_INFO("Already connecting to %s", deviceId.to_c_str());
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
    auto connectionsIt = connections_.find({peerId, device});
    if (connectionsIt == connections_.end())
        return;
    auto& connections = connectionsIt->second;
    auto conn = std::find(connections.begin(), connections.end(), socket);
    if (conn != connections.end())
        connections.erase(conn);
    if (connections.empty())
        connections_.erase(connectionsIt);
}

std::shared_ptr<dhtnet::ChannelSocket>
MessageChannelHandler::getChannel(const std::string& peer, const DeviceId& deviceId) const
{
    std::lock_guard lk(pimpl_->connectionsMtx_);
    auto it = pimpl_->connections_.find({peer, deviceId});
    if (it == pimpl_->connections_.end())
        return nullptr;
    if (it->second.empty())
        return nullptr;
    return it->second.front();
}

std::vector<std::shared_ptr<dhtnet::ChannelSocket>>
MessageChannelHandler::getChannels(const std::string& peer) const
{
    std::vector<std::shared_ptr<dhtnet::ChannelSocket>> sockets;
    std::lock_guard lk(pimpl_->connectionsMtx_);
    auto lower = pimpl_->connections_.lower_bound({peer, DeviceId()});
    for (auto it = lower; it != pimpl_->connections_.end() && it->first.first == peer; ++it)
        sockets.insert(sockets.end(), it->second.begin(), it->second.end());
    return sockets;
}

bool
MessageChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                 const std::string& /* name */)
{
    auto acc = pimpl_->account_.lock();
    if (!cert || !cert->issuer || !acc)
        return false;
    return true;
    // return cert->issuer->getId().toString() == acc->getUsername();
}

void
MessageChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& cert,
                               const std::string&,
                               std::shared_ptr<dhtnet::ChannelSocket> socket)
{
    auto acc = pimpl_->account_.lock();
    if (!cert || !cert->issuer || !acc)
        return;
    auto peerId = cert->issuer->getId().toString();
    auto device = cert->getLongId();
    std::lock_guard lk(pimpl_->connectionsMtx_);
    pimpl_->connections_[{peerId, device}].emplace_back(socket);

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

    socket->setOnRecv([acc = pimpl_->account_.lock(),
                       peerId,
                       deviceId = device.toString(),
                       ctx = std::make_shared<DecodingContext>()](const uint8_t* buf, size_t len) {
        if (!buf || !acc)
            return len;

        ctx->pac.reserve_buffer(len);
        std::copy_n(buf, len, ctx->pac.buffer());
        ctx->pac.buffer_consumed(len);

        msgpack::object_handle oh;
        try {
            while (ctx->pac.next(oh)) {
                Message msg;
                oh.get().convert(msg);
                acc->onTextMessage("", peerId, deviceId, {{msg.t, msg.c}});
            }
        } catch (const std::exception& e) {
            JAMI_WARNING("[convInfo] Error parsing message: {:s}", e.what());
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
