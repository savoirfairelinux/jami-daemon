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
#include "jamidht/svc_discovery_channel_handler.h"

#include "jamidht/account_manager.h"
#include "jamidht/contact_list.h"
#include "jamidht/service_manager.h"
#include "logger.h"

#include <algorithm>
#include <cstring>

namespace jami {

namespace {
template<typename T>
bool
sendMsg(const std::shared_ptr<dhtnet::ChannelSocket>& s, const T& msg)
{
    if (!s)
        return false;
    msgpack::sbuffer buf;
    msgpack::pack(buf, msg);
    std::error_code ec;
    auto sent = s->write(reinterpret_cast<const uint8_t*>(buf.data()), buf.size(), ec);
    if (ec) {
        JAMI_WARNING("[SvcDiscovery] write error: {}", ec.message());
        return false;
    }
    return sent == buf.size();
}
} // namespace

SvcDiscoveryChannelHandler::SvcDiscoveryChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                                       dhtnet::ConnectionManager& cm)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
    , state_(std::make_shared<State>())
{}

SvcDiscoveryChannelHandler::~SvcDiscoveryChannelHandler() = default;

void
SvcDiscoveryChannelHandler::setOnResponse(ResponseCb cb)
{
    std::lock_guard lk(state_->mtx);
    state_->responseCb = std::move(cb);
}

svc_protocol::SvcDiscResponse
SvcDiscoveryChannelHandler::buildResponse(JamiAccount& account, const std::string& peerAccountUri)
{
    svc_protocol::SvcDiscResponse out;
    auto checker = [&account](const std::string& uri) { return account.isConfirmedContact(uri); };
    auto visible = account.serviceManager().getVisibleServices(peerAccountUri, checker);
    out.services.reserve(visible.size());
    for (auto& r : visible) {
        svc_protocol::SvcInfo info;
        info.id = std::move(r.id);
        info.name = std::move(r.name);
        info.description = std::move(r.description);
        info.proto = "tcp";
        out.services.push_back(std::move(info));
    }
    return out;
}

void
SvcDiscoveryChannelHandler::connect(const DeviceId& deviceId,
                                    const std::string& /*name*/,
                                    ConnectCb&& cb,
                                    const std::string& /*connectionType*/,
                                    bool /*forceNewConnection*/)
{
    auto userCb = std::make_shared<ConnectCb>(std::move(cb));
    auto state = state_;
    auto wacc = account_;
    connectionManager_.connectDevice(
        deviceId,
        svc_protocol::kDiscoveryChannelName,
        [userCb, state, wacc, this](std::shared_ptr<dhtnet::ChannelSocket> socket,
                                    const DeviceId& dev) {
            if (socket) {
                // Retain the channel for its full lifetime so the response
                // can come back even if no one else holds it.
                {
                    std::lock_guard lk(state->mtx);
                    state->channels.push_back(socket);
                }
                socket->onShutdown([state, ws = std::weak_ptr(socket)](const std::error_code&) {
                    auto s = ws.lock();
                    if (!s)
                        return;
                    std::lock_guard lk(state->mtx);
                    auto it = std::find(state->channels.begin(), state->channels.end(), s);
                    if (it != state->channels.end())
                        state->channels.erase(it);
                });
                // The initiating side immediately writes a Query so the server
                // can respond. We need to install a reader to handle the
                // response too — derive peer account uri from the cert.
                auto cert = socket->peerCertificate();
                std::string peerAccountUri;
                if (cert && cert->issuer)
                    peerAccountUri = cert->issuer->getId().toString();
                installReader(socket, peerAccountUri);
                sendMsg(socket, svc_protocol::SvcDiscQuery{});
            }
            if (*userCb)
                (*userCb)(socket, dev);
        });
}

bool
SvcDiscoveryChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer,
                                      const std::string& /*name*/)
{
    return peer && peer->issuer;
}

void
SvcDiscoveryChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                                    const std::string& /*name*/,
                                    std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    if (!channel)
        return;
    // The initiator already retains the socket and installs its reader from
    // connect(); no need to do the work twice.
    if (channel->isInitiator())
        return;
    if (!peer || !peer->issuer) {
        channel->shutdown();
        return;
    }
    auto peerUri = peer->issuer->getId().toString();
    {
        std::lock_guard lk(state_->mtx);
        state_->channels.push_back(channel);
    }
    auto state = state_;
    channel->onShutdown([state, ws = std::weak_ptr(channel)](const std::error_code&) {
        auto s = ws.lock();
        if (!s)
            return;
        std::lock_guard lk(state->mtx);
        auto it = std::find(state->channels.begin(), state->channels.end(), s);
        if (it != state->channels.end())
            state->channels.erase(it);
    });
    installReader(channel, peer->issuer->getId().toString());
}

void
SvcDiscoveryChannelHandler::installReader(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                                          std::string peerAccountUri)
{
    auto reader = std::make_shared<msgpack::unpacker>();
    reader->reserve_buffer(4096);
    auto wacc = account_;
    auto state = state_;
    std::weak_ptr<dhtnet::ChannelSocket> wsock = channel;

    channel->setOnRecv([reader, wacc, state, wsock,
                        peerAccountUri = std::move(peerAccountUri)](const uint8_t* data,
                                                                    size_t size) -> ssize_t {
        if (size == 0)
            return 0;
        if (reader->buffer_capacity() < size)
            reader->reserve_buffer(size);
        std::memcpy(reader->buffer(), data, size);
        reader->buffer_consumed(size);

        msgpack::object_handle oh;
        while (reader->next(oh)) {
            const auto& obj = oh.get();
            const auto type = svc_protocol::peekType(obj);
            const auto v = svc_protocol::peekVersion(obj);
            auto sock = wsock.lock();
            if (!sock)
                return static_cast<ssize_t>(size);

            if (type == svc_protocol::MsgType::kQuery) {
                auto acc = wacc.lock();
                if (!acc) {
                    sock->shutdown();
                    continue;
                }
                if (v > svc_protocol::kMaxVersion) {
                    svc_protocol::SvcDiscVersionMismatch vm;
                    vm.max_supported = svc_protocol::kMaxVersion;
                    sendMsg(sock, vm);
                    continue;
                }
                auto resp = SvcDiscoveryChannelHandler::buildResponse(*acc, peerAccountUri);
                sendMsg(sock, resp);
            } else if (type == svc_protocol::MsgType::kServiceList) {
                svc_protocol::SvcDiscResponse resp;
                try {
                    obj.convert(resp);
                } catch (const std::exception& e) {
                    JAMI_WARNING("[SvcDiscovery] bad service_list: {}", e.what());
                    continue;
                }
                ResponseCb cb;
                {
                    std::lock_guard lk(state->mtx);
                    cb = state->responseCb;
                }
                if (cb)
                    cb(peerAccountUri, resp.services);
            } else if (type == svc_protocol::MsgType::kVersionMismatch
                       || type == svc_protocol::MsgType::kError) {
                ResponseCb cb;
                {
                    std::lock_guard lk(state->mtx);
                    cb = state->responseCb;
                }
                if (cb)
                    cb(peerAccountUri, {});
            } else {
                JAMI_WARNING("[SvcDiscovery] unknown message type '{}'", type);
            }
        }
        return static_cast<ssize_t>(size);
    });
}

} // namespace jami
