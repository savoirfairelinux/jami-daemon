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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "circle_interface.h"

#include "jamidht/circle_record.h"
#include "jamidht/circle_service.h"
#include "jamidht/jamiaccount.h"
#include "logger.h"
#include "manager.h"

namespace {

std::shared_ptr<jami::CircleService>
circleService(const std::string& accountId)
{
    if (auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return account->circleService();
    return {};
}

std::string_view
stateName(jami::CircleMemberState state)
{
    switch (state) {
    case jami::CircleMemberState::INVITED:
        return "invited";
    case jami::CircleMemberState::ACTIVE:
        return "active";
    case jami::CircleMemberState::LEFT:
        return "left";
    case jami::CircleMemberState::BANNED:
        return "banned";
    }
    return {};
}

} // namespace

namespace libjami {

std::string
createCircle(const std::string& accountId, const std::string& defaults)
{
    const auto circles = circleService(accountId);
    const auto rule = defaults.empty() ? std::optional<jami::PermissionRule>(jami::PermissionRule {})
                                       : jami::circleRuleFromJson(defaults);
    if (!circles || !rule)
        return {};
    return circles->create(*rule).value_or(std::string());
}

std::vector<std::string>
getCircles(const std::string& accountId)
{
    std::vector<std::string> result;
    if (const auto circles = circleService(accountId)) {
        for (const auto& [circle, info] : circles->circles()) {
            if (!info.departed)
                result.emplace_back(circle);
        }
    }
    return result;
}

bool
removeCircle(const std::string& accountId, const std::string& circleId, const std::string& successor)
{
    const auto circles = circleService(accountId);
    return circles && circles->leave(circleId, successor);
}

bool
inviteCircleMember(const std::string& accountId, const std::string& circleId, const std::string& memberUri)
{
    const auto circles = circleService(accountId);
    return circles && circles->invite(circleId, memberUri);
}

bool
banCircleMember(const std::string& accountId, const std::string& circleId, const std::string& memberUri)
{
    const auto circles = circleService(accountId);
    return circles && circles->ban(circleId, memberUri);
}

bool
unbanCircleMember(const std::string& accountId, const std::string& circleId, const std::string& memberUri)
{
    const auto circles = circleService(accountId);
    return circles && circles->unban(circleId, memberUri);
}

std::vector<std::map<std::string, std::string>>
getCircleMembers(const std::string& accountId, const std::string& circleId)
{
    std::vector<std::map<std::string, std::string>> result;
    const auto circles = circleService(accountId);
    const auto repository = circles ? circles->repository(circleId) : nullptr;
    if (!repository)
        return result;
    const auto policy = repository->policy();
    for (const auto& [uri, member] : policy.members) {
        std::string roles;
        for (const auto& role : member.roles) {
            if (!roles.empty())
                roles += ",";
            roles += role;
        }
        result.push_back({{"uri", uri},
                          {"state", std::string(stateName(member.state))},
                          {"roles", roles},
                          {"admin", uri == policy.admin ? "true" : "false"}});
    }
    return result;
}

std::vector<std::map<std::string, std::string>>
getCircleRequests(const std::string& accountId)
{
    std::vector<std::map<std::string, std::string>> result;
    if (const auto circles = circleService(accountId)) {
        for (const auto& [circle, request] : circles->requests()) {
            if (!request.declined)
                result.emplace_back(jami::CircleService::requestMap(circle, request));
        }
    }
    return result;
}

void
acceptCircleRequest(const std::string& accountId, const std::string& circleId)
{
    if (const auto circles = circleService(accountId))
        circles->acceptRequest(circleId, [accountId, circleId](bool joined) {
            if (!joined)
                JAMI_WARNING("[Account {}] Unable to join circle {}", accountId, circleId);
        });
}

void
declineCircleRequest(const std::string& accountId, const std::string& circleId)
{
    if (const auto circles = circleService(accountId))
        circles->declineRequest(circleId);
}

std::string
getCirclePolicy(const std::string& accountId, const std::string& circleId)
{
    const auto circles = circleService(accountId);
    const auto repository = circles ? circles->repository(circleId) : nullptr;
    return repository ? jami::circlePolicyToJson(repository->policy()) : std::string();
}

bool
updateCirclePolicy(const std::string& accountId, const std::string& circleId, const std::string& change)
{
    const auto circles = circleService(accountId);
    return circles && circles->updatePolicy(circleId, change);
}

std::string
createCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& access)
{
    const auto circles = circleService(accountId);
    const auto policy = access.empty() ? std::optional<jami::ChannelPolicy>(jami::ChannelPolicy {})
                                       : jami::channelPolicyFromJson(access);
    if (!circles || !policy)
        return {};
    return circles->createChannel(circleId, *policy).value_or(std::string());
}

std::map<std::string, std::string>
getCircleProfile(const std::string& accountId, const std::string& circleId, const std::string& channelId)
{
    const auto circles = circleService(accountId);
    return circles ? circles->profile(circleId, channelId) : std::map<std::string, std::string>();
}

bool
updateCircleProfile(const std::string& accountId,
                    const std::string& circleId,
                    const std::map<std::string, std::string>& profile,
                    const std::string& channelId)
{
    const auto circles = circleService(accountId);
    return circles && circles->updateProfile(circleId, profile, channelId);
}

bool
archiveCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& channelId)
{
    const auto circles = circleService(accountId);
    return circles && circles->archiveChannel(circleId, channelId);
}

std::vector<std::map<std::string, std::string>>
getCircleChannels(const std::string& accountId, const std::string& circleId)
{
    std::vector<std::map<std::string, std::string>> result;
    const auto circles = circleService(accountId);
    const auto repository = circles ? circles->repository(circleId) : nullptr;
    if (!repository)
        return result;
    const auto catalogue = circles->circles();
    const auto info = catalogue.find(circleId);
    for (const auto& [channel, policy] : repository->policy().channels) {
        const bool joined = info != catalogue.end() && info->second.channels.contains(channel);
        auto entry = circles->profile(circleId, channel);
        entry.emplace("id", channel);
        entry.emplace("archived", policy.archived ? "true" : "false");
        entry.emplace("joined", joined ? "true" : "false");
        result.emplace_back(std::move(entry));
    }
    return result;
}

void
joinCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& channelId)
{
    if (const auto circles = circleService(accountId))
        circles->joinChannel(circleId, channelId, [accountId, channelId](bool joined) {
            if (!joined)
                JAMI_WARNING("[Account {}] Unable to join channel {}", accountId, channelId);
        });
}

void
leaveCircleChannel(const std::string& accountId, const std::string& circleId, const std::string& channelId)
{
    if (const auto circles = circleService(accountId))
        circles->leaveChannel(circleId, channelId);
}

} // namespace libjami
