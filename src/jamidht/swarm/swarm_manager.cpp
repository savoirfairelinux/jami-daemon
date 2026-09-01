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

#include "swarm_manager.h"
#include "jamidht/timestamp.h"
#include <dhtnet/multiplexed_socket.h>
#include <dhtnet/channel_utils.h>
#include <opendht/thread_pool.h>

namespace jami {

using namespace swarm_protocol;

static bool
isNewerLease(const MobileLease& candidate, const MobileLease& reference)
{
    return candidate.expires_at > reference.expires_at
           || (candidate.expires_at == reference.expires_at && candidate.issued_at > reference.issued_at);
}

SwarmManager::SwarmManager(const NodeId& id,
                           bool isMobile,
                           const std::mt19937_64& rand,
                           ToConnectCb&& toConnectCb,
                           std::string conversationId,
                           MobileLeaseProvider mobileLeaseProvider,
                           MobileLeaseIssuerValidator mobileLeaseIssuerValidator,
                           CertificateProvider certificateProvider,
                           CertificateFetcher certificateFetcher)
    : id_(id)
    , isMobile_(isMobile)
    , conversationId_(std::move(conversationId))
    , rd(rand)
    , mobileLeaseProvider_(std::move(mobileLeaseProvider))
    , mobileLeaseIssuerValidator_(std::move(mobileLeaseIssuerValidator))
    , certificateProvider_(std::move(certificateProvider))
    , certificateFetcher_(std::move(certificateFetcher))
    , toConnectCb_(toConnectCb)
{
    routing_table.setId(id);
}

SwarmManager::~SwarmManager()
{
    if (!isShutdown_)
        shutdown();
}

bool
SwarmManager::setKnownNodes(const std::vector<NodeId>& known_nodes)
{
    isShutdown_ = false;
    std::vector<NodeId> newNodes;
    {
        std::lock_guard lock(mutex);
        for (const auto& nodeId : known_nodes) {
            if (addKnownNode(nodeId)) {
                newNodes.emplace_back(nodeId);
            }
        }
    }

    if (newNodes.empty())
        return false;

    dht::ThreadPool::io().run([w = weak(), newNodes = std::move(newNodes)] {
        auto shared = w.lock();
        if (!shared)
            return;
        // If we detect a new node which already got a TCP link
        // we can use it to speed-up the bootstrap (because opening
        // a new channel will be easy)
        std::set<NodeId> toConnect;
        for (const auto& nodeId : newNodes) {
            if (shared->toConnectCb_ && shared->toConnectCb_(nodeId))
                toConnect.emplace(nodeId);
        }
        shared->maintainBuckets(toConnect);
    });
    return true;
}

void
SwarmManager::setMobileNodes(const std::vector<NodeId>& mobile_nodes)
{
    bool changed = false;
    {
        std::lock_guard lock(mutex);
        const auto now = toSecondsSinceEpoch(std::chrono::system_clock::now());
        if (!conversationId_.empty() && now >= LEGACY_MOBILE_NODE_SUNSET)
            return;
        for (const auto& nodeId : mobile_nodes) {
            changed |= addMobileNodes(nodeId);
            if (!conversationId_.empty() && !mobileNodeLeases_.contains(nodeId))
                changed |= legacyMobileNodeExpiries_.try_emplace(nodeId, LEGACY_MOBILE_NODE_SUNSET).second;
        }
        scheduleMobileLeaseExpiryInternal();
    }
    if (changed)
        emitMobileNodesChanged();
}

void
SwarmManager::setMobileNodes(const std::vector<MobileNodeInfo>& mobile_nodes, bool requireLease)
{
    bool changed = false;
    size_t records = 0;
    for (const auto& mobile : mobile_nodes) {
        if (records++ == MAX_MOBILE_NODE_INFOS)
            break;
        changed |= setMobileNodeInfo(mobile, requireLease);
    }
    if (changed)
        emitMobileNodesChanged();
}

bool
SwarmManager::setMobileNodeInfo(const MobileNodeInfo& mobile,
                                bool requireLease,
                                const std::shared_ptr<dhtnet::ChannelSocketInterface>& source)
{
    if (mobile.id == id_)
        return false;

    if (!mobile.lease) {
        if (requireLease)
            return false;
        const auto now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
        if (!conversationId_.empty() && now >= LEGACY_MOBILE_NODE_SUNSET)
            return false;
        std::lock_guard lock(mutex);
        bool changed = addMobileNodes(mobile.id);
        if (!conversationId_.empty() && !mobileNodeLeases_.contains(mobile.id)) {
            changed |= legacyMobileNodeExpiries_.try_emplace(mobile.id, LEGACY_MOBILE_NODE_SUNSET).second;
            scheduleMobileLeaseExpiryInternal();
        }
        return changed;
    }

    const auto& lease = *mobile.lease;
    if (lease.device_id != mobile.id || !precheckLease(lease))
        return false;

    {
        // Gossip re-announces the same lease every round: skip the whole
        // resolution when what we already verified is at least as good.
        std::lock_guard lock(mutex);
        auto known = mobileNodeLeases_.find(mobile.id);
        if (known != mobileNodeLeases_.end() && !isNewerLease(lease, known->second))
            return false;
    }

    // The certificate is never gossiped: resolve it from the account certificate
    // store, which already holds every device we ever connected to (the swarm
    // channel pins its TLS peer certificate) and everything resolved before.
    if (certificateProvider_) {
        if (auto certificate = certificateProvider_(mobile.id)) {
            if (!verifyLease(*certificate, lease))
                return false;
            std::lock_guard lock(mutex);
            return commitLeaseInternal(lease);
        }
    }

    std::lock_guard lock(mutex);
    enqueuePendingLeaseInternal(lease, source);
    return false;
}

void
SwarmManager::addChannel(const std::shared_ptr<dhtnet::ChannelSocketInterface>& channel)
{
    // JAMI_WARNING("[SwarmManager {}] addChannel! with {}", fmt::ptr(this), channel->deviceId().to_view());
    if (channel) {
        auto emit = false;
        auto added = false;
        {
            std::lock_guard lock(mutex);
            emit = routing_table.isEmpty();
            auto bucket = routing_table.findBucket(channel->deviceId());
            added = routing_table.addNode(channel, bucket);
        }
        if (added) {
            std::error_code ec;
            resetNodeExpiry(ec, channel, id_);
        }
        receiveMessage(channel);
        if (emit && onConnectionChanged_) {
            // If it's the first channel we add, we're now connected!
            JAMI_DEBUG("[SwarmManager {}] Bootstrap: Connected!", fmt::ptr(this));
            onConnectionChanged_(true);
        }
    }
}

void
SwarmManager::removeNode(const NodeId& nodeId)
{
    std::unique_lock lk(mutex);
    if (isConnectedWith(nodeId)) {
        removeNodeInternal(nodeId);
        lk.unlock();
        maintainBuckets();
    }
}

void
SwarmManager::changeMobility(const NodeId& nodeId, bool isMobile)
{
    {
        std::lock_guard lock(mutex);
        auto bucket = routing_table.findBucket(nodeId);
        bucket->changeMobility(nodeId, isMobile);
    }
    emitMobileNodesChanged();
}

bool
SwarmManager::isConnectedWith(const NodeId& deviceId)
{
    return routing_table.hasNode(deviceId);
}

void
SwarmManager::shutdown()
{
    if (isShutdown_) {
        return;
    }
    isShutdown_ = true;
    std::lock_guard lock(mutex);
    mobileLeaseExpiryTimer_.cancel();
    for (auto& [peer, state] : outstandingCertRequests_)
        if (state.timer)
            state.timer->cancel();
    outstandingCertRequests_.clear();
    routing_table.shutdownAllNodes();
}

void
SwarmManager::restart()
{
    isShutdown_ = false;
    std::lock_guard lock(mutex);
    scheduleMobileLeaseExpiryInternal();
}

bool
SwarmManager::addKnownNode(const NodeId& nodeId)
{
    return routing_table.addKnownNode(nodeId);
}

bool
SwarmManager::addMobileNodes(const NodeId& nodeId)
{
    if (id_ != nodeId) {
        return routing_table.addMobileNode(nodeId);
    }
    return false;
}

bool
SwarmManager::isMobileNodeCurrentInternal(const NodeId& nodeId, int64_t now) const
{
    if (auto lease = mobileNodeLeases_.find(nodeId); lease != mobileNodeLeases_.end())
        return lease->second.expires_at > now;
    if (auto legacy = legacyMobileNodeExpiries_.find(nodeId); legacy != legacyMobileNodeExpiries_.end())
        return legacy->second > now;
    return conversationId_.empty();
}

bool
SwarmManager::precheckLease(const MobileLease& lease) const
{
    if (lease.format_version != 1 || lease.conversation_id != conversationId_ || lease.conversation_id.empty()
        || lease.conversation_id.size() > MAX_MOBILE_LEASE_IDENTIFIER_SIZE || lease.signature.empty()
        || lease.signature.size() > MAX_MOBILE_LEASE_SIGNATURE_SIZE || !lease.issuer_id
        || !mobileLeaseIssuerValidator_ || !mobileLeaseIssuerValidator_(lease.issuer_id))
        return false;

    constexpr auto MAX_CLOCK_SKEW = std::chrono::seconds(5 * 60);
    const auto now = std::chrono::system_clock::now();
    const auto issued = std::chrono::system_clock::time_point(std::chrono::seconds(lease.issued_at));
    const auto expires = std::chrono::system_clock::time_point(std::chrono::seconds(lease.expires_at));
    if (issued > now + MAX_CLOCK_SKEW || expires <= now || expires <= issued
        || expires - issued > MAX_MOBILE_LEASE_DURATION)
        return false;
    return true;
}

bool
SwarmManager::verifyLease(const dht::crypto::Certificate& certificate, const MobileLease& lease) const
{
    try {
        if (certificate.getLongId() != lease.device_id || !certificate.issuer
            || certificate.issuer->getId() != lease.issuer_id)
            return false;
        dht::crypto::TrustList trust;
        trust.add(*certificate.issuer);
        if (!trust.verify(certificate))
            return false;
        auto certificateExpiry = toSecondsSinceEpoch(certificate.getExpiration());
        if (lease.expires_at > certificateExpiry)
            return false;
        const auto payload = mobileLeasePayload(lease);
        return certificate.getPublicKey().checkSignature(payload, lease.signature);
    } catch (const std::exception& e) {
        JAMI_WARNING("Ignoring invalid mobile lease for {}: {}", lease.device_id, e.what());
        return false;
    }
}

bool
SwarmManager::commitLeaseInternal(const MobileLease& lease)
{
    auto pending = pendingMobileLeases_.find(lease.device_id);
    if (pending != pendingMobileLeases_.end()) {
        if (!isNewerLease(pending->second.lease, lease))
            pendingMobileLeases_.erase(pending);
    }

    bool changed = addMobileNodes(lease.device_id);
    auto current = mobileNodeLeases_.find(lease.device_id);
    if (current == mobileNodeLeases_.end() || isNewerLease(lease, current->second)) {
        mobileNodeLeases_.insert_or_assign(lease.device_id, lease);
        legacyMobileNodeExpiries_.erase(lease.device_id);
        changed = true;
    }
    scheduleMobileLeaseExpiryInternal();
    return changed;
}

void
SwarmManager::enqueuePendingLeaseInternal(const MobileLease& lease,
                                          const std::shared_ptr<dhtnet::ChannelSocketInterface>& source)
{
    const auto& nodeId = lease.device_id;
    auto pending = pendingMobileLeases_.find(nodeId);
    if (pending != pendingMobileLeases_.end()) {
        if (isNewerLease(lease, pending->second.lease))
            pending->second.lease = lease;
        if (source)
            pending->second.source = source;
    } else {
        if (pendingMobileLeases_.size() >= MAX_PENDING_MOBILE_LEASES) {
            // Evict the entry that would expire first: it is the least useful to keep resolving.
            auto oldest = std::min_element(pendingMobileLeases_.begin(),
                                           pendingMobileLeases_.end(),
                                           [](const auto& a, const auto& b) {
                                               return a.second.lease.expires_at < b.second.lease.expires_at;
                                           });
            if (oldest != pendingMobileLeases_.end() && oldest->second.lease.expires_at >= lease.expires_at)
                return;
            pendingMobileLeases_.erase(oldest);
        }
        pendingMobileLeases_.emplace(nodeId, PendingLease {lease, source});
    }

    if (certFetchInFlight_.count(nodeId))
        return;

    if (source) {
        certFetchInFlight_.emplace(nodeId);
        dht::ThreadPool::io().run([w = weak(), source, nodeId] {
            if (auto shared = w.lock())
                shared->requestCertificates(source, {nodeId});
        });
    } else {
        certFetchInFlight_.emplace(nodeId);
        dht::ThreadPool::io().run([w = weak(), nodeId] {
            if (auto shared = w.lock())
                shared->fetchCertificateFromDht(nodeId);
        });
    }
}

void
SwarmManager::abandonLeaseInternal(const NodeId& nodeId)
{
    certFetchInFlight_.erase(nodeId);
    pendingMobileLeases_.erase(nodeId);
}

void
SwarmManager::requestCertificates(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                                  const std::vector<NodeId>& ids)
{
    if (!socket || ids.empty() || isShutdown_) {
        std::lock_guard lock(mutex);
        for (const auto& id : ids)
            certFetchInFlight_.erase(id);
        return;
    }
    const auto peer = NodeId(socket->deviceId());
    CertRequest request;
    {
        std::lock_guard lock(mutex);
        auto& state = outstandingCertRequests_[peer];
        if (state.timer) {
            // One request in flight per peer: drop the resolution so that the
            // next gossip round asks again.
            for (const auto& id : ids)
                certFetchInFlight_.erase(id);
            return;
        }
        for (const auto& id : ids) {
            if (request.ids.size() >= MAX_CERT_REQUEST_IDS) {
                certFetchInFlight_.erase(id);
                continue;
            }
            request.ids.emplace_back(id);
        }
        if (request.ids.empty()) {
            outstandingCertRequests_.erase(peer);
            return;
        }
        state.ids.insert(request.ids.begin(), request.ids.end());
        state.timer = std::make_shared<asio::steady_timer>(*Manager::instance().ioContext());
        state.timer->expires_after(CERT_REQUEST_TIMEOUT);
        state.timer->async_wait([w = weak(), peer](const asio::error_code& ec) {
            if (ec == asio::error::operation_aborted)
                return;
            auto shared = w.lock();
            if (!shared)
                return;
            std::vector<NodeId> unanswered;
            {
                std::lock_guard lock(shared->mutex);
                auto it = shared->outstandingCertRequests_.find(peer);
                if (it == shared->outstandingCertRequests_.end())
                    return;
                unanswered.assign(it->second.ids.begin(), it->second.ids.end());
                shared->outstandingCertRequests_.erase(it);
            }
            // The peer did not answer: fall back to the DHT.
            for (const auto& id : unanswered)
                shared->fetchCertificateFromDht(id);
        });
    }

    Message msg;
    msg.is_mobile = isMobile_;
    msg.cert_request = std::move(request);

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(msg);

    std::error_code ec;
    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    if (ec)
        JAMI_ERROR("{}", ec.message());
}

void
SwarmManager::onCertRequest(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, const CertRequest& request)
{
    if (!socket || request.ids.empty() || !certificateProvider_)
        return;

    std::vector<NodeId> toAnswer;
    {
        std::lock_guard lock(mutex);
        for (const auto& id : request.ids) {
            if (toAnswer.size() >= MAX_CERT_REQUEST_IDS)
                break;
            // A peer must not be able to use the swarm as a generic certificate
            // oracle: only serve certificates for devices we ourselves announced
            // as mobile in this conversation, plus our own when we are mobile.
            if (id != id_ && !mobileNodeLeases_.count(id))
                continue;
            toAnswer.emplace_back(id);
        }
    }
    if (toAnswer.empty())
        return;

    CertResponse response;
    size_t totalSize = 0;
    for (const auto& id : toAnswer) {
        auto certificate = certificateProvider_(id);
        if (!certificate)
            continue;
        auto packed = certificate->getPacked();
        if (packed.empty() || packed.size() > MAX_MOBILE_CERTIFICATE_SIZE
            || totalSize + packed.size() > MAX_MOBILE_CERTIFICATES_SIZE)
            continue;
        totalSize += packed.size();
        response.certificates.emplace_back(std::move(packed));
    }
    if (response.certificates.empty())
        return;

    Message msg;
    msg.is_mobile = isMobile_;
    msg.cert_response = std::move(response);

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(msg);

    std::error_code ec;
    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    if (ec)
        JAMI_ERROR("{}", ec.message());
}

void
SwarmManager::onCertResponse(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, const CertResponse& response)
{
    if (!socket)
        return;
    const auto peer = NodeId(socket->deviceId());

    std::set<NodeId> requested;
    {
        std::lock_guard lock(mutex);
        auto it = outstandingCertRequests_.find(peer);
        if (it == outstandingCertRequests_.end())
            return; // Unsolicited.
        requested = std::move(it->second.ids);
        if (it->second.timer)
            it->second.timer->cancel();
        outstandingCertRequests_.erase(it);
    }

    size_t totalSize = 0;
    for (const auto& packed : response.certificates) {
        if (packed.empty() || packed.size() > MAX_MOBILE_CERTIFICATE_SIZE
            || totalSize + packed.size() > MAX_MOBILE_CERTIFICATES_SIZE)
            break;
        totalSize += packed.size();
        try {
            auto certificate = std::make_shared<dht::crypto::Certificate>(packed);
            const auto nodeId = certificate->getLongId();
            if (!requested.erase(nodeId))
                continue; // Not something we asked for.
            onCertificateResolved(nodeId, certificate);
        } catch (const std::exception& e) {
            JAMI_WARNING("Ignoring invalid certificate from {}: {}", peer, e.what());
        }
    }

    // Whatever the peer could not provide is worth one DHT lookup.
    for (const auto& nodeId : requested)
        fetchCertificateFromDht(nodeId);
}

void
SwarmManager::fetchCertificateFromDht(const NodeId& nodeId)
{
    if (isShutdown_)
        return;
    if (!certificateFetcher_) {
        std::lock_guard lock(mutex);
        abandonLeaseInternal(nodeId);
        return;
    }
    certificateFetcher_(nodeId, [w = weak(), nodeId](const std::shared_ptr<dht::crypto::Certificate>& certificate) {
        auto shared = w.lock();
        if (!shared)
            return;
        if (certificate && certificate->getLongId() == nodeId)
            shared->onCertificateResolved(nodeId, certificate);
        else {
            std::lock_guard lock(shared->mutex);
            shared->abandonLeaseInternal(nodeId);
        }
    });
}

void
SwarmManager::onCertificateResolved(const NodeId& nodeId, const std::shared_ptr<dht::crypto::Certificate>& certificate)
{
    if (!certificate)
        return;

    std::optional<MobileLease> lease;
    {
        std::lock_guard lock(mutex);
        certFetchInFlight_.erase(nodeId);
        auto pending = pendingMobileLeases_.find(nodeId);
        if (pending == pendingMobileLeases_.end())
            return;
        lease = pending->second.lease;
    }

    // Re-run the cheap checks: the lease may have expired while we were resolving.
    if (!precheckLease(*lease) || !verifyLease(*certificate, *lease)) {
        std::lock_guard lock(mutex);
        auto pending = pendingMobileLeases_.find(nodeId);
        if (pending != pendingMobileLeases_.end() && !isNewerLease(pending->second.lease, *lease))
            abandonLeaseInternal(nodeId);
        return;
    }

    bool changed = false;
    {
        std::lock_guard lock(mutex);
        changed = commitLeaseInternal(*lease);
    }
    if (changed)
        emitMobileNodesChanged();
}

std::optional<MobileNodeInfo>
SwarmManager::localMobileNodeInfo()
{
    if (!isMobile_ || !mobileLeaseProvider_)
        return std::nullopt;

    std::lock_guard renewalLock(mobileLeaseRenewalMtx_);

    auto renewalThresholdTime = toSecondsSinceEpoch(std::chrono::system_clock::now() + MOBILE_LEASE_RENEWAL_THRESHOLD);
    {
        std::lock_guard lock(mutex);
        if (localMobileNodeInfo_ && localMobileNodeInfo_->lease
            && localMobileNodeInfo_->lease->expires_at > renewalThresholdTime)
            return localMobileNodeInfo_;
    }

    auto renewed = mobileLeaseProvider_();
    if (!renewed || renewed->id != id_ || !renewed->lease || renewed->lease->device_id != id_
        || !precheckLease(*renewed->lease))
        return std::nullopt;
    std::lock_guard lock(mutex);
    localMobileNodeInfo_ = std::move(renewed);
    return localMobileNodeInfo_;
}

void
SwarmManager::scheduleMobileLeaseExpiryInternal()
{
    mobileLeaseExpiryTimer_.cancel();
    if ((mobileNodeLeases_.empty() && legacyMobileNodeExpiries_.empty()) || isShutdown_)
        return;

    auto nearestExpiry = std::numeric_limits<int64_t>::max();
    for (const auto& [node, lease] : mobileNodeLeases_)
        nearestExpiry = std::min(nearestExpiry, lease.expires_at);
    for (const auto& [node, expiry] : legacyMobileNodeExpiries_)
        nearestExpiry = std::min(nearestExpiry, expiry);
    auto expiryTime = timePointFromSeconds(nearestExpiry);
    const auto now = std::chrono::system_clock::now();
    constexpr auto MAX_TIMER_DELAY_SECONDS = std::chrono::minutes(1);
    const auto delay = std::min<std::chrono::system_clock::duration>(expiryTime > now ? expiryTime - now : std::chrono::seconds(0), MAX_TIMER_DELAY_SECONDS);
    mobileLeaseExpiryTimer_.expires_after(delay);
    mobileLeaseExpiryTimer_.async_wait([w = weak()](const asio::error_code& ec) {
        if (auto shared = w.lock())
            shared->expireMobileLeases(ec);
    });
}

void
SwarmManager::expireMobileLeases(const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted)
        return;

    bool changed = false;
    {
        std::lock_guard lock(mutex);
        auto now = toSecondsSinceEpoch(std::chrono::system_clock::now());
        for (auto it = mobileNodeLeases_.begin(); it != mobileNodeLeases_.end();) {
            if (it->second.expires_at > now) {
                ++it;
                continue;
            }
            const auto nodeId = it->first;
            it = mobileNodeLeases_.erase(it);
            auto legacy = legacyMobileNodeExpiries_.find(nodeId);
            if (legacy == legacyMobileNodeExpiries_.end() || legacy->second <= now) {
                routing_table.removeMobileNode(nodeId);
                routing_table.findBucket(nodeId)->changeMobility(nodeId, false);
            }
            changed = true;
        }
        for (auto it = legacyMobileNodeExpiries_.begin(); it != legacyMobileNodeExpiries_.end();) {
            if (it->second > now) {
                ++it;
                continue;
            }
            const auto nodeId = it->first;
            it = legacyMobileNodeExpiries_.erase(it);
            auto lease = mobileNodeLeases_.find(nodeId);
            if (lease == mobileNodeLeases_.end() || lease->second.expires_at <= now) {
                routing_table.removeMobileNode(nodeId);
                routing_table.findBucket(nodeId)->changeMobility(nodeId, false);
            }
            changed = true;
        }
        for (auto it = pendingMobileLeases_.begin(); it != pendingMobileLeases_.end();) {
            if (it->second.lease.expires_at > now)
                ++it;
            else
                it = pendingMobileLeases_.erase(it);
        }
        scheduleMobileLeaseExpiryInternal();
    }
    if (changed)
        emitMobileNodesChanged();
}

void
SwarmManager::emitMobileNodesChanged()
{
    std::lock_guard emissionLock(mobileNodesEmissionMtx_);
    auto mobileNodes = getKnownMobileNodes();
    auto mobileNodeInfos = getKnownMobileNodeInfos();
    OnMobileNodesChanged callback;
    OnMobileNodeInfosChanged infosCallback;
    {
        std::lock_guard callbackLock(onMobileNodesChangedMtx_);
        callback = onMobileNodesChanged_;
        infosCallback = onMobileNodeInfosChanged_;
    }
    if (callback)
        callback(mobileNodes);
    if (infosCallback)
        infosCallback(mobileNodeInfos);
}

void
SwarmManager::maintainBuckets(const std::set<NodeId>& toConnect)
{
    maintainBuckets(maintenancePolicy(), toConnect);
}

void
SwarmManager::maintainBuckets(ConnectionPolicy policy, const std::set<NodeId>& toConnect)
{
    std::set<NodeId> nodes = toConnect;
    std::unique_lock lock(mutex);
    auto& buckets = routing_table.getBuckets();
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        auto& bucket = *it;
        bool myBucket = routing_table.contains(it, id_);
        auto connecting_nodes = myBucket ? bucket.getConnectingNodesSize()
                                         : bucket.getConnectingNodesSize() + bucket.getNodesSize();
        if (connecting_nodes < Bucket::BUCKET_MAX_SIZE) {
            auto nodesToTry = bucket.getKnownNodesRandom(Bucket::BUCKET_MAX_SIZE - connecting_nodes, rd);
            for (auto& node : nodesToTry)
                routing_table.addConnectingNode(node);

            nodes.insert(nodesToTry.begin(), nodesToTry.end());
        }
    }
    lock.unlock();
    for (const auto& node : nodes)
        tryConnect(node, policy);
}

void
SwarmManager::sendRequest(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                          const NodeId& nodeId,
                          Query q,
                          int numberNodes)
{
    auto selfMobileInfo = localMobileNodeInfo();
    dht::ThreadPool::io().run(
        [socket, isMobile = isMobile_, selfMobileInfo = std::move(selfMobileInfo), nodeId, q, numberNodes] {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> pk(&buffer);
            Message msg;
            msg.is_mobile = isMobile;
            msg.self_mobile_info = selfMobileInfo;
            msg.request = Request {q, numberNodes, nodeId};
            pk.pack(msg);

            std::error_code ec;
            socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            if (ec) {
                JAMI_ERROR("{}", ec.message());
            }
        });
}

void
SwarmManager::sendAnswer(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, const Message& msg_)
{
    if (msg_.request->q != Query::FIND)
        return;

    auto selfMobileInfo = localMobileNodeInfo();
    Message msg;
    {
        std::lock_guard lock(mutex);
        auto nodes = routing_table.closestNodes(msg_.request->nodeId, msg_.request->num);
        auto bucket = routing_table.findBucket(msg_.request->nodeId);
        const auto& m_nodes = bucket->getMobileNodes();
        std::vector<NodeId> responseMobileNodes;
        responseMobileNodes.reserve(m_nodes.size());
        std::vector<MobileNodeInfo> mobileNodeInfos;
        mobileNodeInfos.reserve(m_nodes.size());
        const auto now = toSecondsSinceEpoch(std::chrono::system_clock::now());
        for (const auto& node : m_nodes) {
            if (!isMobileNodeCurrentInternal(node, now))
                continue;
            responseMobileNodes.emplace_back(node);
            if (mobileNodeInfos.size() >= MAX_MOBILE_NODE_INFOS)
                continue;
            auto lease = mobileNodeLeases_.find(node);
            if (lease == mobileNodeLeases_.end()) {
                if (msg_.v >= 3)
                    continue;
                mobileNodeInfos.emplace_back(MobileNodeInfo {node, std::nullopt});
            } else {
                mobileNodeInfos.emplace_back(MobileNodeInfo {node, lease->second});
            }
        }
        Response toResponse {Query::FOUND, nodes, std::move(responseMobileNodes), std::move(mobileNodeInfos)};

        msg.is_mobile = isMobile_;
        msg.self_mobile_info = std::move(selfMobileInfo);
        msg.response = std::move(toResponse);
    }

    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    pk.pack(msg);

    std::error_code ec;
    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    if (ec) {
        JAMI_ERROR("{}", ec.message());
        return;
    }
}

void
SwarmManager::receiveMessage(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    socket->setOnRecv(dhtnet::buildMsgpackReader<Message>(
        [w = weak(), wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket)](Message&& msg) {
            auto shared = w.lock();
            auto socket = wsocket.lock();
            if (!shared || !socket)
                return std::make_error_code(std::errc::operation_canceled);

            auto validMobileAnnouncement = msg.v < 3 || shared->conversationId_.empty();
            if (msg.self_mobile_info && msg.self_mobile_info->id == socket->deviceId()) {
                // The peer's own certificate is authenticated by the channel's TLS
                // handshake and pinned when the swarm channel was added, so this
                // resolves locally without any lookup.
                validMobileAnnouncement = msg.self_mobile_info->lease
                                          && msg.self_mobile_info->lease->device_id == msg.self_mobile_info->id
                                          && shared->precheckLease(*msg.self_mobile_info->lease);
                if (validMobileAnnouncement && shared->setMobileNodeInfo(*msg.self_mobile_info, true, socket))
                    shared->emitMobileNodesChanged();
            }
            if (msg.is_mobile && validMobileAnnouncement) {
                if (msg.v < 3 && !shared->conversationId_.empty())
                    shared->setMobileNodes(std::vector<NodeId> {socket->deviceId()});
                shared->changeMobility(socket->deviceId(), true);
            }

            if (msg.cert_request) {
                shared->onCertRequest(socket, *msg.cert_request);
            } else if (msg.cert_response) {
                shared->onCertResponse(socket, *msg.cert_response);
            } else if (msg.request) {
                shared->sendAnswer(socket, msg);

            } else if (msg.response) {
                shared->setKnownNodes(msg.response->nodes);
                const auto requireLease = msg.v >= 3 && !shared->conversationId_.empty();
                bool changed = false;
                size_t records = 0;
                for (const auto& mobile : msg.response->mobile_node_infos) {
                    if (records++ == MAX_MOBILE_NODE_INFOS)
                        break;
                    changed |= shared->setMobileNodeInfo(mobile, requireLease, socket);
                }
                if (changed)
                    shared->emitMobileNodesChanged();
                const auto acceptLegacy = msg.v < 3 || shared->conversationId_.empty();
                if (acceptLegacy)
                    shared->setMobileNodes(msg.response->mobile_nodes);
            }
            return std::error_code();
        }));

    socket->onShutdown([w = weak(), deviceId = socket->deviceId()](const std::error_code&) {
        dht::ThreadPool::io().run([w, deviceId] {
            auto shared = w.lock();
            if (shared && !shared->isShutdown_) {
                shared->removeNode(deviceId);
            }
        });
    });
}

void
SwarmManager::resetNodeExpiry(const asio::error_code& ec,
                              const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
                              NodeId node)
{
    NodeId idToFind;
    std::list<Bucket>::iterator bucket;

    if (ec == asio::error::operation_aborted)
        return;

    if (!node) {
        bucket = routing_table.findBucket(socket->deviceId());
        idToFind = bucket->randomId(rd);
    } else {
        bucket = routing_table.findBucket(node);
        idToFind = node;
    }

    sendRequest(socket, idToFind, Query::FIND, Bucket::BUCKET_MAX_SIZE);

    if (!node) {
        auto& nodeTimer = bucket->getNodeTimer(socket);
        nodeTimer.expires_after(FIND_PERIOD);
        nodeTimer.async_wait(std::bind(&jami::SwarmManager::resetNodeExpiry,
                                       shared_from_this(),
                                       std::placeholders::_1,
                                       socket,
                                       NodeId {}));
    }
}

void
SwarmManager::tryConnect(const NodeId& nodeId, ConnectionPolicy policy)
{
    if (needSocketCb_)
        needSocketCb_(
            nodeId.toString(),
            [w = weak(), nodeId](const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket) {
                auto shared = w.lock();
                if (!shared || shared->isShutdown_)
                    return true;
                if (socket) {
                    shared->addChannel(socket);
                    return true;
                }
                std::unique_lock lk(shared->mutex);
                auto bucket = shared->routing_table.findBucket(nodeId);
                bucket->removeConnectingNode(nodeId);
                if (!bucket->hasMobileNode(nodeId))
                    bucket->addKnownNode(nodeId);
                if (shared->routing_table.getActiveNodesCount() == 0 && shared->onConnectionChanged_) {
                    lk.unlock();
                    JAMI_LOG("[SwarmManager {:p}] Bootstrap: all connections failed", fmt::ptr(shared.get()));
                    shared->onConnectionChanged_(false);
                }
                return true;
            },
            policy == ConnectionPolicy::REUSE_EXISTING);
}

void
SwarmManager::removeNodeInternal(const NodeId& nodeId)
{
    routing_table.removeNode(nodeId);
}

void
SwarmManager::connectNode(const NodeId& nodeId)
{
    {
        std::lock_guard lock(mutex);
        if (isShutdown_)
            return;
        if (isConnectedWith(nodeId))
            return;
        addKnownNode(nodeId);
        if (!routing_table.addConnectingNode(nodeId))
            return;
    }
    tryConnect(nodeId, ConnectionPolicy::REUSE_EXISTING);
}

std::vector<NodeId>
SwarmManager::getAllNodes() const
{
    std::lock_guard lock(mutex);
    return routing_table.getAllNodes();
}

std::vector<NodeId>
SwarmManager::getConnectedNodes() const
{
    std::lock_guard lock(mutex);
    return routing_table.getConnectedNodes();
}

std::vector<NodeId>
SwarmManager::getMobileNodesToNotify()
{
    std::lock_guard lock(mutex);
    return routing_table.getMobileNodesToNotify();
}

std::vector<NodeId>
SwarmManager::getKnownMobileNodes() const
{
    std::lock_guard lock(mutex);
    return routing_table.getKnownMobileNodes();
}

std::vector<MobileNodeInfo>
SwarmManager::getKnownMobileNodeInfos() const
{
    std::lock_guard lock(mutex);
    std::vector<MobileNodeInfo> infos;
    const auto now = toSecondsSinceEpoch(std::chrono::system_clock::now());
    for (const auto& node : routing_table.getKnownMobileNodes()) {
        if (!isMobileNodeCurrentInternal(node, now))
            continue;
        auto lease = mobileNodeLeases_.find(node);
        infos.emplace_back(MobileNodeInfo {node,
                                           lease == mobileNodeLeases_.end()
                                               ? std::nullopt
                                               : std::optional<MobileLease>(lease->second)});
    }
    return infos;
}

std::vector<MobileNodeInfo>
SwarmManager::getMobileNodeInfosToNotify()
{
    std::lock_guard lock(mutex);
    std::vector<MobileNodeInfo> infos;
    const auto now = toSecondsSinceEpoch(std::chrono::system_clock::now());
    for (const auto& node : routing_table.getMobileNodesToNotify()) {
        if (!isMobileNodeCurrentInternal(node, now))
            continue;
        auto lease = mobileNodeLeases_.find(node);
        infos.emplace_back(MobileNodeInfo {node,
                                           lease == mobileNodeLeases_.end()
                                               ? std::nullopt
                                               : std::optional<MobileLease>(lease->second)});
    }
    return infos;
}

std::vector<std::map<std::string, std::string>>
SwarmManager::getRoutingTableInfo() const
{
    std::lock_guard lock(mutex);
    auto stats = routing_table.getRoutingTableStats();
    const auto toNotify = routing_table.getMobileNodesToNotify();
    std::set<std::string> responsible;
    for (const auto& node : toNotify)
        responsible.emplace(node.toString());
    std::vector<std::map<std::string, std::string>> result;
    result.reserve(stats.size());
    for (const auto& stat : stats) {
        result.push_back({{"id", stat.id},
                          {"device", stat.id},
                          {"status", stat.status},
                          {"remoteAddress", stat.remoteAddress},
                          {"mobile", stat.isMobile ? "true" : "false"},
                          {"responsible", responsible.count(stat.id) ? "true" : "false"}});
        if (stat.connectionTime != std::chrono::system_clock::time_point::min()) {
            auto tt = std::chrono::system_clock::to_time_t(stat.connectionTime);
            result.back().emplace("connectionTime", std::to_string(tt));
        }
    }
    return result;
}

bool
SwarmManager::isConnected() const
{
    std::lock_guard lock(mutex);
    return !routing_table.isEmpty();
}

void
SwarmManager::deleteNode(const std::vector<NodeId>& nodes)
{
    bool mobileNodesChanged = false;
    {
        std::lock_guard lock(mutex);
        auto mobileNodes = routing_table.getKnownMobileNodes();
        for (const auto& node : nodes) {
            routing_table.deleteNode(node);
            mobileNodesChanged |= mobileNodeLeases_.erase(node) != 0;
            mobileNodesChanged |= legacyMobileNodeExpiries_.erase(node) != 0;
            pendingMobileLeases_.erase(node);
        }
        scheduleMobileLeaseExpiryInternal();
        mobileNodesChanged |= mobileNodes != routing_table.getKnownMobileNodes();
    }
    if (mobileNodesChanged)
        emitMobileNodesChanged();
    maintainBuckets();
}

} // namespace jami
