#pragma once

#include "circle_policy.h"
#include "commit_message.h"

#include <cstddef>

namespace jami {

struct CircleGenesis
{
    std::string admin;
    PermissionRule defaults;
    std::string nonce;
};

struct CirclePolicyMerge
{
    std::string other;
};

struct CircleChannelGenesis
{
    std::string creator;
    std::string nonce;
    ChannelPolicy access;
};

struct CircleChannelContent
{
    std::string channel;
    std::string parent;
    CommitMessage message;
};

struct CircleChannelMerge
{
    std::string channel;
    std::string parent;
    std::string other;
};

struct LIBJAMI_TEST_EXPORT CircleRecord
{
    static constexpr unsigned VERSION = 1;
    static constexpr size_t MAX_SIZE = 128 * 1024;
    static constexpr size_t MAX_SCOPE_SIZE = 1024;
    static constexpr size_t MAX_ROLE_ID_SIZE = 64;
    static constexpr size_t MAX_CHANNEL_PROOF_SIZE = 16 * 1024;
    static constexpr size_t MAX_CHANNEL_BODY_SIZE = 64 * 1024;

    std::variant<CircleGenesis,
                 CirclePolicyChange,
                 CircleModerationVote,
                 CircleModerationResolution,
                 CirclePolicyMerge,
                 CircleChannelGenesis,
                 CircleChannelContent,
                 CircleChannelMerge>
        payload;
    std::string circle;
    std::string base;

    std::optional<std::string> toString() const;
    static std::optional<CircleRecord> fromString(std::string_view text);
    std::optional<CirclePolicy> initialPolicy(std::string_view authenticatedActor) const;
    std::optional<CirclePolicy> apply(const CirclePolicy& policy,
                                      std::string_view authenticatedActor,
                                      std::string_view circleId,
                                      std::string_view baseCommit) const;
};

// A policy snapshot for clients, with the field and permission names used in records.
LIBJAMI_TEST_EXPORT std::string circlePolicyToJson(const CirclePolicy& policy);
// Parses one operation as written in records, e.g. {"type":"set-role","role":"writer","rule":...}.
LIBJAMI_TEST_EXPORT std::optional<CirclePolicyChange> circlePolicyChangeFromJson(std::string_view text);
// A rule, {"allow":[...],"deny":[...]}, or a channel's initial access, {"defaults":...,"roles":{...}}.
LIBJAMI_TEST_EXPORT std::optional<PermissionRule> circleRuleFromJson(std::string_view text);
LIBJAMI_TEST_EXPORT std::optional<ChannelPolicy> channelPolicyFromJson(std::string_view text);

} // namespace jami
