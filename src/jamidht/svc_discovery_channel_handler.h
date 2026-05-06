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
#include "jamidht/svc_protocol.h"

#include <dhtnet/connectionmanager.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace jami {

/**
 * Channel handler for service-discovery requests.
 *
 * Server side: accepts any incoming `svcdisc://query` channel from a peer
 * with a valid Jami certificate and replies with a service_list filtered
 * through `ServiceManager::getVisibleServices`.
 *
 * Client side: `connect()` opens a `svcdisc://query` channel and sends a
 * SvcDiscQuery as soon as the channel is ready; the response is delivered
 * through the callback registered via `setOnResponse`.
 *
 * Cache: maintains a per-device service cache that is proactively refreshed
 * when a new device connects. queryPeerServices() reads from this cache.
 */
class SvcDiscoveryChannelHandler : public ChannelHandlerInterface
{
public:
    /// Callback delivered to a client when a discovery response arrives.
    /// `services` is empty on error or version mismatch. `peerDeviceId` is
    /// the long device id of the responder, or empty if the response did
    /// not carry one (older peers / error paths).
    using ResponseCb = std::function<void(const std::string& peerAccountUri,
                                          const std::string& peerDeviceId,
                                          const std::vector<svc_protocol::SvcInfo>& services)>;

    SvcDiscoveryChannelHandler(const std::shared_ptr<JamiAccount>& acc,
                               dhtnet::ConnectionManager& cm,
                               std::filesystem::path cachePath);
    ~SvcDiscoveryChannelHandler() override;

    /// Register the handler invoked when a discovery response is received from
    /// any peer device. Replaces any previous registration.
    void setOnResponse(ResponseCb cb);

    /// Open a discovery channel to the given device. As soon as the channel
    /// is ready, a SvcDiscQuery is automatically sent.
    void connect(const DeviceId& deviceId,
                 const std::string& name,
                 ConnectCb&& cb,
                 const std::string& connectionType = "",
                 bool forceNewConnection = false) override;

    bool onRequest(const std::shared_ptr<dht::crypto::Certificate>& peer, const std::string& name) override;

    void onReady(const std::shared_ptr<dht::crypto::Certificate>& peer,
                 const std::string& name,
                 std::shared_ptr<dhtnet::ChannelSocket> channel) override;

    /// Build the SvcDiscResponse the host would send to the given peer,
    /// based on the current ServiceManager state. Exposed for testing.
    static svc_protocol::SvcDiscResponse buildResponse(JamiAccount& account, const std::string& peerAccountUri);

    // ---- Cache API ----

    /// Callback invoked when the service cache is updated for a peer device.
    using CacheUpdateCb = std::function<
        void(const std::string& peerUri, const DeviceId& deviceId, const std::vector<svc_protocol::SvcInfo>& services)>;

    /// Set a callback to be notified whenever the cache is updated.
    void onCacheUpdated(CacheUpdateCb cb);

    /// Proactively discover services on a newly-connected device and cache
    /// the result. Called from onNewDeviceConnection.
    void refreshDevice(const std::string& peerUri, const DeviceId& deviceId);

    /// A service entry annotated with the device that offers it.
    struct CachedSvcInfo
    {
        DeviceId deviceId;
        svc_protocol::SvcInfo info;
    };

    /// Return the cached services for a peer (all devices aggregated).
    /// Each entry carries the device ID that advertised it.
    /// Returns an empty vector if nothing is cached yet.
    std::vector<CachedSvcInfo> getCachedServices(const std::string& peerUri) const;

    /// Remove all cached entries for a specific device.
    void removeDevice(const DeviceId& deviceId);

private:
    /// State shared between the handler and per-channel read closures, so
    /// closures remain safe even if the handler is destroyed before the
    /// channel sockets it produced.
    struct State
    {
        std::mutex mtx;
        ResponseCb responseCb;
        CacheUpdateCb cacheUpdateCb;
        /// Strong references to channels we have onReady'd, so they remain
        /// alive long enough to send/receive their messages even if the
        /// caller of `connect()` did not retain the socket.
        std::vector<std::shared_ptr<dhtnet::ChannelSocket>> channels;

        // ---- Cache state ----
        struct CachedDeviceServices
        {
            std::string peerUri;
            std::vector<svc_protocol::SvcInfo> services;
            MSGPACK_DEFINE_MAP(peerUri, services)
        };
        /// deviceId -> cached services
        std::map<DeviceId, CachedDeviceServices> cache;
    };

    void installReader(const std::shared_ptr<dhtnet::ChannelSocket>& channel, std::string peerAccountUri);
    void saveCache() const;
    void loadCache();

    std::weak_ptr<JamiAccount> account_;
    dhtnet::ConnectionManager& connectionManager_;
    std::shared_ptr<State> state_;
    std::filesystem::path cachePath_;
};

} // namespace jami
