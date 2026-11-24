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
#include "swarm_membership.h"

#include "fileutils.h"
#include "jamiaccount.h"
#include "logger.h"
#include "manager.h"
#include "presence_manager.h"
#include "swarm/swarm_protocol.h"
#include "timestamp.h"

#include <dhtnet/fileutils.h>
#include <opendht/thread_pool.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace jami {

SwarmMembership::SwarmMembership(const std::shared_ptr<JamiAccount>& account,
                                 std::string swarmNamespace,
                                 std::filesystem::path mobileNodesPath,
                                 SwarmManager::MobileLeaseIssuerValidator leaseIssuers)
    : account_(account)
    , namespace_(std::move(swarmNamespace))
    , mobileNodesPath_(std::move(mobileNodesPath))
    , manager_(std::make_shared<SwarmManager>(
          NodeId(account->currentDeviceId()),
          account->isMobile(),
          Manager::instance().getSeededRandomEngine(),
          [account = account_](const DeviceId& deviceId) {
              if (auto acc = account.lock())
                  return acc->isConnectedWith(deviceId);
              return false;
          },
          namespace_,
          [account = account_, swarm = namespace_, deviceId = NodeId(account->currentDeviceId())]()
              -> std::optional<MobileNodeInfo> {
              auto acc = account.lock();
              if (!acc || !acc->isMobile())
                  return std::nullopt;
              const auto& identity = acc->identity();
              if (!identity.first || !identity.second || !identity.second->issuer)
                  return std::nullopt;

              const auto now = std::chrono::system_clock::now();
              const auto certificateExpiry = identity.second->getExpiration();
              const auto maximumExpiry = now + MAX_MOBILE_LEASE_DURATION;
              const auto expiry = std::min(certificateExpiry, maximumExpiry);
              if (expiry <= now)
                  return std::nullopt;

              MobileLease lease {1,
                                 swarm,
                                 identity.second->issuer->getId(),
                                 deviceId,
                                 toSecondsSinceEpoch(now),
                                 toSecondsSinceEpoch(expiry),
                                 {}};
              lease.signature = identity.first->sign(mobileLeasePayload(lease));
              return MobileNodeInfo {deviceId, std::move(lease)};
          },
          std::move(leaseIssuers),
          [account = account_](const NodeId& deviceId) -> std::shared_ptr<dht::crypto::Certificate> {
              if (auto acc = account.lock())
                  return acc->certStore().getCertificate(deviceId.toString());
              return {};
          },
          [account = account_](const NodeId& deviceId,
                               std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb) {
              if (auto acc = account.lock())
                  acc->findCertificate(deviceId, std::move(cb));
              else
                  cb(nullptr);
          }))
{
    // Restore mobility knowledge: in a swarm made of mostly mobile
    // devices, gossip alone is unavailable after a restart (nobody is
    // connected), so the persisted list is the only way to know which
    // devices need a wake-up notification.
    loadMobileNodes();
    // Self-contained callback: the swarm manager can outlive this object
    // (its timers hold shared_from_this), so do not capture `this`.
    manager_->onMobileNodeInfosChanged([path = mobileNodesPath_](const std::vector<MobileNodeInfo>& mobileNodes) {
        std::lock_guard lk {dhtnet::fileutils::getFileLock(path)};
        std::ofstream file(path, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, mobileNodes);
    });
}

SwarmMembership::~SwarmMembership()
{
    stopTracking();
}

void
SwarmMembership::loadMobileNodes()
{
    try {
        auto file = fileutils::loadFile(mobileNodesPath_);
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        try {
            std::vector<MobileNodeInfo> nodes;
            oh.get().convert(nodes);
            manager_->setMobileNodes(nodes);
        } catch (const std::exception&) {
            std::vector<NodeId> nodes;
            oh.get().convert(nodes);
            manager_->setMobileNodes(nodes);
        }
    } catch (const std::exception&) {
    }
}

void
SwarmMembership::setMembers(Members members, IsMember isMember)
{
    std::lock_guard lk(trackedMembersMtx_);
    members_ = std::move(members);
    isMember_ = std::move(isMember);
}

void
SwarmMembership::setOpenChannel(OpenChannel open)
{
    manager_->needSocketCb_ = [w = weak_from_this(), open = std::move(open)](const std::string& deviceId,
                                                                             SwarmManager::ChannelCb&& cb,
                                                                             bool noNewSocket) {
        ChannelCb wrapped = [cb = std::move(cb), w, deviceId](const std::shared_ptr<dhtnet::ChannelSocket>& socket) {
            if (auto sthis = w.lock()) {
                if (!socket) {
                    auto account = sthis->account_.lock();
                    if (!account)
                        return false;
                    auto cert = account->certStore().getCertificate(deviceId);
                    if (cert && cert->issuer)
                        sthis->rotateTrackedMembers(cert->issuer->getId().toString(), DeviceId(deviceId));
                    else
                        JAMI_WARNING("[Swarm {}] Unable to get member URI from device ID {}",
                                     sthis->namespace_,
                                     deviceId);
                }
            }
            return cb(socket);
        };
        open(deviceId, std::move(wrapped), noNewSocket);
    };
}

void
SwarmMembership::monitor(std::function<void(bool)> onChanged)
{
    if (!manager_->isConnected())
        startTracking();

    manager_->onConnectionChanged([w = weak_from_this(), onChanged = std::move(onChanged)](bool ok) {
        dht::ThreadPool::io().run([w, ok, onChanged] {
            if (auto sthis = w.lock()) {
                // Check the current connection state rather than relying on the
                // captured `ok` value, because the thread pool does not guarantee
                // execution order. Two rapid state changes (connected then
                // disconnected) could otherwise cause stopTracking to run after
                // startTracking, leaving the swarm untracked while the DRT
                // has no connected nodes.
                if (sthis->manager_->isConnected())
                    sthis->stopTracking();
                else
                    sthis->startTracking();
                if (onChanged)
                    onChanged(ok);
            }
        });
    });
}

void
SwarmMembership::addKnownDevices(const std::vector<DeviceId>& devices, const std::string& memberUri)
{
    if (devices.empty())
        return;
    if (!memberUri.empty()) {
        std::lock_guard lk(trackedMembersMtx_);
        auto it = trackedMembers_.find(memberUri);
        if (it != trackedMembers_.end()) {
            it->second.presenceReported = true;
            it->second.devices.insert(devices.begin(), devices.end());
            // Presence only reports a device once it was absent, so a failure
            // recorded before this announce says nothing about the device now.
            for (const auto& device : devices)
                it->second.failedDevices.erase(device);
        }
    } else {
        JAMI_ERROR("[Swarm {}] Adding {} known devices without member URI", namespace_, devices.size());
    }
    manager_->setKnownNodes(devices);
}

void
SwarmMembership::pruneMembers(const std::set<std::string>& members)
{
    std::lock_guard lk(trackedMembersMtx_);
    if (!isTracking_)
        return;
    auto account = account_.lock();
    if (!account)
        return;
    for (auto it = trackedMembers_.begin(); it != trackedMembers_.end();) {
        if (members.find(it->first) == members.end()) {
            account->presenceManager()->untrackBuddy(it->first);
            it = trackedMembers_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::map<std::string, std::string>>
SwarmMembership::trackedMembers() const
{
    std::vector<std::map<std::string, std::string>> result;
    std::lock_guard lk(trackedMembersMtx_);
    for (const auto& [uri, member] : trackedMembers_) {
        std::map<std::string, std::string> map;
        map["uri"] = uri;
        std::string devicesStr;
        for (const auto& dev : member.devices) {
            if (!devicesStr.empty())
                devicesStr += ";";
            devicesStr += dev.toString();
        }
        map["devices"] = devicesStr;
        result.emplace_back(std::move(map));
    }
    return result;
}

void
SwarmMembership::addChannel(std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    if (auto account = account_.lock(); account && channel->peerCertificate())
        account->certStore().pinCertificate(channel->peerCertificate());
    manager_->addChannel(std::move(channel));
}

std::vector<std::string>
SwarmMembership::unconnectedMembers(const std::vector<NodeId>& peers,
                                    const std::set<std::string>& members,
                                    size_t count) const
{
    auto account = account_.lock();
    if (!account)
        return {};
    std::set<std::string> connected;
    for (const auto& peer : peers) {
        auto cert = account->certStore().getCertificate(peer.toString());
        if (cert && cert->issuer)
            connected.emplace(cert->issuer->getId().toString());
    }
    std::vector<std::string> result;
    std::set_difference(members.begin(), members.end(), connected.begin(), connected.end(), std::back_inserter(result));
    std::shuffle(result.begin(), result.end(), account->rand);
    if (result.size() > count)
        result.resize(count);
    return result;
}

std::vector<SwarmTarget>
SwarmMembership::mobileTargets(
    const std::function<std::string(const std::string& device)>& memberOf,
    const std::function<bool(const std::string& member, const std::string& device)>& allowed) const
{
    std::vector<SwarmTarget> targets;
    auto account = account_.lock();
    if (!account)
        return targets;
    for (const auto& info : manager_->getMobileNodeInfosToNotify()) {
        const auto deviceId = info.id.toString();
        auto uri = memberOf(deviceId);
        // A verified lease binds the device to its issuer, so the account URI is
        // already known without touching the certificate.
        if (uri.empty() && info.lease)
            uri = info.lease->issuer_id.toString();
        if (uri.empty()) {
            auto cert = account->certStore().getCertificate(deviceId);
            if (cert && cert->issuer)
                uri = cert->issuer->getId().toString();
        }
        if (!uri.empty() && allowed(uri, deviceId))
            targets.emplace_back(SwarmTarget {std::move(uri), info.id});
    }
    return targets;
}

void
SwarmMembership::startTracking()
{
    auto account = account_.lock();
    if (!account)
        return;

    {
        std::lock_guard lk(trackedMembersMtx_);
        if (isTracking_)
            return;
        isTracking_ = true;
        presenceDeviceListenerToken_ = account->presenceManager()->addDeviceListener(
            [w = weak_from_this()](const std::string& uri, const DeviceId& deviceId, bool online) {
                if (auto sthis = w.lock()) {
                    if (online) {
                        IsMember isMember;
                        {
                            std::lock_guard lk(sthis->trackedMembersMtx_);
                            isMember = sthis->isMember_;
                        }
                        if (isMember && isMember(uri))
                            sthis->addKnownDevices({deviceId}, uri);
                    } else {
                        sthis->onDeviceOffline(deviceId, uri);
                    }
                }
            });
    }

    rotateTrackedMembers();
}

void
SwarmMembership::stopTracking()
{
    // Collect data under trackedMembersMtx_, then release it before calling
    // into PresenceManager to avoid lock-ordering inversion with the
    // PresenceManager mutex (which is held when notifyListeners calls
    // addKnownDevices, which in turn acquires trackedMembersMtx_).
    uint64_t token = 0;
    std::vector<std::string> urisToUntrack;
    {
        std::lock_guard lk(trackedMembersMtx_);
        if (!isTracking_)
            return;
        isTracking_ = false;
        token = presenceDeviceListenerToken_;
        presenceDeviceListenerToken_ = 0;
        for (const auto& [uri, _] : trackedMembers_)
            urisToUntrack.push_back(uri);
        trackedMembers_.clear();
    }

    auto account = account_.lock();
    if (!account)
        return;
    if (token)
        account->presenceManager()->removeDeviceListener(token);
    for (const auto& uri : urisToUntrack)
        account->presenceManager()->untrackBuddy(uri);
}

void
SwarmMembership::rotateTrackedMembers(const std::string& memberUri, const DeviceId& deviceId)
{
    auto account = account_.lock();
    if (!account)
        return;

    // The members callback may release the swarm's owner, whose destruction
    // stops tracking: never call it with trackedMembersMtx_ held.
    Members getMembers;
    {
        std::lock_guard lk(trackedMembersMtx_);
        if (!isTracking_ || !members_)
            return;
        getMembers = members_;
    }
    auto members = getMembers();

    std::lock_guard lk(trackedMembersMtx_);
    if (!isTracking_)
        return;

    if (!memberUri.empty()) {
        auto it = trackedMembers_.find(memberUri);
        if (it == trackedMembers_.end())
            return;
        auto& info = it->second;
        if (deviceId) {
            JAMI_WARNING("[Swarm {}] [device {}] Rotating tracked members after connection failure",
                         namespace_,
                         deviceId);
            info.failedDevices.insert(deviceId);
        } else {
            JAMI_WARNING("[Swarm {}] Rotating tracked members, {} has no device online", namespace_, memberUri);
        }
        // std::includes() is vacuously true on an empty second range, so a
        // member whose devices are still unknown would be dropped on its very
        // first failure. That set is only filled by addKnownDevices() once
        // the presence manager reports the member online, while a failure
        // is reported for any device the DRT happens to try, so it is
        // routinely empty right after the member was picked. Give presence
        // time to answer; past that, an empty set means nobody is online
        // and the member is as good as fully failed.
        constexpr auto PRESENCE_GRACE = std::chrono::seconds(30);
        auto presencePending = info.devices.empty() && !info.presenceReported
                               && std::chrono::steady_clock::now() - info.trackedSince < PRESENCE_GRACE;
        if (!presencePending
            && std::includes(info.failedDevices.begin(),
                             info.failedDevices.end(),
                             info.devices.begin(),
                             info.devices.end())) {
            // Rotating only makes sense if another member can take the slot. In
            // a one-to-one conversation there is none: dropping the peer would
            // leave presence watched for nobody, so addKnownDevices() is never
            // called again and the swarm stays silent until the account reloads.
            auto replaceable = std::any_of(members.begin(), members.end(), [&](const auto& uri) {
                return uri != memberUri && trackedMembers_.find(uri) == trackedMembers_.end();
            });
            if (replaceable) {
                account->presenceManager()->untrackBuddy(it->first);
                trackedMembers_.erase(it);
            }
        }
    }

    size_t N = members.size();
    size_t K = std::min(N, 3 + (size_t) std::log2(N));

    // If we are already trying enough devices, don't rotate
    auto activeDevices = manager_->getActiveNodesCount();
    if (activeDevices >= 2 * K)
        return;

    JAMI_WARNING("[Swarm {}] Refreshing tracked members: {}/{} active ({}/{} devices)",
                 namespace_,
                 trackedMembers_.size(),
                 K,
                 activeDevices,
                 2 * K);

    // Add new members if we have space
    if (trackedMembers_.size() < K) {
        std::vector<std::string> candidates;
        candidates.reserve(N - trackedMembers_.size());
        for (const auto& uri : members) {
            if (uri != memberUri && trackedMembers_.find(uri) == trackedMembers_.end())
                candidates.push_back(uri);
        }
        if (!candidates.empty()) {
            std::vector<std::string> chosen;
            std::sample(candidates.begin(),
                        candidates.end(),
                        std::back_inserter(chosen),
                        K - trackedMembers_.size(),
                        Manager::instance().getSeededRandomEngine());
            for (const auto& uri : chosen) {
                account->presenceManager()->trackBuddy(uri);
                trackedMembers_.emplace(uri, TrackedMember {});
            }
        }
    }
}

void
SwarmMembership::onDeviceOffline(const DeviceId& deviceId, const std::string& memberUri)
{
    // A device that left is neither a target nor a failure to hold against its member.
    bool lastDevice = false;
    {
        std::lock_guard lk(trackedMembersMtx_);
        auto it = trackedMembers_.find(memberUri);
        if (it == trackedMembers_.end())
            return;
        it->second.presenceReported = true;
        it->second.devices.erase(deviceId);
        it->second.failedDevices.erase(deviceId);
        lastDevice = it->second.devices.empty();
    }
    // Nobody left to reach: watch someone else rather than wait for the DRT
    // to fail on ids it may never retry.
    if (lastDevice)
        rotateTrackedMembers(memberUri);
}

} // namespace jami
