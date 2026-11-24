#include "circle_policy.h"

#include <algorithm>
#include <array>
#include <type_traits>

namespace jami {

namespace {

constexpr std::array governancePermissions {Permission::AddMember,
                                            Permission::BanUnbanMember,
                                            Permission::ManageChannels,
                                            Permission::ManageRoles,
                                            Permission::AssignRoles,
                                            Permission::ModerateConference};

void
restrictArchived(PermissionSet& permissions)
{
    std::erase_if(permissions, [](auto permission) {
        return permission != Permission::ReadChannel
               && std::find(governancePermissions.begin(), governancePermissions.end(), permission)
                      == governancePermissions.end();
    });
}

bool
isValidRule(const PermissionRule& rule)
{
    return std::all_of(rule.allow.begin(), rule.allow.end(), isValidPermission)
           && std::all_of(rule.deny.begin(), rule.deny.end(), isValidPermission);
}

bool
changesWithin(const PermissionSet& before, const PermissionSet& after, const PermissionSet& allowed)
{
    for (auto permission : before) {
        if (!after.contains(permission) && !allowed.contains(permission))
            return false;
    }
    for (auto permission : after) {
        if (!before.contains(permission) && !allowed.contains(permission))
            return false;
    }
    return true;
}

const PermissionRule&
channelRule(const CirclePolicy& policy, const std::string& channelId, const std::optional<std::string>& roleId)
{
    const auto& channel = policy.channels.at(channelId);
    if (!roleId)
        return channel.defaults ? *channel.defaults : policy.defaults;
    auto rule = channel.roles.find(*roleId);
    return rule != channel.roles.end() ? rule->second : policy.roles.at(*roleId);
}

PermissionSet
permissionsForRoles(const CirclePolicy& policy, const CircleMember& member, const ChannelPolicy* channel)
{
    PermissionSet allowed;
    PermissionSet denied;
    auto apply = [&](const PermissionRule& rule) {
        if (!isValidRule(rule))
            return false;
        allowed.insert(rule.allow.begin(), rule.allow.end());
        denied.insert(rule.deny.begin(), rule.deny.end());
        return true;
    };
    if (!apply(channel && channel->defaults ? *channel->defaults : policy.defaults))
        return {};
    for (const auto& roleId : member.roles) {
        auto role = policy.roles.find(roleId);
        if (role == policy.roles.end())
            return {};
        const PermissionRule* rule = &role->second;
        if (channel) {
            auto override = channel->roles.find(roleId);
            if (override != channel->roles.end())
                rule = &override->second;
        }
        if (!apply(*rule))
            return {};
    }
    for (auto permission : denied)
        allowed.erase(permission);
    if (channel && !allowed.contains(Permission::ReadChannel))
        return {};
    if (channel && channel->archived)
        restrictArchived(allowed);
    return allowed;
}

bool
hasGovernance(const CirclePolicy& policy, const CircleMember& member)
{
    auto governs = [&](const ChannelPolicy* channel) {
        const auto permissions = permissionsForRoles(policy, member, channel);
        return std::any_of(governancePermissions.begin(), governancePermissions.end(), [&](auto permission) {
            return permissions.contains(permission);
        });
    };
    if (governs(nullptr))
        return true;
    return std::any_of(policy.channels.begin(), policy.channels.end(), [&](const auto& entry) {
        return governs(&entry.second);
    });
}

bool
preservesPeerGovernance(const CirclePolicy& before, const CirclePolicy& after, std::string_view actor)
{
    auto includes = [](const auto& available, const auto& required) {
        return std::includes(available.begin(), available.end(), required.begin(), required.end());
    };
    for (const auto& [uri, member] : before.members) {
        if (uri == actor || uri == before.admin)
            continue;
        const auto& next = after.members.at(uri);
        auto preserves = [&](const ChannelPolicy* previousChannel, const ChannelPolicy* nextChannel) {
            const auto previous = permissionsForRoles(before, member, previousChannel);
            const auto current = permissionsForRoles(after, next, nextChannel);
            return std::all_of(governancePermissions.begin(), governancePermissions.end(), [&](auto permission) {
                return !previous.contains(permission) || current.contains(permission);
            });
        };
        if (!preserves(nullptr, nullptr))
            return false;
        for (const auto& [channel, policy] : before.channels) {
            if (!preserves(&policy, &after.channels.at(channel)))
                return false;
        }
        const auto permissions = permissionsForRoles(before, member, nullptr);
        auto covered = [&](auto&& matches) {
            return std::any_of(next.roles.begin(), next.roles.end(), [&](const auto& role) {
                const auto delegation = after.delegations.find(role);
                return delegation != after.delegations.end() && matches(delegation->second);
            });
        };
        for (const auto& role : member.roles) {
            const auto found = before.delegations.find(role);
            if (found == before.delegations.end())
                continue;
            const auto& delegation = found->second;
            if (delegation.circleWide && permissions.contains(Permission::AssignRoles)
                && !delegation.assignableRoles.empty() && !covered([&](const auto& candidate) {
                       return candidate.circleWide && includes(candidate.assignableRoles, delegation.assignableRoles);
                   }))
                return false;
            auto coversEdit = [&](const auto& candidate) {
                return includes(candidate.permissions, delegation.permissions)
                       && includes(candidate.editableRoles, delegation.editableRoles);
            };
            if (delegation.circleWide && permissions.contains(Permission::ManageRoles)
                && !delegation.editableRoles.empty()
                && !covered([&](const auto& candidate) { return candidate.circleWide && coversEdit(candidate); }))
                return false;
            for (const auto& channel : delegation.channels) {
                const auto previous = permissionsForRoles(before, member, &before.channels.at(channel));
                if (previous.contains(Permission::ManageRoles) && !covered([&](const auto& candidate) {
                        return candidate.channels.contains(channel) && coversEdit(candidate);
                    }))
                    return false;
            }
        }
    }
    return true;
}

bool
gainsWithin(const CirclePolicy& before,
            const CirclePolicy& after,
            const RoleDelegation& delegation,
            std::optional<std::string_view> role,
            std::optional<std::string_view> channel)
{
    auto permitted = [&](const std::string& uri, std::optional<std::string_view> channelId) {
        auto previousChannel = channelId ? &before.channels.find(*channelId)->second : nullptr;
        auto nextChannel = channelId ? &after.channels.find(*channelId)->second : nullptr;
        auto previous = permissionsForRoles(before, before.members.at(uri), previousChannel);
        for (auto permission : permissionsForRoles(after, after.members.at(uri), nextChannel)) {
            if (!previous.contains(permission) && !delegation.permissions.contains(permission))
                return false;
        }
        return true;
    };
    for (const auto& [uri, member] : before.members) {
        if (uri == before.admin || (role && !member.roles.contains(*role)))
            continue;
        if (channel) {
            if (!permitted(uri, channel))
                return false;
        } else {
            if (!permitted(uri, std::nullopt))
                return false;
            for (const auto& [channelId, policy] : before.channels) {
                if (!permitted(uri, channelId))
                    return false;
            }
        }
    }
    return true;
}

} // namespace

bool
CirclePolicy::isValid() const
{
    auto administrator = members.find(admin);
    if (admin.empty() || administrator == members.end() || administrator->second.state != CircleMemberState::ACTIVE
        || !isValidRule(defaults))
        return false;

    for (const auto& [roleId, rule] : roles) {
        if (roleId.empty() || !isValidRule(rule))
            return false;
    }
    for (const auto& [uri, member] : members) {
        if (uri.empty())
            return false;
        switch (member.state) {
        case CircleMemberState::INVITED:
        case CircleMemberState::ACTIVE:
        case CircleMemberState::LEFT:
        case CircleMemberState::BANNED:
            break;
        default:
            return false;
        }
        for (const auto& roleId : member.roles) {
            if (!roles.contains(roleId))
                return false;
        }
    }
    for (const auto& [channelId, channel] : channels) {
        if (channelId.empty() || (channel.defaults && !isValidRule(*channel.defaults)))
            return false;
        for (const auto& [roleId, rule] : channel.roles) {
            if (!roles.contains(roleId) || !isValidRule(rule))
                return false;
        }
    }
    for (const auto& [roleId, delegation] : delegations) {
        if (!roles.contains(roleId)
            || !std::all_of(delegation.permissions.begin(), delegation.permissions.end(), isValidPermission))
            return false;
        for (const auto& editable : delegation.editableRoles) {
            if (!roles.contains(editable))
                return false;
        }
        for (const auto& assignable : delegation.assignableRoles) {
            if (!roles.contains(assignable))
                return false;
        }
        for (const auto& channelId : delegation.channels) {
            if (!channels.contains(channelId))
                return false;
        }
    }
    return true;
}

PermissionSet
CirclePolicy::permissions(std::string_view uri, std::optional<std::string_view> channel) const
{
    auto member = members.find(uri);
    auto administrator = members.find(admin);
    if (uri.empty() || admin.empty() || member == members.end() || administrator == members.end()
        || member->second.state != CircleMemberState::ACTIVE
        || administrator->second.state != CircleMemberState::ACTIVE)
        return {};

    const ChannelPolicy* channelPolicy = nullptr;
    if (channel) {
        auto found = channels.find(*channel);
        if (found == channels.end())
            return {};
        channelPolicy = &found->second;
    }

    if (uri == admin) {
        PermissionSet allowed;
        for (int value = 0; value < static_cast<int>(Permission::Count); ++value)
            allowed.insert(static_cast<Permission>(value));
        if (channelPolicy && channelPolicy->archived)
            restrictArchived(allowed);
        if (ended)
            std::erase_if(allowed, [](auto permission) { return permission != Permission::ReadChannel; });
        return allowed;
    }

    auto allowed = permissionsForRoles(*this, member->second, channelPolicy);
    if (ended)
        std::erase_if(allowed, [](auto permission) { return permission != Permission::ReadChannel; });
    return allowed;
}

bool
CirclePolicy::hasPermission(std::string_view uri, Permission permission, std::optional<std::string_view> channel) const
{
    return isValidPermission(permission) && permissions(uri, channel).contains(permission);
}

std::optional<CirclePolicy>
CirclePolicy::moderate(std::string_view actor, const CircleModerationVote& vote) const
{
    if (!isValid() || ended || !hasPermission(actor, Permission::BanUnbanMember) || vote.member == admin
        || vote.member == actor)
        return std::nullopt;
    const auto target = members.find(vote.member);
    if (target == members.end())
        return std::nullopt;
    auto next = *this;
    if (vote.action == CircleModerationAction::BAN_MEMBER) {
        if (target->second.state == CircleMemberState::BANNED
            || (actor != admin && hasGovernance(*this, target->second)))
            return std::nullopt;
        next.members.at(vote.member) = CircleMember {CircleMemberState::BANNED, {}};
    } else if (vote.action == CircleModerationAction::UNBAN_MEMBER) {
        if (target->second.state != CircleMemberState::BANNED)
            return std::nullopt;
        next.members.at(vote.member) = CircleMember {CircleMemberState::INVITED, {}};
    } else {
        return std::nullopt;
    }
    return next;
}

bool
CirclePolicy::preservesGovernance(const CirclePolicy& next, std::string_view actor) const
{
    return actor == admin || preservesPeerGovernance(*this, next, actor);
}

std::optional<CirclePolicy>
CirclePolicy::apply(std::string_view actor, const CirclePolicyChange& change) const
{
    if (!isValid() || ended || change.valueless_by_exception())
        return std::nullopt;
    auto member = members.find(actor);
    const bool joining = std::holds_alternative<JoinCircleMember>(change);
    if (member == members.end()
        || (member->second.state != CircleMemberState::ACTIVE
            && !(joining && member->second.state == CircleMemberState::INVITED)))
        return std::nullopt;

    auto next = *this;
    const bool administrator = actor == admin;
    auto anyDelegation = [&](auto&& matches) {
        for (const auto& roleId : member->second.roles) {
            auto delegation = delegations.find(roleId);
            if (delegation != delegations.end() && matches(delegation->second))
                return true;
        }
        return false;
    };
    auto allowedRule = [&](const RoleDelegation& delegation,
                           const PermissionRule& before,
                           const PermissionRule& after,
                           std::optional<std::string_view> role,
                           std::optional<std::string_view> channel) {
        return changesWithin(before.allow, after.allow, delegation.permissions)
               && changesWithin(before.deny, after.deny, delegation.permissions)
               && gainsWithin(*this, next, delegation, role, channel);
    };
    const bool authorized = std::visit(
        [&](const auto& operation) {
            using Operation = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<Operation, SetCircleDefaults>) {
                next.defaults = operation.rule;
                return administrator;
            } else if constexpr (std::is_same_v<Operation, SetCircleRole>) {
                next.roles[operation.role] = operation.rule;
                if (administrator)
                    return true;
                auto previous = roles.find(operation.role);
                return previous != roles.end() && isValidRule(operation.rule)
                       && hasPermission(actor, Permission::ManageRoles)
                       && anyDelegation([&](const RoleDelegation& delegation) {
                              return delegation.circleWide && delegation.editableRoles.contains(operation.role)
                                     && allowedRule(delegation,
                                                    previous->second,
                                                    operation.rule,
                                                    operation.role,
                                                    std::nullopt);
                          });
            } else if constexpr (std::is_same_v<Operation, SetCircleRoleAssignment>) {
                auto target = next.members.find(operation.member);
                if (target == next.members.end() || target->second.state != CircleMemberState::ACTIVE
                    || !roles.contains(operation.role) || (!administrator && operation.member == admin))
                    return false;
                if (operation.assigned)
                    target->second.roles.insert(operation.role);
                else
                    target->second.roles.erase(operation.role);
                return administrator
                       || (hasPermission(actor, Permission::AssignRoles)
                           && anyDelegation([&](const RoleDelegation& delegation) {
                                  return delegation.circleWide && delegation.assignableRoles.contains(operation.role);
                              }));
            } else if constexpr (std::is_same_v<Operation, SetCircleChannelRule>) {
                auto channel = next.channels.find(operation.channel);
                if (channel == next.channels.end() || (operation.role && !roles.contains(*operation.role))
                    || (operation.rule && !isValidRule(*operation.rule)))
                    return false;
                if (operation.role) {
                    if (operation.rule)
                        channel->second.roles[*operation.role] = *operation.rule;
                    else
                        channel->second.roles.erase(*operation.role);
                } else {
                    channel->second.defaults = operation.rule;
                }
                return administrator
                       || (hasPermission(actor, Permission::ManageRoles, operation.channel)
                           && anyDelegation([&](const RoleDelegation& delegation) {
                                  return delegation.channels.contains(operation.channel)
                                         && (!operation.role || delegation.editableRoles.contains(*operation.role))
                                         && allowedRule(delegation,
                                                        channelRule(*this, operation.channel, operation.role),
                                                        channelRule(next, operation.channel, operation.role),
                                                        operation.role,
                                                        operation.channel);
                              }));
            } else if constexpr (std::is_same_v<Operation, SetCircleRoleDelegation>) {
                if (!roles.contains(operation.role))
                    return false;
                if (operation.delegation)
                    next.delegations[operation.role] = *operation.delegation;
                else
                    next.delegations.erase(operation.role);
                return administrator;
            } else if constexpr (std::is_same_v<Operation, InviteCircleMember>) {
                if (operation.member.empty() || !hasPermission(actor, Permission::AddMember))
                    return false;
                auto target = members.find(operation.member);
                if (target != members.end() && target->second.state != CircleMemberState::LEFT)
                    return false;
                next.members[operation.member] = CircleMember {CircleMemberState::INVITED, {}};
                return true;
            } else if constexpr (std::is_same_v<Operation, JoinCircleMember>) {
                if (operation.member != actor || member->second.state != CircleMemberState::INVITED)
                    return false;
                next.members.at(operation.member) = CircleMember {CircleMemberState::ACTIVE, {}};
                return true;
            } else if constexpr (std::is_same_v<Operation, LeaveCircleMember>) {
                if (operation.member != actor || administrator)
                    return false;
                next.members.at(operation.member) = CircleMember {CircleMemberState::LEFT, {}};
                return true;
            } else if constexpr (std::is_same_v<Operation, RegisterCircleDevice>) {
                return operation.member == actor && !operation.device.empty();
            } else if constexpr (std::is_same_v<Operation, RegisterCircleChannel>) {
                if (operation.channel.empty() || operation.authorization.empty() || operation.policy.archived
                    || channels.contains(operation.channel) || !hasPermission(actor, Permission::ManageChannels))
                    return false;
                next.channels.emplace(operation.channel, operation.policy);
                next.channels.at(operation.channel).authorization = operation.authorization;
                if (administrator)
                    return true;
                for (const auto& [uri, state] : members) {
                    if (uri == admin)
                        continue;
                    const auto inherited = permissionsForRoles(*this, state, nullptr);
                    for (auto permission : permissionsForRoles(next, state, &next.channels.at(operation.channel))) {
                        if (!inherited.contains(permission))
                            return false;
                    }
                }
                return true;
            } else if constexpr (std::is_same_v<Operation, ArchiveCircleChannel>) {
                const auto found = channels.find(operation.channel);
                if (found == channels.end() || found->second.archived
                    || !hasPermission(actor, Permission::ManageChannels, operation.channel))
                    return false;
                next.channels.at(operation.channel).archived = true;
                return true;
            } else if constexpr (std::is_same_v<Operation, TransferCircleAdmin>) {
                const auto target = members.find(operation.member);
                if (!administrator || operation.member == actor || target == members.end()
                    || target->second.state != CircleMemberState::ACTIVE)
                    return false;
                next.admin = operation.member;
                return true;
            } else if constexpr (std::is_same_v<Operation, EndCircle>) {
                next.ended = true;
                return administrator;
            } else if constexpr (std::is_same_v<Operation, SetCircleProfile>) {
                return operation.channel ? hasPermission(actor, Permission::ManageChannels, *operation.channel)
                                         : hasPermission(actor, Permission::UpdateProfile);
            }
        },
        change);
    if (!authorized || !next.isValid() || !preservesGovernance(next, actor))
        return std::nullopt;
    return next;
}

} // namespace jami
