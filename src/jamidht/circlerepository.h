#pragma once

#include "circle_policy.h"
#include "commit_message.h"

#include <opendht/crypto.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace jami {

struct CircleRecord;

enum class ChannelValidationStatus { VALID, INVALID, PENDING_DEPENDENCY, UNREGISTERED, LIMIT_EXCEEDED, DIVERGED };

struct ChannelValidationResult
{
    ChannelValidationStatus status {ChannelValidationStatus::INVALID};
    std::vector<std::string> missingPolicies;
    std::string head;
};

struct CircleModerationResult
{
    std::string vote;
    std::optional<std::string> resolution;
};

struct CircleChannelResult
{
    std::string channel;
    std::optional<std::string> registration;
};

struct CircleChannelEntry
{
    std::string id;
    std::vector<std::string> parents;
    std::string author;
    std::string device;
    int64_t timestamp {0};
    // Absent for the channel genesis and merges.
    std::optional<CommitMessage> message;
};

class LIBJAMI_TEST_EXPORT CircleRepository
{
public:
    static constexpr size_t DEFAULT_VALIDATION_LIMIT = 100000;
    static constexpr size_t MAX_PENDING_VOTES = 1024;
    static constexpr size_t MAX_MERGE_COMMITS = 4096;
    static constexpr size_t MAX_VALIDATION_FRONTIER = 4096;
    // Bump whenever validation rules change: saved states from older rules are replayed again.
    static constexpr unsigned STATE_VERSION = 4;
    static constexpr const char* STATE_FILE = "jami-state";

    static std::unique_ptr<CircleRepository> create(const std::filesystem::path& path,
                                                    const dht::crypto::Identity& identity,
                                                    const PermissionRule& defaults = {});
    static std::unique_ptr<CircleRepository> open(const std::filesystem::path& path,
                                                  std::string_view expectedCircle,
                                                  size_t validationLimit = DEFAULT_VALIDATION_LIMIT);
    ~CircleRepository();

    const std::string& id() const;
    std::string head() const;
    CirclePolicy policy() const;
    std::string profile(std::optional<std::string_view> channel = std::nullopt) const;
    std::optional<std::string> updateProfile(const dht::crypto::Identity& identity,
                                             std::string_view vcard,
                                             std::optional<std::string_view> channel = std::nullopt);
    // Whether device may sign for its member; any device of an active member may read.
    bool enrolled(std::string_view device) const;
    uint64_t generation() const;
    bool authorizePeer(const dht::crypto::Certificate& certificate,
                       std::optional<std::string_view> channel = std::nullopt) const;
    std::optional<std::string> fastForward(std::string_view other, size_t validationLimit = DEFAULT_VALIDATION_LIMIT);
    bool reload();
    std::optional<std::string> append(const dht::crypto::Identity& identity, const CirclePolicyChange& change);
    std::optional<std::string> vote(const dht::crypto::Identity& identity, const CircleModerationVote& operation);
    std::optional<std::string> resolveVote(const dht::crypto::Identity& identity, std::string_view voteCommit);
    std::optional<CircleModerationResult> moderate(const dht::crypto::Identity& identity,
                                                   const CircleModerationVote& operation);
    std::vector<std::string> pendingVotes() const;
    std::optional<std::string> merge(const dht::crypto::Identity& identity, std::string_view otherCommit);
    std::optional<CircleChannelResult> createChannel(const std::filesystem::path& path,
                                                     const dht::crypto::Identity& identity,
                                                     const ChannelPolicy& policy = {});
    bool verifyChannel(const std::filesystem::path& path, std::string_view channel) const;
    ChannelValidationResult validateChannel(const std::filesystem::path& path,
                                            std::string_view channel,
                                            size_t validationLimit = DEFAULT_VALIDATION_LIMIT) const;
    ChannelValidationResult fastForwardChannel(const std::filesystem::path& path,
                                               std::string_view channel,
                                               std::string_view other);
    ChannelValidationResult mergeChannel(const std::filesystem::path& path,
                                         std::string_view channel,
                                         const dht::crypto::Identity& identity,
                                         std::string_view other);
    std::optional<std::string> appendChannelMessage(const std::filesystem::path& path,
                                                    std::string_view channel,
                                                    const dht::crypto::Identity& identity,
                                                    const CommitMessage& message);
    /**
     * Visits the verified channel history from its head, newest first, until `visit` returns
     * false: starting at `from` when given, and without the history of `stop`. Fails unless the
     * channel is valid and `from` belongs to its history. `visit` must not use the repository.
     */
    bool channelLog(const std::filesystem::path& path,
                    std::string_view channel,
                    const std::function<bool(CircleChannelEntry&&)>& visit,
                    std::string_view from = {},
                    std::string_view stop = {}) const;

private:
    std::optional<std::string> appendRecord(const dht::crypto::Identity& identity,
                                            const CircleRecord& record,
                                            std::optional<std::string_view> profile = std::nullopt);
    struct Impl;
    explicit CircleRepository(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
