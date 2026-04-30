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

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace jami {

/**
 * Channel handler that ferries TCP byte streams between two Jami devices.
 *
 * Two roles:
 *
 *  - Server (host): accepts incoming `svc://<service-uuid>` channels, looks
 *    up the matching ServiceRecord and authorizes the requesting peer using
 *    `ServiceManager::isAuthorized`. On success, an outgoing `asio::tcp::socket`
 *    is connected to the local target and bytes are relayed in both directions
 *    until either side closes.
 *
 *  - Client: `openTunnel()` starts a local `asio::tcp::acceptor` bound to
 *    127.0.0.1; each accepted local TCP connection triggers a fresh
 *    `connectDevice("svc://<id>")` call to the host, and the resulting
 *    ChannelSocket is wired up as a bidirectional relay with the local TCP
 *    socket. The same tunnel can therefore serve multiple simultaneous TCP
 *    connections to the same remote service.
 *
 *  `closeTunnel()` shuts down the local acceptor; existing per-connection
 *  channels survive until they are closed by either side.
 */
class SvcTunnelChannelHandler : public ChannelHandlerInterface
{
public:
    /// Description of an active client-side tunnel.
    struct Tunnel
    {
        std::string id;          ///< opaque tunnel identifier
        std::string peerUri;     ///< remote account URI
        std::string peerDevice;  ///< remote device id (string form)
        std::string serviceId;   ///< remote service uuid
        std::string serviceName; ///< cached human-readable name
        uint16_t localPort {0};  ///< actual TCP port the local listener is bound to
    };

    using OnTunnelOpened = std::function<void(const std::string& tunnelId, uint16_t localPort)>;
    using OnTunnelClosed = std::function<void(const std::string& tunnelId, const std::string& reason)>;

    SvcTunnelChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                            dhtnet::ConnectionManager& cm,
                            std::shared_ptr<asio::io_context> io);
    ~SvcTunnelChannelHandler() override;

    /// Server-only: ChannelHandlerInterface entry-point. The client side
    /// uses `openTunnel` instead. Calls `cb(nullptr, deviceId)` on the next
    /// io tick because tunnels are managed at a higher level.
    void connect(const DeviceId& deviceId,
                 const std::string& name,
                 ConnectCb&& cb,
                 const std::string& connectionType = "",
                 bool forceNewConnection = false) override;

    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer,
                   const std::string& name) override;

    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<dhtnet::ChannelSocket> channel) override;

    /// Client API: start a local TCP listener and prepare to forward each
    /// accepted connection to the remote service through dhtnet.
    /// @param peerUri      Account URI of the remote peer.
    /// @param peerDevice   Device id of the remote peer.
    /// @param serviceId    Remote service UUID.
    /// @param serviceName  Cached name (for getActiveTunnels()).
    /// @param localPort    Local TCP port to bind to (0 for any free port).
    /// @param onOpened     Called once with the tunnel id and the actual bound port.
    /// @param onClosed     Called when the local listener is shut down.
    /// @return Tunnel id, or empty string on immediate failure.
    std::string openTunnel(std::string peerUri,
                           DeviceId peerDevice,
                           std::string serviceId,
                           std::string serviceName,
                           uint16_t localPort,
                           OnTunnelOpened onOpened,
                           OnTunnelClosed onClosed);

    /// Stop a tunnel previously created with openTunnel. Returns true if a
    /// tunnel with this id was found.
    bool closeTunnel(const std::string& tunnelId);

    /// Snapshot of currently-active client tunnels.
    std::vector<Tunnel> activeTunnels() const;

    /// Parse "svc://<uuid>" channel name, returning the uuid (empty if
    /// malformed). Exposed for testing.
    static std::string parseServiceId(const std::string& channelName);

private:
    struct ClientTunnel;
    void acceptLoop(const std::shared_ptr<ClientTunnel>& tunnel);
    void onClientChannelReady(const std::shared_ptr<ClientTunnel>& tunnel,
                              std::shared_ptr<asio::ip::tcp::socket> tcp,
                              std::shared_ptr<dhtnet::ChannelSocket> channel);
    void relay(std::shared_ptr<dhtnet::ChannelSocket> channel,
               std::shared_ptr<asio::ip::tcp::socket> tcp);
    void relayTcpToChannel(std::shared_ptr<dhtnet::ChannelSocket> channel,
                           std::shared_ptr<asio::ip::tcp::socket> tcp);

    std::weak_ptr<JamiAccount> account_;
    dhtnet::ConnectionManager& connectionManager_;
    std::shared_ptr<asio::io_context> io_;

    mutable std::mutex mtx_;
    std::map<std::string, std::shared_ptr<ClientTunnel>> tunnels_;
};

} // namespace jami
