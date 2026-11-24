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

#ifndef LIBJAMI_CIRCLEI_H
#define LIBJAMI_CIRCLEI_H

#include "def.h"

#include <map>
#include <string>
#include <vector>

/**
 * Circles are communities with their own membership, roles and channels, synced between
 * members like conversations. Rules, roles and delegations use the JSON of circle records:
 * a rule is {"allow":[...],"deny":[...]} with permission names such as "read-channel" or "text".
 *
 * A joined channel holds messages like a conversation: its id is used as a conversation id with
 * sendMessage, loadConversation, loadSwarmUntil, clearCache and getConversationPermissions, and
 * with the message, reaction and error signals of ConversationSignal. Channels are otherwise not
 * conversations: they are not listed with them, and are managed with the functions below.
 */
namespace libjami {

/// Creates a circle administered by the account. @p defaults is the rule of every member.
LIBJAMI_PUBLIC std::string createCircle(const std::string& accountId, const std::string& defaults = "");
LIBJAMI_PUBLIC std::vector<std::string> getCircles(const std::string& accountId);
/// Leaves a circle. Its ADMIN first hands it to @p successor, or ends it for everyone without one.
LIBJAMI_PUBLIC bool removeCircle(const std::string& accountId,
                                 const std::string& circleId,
                                 const std::string& successor = "");

LIBJAMI_PUBLIC bool inviteCircleMember(const std::string& accountId,
                                       const std::string& circleId,
                                       const std::string& memberUri);
LIBJAMI_PUBLIC bool banCircleMember(const std::string& accountId,
                                    const std::string& circleId,
                                    const std::string& memberUri);
LIBJAMI_PUBLIC bool unbanCircleMember(const std::string& accountId,
                                      const std::string& circleId,
                                      const std::string& memberUri);
/// Members with "uri", "state" (invited, active, left or banned), "roles" (comma separated) and
/// "admin" ("true" for the circle's ADMIN).
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getCircleMembers(const std::string& accountId,
                                                                                const std::string& circleId);

/// Pending invitations with "id", "from", "received" and an unverified "name" hint.
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getCircleRequests(const std::string& accountId);
/// Clones the circle from the inviting device and joins it; CircleReady follows.
LIBJAMI_PUBLIC void acceptCircleRequest(const std::string& accountId, const std::string& circleId);
LIBJAMI_PUBLIC void declineCircleRequest(const std::string& accountId, const std::string& circleId);

/// The circle policy as JSON: admin, defaults, members, roles, channels and delegations.
LIBJAMI_PUBLIC std::string getCirclePolicy(const std::string& accountId, const std::string& circleId);
/// Applies one operation: set-defaults, set-role, set-role-assignment, set-channel-rule or
/// set-role-delegation, e.g. {"type":"set-role","role":"writer","rule":{...}}.
LIBJAMI_PUBLIC bool updateCirclePolicy(const std::string& accountId,
                                       const std::string& circleId,
                                       const std::string& change);

/// Shared vCard fields: title, description and avatar (base64). A channel has no avatar.
LIBJAMI_PUBLIC std::map<std::string, std::string> getCircleProfile(const std::string& accountId,
                                                                   const std::string& circleId,
                                                                   const std::string& channelId = "");
/// Updates supplied fields; an empty value clears a field. CircleUpdated follows a successful write.
LIBJAMI_PUBLIC bool updateCircleProfile(const std::string& accountId,
                                        const std::string& circleId,
                                        const std::map<std::string, std::string>& profile,
                                        const std::string& channelId = "");

/// Creates and joins a channel. @p access is its initial access, {"defaults":rule|null,"roles":{}}.
LIBJAMI_PUBLIC std::string createCircleChannel(const std::string& accountId,
                                               const std::string& circleId,
                                               const std::string& access = "");
LIBJAMI_PUBLIC bool archiveCircleChannel(const std::string& accountId,
                                         const std::string& circleId,
                                         const std::string& channelId);
/// Channels with "id", "archived", "joined" ("true" or "false") and shared "title"/"description" fields.
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getCircleChannels(const std::string& accountId,
                                                                                 const std::string& circleId);
/// Clones a channel the account may read from a connected member; CircleChannelReady follows.
LIBJAMI_PUBLIC void joinCircleChannel(const std::string& accountId,
                                      const std::string& circleId,
                                      const std::string& channelId);
LIBJAMI_PUBLIC void leaveCircleChannel(const std::string& accountId,
                                       const std::string& circleId,
                                       const std::string& channelId);

struct LIBJAMI_PUBLIC CircleSignal
{
    struct LIBJAMI_PUBLIC CircleRequestReceived
    {
        constexpr static const char* name = "CircleRequestReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*circleId*/,
                             std::map<std::string, std::string> /*request*/);
    };
    /// The circle was created, joined, or cloned by another device of the account.
    struct LIBJAMI_PUBLIC CircleReady
    {
        constexpr static const char* name = "CircleReady";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /*circleId*/);
    };
    /// The circle changed: members, roles, rules, channels or shared profiles.
    struct LIBJAMI_PUBLIC CircleUpdated
    {
        constexpr static const char* name = "CircleUpdated";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /*circleId*/);
    };
    struct LIBJAMI_PUBLIC CircleRemoved
    {
        constexpr static const char* name = "CircleRemoved";
        using cb_type = void(const std::string& /*accountId*/, const std::string& /*circleId*/);
    };
    /// The channel was created or joined: its id can now be used as a conversation id.
    struct LIBJAMI_PUBLIC CircleChannelReady
    {
        constexpr static const char* name = "CircleChannelReady";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*circleId*/,
                             const std::string& /*channelId*/);
    };
};

} // namespace libjami

#endif // LIBJAMI_CIRCLEI_H
