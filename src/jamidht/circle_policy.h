#pragma once

#include "jami/def.h"
#include "permission.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>

namespace jami {

using PermissionSet = std::set<Permission>;

enum class CircleMemberState { INVITED, ACTIVE, LEFT, BANNED };

struct PermissionRule
{
    PermissionSet allow;
    PermissionSet deny;

    bool operator==(const PermissionRule&) const = default;
};

struct CircleMember
{
    CircleMemberState state {CircleMemberState::INVITED};
    std::set<std::string, std::less<>> roles;

    bool operator==(const CircleMember&) const = default;
};

struct ChannelPolicy
{
    std::optional<PermissionRule> defaults;
    std::map<std::string, PermissionRule, std::less<>> roles;
    bool archived {false};
    std::string authorization;

    bool operator==(const ChannelPolicy&) const = default;
};

struct RoleDelegation
{
    PermissionSet permissions;
    std::set<std::string, std::less<>> editableRoles;
    std::set<std::string, std::less<>> assignableRoles;
    std::set<std::string, std::less<>> channels;
    bool circleWide {false};

    bool operator==(const RoleDelegation&) const = default;
};

struct SetCircleDefaults
{
    PermissionRule rule;
};

struct SetCircleRole
{
    std::string role;
    PermissionRule rule;
};

struct SetCircleRoleAssignment
{
    std::string member;
    std::string role;
    bool assigned {true};
};

struct SetCircleChannelRule
{
    std::string channel;
    std::optional<std::string> role;
    std::optional<PermissionRule> rule;
};

struct SetCircleRoleDelegation
{
    std::string role;
    std::optional<RoleDelegation> delegation;
};

struct InviteCircleMember
{
    std::string member;
};

struct JoinCircleMember
{
    std::string member;
};

struct LeaveCircleMember
{
    std::string member;
};

struct RegisterCircleDevice
{
    std::string member;
    std::string device;
};

struct RegisterCircleChannel
{
    std::string channel;
    std::string authorization;
    ChannelPolicy policy;
    std::string genesis;
};

struct ArchiveCircleChannel
{
    std::string channel;
};

struct TransferCircleAdmin
{
    std::string member;
};

struct EndCircle
{};

struct SetCircleProfile
{
    std::optional<std::string> channel;
};

enum class CircleModerationAction { BAN_MEMBER, UNBAN_MEMBER };

struct CircleModerationVote
{
    CircleModerationAction action {CircleModerationAction::BAN_MEMBER};
    std::string member;
};

struct CircleModerationResolution
{
    std::string vote;
};

using CirclePolicyChange = std::variant<SetCircleDefaults,
                                        SetCircleRole,
                                        SetCircleRoleAssignment,
                                        SetCircleChannelRule,
                                        SetCircleRoleDelegation,
                                        InviteCircleMember,
                                        JoinCircleMember,
                                        LeaveCircleMember,
                                        RegisterCircleDevice,
                                        RegisterCircleChannel,
                                        ArchiveCircleChannel,
                                        TransferCircleAdmin,
                                        EndCircle,
                                        SetCircleProfile>;

struct LIBJAMI_TEST_EXPORT CirclePolicy
{
    std::string admin;
    PermissionRule defaults;
    std::map<std::string, CircleMember, std::less<>> members;
    std::map<std::string, PermissionRule, std::less<>> roles;
    std::map<std::string, ChannelPolicy, std::less<>> channels;
    std::map<std::string, RoleDelegation, std::less<>> delegations;
    // An ended circle only keeps read access and accepts no further operation.
    bool ended {false};

    bool isValid() const;
    PermissionSet permissions(std::string_view member, std::optional<std::string_view> channel = std::nullopt) const;
    bool hasPermission(std::string_view member,
                       Permission permission,
                       std::optional<std::string_view> channel = std::nullopt) const;
    bool preservesGovernance(const CirclePolicy& next, std::string_view actor) const;
    std::optional<CirclePolicy> apply(std::string_view actor, const CirclePolicyChange& change) const;
    std::optional<CirclePolicy> moderate(std::string_view actor, const CircleModerationVote& vote) const;

    bool operator==(const CirclePolicy&) const = default;
};

} // namespace jami
