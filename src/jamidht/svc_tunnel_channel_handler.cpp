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
#include "jamidht/svc_tunnel_channel_handler.h"

#include "jamidht/account_manager.h"
#include "jamidht/contact_list.h"
#include "jamidht/service_manager.h"
#include "jamidht/svc_protocol.h"
#include "logger.h"
#include "manager.h"

#include <asio/connect.hpp>
#include <asio/post.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <atomic>

namespace jami {

namespace {
constexpr size_t kRelayBufSize = 16 * 1024;
}

struct SvcTunnelChannelHandler::ClientTunnel
{
    std::string id;
    std::string peerUri;
    DeviceId peerDevice;
    std::string serviceId;
    std::string serviceName;
    std::shared_ptr<asio::ip::tcp::acceptor> acceptor;
    OnTunnelClosed onClosed;
    std::atomic_bool closed {false};
};

SvcTunnelChannelHandler::SvcTunnelChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                                                 dhtnet::ConnectionManager& cm,
                                                 std::shared_ptr<asio::io_context> io)
    : ChannelHandlerInterface()
    , account_(acc)
    , connectionManager_(cm)
    , io_(std::move(io))
{}

SvcTunnelChannelHandler::~SvcTunnelChannelHandler()
{
    std::vector<std::shared_ptr<ClientTunnel>> snapshot;
    {
        std::lock_guard lk(mtx_);
        for (auto& [_id, t] : tunnels_)
            snapshot.push_back(t);
        tunnels_.clear();
    }
    for (auto& t : snapshot) {
        t->closed = true;
        std::error_code ec;
        if (t->acceptor)
            t->acceptor->close(ec);
    }
}

std::string
SvcTunnelChannelHandler::parseServiceId(const std::string& channelName)
{
    constexpr std::string_view prefix = "svc://";
    if (channelName.size() <= prefix.size())
        return {};
    if (channelName.compare(0, prefix.size(), prefix) != 0)
        return {};
    return channelName.substr(prefix.size());
}

void
SvcTunnelChannelHandler::connect(const DeviceId& deviceId,
                                 const std::string& /*name*/,
                                 ConnectCb&& cb,
                                 const std::string& /*connectionType*/,
                                 bool /*forceNewConnection*/)
{
    // Tunnels are managed at a higher level via openTunnel(); the generic
    // channel-handler entry-point is unused but must not block. Just signal
    // back asynchronously with a null socket so callers can fall through.
    if (cb && io_)
        asio::post(*io_, [cb = std::move(cb), deviceId]() mutable {
            cb(nullptr, deviceId);
        });
}

bool
SvcTunnelChannelHandler::onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer,
                                   const std::string& name)
{
    if (!peer || !peer->issuer)
        return false;
    auto serviceId = parseServiceId(name);
    if (serviceId.empty())
        return false;
    auto acc = account_.lock();
    if (!acc)
        return false;
    const auto peerUri = peer->issuer->getId().toString();
    auto checker = [&acc](const std::string& uri) { return acc->isConfirmedContact(uri); };
    return acc->serviceManager().isAuthorized(serviceId, peerUri, checker);
}

void
SvcTunnelChannelHandler::onReady(const std::shared_ptr<dht::crypto::Certificate>& /*peer*/,
                                 const std::string& name,
                                 std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    if (!channel)
        return;
    // The initiator side wires its own onClientChannelReady callback via
    // connectDevice(). Only the receiving (server) side performs the local
    // TCP connect here.
    if (channel->isInitiator())
        return;
    auto serviceId = parseServiceId(name);
    auto acc = account_.lock();
    if (!acc || serviceId.empty()) {
        channel->shutdown();
        return;
    }
    auto rec = acc->serviceManager().getService(serviceId);
    if (!rec || !rec->enabled) {
        channel->shutdown();
        return;
    }
    if (!io_) {
        channel->shutdown();
        return;
    }

    auto tcp = std::make_shared<asio::ip::tcp::socket>(*io_);
    asio::ip::tcp::resolver resolver(*io_);
    std::error_code ec;
    auto endpoints = resolver.resolve(rec->localHost, std::to_string(rec->localPort), ec);
    if (ec) {
        JAMI_WARNING("[SvcTunnel] resolve {}:{} failed: {}",
                     rec->localHost, rec->localPort, ec.message());
        channel->shutdown();
        return;
    }

    // Buffer any bytes received from the remote peer before we have a local
    // TCP connection up; flush them through once async_connect succeeds.
    struct PreConnectBuf
    {
        std::mutex m;
        std::vector<uint8_t> bytes;
        bool tcpReady {false};
        std::shared_ptr<asio::ip::tcp::socket> tcp;
    };
    auto pre = std::make_shared<PreConnectBuf>();
    pre->tcp = tcp;
    auto channelKeep = channel;
    channel->setOnRecv([pre, channelKeep](const uint8_t* data, size_t size) -> ssize_t {
        std::lock_guard lk(pre->m);
        if (pre->tcpReady) {
            auto buf = std::make_shared<std::vector<uint8_t>>(data, data + size);
            asio::async_write(*pre->tcp,
                              asio::buffer(*buf),
                              [buf](const std::error_code& ec, std::size_t) {
                                  if (ec)
                                      JAMI_DEBUG("[SvcTunnel] write to TCP failed: {}",
                                                 ec.message());
                              });
        } else {
            pre->bytes.insert(pre->bytes.end(), data, data + size);
        }
        return static_cast<ssize_t>(size);
    });

    asio::async_connect(*tcp,
                        endpoints,
                        [this, channel = channelKeep, tcp, pre](const std::error_code& cec,
                                                                const asio::ip::tcp::endpoint&) {
                            if (cec) {
                                JAMI_WARNING("[SvcTunnel] connect failed: {}", cec.message());
                                channel->shutdown();
                                return;
                            }
                            std::vector<uint8_t> drained;
                            {
                                std::lock_guard lk(pre->m);
                                pre->tcpReady = true;
                                drained.swap(pre->bytes);
                            }
                            if (!drained.empty()) {
                                auto buf = std::make_shared<std::vector<uint8_t>>(std::move(drained));
                                asio::async_write(*tcp,
                                                  asio::buffer(*buf),
                                                  [buf](const std::error_code& ec, std::size_t) {
                                                      if (ec)
                                                          JAMI_DEBUG(
                                                              "[SvcTunnel] flush failed: {}",
                                                              ec.message());
                                                  });
                            }
                            // Switch to a hot-path setOnRecv now that tcp is up.
                            channel->setOnRecv([tcp, channel](const uint8_t* data, size_t n) -> ssize_t {
                                auto buf = std::make_shared<std::vector<uint8_t>>(data, data + n);
                                asio::async_write(*tcp,
                                                  asio::buffer(*buf),
                                                  [buf](const std::error_code& ec, std::size_t) {
                                                      if (ec)
                                                          JAMI_DEBUG(
                                                              "[SvcTunnel] write to TCP failed: {}",
                                                              ec.message());
                                                  });
                                return static_cast<ssize_t>(n);
                            });
                            relayTcpToChannel(channel, tcp);
                        });
}

void
SvcTunnelChannelHandler::relay(std::shared_ptr<dhtnet::ChannelSocket> channel,
                               std::shared_ptr<asio::ip::tcp::socket> tcp)
{
    // ChannelSocket -> TCP
    auto channelHold = channel;
    channel->setOnRecv([tcp, channelHold](const uint8_t* data, size_t size) -> ssize_t {
        auto buf = std::make_shared<std::vector<uint8_t>>(data, data + size);
        asio::async_write(*tcp,
                          asio::buffer(*buf),
                          [buf](const std::error_code& ec, std::size_t /*n*/) {
                              if (ec)
                                  JAMI_DEBUG("[SvcTunnel] write to TCP failed: {}", ec.message());
                          });
        return static_cast<ssize_t>(size);
    });

    // TCP -> ChannelSocket
    relayTcpToChannel(channel, tcp);
}

void
SvcTunnelChannelHandler::relayTcpToChannel(std::shared_ptr<dhtnet::ChannelSocket> channel,
                                           std::shared_ptr<asio::ip::tcp::socket> tcp)
{
    auto buf = std::make_shared<std::vector<uint8_t>>(kRelayBufSize);
    auto channelKeep = channel;
    auto tcpKeep = tcp;
    auto reader = std::make_shared<std::function<void()>>();
    *reader = [channelKeep, tcpKeep, buf, reader]() {
        tcpKeep->async_read_some(asio::buffer(*buf),
                                 [channelKeep, tcpKeep, buf, reader](const std::error_code& ec,
                                                                     std::size_t n) {
            if (ec || n == 0) {
                channelKeep->shutdown();
                std::error_code ig;
                tcpKeep->close(ig);
                return;
            }
            std::error_code wec;
            channelKeep->write(buf->data(), n, wec);
            if (wec) {
                channelKeep->shutdown();
                std::error_code ig;
                tcpKeep->close(ig);
                return;
            }
            (*reader)();
        });
    };
    (*reader)();

    channel->onShutdown([tcp = tcpKeep](const std::error_code&) {
        std::error_code ig;
        tcp->close(ig);
    });
}

std::string
SvcTunnelChannelHandler::openTunnel(std::string peerUri,
                                    DeviceId peerDevice,
                                    std::string serviceId,
                                    std::string serviceName,
                                    uint16_t localPort,
                                    OnTunnelOpened onOpened,
                                    OnTunnelClosed onClosed)
{
    if (!io_ || serviceId.empty() || peerUri.empty())
        return {};

    auto t = std::make_shared<ClientTunnel>();
    t->id = generateServiceUuid();
    t->peerUri = std::move(peerUri);
    t->peerDevice = peerDevice;
    t->serviceId = std::move(serviceId);
    t->serviceName = std::move(serviceName);
    t->onClosed = std::move(onClosed);
    try {
        asio::ip::tcp::endpoint ep(asio::ip::address_v4::loopback(), localPort);
        t->acceptor = std::make_shared<asio::ip::tcp::acceptor>(*io_);
        t->acceptor->open(ep.protocol());
        t->acceptor->set_option(asio::socket_base::reuse_address(true));
        t->acceptor->bind(ep);
        t->acceptor->listen();
    } catch (const std::exception& e) {
        JAMI_WARNING("[SvcTunnel] cannot bind 127.0.0.1:{}: {}", localPort, e.what());
        return {};
    }

    auto bound = static_cast<uint16_t>(t->acceptor->local_endpoint().port());

    {
        std::lock_guard lk(mtx_);
        tunnels_[t->id] = t;
    }
    if (onOpened)
        onOpened(t->id, bound);

    acceptLoop(t);
    return t->id;
}

void
SvcTunnelChannelHandler::acceptLoop(const std::shared_ptr<ClientTunnel>& tunnel)
{
    auto self = tunnel;
    auto sock = std::make_shared<asio::ip::tcp::socket>(*io_);
    self->acceptor->async_accept(*sock,
                                 [this, self, sock](const std::error_code& ec) {
        if (self->closed) {
            return;
        }
        if (ec) {
            if (ec == asio::error::operation_aborted)
                return;
            JAMI_DEBUG("[SvcTunnel] accept error: {}", ec.message());
            return;
        }
        // Open a fresh dhtnet channel for this TCP connection.
        std::string channelName = std::string(svc_protocol::kTunnelChannelPrefix) + self->serviceId;
        connectionManager_.connectDevice(
            self->peerDevice,
            channelName,
            [this, self, sock](std::shared_ptr<dhtnet::ChannelSocket> channel,
                               const DeviceId&) {
                if (!channel) {
                    JAMI_WARNING("[SvcTunnel] client-side connectDevice returned null");
                    std::error_code ig;
                    sock->close(ig);
                    return;
                }
                onClientChannelReady(self, sock, std::move(channel));
            });
        // Continue accepting.
        acceptLoop(self);
    });
}

void
SvcTunnelChannelHandler::onClientChannelReady(const std::shared_ptr<ClientTunnel>& /*tunnel*/,
                                              std::shared_ptr<asio::ip::tcp::socket> tcp,
                                              std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    relay(std::move(channel), std::move(tcp));
}

bool
SvcTunnelChannelHandler::closeTunnel(const std::string& tunnelId)
{
    std::shared_ptr<ClientTunnel> t;
    {
        std::lock_guard lk(mtx_);
        auto it = tunnels_.find(tunnelId);
        if (it == tunnels_.end())
            return false;
        t = it->second;
        tunnels_.erase(it);
    }
    t->closed = true;
    std::error_code ec;
    if (t->acceptor)
        t->acceptor->close(ec);
    if (t->onClosed)
        t->onClosed(tunnelId, "closed");
    return true;
}

std::vector<SvcTunnelChannelHandler::Tunnel>
SvcTunnelChannelHandler::activeTunnels() const
{
    std::lock_guard lk(mtx_);
    std::vector<Tunnel> out;
    out.reserve(tunnels_.size());
    for (const auto& [_id, t] : tunnels_) {
        Tunnel info;
        info.id = t->id;
        info.peerUri = t->peerUri;
        info.peerDevice = t->peerDevice.toString();
        info.serviceId = t->serviceId;
        info.serviceName = t->serviceName;
        info.localPort = t->acceptor ? static_cast<uint16_t>(t->acceptor->local_endpoint().port()) : 0;
        out.push_back(std::move(info));
    }
    return out;
}

} // namespace jami
