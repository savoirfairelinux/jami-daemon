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
#include <dhtnet/multiplexed_socket.h>
#include <dhtnet/channel_utils.h>
#include <opendht/thread_pool.h>

namespace jami {

using namespace swarm_protocol;

SwarmManager::SwarmManager(const NodeId& id,
                           bool isMobile,
                           const std::mt19937_64& rand,
                           ToConnectCb&& toConnectCb,
                           std::string conversationId,
                           MobileLeaseProvider mobileLeaseProvider)
    : id_(id)
    , isMobile_(isMobile)
    , conversationId_(std::move(conversationId))
    , rd(rand)
    , mobileLeaseProvider_(std::move(mobileLeaseProvider))
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
        const auto now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
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
    size_t certificatesSize = 0;
    size_t records = 0;
    for (const auto& mobile : mobile_nodes) {
        if (records++ == MAX_MOBILE_NODE_INFOS)
            break;
        if (certificatesSize + mobile.certificate.size() > MAX_MOBILE_CERTIFICATES_SIZE)
            break;
        certificatesSize += mobile.certificate.size();
        changed |= setMobileNodeInfo(mobile, requireLease);
    }
    if (changed)
        emitMobileNodesChanged();
}

bool
SwarmManager::setMobileNodeInfo(const MobileNodeInfo& mobile, bool requireLease)
{
    if (mobile.id == id_)
        return false;

    if ((requireLease && !mobile.lease) || (mobile.lease && !validateMobileNodeInfo(mobile)))
        return false;

    const auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    if (!mobile.lease && !conversationId_.empty() && now >= LEGACY_MOBILE_NODE_SUNSET)
        return false;

    std::lock_guard lock(mutex);
    bool changed = addMobileNodes(mobile.id);
    changed |= setMobileNodeCertificateInternal(mobile.id, mobile.certificate);
    if (mobile.lease) {
        auto current = mobileNodeLeases_.find(mobile.id);
        if (current == mobileNodeLeases_.end() || mobile.lease->expires_at > current->second.expires_at
            || (mobile.lease->expires_at == current->second.expires_at
                && mobile.lease->issued_at > current->second.issued_at)) {
            mobileNodeLeases_.insert_or_assign(mobile.id, *mobile.lease);
            legacyMobileNodeExpiries_.erase(mobile.id);
            changed = true;
        }
        scheduleMobileLeaseExpiryInternal();
    } else if (!conversationId_.empty()) {
        changed |= legacyMobileNodeExpiries_.try_emplace(mobile.id, LEGACY_MOBILE_NODE_SUNSET).second;
        scheduleMobileLeaseExpiryInternal();
    }
    return changed;
}

void
SwarmManager::setMobileNodeCertificate(const NodeId& nodeId, const dht::Blob& certificate)
{
    bool changed = false;
    bool isKnownMobile = false;
    {
        std::lock_guard lock(mutex);
        changed = setMobileNodeCertificateInternal(nodeId, certificate);
        if (changed) {
            const auto mobileNodes = routing_table.getKnownMobileNodes();
            isKnownMobile = std::find(mobileNodes.begin(), mobileNodes.end(), nodeId) != mobileNodes.end();
        }
    }
    if (changed && isKnownMobile)
        emitMobileNodesChanged();
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
            emit = routing_table.findBucket(getId())->isEmpty();
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
SwarmManager::setMobileNodeCertificateInternal(const NodeId& nodeId, const dht::Blob& certificate)
{
    if (certificate.empty() || certificate.size() > MAX_MOBILE_CERTIFICATE_SIZE)
        return false;
    try {
        dht::crypto::Certificate cert(certificate);
        if (cert.getLongId() != nodeId || cert.getIssuerUID().empty())
            return false;
    } catch (const std::exception& e) {
        JAMI_WARNING("Ignoring invalid mobile certificate for {}: {}", nodeId, e.what());
        return false;
    }
    auto current = mobileNodeCertificates_.find(nodeId);
    if (current != mobileNodeCertificates_.end() && current->second == certificate)
        return false;
    mobileNodeCertificates_.insert_or_assign(nodeId, certificate);
    return true;
}

bool
SwarmManager::isMobileNodeCurrentInternal(const NodeId& nodeId, uint64_t now) const
{
    if (auto lease = mobileNodeLeases_.find(nodeId); lease != mobileNodeLeases_.end())
        return lease->second.expires_at > now;
    if (auto legacy = legacyMobileNodeExpiries_.find(nodeId); legacy != legacyMobileNodeExpiries_.end())
        return legacy->second > now;
    return conversationId_.empty();
}

bool
SwarmManager::validateMobileNodeInfo(const MobileNodeInfo& mobile) const
{
    if (!mobile.lease || mobile.certificate.empty() || mobile.certificate.size() > MAX_MOBILE_CERTIFICATE_SIZE)
        return false;

    const auto& lease = *mobile.lease;
    if (lease.format_version != 1 || lease.device_id != mobile.id || lease.conversation_id != conversationId_
        || lease.signature.empty() || lease.signature.size() > MAX_MOBILE_LEASE_SIGNATURE_SIZE
        || lease.conversation_id.empty() || lease.conversation_id.size() > MAX_MOBILE_LEASE_IDENTIFIER_SIZE
        || !lease.issuer_id)
        return false;

    constexpr uint64_t MAX_CLOCK_SKEW_SECONDS = 5 * 60;
    constexpr uint64_t MAX_LEASE_SECONDS = std::chrono::duration_cast<std::chrono::seconds>(MAX_MOBILE_LEASE_DURATION)
                                               .count();
    const auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    if (lease.issued_at > now + MAX_CLOCK_SKEW_SECONDS || lease.expires_at <= now || lease.expires_at <= lease.issued_at
        || lease.expires_at - lease.issued_at > MAX_LEASE_SECONDS)
        return false;

    try {
        dht::crypto::Certificate certificate(mobile.certificate);
        if (certificate.getLongId() != mobile.id || !certificate.issuer
            || certificate.issuer->getId() != lease.issuer_id)
            return false;
        dht::crypto::TrustList trust;
        trust.add(*certificate.issuer);
        if (!trust.verify(certificate))
            return false;
        const auto certificateExpiry = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(certificate.getExpiration().time_since_epoch()).count());
        if (lease.expires_at > certificateExpiry)
            return false;
        const auto payload = mobileLeasePayload(lease);
        return certificate.getPublicKey().checkSignature(payload, lease.signature);
    } catch (const std::exception& e) {
        JAMI_WARNING("Ignoring invalid mobile lease for {}: {}", mobile.id, e.what());
        return false;
    }
}

std::optional<MobileNodeInfo>
SwarmManager::localMobileNodeInfo()
{
    if (!isMobile_ || !mobileLeaseProvider_)
        return std::nullopt;

    std::lock_guard renewalLock(mobileLeaseRenewalMtx_);

    const auto renewalThreshold = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(MOBILE_LEASE_RENEWAL_THRESHOLD).count());
    const auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    {
        std::lock_guard lock(mutex);
        if (localMobileNodeInfo_ && localMobileNodeInfo_->lease
            && localMobileNodeInfo_->lease->expires_at > now + renewalThreshold)
            return localMobileNodeInfo_;
    }

    auto renewed = mobileLeaseProvider_();
    if (!renewed || renewed->id != id_ || !validateMobileNodeInfo(*renewed))
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

    auto nearestExpiry = std::numeric_limits<uint64_t>::max();
    for (const auto& [node, lease] : mobileNodeLeases_)
        nearestExpiry = std::min(nearestExpiry, lease.expires_at);
    for (const auto& [node, expiry] : legacyMobileNodeExpiries_)
        nearestExpiry = std::min(nearestExpiry, expiry);
    const auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    constexpr uint64_t MAX_TIMER_DELAY_SECONDS = 60 * 60;
    const auto delay = std::min(nearestExpiry > now ? nearestExpiry - now : 0, MAX_TIMER_DELAY_SECONDS);
    mobileLeaseExpiryTimer_.expires_after(std::chrono::seconds(delay));
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
        const auto now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
        for (auto it = mobileNodeLeases_.begin(); it != mobileNodeLeases_.end();) {
            if (it->second.expires_at > now) {
                ++it;
                continue;
            }
            const auto nodeId = it->first;
            routing_table.removeMobileNode(nodeId);
            routing_table.findBucket(nodeId)->changeMobility(nodeId, false);
            mobileNodeCertificates_.erase(nodeId);
            it = mobileNodeLeases_.erase(it);
            changed = true;
        }
        for (auto it = legacyMobileNodeExpiries_.begin(); it != legacyMobileNodeExpiries_.end();) {
            if (it->second > now) {
                ++it;
                continue;
            }
            const auto nodeId = it->first;
            routing_table.removeMobileNode(nodeId);
            routing_table.findBucket(nodeId)->changeMobility(nodeId, false);
            mobileNodeCertificates_.erase(nodeId);
            it = legacyMobileNodeExpiries_.erase(it);
            changed = true;
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
        tryConnect(node);
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
    auto selfMobileInfo = localMobileNodeInfo();
    std::lock_guard lock(mutex);

    if (msg_.request->q == Query::FIND) {
        auto nodes = routing_table.closestNodes(msg_.request->nodeId, msg_.request->num);
        auto bucket = routing_table.findBucket(msg_.request->nodeId);
        const auto& m_nodes = bucket->getMobileNodes();
        std::vector<NodeId> responseMobileNodes;
        responseMobileNodes.reserve(m_nodes.size());
        std::vector<MobileNodeInfo> mobileNodeInfos;
        mobileNodeInfos.reserve(m_nodes.size());
        size_t certificatesSize = 0;
        const auto now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
        for (const auto& node : m_nodes) {
            if (!isMobileNodeCurrentInternal(node, now))
                continue;
            responseMobileNodes.emplace_back(node);
            if (auto certificate = mobileNodeCertificates_.find(node);
                certificate != mobileNodeCertificates_.end()
                && certificatesSize + certificate->second.size() <= MAX_MOBILE_CERTIFICATES_SIZE) {
                auto lease = mobileNodeLeases_.find(node);
                if (msg_.v >= 3 && lease == mobileNodeLeases_.end())
                    continue;
                mobileNodeInfos.emplace_back(MobileNodeInfo {node,
                                                             certificate->second,
                                                             lease == mobileNodeLeases_.end()
                                                                 ? std::nullopt
                                                                 : std::optional<MobileLease>(lease->second)});
                certificatesSize += certificate->second.size();
            }
        }
        Response toResponse {Query::FOUND, nodes, std::move(responseMobileNodes), std::move(mobileNodeInfos)};

        Message msg;
        msg.is_mobile = isMobile_;
        msg.self_mobile_info = std::move(selfMobileInfo);
        msg.response = std::move(toResponse);

        msgpack::sbuffer buffer((size_t) 60000);
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack(msg);

        std::error_code ec;
        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        if (ec) {
            JAMI_ERROR("{}", ec.message());
            return;
        }
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
                validMobileAnnouncement = shared->validateMobileNodeInfo(*msg.self_mobile_info);
                if (validMobileAnnouncement && shared->setMobileNodeInfo(*msg.self_mobile_info, true))
                    shared->emitMobileNodesChanged();
            }
            if (msg.is_mobile && validMobileAnnouncement)
                shared->changeMobility(socket->deviceId(), true);

            if (msg.request) {
                shared->sendAnswer(socket, msg);

            } else if (msg.response) {
                shared->setKnownNodes(msg.response->nodes);
                shared->setMobileNodes(msg.response->mobile_node_infos, msg.v >= 3 && !shared->conversationId_.empty());
                if (msg.v < 3 || shared->conversationId_.empty())
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
SwarmManager::tryConnect(const NodeId& nodeId, bool noNewSocket)
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
                bucket = shared->routing_table.findBucket(shared->getId());
                if (bucket->getConnectingNodesSize() == 0 && bucket->isEmpty() && shared->onConnectionChanged_) {
                    lk.unlock();
                    JAMI_LOG("[SwarmManager {:p}] Bootstrap: all connections failed", fmt::ptr(shared.get()));
                    shared->onConnectionChanged_(false);
                }
                return true;
            },
            noNewSocket);
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
    tryConnect(nodeId, true);
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
    const auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for (const auto& node : routing_table.getKnownMobileNodes()) {
        if (!isMobileNodeCurrentInternal(node, now))
            continue;
        auto certificate = mobileNodeCertificates_.find(node);
        auto lease = mobileNodeLeases_.find(node);
        infos.emplace_back(
            MobileNodeInfo {node,
                            certificate == mobileNodeCertificates_.end() ? dht::Blob {} : certificate->second,
                            lease == mobileNodeLeases_.end() ? std::nullopt
                                                             : std::optional<MobileLease>(lease->second)});
    }
    return infos;
}

std::vector<MobileNodeInfo>
SwarmManager::getMobileNodeInfosToNotify()
{
    std::lock_guard lock(mutex);
    std::vector<MobileNodeInfo> infos;
    const auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for (const auto& node : routing_table.getMobileNodesToNotify()) {
        if (!isMobileNodeCurrentInternal(node, now))
            continue;
        auto certificate = mobileNodeCertificates_.find(node);
        auto lease = mobileNodeLeases_.find(node);
        infos.emplace_back(
            MobileNodeInfo {node,
                            certificate == mobileNodeCertificates_.end() ? dht::Blob {} : certificate->second,
                            lease == mobileNodeLeases_.end() ? std::nullopt
                                                             : std::optional<MobileLease>(lease->second)});
    }
    return infos;
}

std::vector<std::map<std::string, std::string>>
SwarmManager::getRoutingTableInfo() const
{
    std::lock_guard lock(mutex);
    auto stats = routing_table.getRoutingTableStats();
    std::vector<std::map<std::string, std::string>> result;
    result.reserve(stats.size());
    for (const auto& stat : stats) {
        result.push_back({{"id", stat.id},
                          {"device", stat.id},
                          {"status", stat.status},
                          {"remoteAddress", stat.remoteAddress},
                          {"mobile", stat.isMobile ? "true" : "false"}});
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
            mobileNodesChanged |= mobileNodeCertificates_.erase(node) != 0;
            mobileNodesChanged |= mobileNodeLeases_.erase(node) != 0;
            mobileNodesChanged |= legacyMobileNodeExpiries_.erase(node) != 0;
        }
        scheduleMobileLeaseExpiryInternal();
        mobileNodesChanged |= mobileNodes != routing_table.getKnownMobileNodes();
    }
    if (mobileNodesChanged)
        emitMobileNodesChanged();
    maintainBuckets();
}

} // namespace jami
