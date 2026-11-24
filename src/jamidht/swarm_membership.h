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

#include "swarm/swarm_manager.h"

#include <dhtnet/multiplexed_socket.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace jami {

class JamiAccount;
using DeviceId = dht::PkId;

struct SwarmTarget
{
    std::string member;
    DeviceId device;
};

/**
 * The routing table of one swarm of an account (a conversation or a circle), with the account
 * callbacks it needs, persisted mobile nodes, and presence tracking of a sample of members
 * while disconnected.
 */
class SwarmMembership : public std::enable_shared_from_this<SwarmMembership>
{
public:
    // Member URIs to sample from, and whether a URI is still allowed to join the swarm.
    using Members = std::function<std::vector<std::string>()>;
    using IsMember = std::function<bool(const std::string&)>;
    using ChannelCb = std::function<bool(const std::shared_ptr<dhtnet::ChannelSocket>&)>;
    using OpenChannel = std::function<void(const std::string& deviceId, ChannelCb&&, bool noNewSocket)>;

    SwarmMembership(const std::shared_ptr<JamiAccount>& account,
                    std::string swarmNamespace,
                    std::filesystem::path mobileNodesPath,
                    SwarmManager::MobileLeaseIssuerValidator leaseIssuers);
    ~SwarmMembership();

    const std::shared_ptr<SwarmManager>& manager() const { return manager_; }

    void setMembers(Members members, IsMember isMember);
    // Opens swarm channels through open; failed devices rotate the tracked members.
    void setOpenChannel(OpenChannel open);
    // Tracks presence while disconnected; onChanged runs on the I/O pool after each change.
    void monitor(std::function<void(bool connected)> onChanged);
    void addKnownDevices(const std::vector<DeviceId>& devices, const std::string& memberUri);
    void pruneMembers(const std::set<std::string>& members);
    void stopTracking();
    std::vector<std::map<std::string, std::string>> trackedMembers() const;

    // Adds an authenticated channel, pinning its certificate so the peer's leases verify offline.
    void addChannel(std::shared_ptr<dhtnet::ChannelSocket> channel);
    // Up to count random members without a device among peers, to reach devices outside the table.
    std::vector<std::string> unconnectedMembers(const std::vector<NodeId>& peers,
                                                const std::set<std::string>& members,
                                                size_t count) const;
    // Mobile devices this node is closest to, which only a DHT message can wake. memberOf may
    // return an empty URI, then the lease issuer or certificate store identify the member.
    std::vector<SwarmTarget> mobileTargets(
        const std::function<std::string(const std::string& device)>& memberOf,
        const std::function<bool(const std::string& member, const std::string& device)>& allowed) const;

private:
    struct TrackedMember
    {
        std::set<DeviceId> devices;
        std::set<DeviceId> failedDevices;
        std::chrono::steady_clock::time_point trackedSince {std::chrono::steady_clock::now()};
        bool presenceReported {false};
    };

    void startTracking();
    void rotateTrackedMembers(const std::string& memberUri = "", const DeviceId& deviceId = {});
    void onDeviceOffline(const DeviceId& deviceId, const std::string& memberUri);
    void loadMobileNodes();

    std::weak_ptr<JamiAccount> account_;
    std::string namespace_;
    std::filesystem::path mobileNodesPath_;
    std::shared_ptr<SwarmManager> manager_;

    mutable std::mutex trackedMembersMtx_;
    Members members_;
    IsMember isMember_;
    std::map<std::string, TrackedMember> trackedMembers_;
    bool isTracking_ {false};
    uint64_t presenceDeviceListenerToken_ {0};
};

} // namespace jami
