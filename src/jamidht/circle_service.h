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

#include "circle_info.h"
#include "circle_module.h"
#include "swarm_history.h"
#include "swarm_membership.h"

#include <asio/steady_timer.hpp>
#include <dhtnet/multiplexed_socket.h>

#include <chrono>
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

/**
 * Keeps an account's circles in sync: one routing table per circle, head announcements to
 * members (channel heads only to their readers), pulls of announced heads, invitations, the
 * local member's operations, and the account's other devices. Circles live in
 * <root>/<id>/{repo,channels/<id>,mobileNodes}.
 */
class CircleService : public std::enable_shared_from_this<CircleService>
{
public:
    static constexpr const char* MIME_TYPE_HEAD = "application/im-circle-head";
    static constexpr const char* MIME_TYPE_INVITE = "application/invite-circle+json";
    // A departed circle stops being served after this, even if no peer fetched the leave.
    static constexpr std::chrono::hours DEPARTURE_GRACE {7 * 24};
    static constexpr size_t MAX_REQUESTS = 256;

    using Done = std::function<void(bool)>;

    // Loads the saved circles; they are mounted on first use.
    CircleService(const std::shared_ptr<JamiAccount>& account, std::filesystem::path root);
    ~CircleService();

    // Starts the circle routing tables once the device is announced.
    void bootstrap();
    void shutdownConnections();
    void connectivityChanged();

    std::filesystem::path repositoryPath(std::string_view circle) const;
    std::filesystem::path channelPath(std::string_view circle, std::string_view channel) const;
    std::map<std::string, CircleInfo> circles() const;
    std::map<std::string, CircleRequest> requests() const;
    // The mounted circle, for reading.
    std::shared_ptr<CircleRepository> repository(const std::string& circle);

    // Operations of the local member; writes return the new head and are announced.
    std::optional<std::string> create(const PermissionRule& defaults = {});
    std::optional<std::string> invite(const std::string& circle, const std::string& member);
    std::optional<std::string> ban(const std::string& circle, const std::string& member);
    std::optional<std::string> unban(const std::string& circle, const std::string& member);
    // Rule, role, assignment, channel rule and delegation changes, in the policy record format.
    std::optional<std::string> updatePolicy(const std::string& circle, std::string_view change);
    std::map<std::string, std::string> profile(const std::string& circle, const std::string& channel = {});
    bool updateProfile(const std::string& circle,
                       const std::map<std::string, std::string>& fields,
                       const std::string& channel = {});
    // Leaves, after handing ADMIN to successor; an ADMIN without successor ends the circle.
    bool leave(const std::string& circle, const std::string& successor = {});
    std::optional<std::string> createChannel(const std::string& circle, const ChannelPolicy& policy = {});
    std::optional<std::string> archiveChannel(const std::string& circle, const std::string& channel);
    // Clones a channel from a connected reader, or from deviceId when given.
    void joinChannel(const std::string& circle, const std::string& channel, Done done, const std::string& deviceId = {});
    void leaveChannel(const std::string& circle, const std::string& channel);

    // Joined channels hold their messages like conversations do, and share their message API.
    bool isChannel(const std::string& id) const;
    std::optional<std::string> sendMessage(const std::string& channel, const CommitMessage& message);
    // Loads up to count more displayed messages, from `from` and no further than `to` when given.
    std::vector<libjami::SwarmMessage> loadMessages(const std::string& channel,
                                                    const std::string& from = {},
                                                    const std::string& to = {},
                                                    size_t count = 0);
    void clearCache(const std::string& channel);
    // What the local member may do in a channel, named as conversation permissions are.
    std::vector<std::string> permissions(const std::string& channel);

    static std::map<std::string, std::string> requestMap(const std::string& circle, const CircleRequest& request);

    void onInvitation(const std::string& from, const std::string& payload);
    // Clones the circle from the inviting device and joins it.
    void acceptRequest(const std::string& circle, Done done);
    void declineRequest(const std::string& circle);
    // The account's other devices shared their circles and requests.
    void onSyncData(const std::map<std::string, CircleInfo>& circles,
                    const std::map<std::string, CircleRequest>& requests,
                    const std::string& deviceId);

    // Tells the members who may read route about its new head; never back to the device it came from.
    void announce(const std::string& route, const std::string& head, const std::string& from = {});
    void onHeadAnnounced(const std::string& from, const std::string& payload);

    // swarm://circles/v1/<id> channels of the circle routing tables.
    bool authorizeSwarmChannel(std::string_view route, const dht::crypto::Certificate& peer);
    void addSwarmChannel(std::string_view route, std::shared_ptr<dhtnet::ChannelSocket> channel);
    void addKnownDevice(const std::string& member, const DeviceId& device);
    // A peer fetched commit from a route this device serves.
    void onServed(const std::string& route, const std::string& commit);

private:
    struct Pull
    {
        std::string wanted;
        bool again {false};
    };

    // The account's module, with the saved routes mounted if it was recreated.
    std::shared_ptr<CircleModule> module();
    std::optional<CirclePolicy> policy(const std::string& circle);
    std::vector<std::string> activeMembers(const std::string& circle);
    bool isActiveMember(const std::string& circle, const std::string& member);
    std::string circleOf(const CircleRoute& route) const;
    std::shared_ptr<SwarmMembership> swarm(const std::string& circle);
    void bootstrapSwarm(const std::string& circle);
    void stopSwarm(const std::string& circle);

    // Saves and starts syncing a circle mounted from repositoryPath(circle).
    bool add(const std::string& circle);
    bool addChannel(const std::string& circle, const std::string& channel);
    void removeChannel(const std::string& circle, const std::string& channel);
    void depart(const std::string& circle, const std::string& leaveHead);
    void remove(const std::string& circle);
    std::optional<std::string> write(const std::string& circle, const CirclePolicyChange& change);
    std::optional<std::string> moderate(const std::string& circle, const CircleModerationVote& vote);
    // Registers this device once the local member is active, so that it can sign.
    void enroll(const std::string& circle);
    // The circle policy changed; fetched changes may also be our own leave from another device.
    void onCircleChanged(const std::string& circle, bool fetched);
    void syncChannels(const std::string& circle, const CircleInfo& remote, const std::string& deviceId);
    bool departed(const std::string& circle) const;
    void clone(const std::string& route,
               const std::string& circle,
               std::vector<std::string> devices,
               const std::filesystem::path& path,
               Done done);

    void sendHeads(const std::string& circle, const DeviceId& device);
    void send(const std::string& route, const std::string& member, const DeviceId& device, const std::string& payload);
    void pull(const std::string& route, const std::string& device, const std::string& head);
    void fetch(const std::string& route, const std::string& device, bool dependencyFetched);
    void onFetched(const std::string& route,
                   const std::string& device,
                   const CircleFetchResult& result,
                   const std::string& previous);
    // Signals the messages a channel gained since previous, oldest first.
    void emitMessages(const std::string& circle, const std::string& channel, const std::string& previous);
    // The messages of a channel as its readers see them, or nothing when the local member may not read it.
    std::shared_ptr<SwarmHistory> history(const std::string& circle, const std::string& channel);
    void finishPull(const std::string& route, const std::string& device);
    void removeExpired();
    void scheduleExpiry();
    void syncDevices();
    void save() const;

    std::weak_ptr<JamiAccount> account_;
    const std::string accountId_;
    const std::filesystem::path root_;
    mutable std::mutex mutex_;
    std::map<std::string, CircleInfo> catalogue_;
    std::map<std::string, CircleRequest> requests_;
    std::set<std::string> cloning_;
    std::map<std::string, std::shared_ptr<SwarmMembership>> swarms_;
    std::map<std::string, std::shared_ptr<SwarmHistory>> histories_;
    std::weak_ptr<CircleModule> mounted_;
    std::map<std::pair<std::string, std::string>, Pull> pulls_;
    // The last device each circle was fetched from, to clone its channels when no peer is connected.
    std::map<std::string, std::string> sources_;
    bool bootstrapped_ {false};
    std::mutex refreshMtx_;
    std::map<std::string, uint64_t> refresh_;
    std::unique_ptr<asio::steady_timer> expiryTimer_;
};

} // namespace jami
