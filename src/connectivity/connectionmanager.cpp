/*
 *  Copyright (C) 2019-2023 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "connectionmanager.h"
#include "peer_connection.h"
#include "logger.h"
#include "fileutils.h"
#include "connectivity/upnp/upnp_control.h"
#include "connectivity/sip_utils.h"
#include "connectivity/security/certstore.h"

#include <asio.hpp>
#include <opendht/crypto.h>
#include <opendht/thread_pool.h>
#include <opendht/value.h>

#include <algorithm>
#include <mutex>
#include <map>
#include <condition_variable>
#include <set>

static constexpr std::chrono::seconds DHT_MSG_TIMEOUT {30};
static constexpr int MAX_TENTATIVES {100};
using ValueIdDist = std::uniform_int_distribution<dht::Value::Id>;
using CallbackId = std::pair<jami::DeviceId, dht::Value::Id>;

namespace jami {

struct ConnectionInfo
{
    ~ConnectionInfo()
    {
        if (socket_)
            socket_->join();
    }

    std::mutex mutex_ {};
    bool responseReceived_ {false};
    PeerConnectionRequest response_ {};
    std::unique_ptr<IceTransport> ice_ {nullptr};
    // Used to store currently non ready TLS Socket
    std::unique_ptr<TlsSocketEndpoint> tls_ {nullptr};
    std::shared_ptr<MultiplexedSocket> socket_ {};
    std::set<CallbackId> cbIds_ {};

    std::function<void(bool)> onConnected_;
    std::unique_ptr<asio::steady_timer> waitForAnswer_ {};

};

/**
 * returns whether or not UPnP is enabled and active_
 * ie: if it is able to make port mappings
 */
bool
ConnectionManager::Config::getUPnPActive() const
{
    std::lock_guard<std::mutex> lk(upnp_mtx);
    if (upnpCtrl_)
        return upnpCtrl_->isReady();
    return false;
}

class ConnectionManager::Impl : public std::enable_shared_from_this<ConnectionManager::Impl>
{
public:
    explicit Impl(std::shared_ptr<ConnectionManager::Config> config_)
        : config_ {std::move(config_)}
    {}
    ~Impl() {}

    std::shared_ptr<dht::DhtRunner> dht() { return config_->dht_; }
    const dht::crypto::Identity& identity() const { return config_->id_; }

    void removeUnusedConnections(const DeviceId& deviceId = {})
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        for (auto it = infos_.begin(); it != infos_.end();) {
            auto& [key, info] = *it;
            bool erased = false;
            if (info && (!deviceId || key.first == deviceId)) {
                if (info->tls_)
                    info->tls_->shutdown();
                if (info->socket_)
                    info->socket_->shutdown();
                if (info->ice_)
                    info->ice_->cancelOperations();
                if (info->waitForAnswer_)
                    info->waitForAnswer_->cancel();
                erased = true;
                it = infos_.erase(it);
            }
            if (!erased)
                ++it;
        }
        if (!deviceId)
            dht::ThreadPool::io().run([infos = std::move(infos_)]() mutable { infos.clear(); });
    }

    void shutdown()
    {
        if (isDestroying_)
            return;
        isDestroying_ = true;
        {
            std::lock_guard<std::mutex> lk(connectCbsMtx_);
            // Call all pending callbacks that channel is not ready
            for (auto& [deviceId, pcbs] : pendingCbs_)
                for (auto& pending : pcbs)
                    pending.cb(nullptr, deviceId);
            pendingCbs_.clear();
        }
        removeUnusedConnections();
    }

    struct PendingCb
    {
        std::string name;
        ConnectCallback cb;
        dht::Value::Id vid;
    };

    void connectDeviceStartIce(const std::shared_ptr<dht::crypto::PublicKey>& devicePk,
                               const dht::Value::Id& vid,
                               const std::string& connType,
                               std::function<void(bool)> onConnected);
    void onResponse(const asio::error_code& ec, const DeviceId& deviceId, const dht::Value::Id& vid);
    bool connectDeviceOnNegoDone(const DeviceId& deviceId,
                                 const std::string& name,
                                 const dht::Value::Id& vid,
                                 const std::shared_ptr<dht::crypto::Certificate>& cert);
    void connectDevice(const DeviceId& deviceId,
                       const std::string& uri,
                       ConnectCallback cb,
                       bool noNewSocket = false,
                       bool forceNewSocket = false,
                       const std::string& connType = "");
    void connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                       const std::string& name,
                       ConnectCallback cb,
                       bool noNewSocket = false,
                       bool forceNewSocket = false,
                       const std::string& connType = "");
    /**
     * Send a ChannelRequest on the TLS socket. Triggers cb when ready
     * @param sock      socket used to send the request
     * @param name      channel's name
     * @param vid       channel's id
     * @param deviceId  to identify the linked ConnectCallback
     */
    void sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock,
                            const std::string& name,
                            const DeviceId& deviceId,
                            const dht::Value::Id& vid);
    /**
     * Triggered when a PeerConnectionRequest comes from the DHT
     */
    void answerTo(IceTransport& ice,
                  const dht::Value::Id& id,
                  const std::shared_ptr<dht::crypto::PublicKey>& fromPk);
    bool onRequestStartIce(const PeerConnectionRequest& req);
    bool onRequestOnNegoDone(const PeerConnectionRequest& req);
    void onDhtPeerRequest(const PeerConnectionRequest& req,
                          const std::shared_ptr<dht::crypto::Certificate>& cert);

    void addNewMultiplexedSocket(const DeviceId& deviceId, const dht::Value::Id& vid);
    void onPeerResponse(const PeerConnectionRequest& req);
    void onDhtConnected(const dht::crypto::PublicKey& devicePk);

    const std::shared_future<tls::DhParams> dhParams() const;

    mutable std::mutex messageMutex_ {};
    std::set<std::string, std::less<>> treatedMessages_ {};

    void loadTreatedMessages();
    void saveTreatedMessages() const;

    /// \return true if the given DHT message identifier has been treated
    /// \note if message has not been treated yet this method store this id and returns true at
    /// further calls
    bool isMessageTreated(std::string_view id);

    std::string cachePath_ = fileutils::get_cache_dir();

    /**
     * Published IPv4/IPv6 addresses, used only if defined by the user in account
     * configuration
     *
     */
    IpAddr publishedIp_[2] {};

    // This will be stored in the configuration
    std::string publishedIpAddress_ {};

    /**
     * Published port, used only if defined by the user
     */
    pj_uint16_t publishedPort_ {sip_utils::DEFAULT_SIP_PORT};

    /**
     * interface name on which this account is bound
     */
    std::string interface_ {"default"};

    /**
     * Get the local interface name on which this account is bound.
     */
    const std::string& getLocalInterface() const { return interface_; }

    /**
     * Get the published IP address, fallbacks to NAT if family is unspecified
     * Prefers the usage of IPv4 if possible.
     */
    IpAddr getPublishedIpAddress(uint16_t family = PF_UNSPEC) const;

    /**
     * Set published IP address according to given family
     */
    void setPublishedAddress(const IpAddr& ip_addr);

    /**
     * Store the local/public addresses used to register
     */
    void storeActiveIpAddress(std::function<void()>&& cb = {});

    /**
     * Create and return ICE options.
     */
    void getIceOptions(std::function<void(IceTransportOptions&&)> cb) noexcept;
    IceTransportOptions getIceOptions() const noexcept;

    /**
     * Inform that a potential peer device have been found.
     * Returns true only if the device certificate is a valid device certificate.
     * In that case (true is returned) the account_id parameter is set to the peer account ID.
     */
    static bool foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                dht::InfoHash& account_id);

    bool findCertificate(const dht::PkId& id,
                         std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb);

    /**
     * returns whether or not UPnP is enabled and active
     * ie: if it is able to make port mappings
     */
    bool getUPnPActive() const;

    /**
     * Triggered when a new TLS socket is ready to use
     * @param ok        If succeed
     * @param deviceId  Related device
     * @param vid       vid of the connection request
     * @param name      non empty if TLS was created by connectDevice()
     */
    void onTlsNegotiationDone(bool ok,
                              const DeviceId& deviceId,
                              const dht::Value::Id& vid,
                              const std::string& name = "");

    std::shared_ptr<ConnectionManager::Config> config_;

    mutable std::mt19937_64 rand;

    iOSConnectedCallback iOSConnectedCb_ {};

    std::mutex infosMtx_ {};
    // Note: Someone can ask multiple sockets, so to avoid any race condition,
    // each device can have multiple multiplexed sockets.
    std::map<CallbackId, std::shared_ptr<ConnectionInfo>> infos_ {};

    std::shared_ptr<ConnectionInfo> getInfo(const DeviceId& deviceId, const dht::Value::Id& id)
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        auto it = infos_.find({deviceId, id});
        if (it != infos_.end())
            return it->second;
        return {};
    }

    std::shared_ptr<ConnectionInfo> getConnectedInfo(const DeviceId& deviceId)
    {
        std::lock_guard<std::mutex> lk(infosMtx_);
        auto it = std::find_if(infos_.begin(), infos_.end(), [&](const auto& item) {
            auto& [key, value] = item;
            return key.first == deviceId && value && value->socket_;
        });
        if (it != infos_.end())
            return it->second;
        return {};
    }

    ChannelRequestCallback channelReqCb_ {};
    ConnectionReadyCallback connReadyCb_ {};
    onICERequestCallback iceReqCb_ {};

    /**
     * Stores callback from connectDevice
     * @note: each device needs a vector because several connectDevice can
     * be done in parallel and we only want one socket
     */
    std::mutex connectCbsMtx_ {};
    std::map<DeviceId, std::vector<PendingCb>> pendingCbs_ {};

    std::vector<PendingCb> extractPendingCallbacks(const DeviceId& deviceId,
                                                   const dht::Value::Id vid = 0)
    {
        std::vector<PendingCb> ret;
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto pendingIt = pendingCbs_.find(deviceId);
        if (pendingIt == pendingCbs_.end())
            return ret;
        auto& pendings = pendingIt->second;
        if (vid == 0) {
            ret = std::move(pendings);
        } else {
            for (auto it = pendings.begin(); it != pendings.end(); ++it) {
                if (it->vid == vid) {
                    ret.emplace_back(std::move(*it));
                    pendings.erase(it);
                    break;
                }
            }
        }
        if (pendings.empty())
            pendingCbs_.erase(pendingIt);
        return ret;
    }

    std::vector<PendingCb> getPendingCallbacks(const DeviceId& deviceId,
                                               const dht::Value::Id vid = 0)
    {
        std::vector<PendingCb> ret;
        std::lock_guard<std::mutex> lk(connectCbsMtx_);
        auto pendingIt = pendingCbs_.find(deviceId);
        if (pendingIt == pendingCbs_.end())
            return ret;
        auto& pendings = pendingIt->second;
        if (vid == 0) {
            ret = pendings;
        } else {
            std::copy_if(pendings.begin(),
                         pendings.end(),
                         std::back_inserter(ret),
                         [&](auto pending) { return pending.vid == vid; });
        }
        return ret;
    }

    std::shared_ptr<ConnectionManager::Impl> shared()
    {
        return std::static_pointer_cast<ConnectionManager::Impl>(shared_from_this());
    }
    std::shared_ptr<ConnectionManager::Impl const> shared() const
    {
        return std::static_pointer_cast<ConnectionManager::Impl const>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager::Impl> weak()
    {
        return std::static_pointer_cast<ConnectionManager::Impl>(shared_from_this());
    }
    std::weak_ptr<ConnectionManager::Impl const> weak() const
    {
        return std::static_pointer_cast<ConnectionManager::Impl const>(shared_from_this());
    }

    std::atomic_bool isDestroying_ {false};
};

void
ConnectionManager::Impl::connectDeviceStartIce(
    const std::shared_ptr<dht::crypto::PublicKey>& devicePk,
    const dht::Value::Id& vid,
    const std::string& connType,
    std::function<void(bool)> onConnected)
{
    auto deviceId = devicePk->getLongId();
    auto info = getInfo(deviceId, vid);
    if (!info) {
        onConnected(false);
        return;
    }

    std::unique_lock<std::mutex> lk(info->mutex_);
    auto& ice = info->ice_;

    if (!ice) {
        JAMI_ERR("No ICE detected");
        onConnected(false);
        return;
    }

    auto iceAttributes = ice->getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto& addr : ice->getLocalCandidates(1)) {
        icemsg << addr << "\n";
        JAMI_DBG() << "Added local ICE candidate " << addr;
    }

    // Prepare connection request as a DHT message
    PeerConnectionRequest val;

    val.id = vid; /* Random id for the message unicity */
    val.ice_msg = icemsg.str();
    val.connType = connType;

    auto value = std::make_shared<dht::Value>(std::move(val));
    value->user_type = "peer_request";

    // Send connection request through DHT
    JAMI_DBG() << "Request connection to " << deviceId;
    dht()->putEncrypted(dht::InfoHash::get(PeerConnectionRequest::key_prefix
                                           + devicePk->getId().toString()),
                        devicePk,
                        value,
                        [deviceId](bool ok) {
                            JAMI_DEBUG("Sent connection request to {:s}. Put encrypted {:s}",
                                       deviceId.toString(),
                                       (ok ? "ok" : "failed"));
                        });
    // Wait for call to onResponse() operated by DHT
    if (isDestroying_) {
        onConnected(true); // This avoid to wait new negotiation when destroying
        return;
    }

    info->onConnected_ = std::move(onConnected);
    info->waitForAnswer_ = std::make_unique<asio::steady_timer>(*Manager::instance().ioContext(), std::chrono::steady_clock::now() + DHT_MSG_TIMEOUT);
    info->waitForAnswer_->async_wait(std::bind(&ConnectionManager::Impl::onResponse, this, std::placeholders::_1, deviceId, vid));
}

void
ConnectionManager::Impl::onResponse(const asio::error_code& ec, const DeviceId& deviceId, const dht::Value::Id& vid)
{
    if (ec == asio::error::operation_aborted)
        return;
    auto info = getInfo(deviceId, vid);
    if (!info)
        return;

    std::unique_lock<std::mutex> lk(info->mutex_);
    auto& ice = info->ice_;
    if (isDestroying_) {
        info->onConnected_(true); // The destructor can wake a pending wait here.
        return;
    }
    if (!info->responseReceived_) {
        JAMI_ERR("no response from DHT to E2E request.");
        info->onConnected_(false);
        return;
    }

    if (!info->ice_) {
        info->onConnected_(false);
        return;
    }

    auto sdp = ice->parseIceCandidates(info->response_.ice_msg);

    if (not ice->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates))) {
        JAMI_WARN("start ICE failed");
        info->onConnected_(false);
        return;
    }
    info->onConnected_(true);
}


bool
ConnectionManager::Impl::connectDeviceOnNegoDone(
    const DeviceId& deviceId,
    const std::string& name,
    const dht::Value::Id& vid,
    const std::shared_ptr<dht::crypto::Certificate>& cert)
{
    auto info = getInfo(deviceId, vid);
    if (!info)
        return false;

    std::unique_lock<std::mutex> lk {info->mutex_};
    if (info->waitForAnswer_) {
        // Negotiation is done and connected, go to handshake
        // and avoid any cancellation at this point.
        info->waitForAnswer_->cancel();
    }
    auto& ice = info->ice_;
    if (!ice || !ice->isRunning()) {
        JAMI_ERR("No ICE detected or not running");
        return false;
    }

    // Build socket
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(
                                                            std::move(ice)),
                                                        true);

    // Negotiate a TLS session
    JAMI_DBG() << "Start TLS session - Initied by connectDevice(). Launched by channel: " << name
               << " - device:" << deviceId << " - vid: " << vid;
    info->tls_ = std::make_unique<TlsSocketEndpoint>(std::move(endpoint),
                                                     identity(),
                                                     dhParams(),
                                                     *cert);

    info->tls_->setOnReady(
        [w = weak(), deviceId = std::move(deviceId), vid = std::move(vid), name = std::move(name)](
            bool ok) {
            if (auto shared = w.lock())
                shared->onTlsNegotiationDone(ok, deviceId, vid, name);
        });
    return true;
}

void
ConnectionManager::Impl::connectDevice(const DeviceId& deviceId,
                                       const std::string& name,
                                       ConnectCallback cb,
                                       bool noNewSocket,
                                       bool forceNewSocket,
                                       const std::string& connType)
{
    if (!dht()) {
        cb(nullptr, deviceId);
        return;
    }
    if (deviceId.toString() == identity().second->getLongId().toString()) {
        cb(nullptr, deviceId);
        return;
    }
    findCertificate(deviceId,
                    [w = weak(),
                     deviceId,
                     name,
                     cb = std::move(cb),
                     noNewSocket,
                     forceNewSocket,
                     connType](const std::shared_ptr<dht::crypto::Certificate>& cert) {
                        if (!cert) {
                            JAMI_ERR("No valid certificate found for device %s",
                                     deviceId.to_c_str());
                            cb(nullptr, deviceId);
                            return;
                        }
                        if (auto shared = w.lock()) {
                            shared->connectDevice(cert,
                                                  name,
                                                  std::move(cb),
                                                  noNewSocket,
                                                  forceNewSocket,
                                                  connType);
                        }
                    });
}

void
ConnectionManager::Impl::connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                       const std::string& name,
                                       ConnectCallback cb,
                                       bool noNewSocket,
                                       bool forceNewSocket,
                                       const std::string& connType)
{
    // Avoid dht operation in a DHT callback to avoid deadlocks
    runOnMainThread([w = weak(),
                     name = std::move(name),
                     cert = std::move(cert),
                     cb = std::move(cb),
                     noNewSocket,
                     forceNewSocket,
                     connType] {
        auto devicePk = std::make_shared<dht::crypto::PublicKey>(cert->getPublicKey());
        auto deviceId = devicePk->getLongId();
        auto sthis = w.lock();
        if (!sthis || sthis->isDestroying_) {
            cb(nullptr, deviceId);
            return;
        }
        dht::Value::Id vid;
        auto tentatives = 0;
        do {
            vid = ValueIdDist(1, JAMI_ID_MAX_VAL)(sthis->rand);
            --tentatives;
        } while (sthis->getPendingCallbacks(deviceId, vid).size() != 0
                 && tentatives != MAX_TENTATIVES);
        if (tentatives == MAX_TENTATIVES) {
            JAMI_ERR("Couldn't get a current random channel number");
            cb(nullptr, deviceId);
            return;
        }
        auto isConnectingToDevice = false;
        {
            std::lock_guard<std::mutex> lk(sthis->connectCbsMtx_);
            // Check if already connecting
            auto pendingsIt = sthis->pendingCbs_.find(deviceId);
            isConnectingToDevice = pendingsIt != sthis->pendingCbs_.end();
            // Save current request for sendChannelRequest.
            // Note: do not return here, cause we can be in a state where first
            // socket is negotiated and first channel is pending
            // so return only after we checked the info
            if (isConnectingToDevice)
                pendingsIt->second.emplace_back(PendingCb {name, std::move(cb), vid});
            else
                sthis->pendingCbs_[deviceId] = {{name, std::move(cb), vid}};
        }

        // Check if already negotiated
        CallbackId cbId(deviceId, vid);
        if (auto info = sthis->getConnectedInfo(deviceId)) {
            std::lock_guard<std::mutex> lk(info->mutex_);
            if (info->socket_) {
                JAMI_DBG("Peer already connected to %s. Add a new channel", deviceId.to_c_str());
                info->cbIds_.emplace(cbId);
                sthis->sendChannelRequest(info->socket_, name, deviceId, vid);
                return;
            }
        }

        if (isConnectingToDevice && !forceNewSocket) {
            JAMI_DBG("Already connecting to %s, wait for the ICE negotiation", deviceId.to_c_str());
            return;
        }
        if (noNewSocket) {
            // If no new socket is specified, we don't try to generate a new socket
            for (const auto& pending : sthis->extractPendingCallbacks(deviceId, vid))
                pending.cb(nullptr, deviceId);
            return;
        }

        // Note: used when the ice negotiation fails to erase
        // all stored structures.
        auto eraseInfo = [w, cbId] {
            if (auto shared = w.lock()) {
                // If no new socket is specified, we don't try to generate a new socket
                for (const auto& pending : shared->extractPendingCallbacks(cbId.first, cbId.second))
                    pending.cb(nullptr, cbId.first);
                std::lock_guard<std::mutex> lk(shared->infosMtx_);
                shared->infos_.erase(cbId);
            }
        };

        // If no socket exists, we need to initiate an ICE connection.
        sthis->getIceOptions([w,
                              deviceId = std::move(deviceId),
                              devicePk = std::move(devicePk),
                              name = std::move(name),
                              cert = std::move(cert),
                              vid,
                              connType,
                              eraseInfo](auto&& ice_config) {
            auto sthis = w.lock();
            if (!sthis) {
                runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                return;
            }
            ice_config.tcpEnable = true;
            ice_config.onInitDone = [w,
                                     deviceId = std::move(deviceId),
                                     devicePk = std::move(devicePk),
                                     name = std::move(name),
                                     cert = std::move(cert),
                                     vid,
                                     connType,
                                     eraseInfo](bool ok) {
                auto sthis = w.lock();
                if (!sthis || !ok) {
                    JAMI_ERR("Cannot initialize ICE session.");
                    runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                    return;
                }

                dht::ThreadPool::io().run([w = std::move(w),
                                           devicePk = std::move(devicePk),
                                           vid = std::move(vid),
                                           eraseInfo,
                                           connType] {
                    auto sthis = w.lock();
                    if (!sthis) {
                        runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                        return;
                    }
                    sthis->connectDeviceStartIce(devicePk, vid, connType, [=](bool ok) {
                        if (!ok) {
                            runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                        }
                    });
                });
            };
            ice_config.onNegoDone = [w,
                                     deviceId = std::move(deviceId),
                                     name = std::move(name),
                                     cert = std::move(cert),
                                     vid,
                                     eraseInfo](bool ok) {
                auto sthis = w.lock();
                if (!sthis || !ok) {
                    JAMI_ERR("ICE negotiation failed.");
                    runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                    return;
                }

                dht::ThreadPool::io().run([w = std::move(w),
                                           deviceId = std::move(deviceId),
                                           name = std::move(name),
                                           cert = std::move(cert),
                                           vid = std::move(vid),
                                           eraseInfo = std::move(eraseInfo)] {
                    auto sthis = w.lock();
                    if (!sthis || !sthis->connectDeviceOnNegoDone(deviceId, name, vid, cert))
                        runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                });
            };

            auto info = std::make_shared<ConnectionInfo>();
            {
                std::lock_guard<std::mutex> lk(sthis->infosMtx_);
                sthis->infos_[{deviceId, vid}] = info;
            }
            std::unique_lock<std::mutex> lk {info->mutex_};
            ice_config.master = false;
            ice_config.streamsCount = 1;
            ice_config.compCountPerStream = 1; // TCP
            info->ice_ = Manager::instance().getIceTransportFactory().createUTransport("");
            if (!info->ice_) {
                JAMI_ERR("Cannot initialize ICE session.");
                eraseInfo();
                return;
            }
            // We need to detect any shutdown if the ice session is destroyed before going to the
            // TLS session;
            info->ice_->setOnShutdown([eraseInfo]() {
                runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
            });
            info->ice_->initIceInstance(ice_config);
        });
    });
}

void
ConnectionManager::Impl::sendChannelRequest(std::shared_ptr<MultiplexedSocket>& sock,
                                            const std::string& name,
                                            const DeviceId& deviceId,
                                            const dht::Value::Id& vid)
{
    auto channelSock = sock->addChannel(name);
    channelSock->onShutdown([name, deviceId, vid, w = weak()] {
        auto shared = w.lock();
        if (shared)
            for (const auto& pending : shared->extractPendingCallbacks(deviceId, vid))
                pending.cb(nullptr, deviceId);
    });
    channelSock->onReady(
        [wSock = std::weak_ptr<ChannelSocket>(channelSock), name, deviceId, vid, w = weak()]() {
            auto shared = w.lock();
            auto channelSock = wSock.lock();
            if (shared)
                for (const auto& pending : shared->extractPendingCallbacks(deviceId, vid))
                    pending.cb(channelSock, deviceId);
        });

    ChannelRequest val;
    val.name = channelSock->name();
    val.state = ChannelRequestState::REQUEST;
    val.channel = channelSock->channel();
    msgpack::sbuffer buffer(256);
    msgpack::pack(buffer, val);

    std::error_code ec;
    int res = sock->write(CONTROL_CHANNEL,
                          reinterpret_cast<const uint8_t*>(buffer.data()),
                          buffer.size(),
                          ec);
    if (res < 0) {
        // TODO check if we should handle errors here
        JAMI_ERR("sendChannelRequest failed - error: %s", ec.message().c_str());
    }
}

void
ConnectionManager::Impl::onPeerResponse(const PeerConnectionRequest& req)
{
    auto device = req.owner->getLongId();
    JAMI_INFO() << "New response received from " << device.to_c_str();
    if (auto info = getInfo(device, req.id)) {
        std::lock_guard<std::mutex> lk {info->mutex_};
        info->responseReceived_ = true;
        info->response_ = std::move(req);
        info->waitForAnswer_->expires_at(std::chrono::steady_clock::now());
        info->waitForAnswer_->async_wait(std::bind(&ConnectionManager::Impl::onResponse, this, std::placeholders::_1, device, req.id));
    } else {
        JAMI_WARN() << "Respond received, but cannot find request";
    }
}

void
ConnectionManager::Impl::onDhtConnected(const dht::crypto::PublicKey& devicePk)
{
    if (!dht())
        return;
    dht()->listen<PeerConnectionRequest>(
        dht::InfoHash::get(PeerConnectionRequest::key_prefix + devicePk.getId().toString()),
        [w = weak()](PeerConnectionRequest&& req) {
            auto shared = w.lock();
            if (!shared)
                return false;
            if (shared->isMessageTreated(to_hex_string(req.id))) {
                // Message already treated. Just ignore
                return true;
            }
            if (req.isAnswer) {
                JAMI_DBG() << "Received request answer from " << req.owner->getLongId();
            } else {
                JAMI_DBG() << "Received request from " << req.owner->getLongId();
            }
            if (req.isAnswer) {
                shared->onPeerResponse(req);
            } else {
                // Async certificate checking
                shared->dht()->findCertificate(
                    req.from,
                    [w, req = std::move(req)](
                        const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                        auto shared = w.lock();
                        if (!shared)
                            return;
                        dht::InfoHash peer_h;
                        if (foundPeerDevice(cert, peer_h)) {
#if TARGET_OS_IOS
                            if (shared->iOSConnectedCb_(req.connType, peer_h))
                                return;
#endif
                            shared->onDhtPeerRequest(req, cert);
                        } else {
                            JAMI_WARN() << "Rejected untrusted connection request from "
                                        << req.owner->getLongId();
                        }
                    });
            }

            return true;
        },
        dht::Value::UserTypeFilter("peer_request"));
}

void
ConnectionManager::Impl::onTlsNegotiationDone(bool ok,
                                              const DeviceId& deviceId,
                                              const dht::Value::Id& vid,
                                              const std::string& name)
{
    auto info = getInfo(deviceId, vid);
    if (!info)
        return;
    // Note: only handle pendingCallbacks here for TLS initied by connectDevice()
    // Note: if not initied by connectDevice() the channel name will be empty (because no channel
    // asked yet)
    auto isDhtRequest = name.empty();
    if (!ok) {
        if (isDhtRequest) {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId
                       << " - Initied by DHT request. Vid: " << vid;
            if (connReadyCb_)
                connReadyCb_(deviceId, "", nullptr);
        } else {
            JAMI_ERR() << "TLS connection failure for peer " << deviceId
                       << " - Initied by connectDevice(). Initied by channel: " << name
                       << " - vid: " << vid;
            for (const auto& pending : extractPendingCallbacks(deviceId))
                pending.cb(nullptr, deviceId);
        }
    } else {
        // The socket is ready, store it
        if (isDhtRequest) {
            JAMI_DBG() << "Connection to " << deviceId << " is ready"
                       << " - Initied by DHT request. Vid: " << vid;
        } else {
            JAMI_DBG() << "Connection to " << deviceId << " is ready"
                       << " - Initied by connectDevice(). Initied by channel: " << name
                       << " - vid: " << vid;
        }
        addNewMultiplexedSocket(deviceId, vid);
        // Finally, open the channel and launch pending callbacks
        if (info->socket_) {
            // Note: do not remove pending there it's done in sendChannelRequest
            for (const auto& pending : getPendingCallbacks(deviceId)) {
                JAMI_DBG("Send request on TLS socket for channel %s to %s",
                         pending.name.c_str(),
                         deviceId.to_c_str());
                sendChannelRequest(info->socket_, pending.name, deviceId, pending.vid);
            }
        }
    }
}

void
ConnectionManager::Impl::answerTo(IceTransport& ice,
                                  const dht::Value::Id& id,
                                  const std::shared_ptr<dht::crypto::PublicKey>& from)
{
    // NOTE: This is a shortest version of a real SDP message to save some bits
    auto iceAttributes = ice.getLocalAttributes();
    std::stringstream icemsg;
    icemsg << iceAttributes.ufrag << "\n";
    icemsg << iceAttributes.pwd << "\n";
    for (const auto& addr : ice.getLocalCandidates(1)) {
        icemsg << addr << "\n";
    }

    // Send PeerConnection response
    PeerConnectionRequest val;
    val.id = id;
    val.ice_msg = icemsg.str();
    val.isAnswer = true;
    auto value = std::make_shared<dht::Value>(std::move(val));
    value->user_type = "peer_request";

    JAMI_DBG() << "[CNX] connection accepted, DHT reply to " << from->getLongId();
    dht()->putEncrypted(dht::InfoHash::get(PeerConnectionRequest::key_prefix
                                           + from->getId().toString()),
                        from,
                        value,
                        [from](bool ok) {
                            JAMI_DEBUG("Answer to connection request from {:s}. Put encrypted {:s}",
                                       from->getLongId().toString(),
                                       (ok ? "ok" : "failed"));
                        });
}

bool
ConnectionManager::Impl::onRequestStartIce(const PeerConnectionRequest& req)
{
    auto deviceId = req.owner->getLongId();
    auto info = getInfo(deviceId, req.id);
    if (!info)
        return false;

    std::unique_lock<std::mutex> lk {info->mutex_};
    auto& ice = info->ice_;
    if (!ice) {
        JAMI_ERR("No ICE detected");
        if (connReadyCb_)
            connReadyCb_(deviceId, "", nullptr);
        return false;
    }

    auto sdp = ice->parseIceCandidates(req.ice_msg);
    answerTo(*ice, req.id, req.owner);
    if (not ice->startIce({sdp.rem_ufrag, sdp.rem_pwd}, std::move(sdp.rem_candidates))) {
        JAMI_ERR("Start ICE failed - fallback to TURN");
        ice = nullptr;
        if (connReadyCb_)
            connReadyCb_(deviceId, "", nullptr);
        return false;
    }
    return true;
}

bool
ConnectionManager::Impl::onRequestOnNegoDone(const PeerConnectionRequest& req)
{
    auto deviceId = req.owner->getLongId();
    auto info = getInfo(deviceId, req.id);
    if (!info)
        return false;

    std::unique_lock<std::mutex> lk {info->mutex_};
    auto& ice = info->ice_;
    if (!ice) {
        JAMI_ERR("No ICE detected");
        return false;
    }

    // Build socket
    auto endpoint = std::make_unique<IceSocketEndpoint>(std::shared_ptr<IceTransport>(
                                                            std::move(ice)),
                                                        false);

    // init TLS session
    auto ph = req.from;
    JAMI_DBG() << "Start TLS session - Initied by DHT request. Device:" << req.from
               << " - vid: " << req.id;
    info->tls_ = std::make_unique<TlsSocketEndpoint>(
        std::move(endpoint),
        identity(),
        dhParams(),
        [ph, w = weak()](const dht::crypto::Certificate& cert) {
            auto shared = w.lock();
            if (!shared)
                return false;
            auto crt = tls::CertificateStore::instance().getCertificate(ph.toString());
            if (!crt)
                return false;
            return crt->getPacked() == cert.getPacked();
        });

    info->tls_->setOnReady(
        [w = weak(), deviceId = std::move(deviceId), vid = std::move(req.id)](bool ok) {
            if (auto shared = w.lock())
                shared->onTlsNegotiationDone(ok, deviceId, vid);
        });
    return true;
}

void
ConnectionManager::Impl::onDhtPeerRequest(const PeerConnectionRequest& req,
                                          const std::shared_ptr<dht::crypto::Certificate>& /*cert*/)
{
    auto deviceId = req.owner->getLongId();
    JAMI_INFO() << "New connection requested by " << deviceId;
    if (!iceReqCb_ || !iceReqCb_(deviceId)) {
        JAMI_INFO("Refuse connection from %s", deviceId.toString().c_str());
        return;
    }

    // Because the connection is accepted, create an ICE socket.
    getIceOptions([w = weak(), req, deviceId](auto&& ice_config) {
        auto shared = w.lock();
        if (!shared)
            return;
        // Note: used when the ice negotiation fails to erase
        // all stored structures.
        auto eraseInfo = [w, id = req.id, deviceId] {
            if (auto shared = w.lock()) {
                // If no new socket is specified, we don't try to generate a new socket
                for (const auto& pending : shared->extractPendingCallbacks(deviceId, id))
                    pending.cb(nullptr, deviceId);
                if (shared->connReadyCb_)
                    shared->connReadyCb_(deviceId, "", nullptr);
                std::lock_guard<std::mutex> lk(shared->infosMtx_);
                shared->infos_.erase({deviceId, id});
            }
        };

        ice_config.tcpEnable = true;
        ice_config.onInitDone = [w, req, eraseInfo](bool ok) {
            auto shared = w.lock();
            if (!shared)
                return;
            if (!ok) {
                JAMI_ERR("Cannot initialize ICE session.");
                runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                return;
            }

            dht::ThreadPool::io().run(
                [w = std::move(w), req = std::move(req), eraseInfo = std::move(eraseInfo)] {
                    auto shared = w.lock();
                    if (!shared)
                        return;
                    if (!shared->onRequestStartIce(req))
                        runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                });
        };

        ice_config.onNegoDone = [w, req, eraseInfo](bool ok) {
            auto shared = w.lock();
            if (!shared)
                return;
            if (!ok) {
                JAMI_ERR("ICE negotiation failed");
                runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                return;
            }

            dht::ThreadPool::io().run(
                [w = std::move(w), req = std::move(req), eraseInfo = std::move(eraseInfo)] {
                    if (auto shared = w.lock())
                        if (!shared->onRequestOnNegoDone(req))
                            runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
                });
        };

        // Negotiate a new ICE socket
        auto info = std::make_shared<ConnectionInfo>();
        {
            std::lock_guard<std::mutex> lk(shared->infosMtx_);
            shared->infos_[{deviceId, req.id}] = info;
        }
        JAMI_INFO("accepting connection from %s", deviceId.toString().c_str());
        std::unique_lock<std::mutex> lk {info->mutex_};
        ice_config.streamsCount = 1;
        ice_config.compCountPerStream = 1; // TCP
        ice_config.master = true;
        info->ice_ = Manager::instance().getIceTransportFactory().createUTransport("");
        if (not info->ice_) {
            JAMI_ERR("Cannot initialize ICE session.");
            eraseInfo();
            return;
        }
        // We need to detect any shutdown if the ice session is destroyed before going to the TLS session;
        info->ice_->setOnShutdown([eraseInfo]() {
            runOnMainThread([eraseInfo = std::move(eraseInfo)] { eraseInfo(); });
        });
        info->ice_->initIceInstance(ice_config);
    });
}

void
ConnectionManager::Impl::addNewMultiplexedSocket(const DeviceId& deviceId, const dht::Value::Id& vid)
{
    auto info = getInfo(deviceId, vid);
    if (!info)
        return;
    info->socket_ = std::make_shared<MultiplexedSocket>(deviceId, std::move(info->tls_));
    info->socket_->setOnReady(
        [w = weak()](const DeviceId& deviceId, const std::shared_ptr<ChannelSocket>& socket) {
            if (auto sthis = w.lock())
                if (sthis->connReadyCb_)
                    sthis->connReadyCb_(deviceId, socket->name(), socket);
        });
    info->socket_->setOnRequest([w = weak()](const std::shared_ptr<dht::crypto::Certificate>& peer,
                                             const uint16_t&,
                                             const std::string& name) {
        if (auto sthis = w.lock())
            if (sthis->channelReqCb_)
                return sthis->channelReqCb_(peer, name);
        return false;
    });
    info->socket_->onShutdown([w = weak(), deviceId, vid]() {
        // Cancel current outgoing connections
        dht::ThreadPool::io().run([w, deviceId, vid] {
            auto sthis = w.lock();
            if (!sthis)
                return;

            std::set<CallbackId> ids;
            if (auto info = sthis->getInfo(deviceId, vid)) {
                std::lock_guard<std::mutex> lk(info->mutex_);
                if (info->socket_) {
                    ids = std::move(info->cbIds_);
                    info->socket_->shutdown();
                }
                if (info->ice_)
                    info->ice_->cancelOperations();
            }
            for (const auto& cbId : ids)
                for (const auto& pending : sthis->extractPendingCallbacks(cbId.first, cbId.second))
                    pending.cb(nullptr, deviceId);

            std::lock_guard<std::mutex> lk(sthis->infosMtx_);
            sthis->infos_.erase({deviceId, vid});
        });
    });
}

const std::shared_future<tls::DhParams>
ConnectionManager::Impl::dhParams() const
{
    return dht::ThreadPool::computation().get<tls::DhParams>(
        std::bind(tls::DhParams::loadDhParams, fileutils::get_cache_dir() + DIR_SEPARATOR_STR "dhParams"));
    ;
}

template<typename ID = dht::Value::Id>
std::set<ID, std::less<>>
loadIdList(const std::string& path)
{
    std::set<ID, std::less<>> ids;
    std::ifstream file = fileutils::ifstream(path);
    if (!file.is_open()) {
        JAMI_DBG("Could not load %s", path.c_str());
        return ids;
    }
    std::string line;
    while (std::getline(file, line)) {
        if constexpr (std::is_same<ID, std::string>::value) {
            ids.emplace(std::move(line));
        } else if constexpr (std::is_integral<ID>::value) {
            ID vid;
            if (auto [p, ec] = std::from_chars(line.data(), line.data() + line.size(), vid, 16);
                ec == std::errc()) {
                ids.emplace(vid);
            }
        }
    }
    return ids;
}

template<typename List = std::set<dht::Value::Id>>
void
saveIdList(const std::string& path, const List& ids)
{
    std::ofstream file = fileutils::ofstream(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        JAMI_ERR("Could not save to %s", path.c_str());
        return;
    }
    for (auto& c : ids)
        file << std::hex << c << "\n";
}

void
ConnectionManager::Impl::loadTreatedMessages()
{
    std::lock_guard<std::mutex> lock(messageMutex_);
    auto path = cachePath_ + DIR_SEPARATOR_STR "treatedMessages";
    treatedMessages_ = loadIdList<std::string>(path);
    if (treatedMessages_.empty()) {
        auto messages = loadIdList(path);
        for (const auto& m : messages)
            treatedMessages_.emplace(to_hex_string(m));
    }
}

void
ConnectionManager::Impl::saveTreatedMessages() const
{
    dht::ThreadPool::io().run([w = weak()]() {
        if (auto sthis = w.lock()) {
            auto& this_ = *sthis;
            std::lock_guard<std::mutex> lock(this_.messageMutex_);
            fileutils::check_dir(this_.cachePath_.c_str());
            saveIdList<decltype(this_.treatedMessages_)>(this_.cachePath_
                                                             + DIR_SEPARATOR_STR "treatedMessages",
                                                         this_.treatedMessages_);
        }
    });
}

bool
ConnectionManager::Impl::isMessageTreated(std::string_view id)
{
    std::lock_guard<std::mutex> lock(messageMutex_);
    auto res = treatedMessages_.emplace(id);
    if (res.second) {
        saveTreatedMessages();
        return false;
    }
    return true;
}

/**
 * returns whether or not UPnP is enabled and active_
 * ie: if it is able to make port mappings
 */
bool
ConnectionManager::Impl::getUPnPActive() const
{
    return config_->getUPnPActive();
}

IpAddr
ConnectionManager::Impl::getPublishedIpAddress(uint16_t family) const
{
    if (family == AF_INET)
        return publishedIp_[0];
    if (family == AF_INET6)
        return publishedIp_[1];

    assert(family == AF_UNSPEC);

    // If family is not set, prefere IPv4 if available. It's more
    // likely to succeed behind NAT.
    if (publishedIp_[0])
        return publishedIp_[0];
    if (publishedIp_[1])
        return publishedIp_[1];
    return {};
}

void
ConnectionManager::Impl::setPublishedAddress(const IpAddr& ip_addr)
{
    if (ip_addr.getFamily() == AF_INET) {
        publishedIp_[0] = ip_addr;
    } else {
        publishedIp_[1] = ip_addr;
    }
}

void
ConnectionManager::Impl::storeActiveIpAddress(std::function<void()>&& cb)
{
    dht()->getPublicAddress([this, cb = std::move(cb)](std::vector<dht::SockAddr>&& results) {
        bool hasIpv4 {false}, hasIpv6 {false};
        for (auto& result : results) {
            auto family = result.getFamily();
            if (family == AF_INET) {
                if (not hasIpv4) {
                    hasIpv4 = true;
                    JAMI_DBG("Store DHT public IPv4 address : %s", result.toString().c_str());
                    setPublishedAddress(*result.get());
                    if (config_->upnpCtrl_) {
                        config_->upnpCtrl_->setPublicAddress(*result.get());
                    }
                }
            } else if (family == AF_INET6) {
                if (not hasIpv6) {
                    hasIpv6 = true;
                    JAMI_DBG("Store DHT public IPv6 address : %s", result.toString().c_str());
                    setPublishedAddress(*result.get());
                }
            }
            if (hasIpv4 and hasIpv6)
                break;
        }
        if (cb)
            cb();
    });
}

void
ConnectionManager::Impl::getIceOptions(std::function<void(IceTransportOptions&&)> cb) noexcept
{
    storeActiveIpAddress([this, cb = std::move(cb)] {
        IceTransportOptions opts = ConnectionManager::Impl::getIceOptions();
        auto publishedAddr = getPublishedIpAddress();

        if (publishedAddr) {
            auto interfaceAddr = ip_utils::getInterfaceAddr(getLocalInterface(),
                                                            publishedAddr.getFamily());
            if (interfaceAddr) {
                opts.accountLocalAddr = interfaceAddr;
                opts.accountPublicAddr = publishedAddr;
            }
        }
        if (cb)
            cb(std::move(opts));
    });
}

IceTransportOptions
ConnectionManager::Impl::getIceOptions() const noexcept
{
    IceTransportOptions opts;
    opts.upnpEnable = getUPnPActive();

    if (config_->stunEnabled_)
        opts.stunServers.emplace_back(StunServerInfo().setUri(config_->stunServer_));
    if (config_->turnEnabled_) {
        auto cached = false;
        std::lock_guard<std::mutex> lk(config_->cachedTurnMutex_);
        cached = config_->cacheTurnV4_ || config_->cacheTurnV6_;
        if (config_->cacheTurnV4_ && *config_->cacheTurnV4_) {
            opts.turnServers.emplace_back(TurnServerInfo()
                                              .setUri(config_->cacheTurnV4_->toString(true))
                                              .setUsername(config_->turnServerUserName_)
                                              .setPassword(config_->turnServerPwd_)
                                              .setRealm(config_->turnServerRealm_));
        }
        // NOTE: first test with ipv6 turn was not concluant and resulted in multiple
        // co issues. So this needs some debug. for now just disable
        // if (cacheTurnV6_ && *cacheTurnV6_) {
        //    opts.turnServers.emplace_back(TurnServerInfo()
        //                                      .setUri(cacheTurnV6_->toString(true))
        //                                      .setUsername(turnServerUserName_)
        //                                      .setPassword(turnServerPwd_)
        //                                      .setRealm(turnServerRealm_));
        //}
        // Nothing cached, so do the resolution
        if (!cached) {
            opts.turnServers.emplace_back(TurnServerInfo()
                                              .setUri(config_->turnServer_)
                                              .setUsername(config_->turnServerUserName_)
                                              .setPassword(config_->turnServerPwd_)
                                              .setRealm(config_->turnServerRealm_));
        }
    }
    return opts;
}

bool
ConnectionManager::Impl::foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt,
                                         dht::InfoHash& account_id)
{
    if (not crt)
        return false;

    auto top_issuer = crt;
    while (top_issuer->issuer)
        top_issuer = top_issuer->issuer;

    // Device certificate can't be self-signed
    if (top_issuer == crt) {
        JAMI_WARN("Found invalid peer device: %s", crt->getLongId().toString().c_str());
        return false;
    }

    // Check peer certificate chain
    // Trust store with top issuer as the only CA
    dht::crypto::TrustList peer_trust;
    peer_trust.add(*top_issuer);
    if (not peer_trust.verify(*crt)) {
        JAMI_WARN("Found invalid peer device: %s", crt->getLongId().toString().c_str());
        return false;
    }

    // Check cached OCSP response
    if (crt->ocspResponse and crt->ocspResponse->getCertificateStatus() != GNUTLS_OCSP_CERT_GOOD) {
        JAMI_ERR("Certificate %s is disabled by cached OCSP response", crt->getLongId().to_c_str());
        return false;
    }

    account_id = crt->issuer->getId();
    JAMI_WARN("Found peer device: %s account:%s CA:%s",
              crt->getLongId().toString().c_str(),
              account_id.toString().c_str(),
              top_issuer->getId().toString().c_str());
    return true;
}

bool
ConnectionManager::Impl::findCertificate(
    const dht::PkId& id, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb)
{
    if (auto cert = tls::CertificateStore::instance().getCertificate(id.toString())) {
        if (cb)
            cb(cert);
    } else if (cb)
        cb(nullptr);
    return true;
}

ConnectionManager::ConnectionManager(std::shared_ptr<ConnectionManager::Config> config_)
    : pimpl_ {std::make_shared<Impl>(config_)}
{}

ConnectionManager::~ConnectionManager()
{
    if (pimpl_)
        pimpl_->shutdown();
}

void
ConnectionManager::connectDevice(const DeviceId& deviceId,
                                 const std::string& name,
                                 ConnectCallback cb,
                                 bool noNewSocket,
                                 bool forceNewSocket,
                                 const std::string& connType)
{
    pimpl_->connectDevice(deviceId, name, std::move(cb), noNewSocket, forceNewSocket, connType);
}

void
ConnectionManager::connectDevice(const std::shared_ptr<dht::crypto::Certificate>& cert,
                                 const std::string& name,
                                 ConnectCallback cb,
                                 bool noNewSocket,
                                 bool forceNewSocket,
                                 const std::string& connType)
{
    pimpl_->connectDevice(cert, name, std::move(cb), noNewSocket, forceNewSocket, connType);
}

bool
ConnectionManager::isConnecting(const DeviceId& deviceId, const std::string& name) const
{
    auto pending = pimpl_->getPendingCallbacks(deviceId);
    return std::find_if(pending.begin(), pending.end(), [&](auto p) { return p.name == name; })
           != pending.end();
}

void
ConnectionManager::closeConnectionsWith(const std::string& peerUri)
{
    std::vector<std::shared_ptr<ConnectionInfo>> connInfos;
    std::set<DeviceId> peersDevices;
    {
        std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
        for (auto iter = pimpl_->infos_.begin(); iter != pimpl_->infos_.end();) {
            auto const& [key, value] = *iter;
            auto deviceId = key.first;
            auto cert = tls::CertificateStore::instance().getCertificate(deviceId.toString());
            if (cert && cert->issuer && peerUri == cert->issuer->getId().toString()) {
                connInfos.emplace_back(value);
                peersDevices.emplace(deviceId);
                iter = pimpl_->infos_.erase(iter);
            } else {
                iter++;
            }
        }
    }
    // Stop connections to all peers devices
    for (const auto& deviceId : peersDevices) {
        for (const auto& pending : pimpl_->extractPendingCallbacks(deviceId))
            pending.cb(nullptr, deviceId);
        // This will close the TLS Session
        pimpl_->removeUnusedConnections(deviceId);
    }
    for (auto& info : connInfos) {
        if (info->ice_)
            info->ice_->cancelOperations();
        if (info->socket_)
            info->socket_->shutdown();
        if (info->waitForAnswer_)
            info->waitForAnswer_->cancel();
        if (info->ice_) {
            std::unique_lock<std::mutex> lk {info->mutex_};
            dht::ThreadPool::io().run(
                [ice = std::shared_ptr<IceTransport>(std::move(info->ice_))] {});
        }
    }
}

void
ConnectionManager::onDhtConnected(const dht::crypto::PublicKey& devicePk)
{
    pimpl_->onDhtConnected(devicePk);
}

void
ConnectionManager::onICERequest(onICERequestCallback&& cb)
{
    pimpl_->iceReqCb_ = std::move(cb);
}

void
ConnectionManager::onChannelRequest(ChannelRequestCallback&& cb)
{
    pimpl_->channelReqCb_ = std::move(cb);
}

void
ConnectionManager::onConnectionReady(ConnectionReadyCallback&& cb)
{
    pimpl_->connReadyCb_ = std::move(cb);
}

void
ConnectionManager::oniOSConnected(iOSConnectedCallback&& cb)
{
    pimpl_->iOSConnectedCb_ = std::move(cb);
}

std::size_t
ConnectionManager::activeSockets() const
{
    std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
    return pimpl_->infos_.size();
}

void
ConnectionManager::monitor() const
{
    std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
    JAMI_DBG("ConnectionManager current status:");
    for (const auto& [_, ci] : pimpl_->infos_) {
        if (ci->socket_)
            ci->socket_->monitor();
    }
    JAMI_DBG("ConnectionManager end status.");
}

void
ConnectionManager::connectivityChanged()
{
    std::lock_guard<std::mutex> lk(pimpl_->infosMtx_);
    for (const auto& [_, ci] : pimpl_->infos_) {
        if (ci->socket_)
            ci->socket_->sendBeacon();
    }
}

void
ConnectionManager::getIceOptions(std::function<void(IceTransportOptions&&)> cb) noexcept
{
    return pimpl_->getIceOptions(std::move(cb));
}

IceTransportOptions
ConnectionManager::getIceOptions() const noexcept
{
    return pimpl_->getIceOptions();
}

IpAddr
ConnectionManager::getPublishedIpAddress(uint16_t family) const
{
    return pimpl_->getPublishedIpAddress(family);
}

void
ConnectionManager::setPublishedAddress(const IpAddr& ip_addr)
{
    return pimpl_->setPublishedAddress(ip_addr);
}

void
ConnectionManager::storeActiveIpAddress(std::function<void()>&& cb)
{
    return pimpl_->storeActiveIpAddress(std::move(cb));
}

std::shared_ptr<ConnectionManager::Config>
ConnectionManager::getConfig()
{
    return pimpl_->config_;
}

} // namespace jami
