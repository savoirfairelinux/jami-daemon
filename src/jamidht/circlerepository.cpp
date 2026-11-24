#include "circlerepository.h"

#include "atomic_file.h"
#include "git_def.h"
#include "git_utils.h"
#include "circle_record.h"
#include "swarm_certificate.h"

#include <gnutls/crypto.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <deque>
#include <functional>
#include <initializer_list>
#include <map>
#include <mutex>
#include <set>
#include <type_traits>
#include <tuple>
#include <vector>

namespace jami {
namespace {

constexpr const char* MAIN_REF = "refs/heads/main";
constexpr size_t MAX_CERTIFICATE_SIZE = 64 * 1024;
constexpr size_t MAX_DIRECTORY_SIZE = 4 * 1024 * 1024;
constexpr size_t MAX_COMMIT_SIZE = CircleRecord::MAX_SIZE + 16 * 1024;

bool
ensureGit()
{
    struct Library
    {
        int initialized {git_libgit2_init()};
        ~Library()
        {
            if (initialized >= 0)
                git_libgit2_shutdown();
        }
    };
    static const Library library;
    return library.initialized >= 0;
}

bool
isHexId(std::string_view id, size_t length)
{
    return id.size() == length && std::all_of(id.begin(), id.end(), [](char character) {
               return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
           });
}

std::string
oidString(const git_oid* oid)
{
    return git::oidString(oid);
}

using git::objectWithin;

GitCommit
lookupCommit(git_repository* repo, const git_oid* oid)
{
    return git::lookupCommit(repo, oid, MAX_COMMIT_SIZE);
}

bool
validAt(const dht::crypto::Certificate& cert, time_t when)
{
    const auto activation = gnutls_x509_crt_get_activation_time(cert.cert);
    const auto expiration = gnutls_x509_crt_get_expiration_time(cert.cert);
    return activation != time_t(-1) && expiration != time_t(-1) && when >= activation && when <= expiration;
}

bool
validIdentity(const dht::crypto::Identity& identity)
{
    try {
        return identity.first && identity.second && identity.second->cert && identity.second->issuer
               && identity.second->issuer->cert
               && identity.first->getPublicKey().getLongId() == identity.second->getLongId()
               && isDeviceOfMember(*identity.second,
                                   *identity.second->issuer,
                                   identity.second->issuer->getId().toString());
    } catch (const std::exception&) {
        return false;
    }
}

using Signer = DeviceIdentity;

GitTree
lookupTree(git_repository* repo, const git_oid* oid, size_t limit = 1024)
{
    return git::lookupTree(repo, oid, limit);
}

std::optional<std::string>
readBlob(git_repository* repo, const git_tree* tree, const std::string& directory, const std::string& name)
{
    return git::readBlob(repo, tree, directory, name, MAX_DIRECTORY_SIZE, MAX_CERTIFICATE_SIZE);
}

std::string
profilePath(const std::optional<std::string>& channel)
{
    return channel ? "channels/" + *channel + ".vcf" : "profile.vcf";
}

std::optional<std::string>
readProfile(git_repository* repo, const git_tree* tree, const std::optional<std::string>& channel)
{
    git_tree_entry* raw = nullptr;
    if (git_tree_entry_bypath(&raw, tree, profilePath(channel).c_str()) < 0)
        return std::nullopt;
    std::unique_ptr<git_tree_entry, decltype(&git_tree_entry_free)> entry(raw, git_tree_entry_free);
    git_blob* blobRaw = nullptr;
    if (git_tree_entry_filemode(entry.get()) != GIT_FILEMODE_BLOB
        || git_blob_lookup(&blobRaw, repo, git_tree_entry_id(entry.get())) < 0)
        return std::nullopt;
    std::unique_ptr<git_blob, decltype(&git_blob_free)> blob(blobRaw, git_blob_free);
    return std::string(static_cast<const char*>(git_blob_rawcontent(blob.get())), git_blob_rawsize(blob.get()));
}

std::optional<Signer>
readSigner(git_repository* repo, const git_tree* tree, std::string_view deviceId, std::string_view memberId = {})
{
    if (!isHexId(deviceId, 64) || (!memberId.empty() && !isHexId(memberId, 40)))
        return std::nullopt;
    auto signer
        = readDeviceIdentity(repo, tree, deviceId, memberId, {"members"}, MAX_DIRECTORY_SIZE, MAX_CERTIFICATE_SIZE);
    if (!signer || !isHexId(signer->memberId, 40))
        return std::nullopt;
    return signer;
}

std::optional<CircleRecord>
verifyCommit(git_repository* repo, git_commit* commit, const Signer& signer)
{
    const auto author = git_commit_author(commit);
    const auto committer = git_commit_committer(commit);
    if (!author || !committer || !author->email || !committer->email || signer.deviceId != author->email
        || signer.deviceId != committer->email || author->when.time != committer->when.time
        || !validAt(*signer.device, static_cast<time_t>(committer->when.time))
        || !validAt(*signer.member, static_cast<time_t>(committer->when.time)))
        return std::nullopt;
    const auto signedCommit = git::extractSignature(repo, git_commit_id(commit));
    if (!signedCommit)
        return std::nullopt;
    const std::string_view signedData(signedCommit->data);
    if (signedData.find('\0') != std::string_view::npos
        || !git::checkSignature(signer.device->getPublicKey(), signedData, signedCommit->signature))
        return std::nullopt;
    const auto body = signedData.find("\n\n");
    if (body == std::string_view::npos)
        return std::nullopt;
    return CircleRecord::fromString(signedData.substr(body + 2));
}

struct PendingVote
{
    CircleModerationVote operation;
    std::string voter;
    std::string voterRevision;
    std::string targetRevision;
};

struct RuleDelta
{
    std::optional<std::string> role;
    std::optional<std::string> channel;
    int8_t present {-1};
    std::array<int8_t, static_cast<size_t>(Permission::Count)> allow;
    std::array<int8_t, static_cast<size_t>(Permission::Count)> deny;

    RuleDelta()
    {
        allow.fill(-1);
        deny.fill(-1);
    }

    bool changed() const
    {
        const auto written = [](int8_t value) {
            return value >= 0;
        };
        return std::any_of(allow.begin(), allow.end(), written) || std::any_of(deny.begin(), deny.end(), written);
    }
};

struct VerifiedState
{
    std::string circle;
    std::string head;
    CirclePolicy policy;
    git_oid tree {};
    std::map<std::string, std::string, std::less<>> devices;
    std::map<std::string, std::string, std::less<>> memberRevisions;
    std::map<std::string, PendingVote, std::less<>> votes;
    std::shared_ptr<const VerifiedState> historyBase;
    std::map<std::string, std::string, std::less<>> memberEpochs;
    std::map<std::string, std::string, std::less<>> memberTransitions;
};

struct HistoryEvent
{
    HistoryEvent(CircleRecord value, std::string signer)
        : record(std::move(value))
        , actor(std::move(signer))
    {}

    CircleRecord record;
    std::string actor;
    // Whether the actor was ADMIN when signing, which may differ from the reconciliation base.
    bool byAdmin {false};
    std::string member;
    std::string previousEpoch;
    std::string epoch;
    std::vector<std::string> affected;
    std::optional<CircleMemberState> lifecycle;
    std::optional<CirclePolicy> authority;
    Permission command {Permission::Count};
    std::optional<std::string> channel;
    std::optional<RoleDelegation> scope;
    std::optional<PermissionRule> assignedRule;
    std::optional<RoleDelegation> delegationBefore;
    std::optional<RuleDelta> rule;
    std::string actorEpoch;
    std::optional<PendingVote> proof;
    std::optional<CirclePolicy> voterAuthority;
    std::set<std::string, std::less<>> targetRoles;
    std::string actorTransition;
    std::string previousTransition;
    std::optional<std::string> requiredChannel;
};

std::optional<RuleDelta>
ruleDelta(const VerifiedState& before,
          const VerifiedState& after,
          const CirclePolicyChange& change,
          std::string_view actor)
{
    (void) actor;
    RuleDelta result;
    PermissionRule previous;
    const PermissionRule* rule = nullptr;
    if (std::holds_alternative<SetCircleDefaults>(change)) {
        previous = before.policy.defaults;
        rule = &after.policy.defaults;
    } else if (const auto role = std::get_if<SetCircleRole>(&change)) {
        result.role = role->role;
        const auto found = before.policy.roles.find(role->role);
        if (found != before.policy.roles.end())
            previous = found->second;
        rule = &after.policy.roles.at(role->role);
    } else if (const auto channel = std::get_if<SetCircleChannelRule>(&change)) {
        result.channel = channel->channel;
        result.role = channel->role;
        result.present = channel->rule.has_value();
        auto effective = [&](const CirclePolicy& policy) -> const PermissionRule& {
            const auto& rules = policy.channels.at(channel->channel);
            if (!channel->role)
                return rules.defaults ? *rules.defaults : policy.defaults;
            const auto found = rules.roles.find(*channel->role);
            return found == rules.roles.end() ? policy.roles.at(*channel->role) : found->second;
        };
        previous = effective(before.policy);
        rule = &effective(after.policy);
    } else {
        return std::nullopt;
    }
    for (size_t index = 0; index < result.allow.size(); ++index) {
        const auto permission = static_cast<Permission>(index);
        if (previous.allow.contains(permission) != rule->allow.contains(permission))
            result.allow[index] = rule->allow.contains(permission);
        if (previous.deny.contains(permission) != rule->deny.contains(permission))
            result.deny[index] = rule->deny.contains(permission);
    }
    return result;
}

void
captureAuthority(HistoryEvent& event, const VerifiedState& before, const VerifiedState& after)
{
    if (event.actor == before.policy.admin)
        return;
    const auto change = std::get_if<CirclePolicyChange>(&event.record.payload);
    if (change) {
        if (std::holds_alternative<SetCircleRole>(*change))
            event.command = Permission::ManageRoles;
        else if (const auto channel = std::get_if<SetCircleChannelRule>(change)) {
            event.command = Permission::ManageRoles;
            event.channel = channel->channel;
        } else if (std::holds_alternative<SetCircleRoleAssignment>(*change))
            event.command = Permission::AssignRoles;
        else if (std::holds_alternative<InviteCircleMember>(*change))
            event.command = Permission::AddMember;
        else if (std::holds_alternative<RegisterCircleChannel>(*change))
            event.command = Permission::ManageChannels;
        else if (const auto archive = std::get_if<ArchiveCircleChannel>(change)) {
            event.command = Permission::ManageChannels;
            event.channel = archive->channel;
        } else if (const auto profile = std::get_if<SetCircleProfile>(change)) {
            event.command = profile->channel ? Permission::ManageChannels : Permission::UpdateProfile;
            event.channel = profile->channel;
        }
    } else {
        event.command = Permission::BanUnbanMember;
    }
    if (event.command == Permission::Count)
        return;
    CirclePolicy authority;
    authority.admin = before.policy.admin;
    authority.defaults = before.policy.defaults;
    authority.members.emplace(event.actor, before.policy.members.at(event.actor));
    authority.members.emplace(authority.admin, before.policy.members.at(authority.admin));
    for (const auto& role : before.policy.members.at(event.actor).roles) {
        authority.roles.emplace(role, before.policy.roles.at(role));
        const auto grant = before.policy.delegations.find(role);
        if (grant != before.policy.delegations.end())
            authority.delegations.emplace(*grant);
    }
    if (event.channel)
        authority.channels.emplace(*event.channel, before.policy.channels.at(*event.channel));
    event.authority = std::move(authority);
    if (event.command != Permission::ManageRoles && event.command != Permission::AssignRoles)
        return;
    RoleDelegation scope;
    scope.circleWide = !event.channel;
    if (event.channel)
        scope.channels.insert(*event.channel);
    if (const auto assignment = std::get_if<SetCircleRoleAssignment>(change)) {
        scope.assignableRoles.insert(assignment->role);
    } else {
        const auto& delta = *event.rule;
        if (delta.role)
            scope.editableRoles.insert(*delta.role);
        for (size_t index = 0; index < delta.allow.size(); ++index) {
            if (delta.allow[index] >= 0 || delta.deny[index] >= 0)
                scope.permissions.insert(static_cast<Permission>(index));
        }
        auto previousPolicy = before.policy;
        auto nextPolicy = after.policy;
        for (auto& [member, state] : previousPolicy.members)
            state.state = CircleMemberState::ACTIVE;
        for (auto& [member, state] : nextPolicy.members)
            state.state = CircleMemberState::ACTIVE;
        for (const auto& [member, state] : before.policy.members) {
            if (member == before.policy.admin || (delta.role && !state.roles.contains(*delta.role)))
                continue;
            auto gains = [&](std::optional<std::string_view> channel) {
                const auto permissions = previousPolicy.permissions(member, channel);
                for (auto permission : nextPolicy.permissions(member, channel)) {
                    if (!permissions.contains(permission))
                        scope.permissions.insert(permission);
                }
            };
            if (event.channel)
                gains(*event.channel);
            else {
                gains(std::nullopt);
                for (const auto& [channel, policy] : before.policy.channels)
                    gains(channel);
            }
        }
    }
    event.scope = std::move(scope);
}

bool
authoritySurvives(const HistoryEvent& event, const std::vector<const HistoryEvent*>& reductions)
{
    if (!event.authority)
        return true;
    auto policy = *event.authority;
    for (const auto reduction : reductions) {
        if (reduction->actor != policy.admin && reduction->actor != event.actor)
            continue;
        if (reduction->member == event.actor && reduction->lifecycle) {
            policy.members.at(event.actor).state = CircleMemberState::LEFT;
            continue;
        }
        const auto change = std::get_if<CirclePolicyChange>(&reduction->record.payload);
        if (!change)
            continue;
        if (const auto assignment = std::get_if<SetCircleRoleAssignment>(change)) {
            if (assignment->member != event.actor)
                continue;
            auto& roles = policy.members.at(event.actor).roles;
            if (!assignment->assigned)
                roles.erase(assignment->role);
            else if (reduction->assignedRule && !roles.contains(assignment->role)) {
                roles.insert(assignment->role);
                policy.roles[assignment->role] = {{}, reduction->assignedRule->deny};
            }
        } else if (const auto delegation = std::get_if<SetCircleRoleDelegation>(change)) {
            auto found = policy.delegations.find(delegation->role);
            if (found == policy.delegations.end())
                continue;
            if (!delegation->delegation) {
                policy.delegations.erase(found);
            } else {
                auto& grant = found->second;
                const auto& retained = *delegation->delegation;
                if (reduction->delegationBefore) {
                    auto remove = [&](auto pointer) {
                        for (const auto& value : *reduction->delegationBefore.*pointer) {
                            if (!(retained.*pointer).contains(value))
                                (grant.*pointer).erase(value);
                        }
                    };
                    remove(&RoleDelegation::permissions);
                    remove(&RoleDelegation::editableRoles);
                    remove(&RoleDelegation::assignableRoles);
                    remove(&RoleDelegation::channels);
                    if (reduction->delegationBefore->circleWide && !retained.circleWide)
                        grant.circleWide = false;
                }
            }
        } else if (reduction->rule) {
            const auto& delta = *reduction->rule;
            if (delta.role && !policy.roles.contains(*delta.role))
                continue;
            PermissionRule* rule = nullptr;
            if (delta.channel) {
                if (!event.channel || *event.channel != *delta.channel)
                    continue;
                auto& channel = policy.channels.at(*delta.channel);
                if (delta.role) {
                    if (!channel.roles.contains(*delta.role))
                        channel.roles[*delta.role] = policy.roles.at(*delta.role);
                    rule = &channel.roles.at(*delta.role);
                } else {
                    if (!channel.defaults)
                        channel.defaults = policy.defaults;
                    rule = &*channel.defaults;
                }
            } else {
                rule = delta.role ? &policy.roles.at(*delta.role) : &policy.defaults;
            }
            for (size_t index = 0; index < delta.allow.size(); ++index) {
                if (delta.allow[index] == 0)
                    rule->allow.erase(static_cast<Permission>(index));
                if (delta.deny[index] == 1)
                    rule->deny.insert(static_cast<Permission>(index));
            }
        }
    }
    if (!policy.hasPermission(event.actor,
                              event.command,
                              event.channel ? std::optional<std::string_view>(*event.channel) : std::nullopt))
        return false;
    if (!event.scope)
        return true;
    const auto& required = *event.scope;
    auto includes = [](const auto& available, const auto& needed) {
        return std::includes(available.begin(), available.end(), needed.begin(), needed.end());
    };
    for (const auto& role : policy.members.at(event.actor).roles) {
        const auto found = policy.delegations.find(role);
        if (found == policy.delegations.end())
            continue;
        const auto& grant = found->second;
        if ((!required.circleWide || grant.circleWide) && includes(grant.permissions, required.permissions)
            && includes(grant.editableRoles, required.editableRoles)
            && includes(grant.assignableRoles, required.assignableRoles) && includes(grant.channels, required.channels))
            return true;
    }
    return false;
}

struct TreeEdit
{
    std::string directory;
    std::string name;
    std::optional<std::string> contents;
    bool replace {false};
};

std::optional<git_oid> writeChanges(git_repository* repo, const git_tree* before, const std::vector<TreeEdit>& edits);

std::optional<std::vector<TreeEdit>>
treeEdits(const VerifiedState& state, const CirclePolicyChange& change, const Signer& signer)
{
    std::vector<TreeEdit> edits;
    const bool enrolled = state.devices.contains(signer.deviceId);
    const bool permitted = std::visit(
        [&](const auto& operation) {
            using Operation = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<Operation, JoinCircleMember>
                          || std::is_same_v<Operation, RegisterCircleDevice>) {
                if (enrolled || operation.member != signer.memberId)
                    return false;
                if constexpr (std::is_same_v<Operation, JoinCircleMember>) {
                    edits.push_back({"invited", signer.memberId, std::nullopt});
                    edits.push_back({"members", signer.memberId + ".crt", signer.memberData});
                } else if (operation.device != signer.deviceId) {
                    return false;
                }
                edits.push_back({"devices", signer.deviceId + ".crt", signer.deviceData});
            } else {
                if (!enrolled || state.devices.at(signer.deviceId) != signer.memberId)
                    return false;
                if constexpr (std::is_same_v<Operation, InviteCircleMember>) {
                    edits.push_back({"invited", operation.member, std::string()});
                } else if constexpr (std::is_same_v<Operation, LeaveCircleMember>) {
                    edits.push_back({"members", operation.member + ".crt", std::nullopt});
                    for (const auto& [device, member] : state.devices) {
                        if (member == operation.member)
                            edits.push_back({"devices", device + ".crt", std::nullopt});
                    }
                }
            }
            return true;
        },
        change);
    if (!permitted)
        return std::nullopt;
    return edits;
}

bool
freshVote(const VerifiedState& state, const PendingVote& vote)
{
    const auto voter = state.memberRevisions.find(vote.voter);
    const auto target = state.memberRevisions.find(vote.operation.member);
    return voter != state.memberRevisions.end() && target != state.memberRevisions.end()
           && voter->second == vote.voterRevision && target->second == vote.targetRevision;
}

bool
canVote(const VerifiedState& state, std::string_view actor, const CircleModerationVote& operation)
{
    if (state.votes.size() >= CircleRepository::MAX_PENDING_VOTES || !state.policy.moderate(actor, operation))
        return false;
    for (const auto& [id, pending] : state.votes) {
        if (pending.voter == actor && pending.operation.member == operation.member
            && pending.operation.action == operation.action && freshVote(state, pending))
            return false;
    }
    return true;
}

const PendingVote*
resolvableVote(const VerifiedState& state, std::string_view actor, std::string_view voteCommit)
{
    const auto vote = state.votes.find(voteCommit);
    if (vote == state.votes.end() || !freshVote(state, vote->second)
        || !state.policy.moderate(vote->second.voter, vote->second.operation)
        || !state.policy.moderate(actor, vote->second.operation))
        return nullptr;
    return &vote->second;
}

std::vector<TreeEdit>
moderationEdits(const VerifiedState& state, const CircleModerationVote& operation)
{
    std::vector<TreeEdit> edits;
    if (operation.action == CircleModerationAction::UNBAN_MEMBER) {
        edits.push_back({"invited", operation.member, std::string()});
    } else {
        const auto member = state.policy.members.at(operation.member).state;
        if (member == CircleMemberState::INVITED)
            edits.push_back({"invited", operation.member, std::nullopt});
        else if (member == CircleMemberState::ACTIVE)
            edits.push_back({"members", operation.member + ".crt", std::nullopt});
        for (const auto& [device, owner] : state.devices) {
            if (owner == operation.member)
                edits.push_back({"devices", device + ".crt", std::nullopt});
        }
    }
    return edits;
}

std::optional<std::vector<TreeEdit>>
recordEdits(const VerifiedState& state,
            const CircleRecord& record,
            const Signer& signer,
            std::optional<std::string_view> profile = std::nullopt)
{
    if (record.circle != state.circle || record.base != state.head)
        return std::nullopt;
    if (const auto change = std::get_if<CirclePolicyChange>(&record.payload)) {
        if (!record.apply(state.policy, signer.memberId, state.circle, state.head))
            return std::nullopt;
        auto edits = treeEdits(state, *change, signer);
        if (const auto target = std::get_if<SetCircleProfile>(change)) {
            if (!edits || !profile)
                return std::nullopt;
            edits->push_back({target->channel ? "channels" : "",
                              target->channel ? *target->channel + ".vcf" : "profile.vcf",
                              std::string(*profile),
                              true});
        }
        return edits;
    }
    const auto enrolled = state.devices.find(signer.deviceId);
    if (enrolled == state.devices.end() || enrolled->second != signer.memberId)
        return std::nullopt;
    if (const auto vote = std::get_if<CircleModerationVote>(&record.payload)) {
        if (canVote(state, signer.memberId, *vote))
            return std::vector<TreeEdit> {};
    } else if (const auto resolution = std::get_if<CircleModerationResolution>(&record.payload)) {
        if (const auto vote = resolvableVote(state, signer.memberId, resolution->vote))
            return moderationEdits(state, vote->operation);
    }
    return std::nullopt;
}

template<typename Map>
bool
sameValue(const Map& before, const Map& after, const std::string& key)
{
    const auto previous = before.find(key);
    const auto next = after.find(key);
    if (previous == before.end() || next == after.end())
        return previous == before.end() && next == after.end();
    return previous->second == next->second;
}

void
updateMemberRevisions(const VerifiedState& before, VerifiedState& after, const CirclePolicyChange& change)
{
    const bool defaultsChanged = before.policy.defaults != after.policy.defaults;
    const auto channel = std::get_if<SetCircleChannelRule>(&change);
    for (const auto& [uri, member] : after.policy.members) {
        const auto previous = before.policy.members.find(uri);
        bool affected = previous == before.policy.members.end() || previous->second != member || defaultsChanged
                        || (before.policy.admin != after.policy.admin
                            && (uri == before.policy.admin || uri == after.policy.admin));
        for (const auto& role : member.roles) {
            affected = affected || !sameValue(before.policy.roles, after.policy.roles, role)
                       || !sameValue(before.policy.delegations, after.policy.delegations, role);
        }
        if (channel && (!channel->role || member.roles.contains(*channel->role))
            && before.policy.channels.at(channel->channel) != after.policy.channels.at(channel->channel))
            affected = true;
        if (affected)
            after.memberRevisions[uri] = after.head;
    }
}

void
pruneVotes(VerifiedState& state)
{
    std::erase_if(state.votes, [&](const auto& entry) { return !freshVote(state, entry.second); });
}

bool
validTreeShape(git_repository* repo, const git_tree* tree, const VerifiedState& state)
{
    size_t active = 0, invited = 0;
    for (const auto& [uri, member] : state.policy.members) {
        active += member.state == CircleMemberState::ACTIVE;
        invited += member.state == CircleMemberState::INVITED;
    }
    const std::array<std::pair<const char*, size_t>, 3> directories {
        {{"members", active}, {"devices", state.devices.size()}, {"invited", invited}}};
    size_t count = 0;
    for (const auto& [name, expected] : directories) {
        const auto entry = git_tree_entry_byname(tree, name);
        if (!expected) {
            if (entry)
                return false;
            continue;
        }
        ++count;
        if (!entry || git_tree_entry_filemode(entry) != GIT_FILEMODE_TREE)
            return false;
        const auto child = lookupTree(repo, git_tree_entry_id(entry), MAX_DIRECTORY_SIZE);
        if (!child || git_tree_entrycount(child.get()) != expected)
            return false;
        for (size_t index = 0; index < expected; ++index) {
            const auto file = git_tree_entry_byindex(child.get(), index);
            if (git_tree_entry_filemode(file) != GIT_FILEMODE_BLOB)
                return false;
            const std::string_view filename(git_tree_entry_name(file));
            if (std::string_view(name) == "devices") {
                if (filename.size() != 68 || !filename.ends_with(".crt")
                    || !state.devices.contains(filename.substr(0, 64)))
                    return false;
            } else {
                const bool invitation = std::string_view(name) == "invited";
                if (filename.size() != (invitation ? 40 : 44) || (!invitation && !filename.ends_with(".crt")))
                    return false;
                const auto member = state.policy.members.find(filename.substr(0, 40));
                if (member == state.policy.members.end()
                    || member->second.state != (invitation ? CircleMemberState::INVITED : CircleMemberState::ACTIVE))
                    return false;
            }
        }
    }
    if (const auto profile = git_tree_entry_byname(tree, "profile.vcf")) {
        if (git_tree_entry_filemode(profile) != GIT_FILEMODE_BLOB
            || !objectWithin(repo, git_tree_entry_id(profile), GIT_OBJECT_BLOB, git::UNLIMITED))
            return false;
        ++count;
    }
    if (const auto profiles = git_tree_entry_byname(tree, "channels")) {
        if (git_tree_entry_filemode(profiles) != GIT_FILEMODE_TREE)
            return false;
        const auto entries = lookupTree(repo, git_tree_entry_id(profiles), MAX_DIRECTORY_SIZE);
        if (!entries || !git_tree_entrycount(entries.get())
            || git_tree_entrycount(entries.get()) > state.policy.channels.size())
            return false;
        for (size_t index = 0; index < git_tree_entrycount(entries.get()); ++index) {
            const auto entry = git_tree_entry_byindex(entries.get(), index);
            const std::string_view name(git_tree_entry_name(entry));
            if (name.size() != 44 || !name.ends_with(".vcf") || !state.policy.channels.contains(name.substr(0, 40))
                || git_tree_entry_filemode(entry) != GIT_FILEMODE_BLOB
                || !objectWithin(repo, git_tree_entry_id(entry), GIT_OBJECT_BLOB, git::UNLIMITED))
                return false;
        }
        ++count;
    }
    return git_tree_entrycount(tree) == count;
}

bool
validTreeChanges(git_repository* repo, git_tree* before, git_tree* after, const std::vector<TreeEdit>& edits)
{
    if (edits.empty())
        return git_oid_equal(git_tree_id(before), git_tree_id(after));
    git_diff* raw = nullptr;
    git_diff_options options {};
    if (git_diff_options_init(&options, GIT_DIFF_OPTIONS_VERSION) < 0
        || git_diff_tree_to_tree(&raw, repo, before, after, &options) < 0)
        return false;
    GitDiff diff(raw);
    if (git_diff_num_deltas(diff.get()) != edits.size())
        return false;
    std::map<std::string, const TreeEdit*, std::less<>> expected;
    for (const auto& edit : edits)
        expected.emplace(edit.directory.empty() ? edit.name : edit.directory + "/" + edit.name, &edit);
    for (size_t index = 0; index < git_diff_num_deltas(diff.get()); ++index) {
        const auto delta = git_diff_get_delta(diff.get(), index);
        const bool added = delta->status == GIT_DELTA_ADDED;
        const bool modified = delta->status == GIT_DELTA_MODIFIED;
        if ((!added && !modified && delta->status != GIT_DELTA_DELETED)
            || (added || modified ? delta->new_file.mode : delta->old_file.mode) != GIT_FILEMODE_BLOB
            || (modified && delta->old_file.mode != GIT_FILEMODE_BLOB))
            return false;
        const auto path = added || modified ? delta->new_file.path : delta->old_file.path;
        const auto found = path ? expected.find(path) : expected.end();
        if (found == expected.end() || found->second->contents.has_value() != (added || modified)
            || (modified && !found->second->replace))
            return false;
        if (added || modified) {
            git_oid blob {};
            const auto& contents = *found->second->contents;
            if (git_odb_hash(&blob, contents.data(), contents.size(), GIT_OBJECT_BLOB) < 0
                || !git_oid_equal(&blob, &delta->new_file.id))
                return false;
        }
        expected.erase(found);
    }
    return expected.empty();
}

std::optional<VerifiedState> validateHistory(git_repository* repo,
                                             std::string_view expectedCircle,
                                             const git_oid& head,
                                             size_t limit);

std::optional<git_oid>
emptyTree(git_repository* repo)
{
    git_treebuilder* raw = nullptr;
    if (git_treebuilder_new(&raw, repo, nullptr) < 0)
        return std::nullopt;
    std::unique_ptr<git_treebuilder, decltype(&git_treebuilder_free)> builder(raw, git_treebuilder_free);
    git_oid result {};
    if (git_treebuilder_write(&result, builder.get()) < 0)
        return std::nullopt;
    return result;
}

std::optional<std::string>
rawChannelGenesis(git_repository* repo, const git_oid& oid)
{
    if (!objectWithin(repo, &oid, GIT_OBJECT_COMMIT, CircleRecord::MAX_CHANNEL_PROOF_SIZE))
        return std::nullopt;
    git_odb* raw = nullptr;
    if (git_repository_odb(&raw, repo) < 0)
        return std::nullopt;
    GitOdb odb(raw);
    git_odb_object* objectRaw = nullptr;
    if (git_odb_read(&objectRaw, odb.get(), &oid) < 0)
        return std::nullopt;
    std::unique_ptr<git_odb_object, decltype(&git_odb_object_free)> object(objectRaw, git_odb_object_free);
    return std::string(static_cast<const char*>(git_odb_object_data(object.get())), git_odb_object_size(object.get()));
}

bool
verifyChannelGenesis(git_repository* repo,
                     std::string_view proof,
                     std::string_view channel,
                     const VerifiedState& authorization,
                     const ChannelPolicy* access = nullptr)
{
    if (proof.empty() || proof.size() > CircleRecord::MAX_CHANNEL_PROOF_SIZE || !isHexId(channel, 40))
        return false;
    git_oid oid {};
    if (git_odb_hash(&oid, proof.data(), proof.size(), GIT_OBJECT_COMMIT) < 0 || oidString(&oid) != channel)
        return false;
    git_odb* raw = nullptr;
    if (git_repository_odb(&raw, repo) < 0)
        return false;
    GitOdb odb(raw);
    if (git_odb_write(&oid, odb.get(), proof.data(), proof.size(), GIT_OBJECT_COMMIT) < 0)
        return false;
    const auto commit = lookupCommit(repo, &oid);
    const auto empty = emptyTree(repo);
    if (!commit || !empty || git_commit_parentcount(commit.get())
        || !git_oid_equal(git_commit_tree_id(commit.get()), &*empty))
        return false;
    const auto author = git_commit_author(commit.get());
    if (!author || !author->email)
        return false;
    const auto enrolled = authorization.devices.find(author->email);
    const auto tree = lookupTree(repo, &authorization.tree);
    const auto signer = enrolled != authorization.devices.end() && tree
                            ? readSigner(repo, tree.get(), author->email, enrolled->second)
                            : std::nullopt;
    const auto record = signer ? verifyCommit(repo, commit.get(), *signer) : std::nullopt;
    const auto genesis = record ? std::get_if<CircleChannelGenesis>(&record->payload) : nullptr;
    return genesis && (!access || genesis->access == *access) && genesis->creator == signer->memberId
           && record->circle == authorization.circle && record->base == authorization.head
           && authorization.policy
                  .apply(signer->memberId,
                         RegisterCircleChannel {std::string(channel),
                                                authorization.head,
                                                genesis->access,
                                                std::string(proof)})
                  .has_value();
}

std::optional<VerifiedState>
validateUpdate(git_repository* repo,
               git_commit* commit,
               const VerifiedState& before,
               std::optional<RuleDelta>* delta = nullptr,
               std::optional<HistoryEvent>* event = nullptr,
               const VerifiedState* authorization = nullptr)
{
    if (git_commit_parentcount(commit) != 1 || oidString(git_commit_parent_id(commit, 0)) != before.head)
        return std::nullopt;
    const auto source = lookupTree(repo, &before.tree);
    const auto tree = lookupTree(repo, git_commit_tree_id(commit));
    const auto author = git_commit_author(commit);
    if (!source || !tree || !author || !author->email)
        return std::nullopt;
    const auto registered = before.devices.find(author->email);
    const auto signer = registered == before.devices.end()
                            ? readSigner(repo, tree.get(), author->email)
                            : readSigner(repo, source.get(), author->email, registered->second);
    if (!signer)
        return std::nullopt;
    const auto record = verifyCommit(repo, commit, *signer);
    if (!record)
        return std::nullopt;
    if (const auto change = std::get_if<CirclePolicyChange>(&record->payload)) {
        if (const auto registration = std::get_if<RegisterCircleChannel>(change)) {
            if (!authorization || authorization->head != registration->authorization)
                return std::nullopt;
            git_oid authorized {}, parent {};
            if (git_oid_fromstr(&authorized, authorization->head.c_str()) < 0
                || git_oid_fromstr(&parent, before.head.c_str()) < 0
                || (!git_oid_equal(&authorized, &parent) && git_graph_descendant_of(repo, &parent, &authorized) != 1)
                || !verifyChannelGenesis(repo,
                                         registration->genesis,
                                         registration->channel,
                                         *authorization,
                                         &registration->policy))
                return std::nullopt;
        }
    }
    const auto change = std::get_if<CirclePolicyChange>(&record->payload);
    const auto targetProfile = change ? std::get_if<SetCircleProfile>(change) : nullptr;
    const auto profile = targetProfile ? readProfile(repo, tree.get(), targetProfile->channel) : std::nullopt;
    const auto edits = recordEdits(before,
                                   *record,
                                   *signer,
                                   profile ? std::optional<std::string_view>(*profile) : std::nullopt);
    if (!edits)
        return std::nullopt;
    auto next = before;
    next.head = oidString(git_commit_id(commit));
    next.tree = *git_tree_id(tree.get());
    if (const auto change = std::get_if<CirclePolicyChange>(&record->payload)) {
        auto policy = record->apply(before.policy, signer->memberId, before.circle, before.head);
        if (!policy)
            return std::nullopt;
        next.policy = std::move(*policy);
        if (std::holds_alternative<JoinCircleMember>(*change) || std::holds_alternative<RegisterCircleDevice>(*change))
            next.devices.emplace(signer->deviceId, signer->memberId);
        else if (const auto leave = std::get_if<LeaveCircleMember>(change))
            std::erase_if(next.devices, [&](const auto& entry) { return entry.second == leave->member; });
        updateMemberRevisions(before, next, *change);
        if (const auto assignment = std::get_if<SetCircleRoleAssignment>(change))
            next.memberRevisions[assignment->member] = next.head;
        if (delta)
            *delta = ruleDelta(before, next, *change, signer->memberId);
    } else if (const auto vote = std::get_if<CircleModerationVote>(&record->payload)) {
        next.votes.emplace(next.head,
                           PendingVote {*vote,
                                        signer->memberId,
                                        before.memberRevisions.at(signer->memberId),
                                        before.memberRevisions.at(vote->member)});
    } else if (const auto resolution = std::get_if<CircleModerationResolution>(&record->payload)) {
        const auto& operation = before.votes.at(resolution->vote).operation;
        auto policy = before.policy.moderate(signer->memberId, operation);
        if (!policy)
            return std::nullopt;
        next.policy = std::move(*policy);
        if (operation.action == CircleModerationAction::BAN_MEMBER)
            std::erase_if(next.devices, [&](const auto& entry) { return entry.second == operation.member; });
        next.memberRevisions[operation.member] = resolution->vote;
        next.votes.erase(resolution->vote);
    } else {
        return std::nullopt;
    }
    pruneVotes(next);
    if (!validTreeShape(repo, tree.get(), next) || !validTreeChanges(repo, source.get(), tree.get(), *edits))
        return std::nullopt;
    HistoryEvent operation {*record, signer->memberId};
    operation.byAdmin = signer->memberId == before.policy.admin;
    operation.actorEpoch = before.memberEpochs.contains(signer->memberId) ? before.memberEpochs.at(signer->memberId)
                                                                          : std::string();
    operation.actorTransition = before.memberTransitions.contains(signer->memberId)
                                    ? before.memberTransitions.at(signer->memberId)
                                    : std::string();
    if (const auto change = std::get_if<CirclePolicyChange>(&record->payload)) {
        operation.rule = ruleDelta(before, next, *change, signer->memberId);
        if (const auto channel = std::get_if<SetCircleChannelRule>(change))
            operation.requiredChannel = channel->channel;
        else if (const auto archive = std::get_if<ArchiveCircleChannel>(change))
            operation.requiredChannel = archive->channel;
        else if (const auto profile = std::get_if<SetCircleProfile>(change))
            operation.requiredChannel = profile->channel;
        std::visit(
            [&](const auto& value) {
                using Operation = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Operation, InviteCircleMember>
                              || std::is_same_v<Operation, JoinCircleMember>
                              || std::is_same_v<Operation, LeaveCircleMember>
                              || std::is_same_v<Operation, RegisterCircleDevice>
                              || std::is_same_v<Operation, SetCircleRoleAssignment>) {
                    operation.member = value.member;
                    operation.previousTransition = before.memberTransitions.contains(value.member)
                                                       ? before.memberTransitions.at(value.member)
                                                       : std::string();
                    const auto previous = before.memberEpochs.find(value.member);
                    operation.previousEpoch = previous == before.memberEpochs.end() ? std::string() : previous->second;
                    operation.epoch = operation.previousEpoch;
                    if constexpr (std::is_same_v<Operation, InviteCircleMember>
                                  || std::is_same_v<Operation, LeaveCircleMember>)
                        operation.epoch = next.head;
                    next.memberEpochs[value.member] = operation.epoch;
                    if constexpr (std::is_same_v<Operation, InviteCircleMember>
                                  || std::is_same_v<Operation, JoinCircleMember>
                                  || std::is_same_v<Operation, LeaveCircleMember>) {
                        operation.lifecycle = next.policy.members.at(value.member).state;
                        next.memberTransitions[value.member] = next.head;
                    }
                    if constexpr (std::is_same_v<Operation, SetCircleRoleAssignment>) {
                        operation.assignedRule = next.policy.roles.at(value.role);
                        operation.targetRoles = before.policy.members.at(value.member).roles;
                    }
                } else if constexpr (std::is_same_v<Operation, SetCircleRoleDelegation>) {
                    const auto found = before.policy.delegations.find(value.role);
                    if (found != before.policy.delegations.end())
                        operation.delegationBefore = found->second;
                }
            },
            *change);
    } else if (const auto resolution = std::get_if<CircleModerationResolution>(&record->payload)) {
        operation.proof = before.votes.at(resolution->vote);
        operation.member = operation.proof->operation.member;
        operation.previousEpoch = before.memberEpochs.at(operation.member);
        operation.previousTransition = before.memberTransitions.at(operation.member);
        operation.epoch = resolution->vote;
        next.memberEpochs[operation.member] = operation.epoch;
        operation.lifecycle = next.policy.members.at(operation.member).state;
        next.memberTransitions[operation.member] = next.head;
        operation.targetRoles = before.policy.members.at(operation.member).roles;
        operation.affected.push_back(operation.member);
        HistoryEvent voter {CircleRecord {operation.proof->operation, before.circle, before.head},
                            operation.proof->voter};
        captureAuthority(voter, before, before);
        operation.voterAuthority = std::move(voter.authority);
    } else if (std::holds_alternative<CircleModerationVote>(record->payload)) {
        operation.proof = next.votes.at(next.head);
    }
    captureAuthority(operation, before, next);
    for (const auto& [member, revision] : next.memberRevisions) {
        if (revision == next.head)
            operation.affected.push_back(member);
    }
    if (event)
        *event = std::move(operation);
    return next;
}

std::optional<VerifiedState>
validateRoot(git_repository* repo, const GitCommit& root, std::string_view expectedCircle)
{
    if (!root || git_commit_parentcount(root.get()) || oidString(git_commit_id(root.get())) != expectedCircle)
        return std::nullopt;
    git_tree* treeRaw = nullptr;
    if (!root || !objectWithin(repo, git_commit_tree_id(root.get()), GIT_OBJECT_TREE, 1024)
        || git_commit_tree(&treeRaw, root.get()) < 0)
        return std::nullopt;
    GitTree tree(treeRaw);
    const auto author = git_commit_author(root.get());
    if (!author || !author->email)
        return std::nullopt;
    const auto members = git_tree_entry_byname(tree.get(), "members");
    if (!members)
        return std::nullopt;
    git_tree* membersRaw = nullptr;
    if (!objectWithin(repo, git_tree_entry_id(members), GIT_OBJECT_TREE, 1024)
        || git_tree_lookup(&membersRaw, repo, git_tree_entry_id(members)) < 0)
        return std::nullopt;
    GitTree memberTree(membersRaw);
    const auto memberEntry = git_tree_entry_byindex(memberTree.get(), 0);
    if (!memberEntry || git_tree_entrycount(memberTree.get()) != 1)
        return std::nullopt;
    const std::string memberFile = git_tree_entry_name(memberEntry);
    if (memberFile.size() != 44 || !memberFile.ends_with(".crt"))
        return std::nullopt;
    auto signer = readSigner(repo, tree.get(), author->email, memberFile.substr(0, 40));
    if (!signer)
        return std::nullopt;
    const auto record = verifyCommit(repo, root.get(), *signer);
    auto policy = record ? record->initialPolicy(signer->memberId) : std::nullopt;
    if (!policy)
        return std::nullopt;
    VerifiedState state {std::string(expectedCircle),
                         std::string(expectedCircle),
                         std::move(*policy),
                         *git_tree_id(tree.get()),
                         {{signer->deviceId, signer->memberId}},
                         {{signer->memberId, std::string(expectedCircle)}},
                         {},
                         nullptr,
                         {{signer->memberId, std::string(expectedCircle)}},
                         {{signer->memberId, std::string(expectedCircle)}}};
    if (git_tree_entrycount(tree.get()) != 2 || !validTreeShape(repo, tree.get(), state))
        return std::nullopt;
    return state;
}

class HistoryValidator
{
public:
    HistoryValidator(git_repository* repo, std::string_view circle, size_t limit)
        : repo_(repo)
        , circle_(circle)
        , limit_(limit)
    {}

    bool run(std::initializer_list<git_oid> heads, const std::function<bool(const VerifiedState&)>& inspect = {})
    {
        if (!limit_ || !isHexId(circle_, 40)) {
            exhausted_ = !limit_;
            return false;
        }
        std::vector<git_oid> pending(heads);
        std::set<std::string> wanted;
        for (const auto& head : heads)
            wanted.insert(oidString(&head));
        while (!pending.empty()) {
            const auto oid = pending.back();
            pending.pop_back();
            const auto id = oidString(&oid);
            if (nodes_.contains(id))
                continue;
            if (nodes_.size() == limit_) {
                exhausted_ = true;
                return false;
            }
            const auto commit = lookupCommit(repo_, &oid);
            if (!commit || git_commit_parentcount(commit.get()) > 2)
                return false;
            Node node;
            node.oid = oid;
            if (const auto message = git_commit_message_raw(commit.get())) {
                const auto record = CircleRecord::fromString(message);
                const auto change = record ? std::get_if<CirclePolicyChange>(&record->payload) : nullptr;
                if (const auto registration = change ? std::get_if<RegisterCircleChannel>(change) : nullptr)
                    node.authorization = registration->authorization;
            }
            for (unsigned index = 0; index < git_commit_parentcount(commit.get()); ++index) {
                const auto parent = *git_commit_parent_id(commit.get(), index);
                node.parents.push_back(oidString(&parent));
                pending.push_back(parent);
            }
            if ((node.parents.empty() && id != circle_)
                || (node.parents.size() == 2 && node.parents[0] == node.parents[1]))
                return false;
            nodes_.emplace(id, std::move(node));
        }
        if (!nodes_.contains(circle_) || !nodes_.at(circle_).parents.empty())
            return false;
        std::set<std::string> ready {circle_};
        for (auto& [id, node] : nodes_) {
            node.waiting = node.parents.size();
            for (const auto& parent : node.parents)
                nodes_.at(parent).children.push_back(id);
        }
        for (auto& [id, node] : nodes_)
            node.users = node.children.size();
        for (const auto& [id, node] : nodes_) {
            if (!node.authorization.empty()) {
                const auto found = nodes_.find(node.authorization);
                if (found == nodes_.end() || node.parents.size() != 1)
                    return false;
                ++found->second.users;
            }
        }
        size_t processed = 0;
        while (!ready.empty()) {
            const auto id = *ready.begin();
            ready.erase(ready.begin());
            auto& node = nodes_.at(id);
            const auto commit = lookupCommit(repo_, &node.oid);
            if (!commit)
                return false;
            std::optional<VerifiedState> state;
            if (node.parents.empty())
                state = validateRoot(repo_, commit, circle_);
            else if (node.parents.size() == 1)
                state = validateUpdate(repo_,
                                       commit.get(),
                                       states_.at(node.parents[0]),
                                       &node.delta,
                                       &node.event,
                                       node.authorization.empty() ? nullptr : &states_.at(node.authorization));
            else
                state = validateMerge(commit.get(), states_.at(node.parents[0]), states_.at(node.parents[1]));
            if (!state)
                return false;
            if (inspect && !inspect(*state))
                return false;
            if (node.children.size() > 1) {
                if (++checkpoints_ > CircleRepository::MAX_VALIDATION_FRONTIER) {
                    exhausted_ = true;
                    return false;
                }
                node.checkpoint = std::make_shared<VerifiedState>(*state);
                state->historyBase = node.checkpoint;
            }
            node.order = processed++;
            for (const auto& parent : node.parents) {
                if (!--nodes_.at(parent).users && !wanted.contains(parent))
                    states_.erase(parent);
            }
            if (!node.authorization.empty() && !--nodes_.at(node.authorization).users
                && !wanted.contains(node.authorization))
                states_.erase(node.authorization);
            if (states_.size() == CircleRepository::MAX_VALIDATION_FRONTIER) {
                exhausted_ = true;
                return false;
            }
            states_.emplace(id, std::move(*state));
            for (const auto& child : node.children) {
                if (!--nodes_.at(child).waiting)
                    ready.insert(child);
            }
        }
        return processed == nodes_.size();
    }

    const VerifiedState* state(std::string_view id) const
    {
        const auto found = states_.find(id);
        return found == states_.end() ? nullptr : &found->second;
    }

    bool exhausted() const { return exhausted_; }

    std::optional<VerifiedState> reconcile(const VerifiedState& left, const VerifiedState& right) const
    {
        std::set<std::string> excluded;
        for (size_t attempt = 0; attempt < CircleRepository::MAX_VALIDATION_FRONTIER; ++attempt) {
            bool retry = false;
            auto result = reconcilePass(left, right, excluded, retry);
            if (!retry)
                return result;
        }
        exhausted_ = true;
        return std::nullopt;
    }

    std::optional<VerifiedState> reconcilePass(const VerifiedState& left,
                                               const VerifiedState& right,
                                               std::set<std::string>& excluded,
                                               bool& retry) const
    {
        auto checkpoint = left.historyBase;
        std::set<std::string> checkpoints;
        for (auto current = right.historyBase; current; current = current->historyBase)
            checkpoints.insert(current->head);
        while (checkpoint && !checkpoints.contains(checkpoint->head))
            checkpoint = checkpoint->historyBase;
        if (!checkpoint)
            return std::nullopt;
        const auto& base = *checkpoint;
        std::set<std::string> window;
        std::vector<std::string> pending {left.head, right.head};
        while (!pending.empty()) {
            const auto id = std::move(pending.back());
            pending.pop_back();
            if (id == base.head || window.contains(id))
                continue;
            if (window.size() == CircleRepository::MAX_MERGE_COMMITS) {
                exhausted_ = true;
                return std::nullopt;
            }
            const auto found = nodes_.find(id);
            if (found == nodes_.end() || (!found->second.event && found->second.parents.size() != 2))
                return std::nullopt;
            window.insert(id);
            pending.insert(pending.end(), found->second.parents.begin(), found->second.parents.end());
        }
        std::vector<std::string> ordered(window.begin(), window.end());
        std::sort(ordered.begin(), ordered.end(), [&](const auto& first, const auto& second) {
            return nodes_.at(first).order < nodes_.at(second).order;
        });
        std::map<std::string, size_t, std::less<>> indices;
        for (size_t index = 0; index < ordered.size(); ++index)
            indices.emplace(ordered[index], index);
        const auto words = (ordered.size() + 63) / 64;
        std::vector<std::vector<uint64_t>> ancestors(ordered.size(), std::vector<uint64_t>(words));
        for (size_t index = 0; index < ordered.size(); ++index) {
            for (const auto& parent : nodes_.at(ordered[index]).parents) {
                if (parent == base.head)
                    continue;
                const auto previous = indices.find(parent);
                if (previous == indices.end() || previous->second >= index)
                    return std::nullopt;
                for (size_t word = 0; word < words; ++word)
                    ancestors[index][word] |= ancestors[previous->second][word];
            }
            ancestors[index][index / 64] |= uint64_t(1) << (index % 64);
        }
        auto add = [&](std::vector<size_t>& frontier, size_t index) {
            if (std::any_of(frontier.begin(), frontier.end(), [&](size_t previous) {
                    return (ancestors[previous][index / 64] >> (index % 64)) & uint64_t(1);
                }))
                return true;
            std::erase_if(frontier, [&](size_t previous) {
                return (ancestors[index][previous / 64] >> (previous % 64)) & uint64_t(1);
            });
            if (frontier.size() == CircleRepository::MAX_VALIDATION_FRONTIER) {
                exhausted_ = true;
                return false;
            }
            frontier.push_back(index);
            return true;
        };
        auto ancestor = [&](size_t previous, size_t next) {
            return (ancestors[next][previous / 64] >> (previous % 64)) & uint64_t(1);
        };
        std::vector<bool> effective(ordered.size(), true);
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            effective[index] = !excluded.contains(ordered[index]);
            if (!event || !effective[index])
                continue;
            std::vector<const HistoryEvent*> reductions;
            std::vector<HistoryEvent> reversed;
            reversed.reserve(index);
            for (size_t candidate = 0; candidate < ordered.size(); ++candidate) {
                const auto& reduction = nodes_.at(ordered[candidate]).event;
                if (reduction && !ancestor(candidate, index) && !ancestor(index, candidate))
                    reductions.push_back(&*reduction);
                else if (reduction && candidate < index && ancestor(candidate, index) && !effective[candidate]) {
                    auto undo = *reduction;
                    undo.actor = event->actor;
                    if (auto change = std::get_if<CirclePolicyChange>(&undo.record.payload)) {
                        if (auto assignment = std::get_if<SetCircleRoleAssignment>(change)) {
                            if (!assignment->assigned)
                                continue;
                            assignment->assigned = false;
                        }
                    }
                    if (undo.rule) {
                        for (size_t permission = 0; permission < undo.rule->allow.size(); ++permission) {
                            undo.rule->allow[permission] = undo.rule->allow[permission] == 1 ? 0 : -1;
                            undo.rule->deny[permission] = undo.rule->deny[permission] == 0 ? 1 : -1;
                        }
                    }
                    reversed.push_back(std::move(undo));
                    reductions.push_back(&reversed.back());
                }
            }
            effective[index] = authoritySurvives(*event, reductions);
            if (effective[index] && event->voterAuthority) {
                auto voter = *event;
                voter.actor = event->proof->voter;
                voter.authority = event->voterAuthority;
                effective[index] = authoritySurvives(voter, reductions);
            }
            for (const auto& dependency : {event->actorTransition, event->previousTransition}) {
                const auto found = indices.find(dependency);
                if (found != indices.end() && found->second < index && !effective[found->second])
                    effective[index] = false;
            }
            if (const auto resolution = std::get_if<CircleModerationResolution>(&event->record.payload)) {
                const auto found = indices.find(resolution->vote);
                if (found != indices.end() && !effective[found->second])
                    effective[index] = false;
            }
        }
        // ADR-015: an end supersedes all concurrent work, a transfer its ADMIN's concurrent work.
        auto controlOf = [&](size_t index) -> const CirclePolicyChange* {
            const auto& event = nodes_.at(ordered[index]).event;
            return event && effective[index] ? std::get_if<CirclePolicyChange>(&event->record.payload) : nullptr;
        };
        std::vector<size_t> ends, transfers;
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto change = controlOf(index);
            if (change && std::holds_alternative<EndCircle>(*change))
                ends.push_back(index);
            else if (change && std::holds_alternative<TransferCircleAdmin>(*change))
                transfers.push_back(index);
        }
        auto concurrent = [&](size_t first, size_t second) {
            return first != second && !ancestor(first, second) && !ancestor(second, first);
        };
        auto exclude = [&](size_t index) {
            if (excluded.insert(ordered[index]).second)
                retry = true;
        };
        auto isOne = [](const std::vector<size_t>& list, size_t index) {
            return std::find(list.begin(), list.end(), index) != list.end();
        };
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            if (!isOne(ends, index)
                && std::any_of(ends.begin(), ends.end(), [&](size_t end) { return concurrent(index, end); })) {
                exclude(index);
                continue;
            }
            for (const auto transfer : transfers) {
                if (!concurrent(index, transfer) || isOne(ends, index))
                    continue;
                if (isOne(transfers, index) ? ordered[transfer] < ordered[index]
                                            : event->actor == nodes_.at(ordered[transfer]).event->actor)
                    exclude(index);
            }
            // ADMIN authority gained in this window must come from a surviving transfer.
            if (event->byAdmin && event->actor != base.policy.admin
                && std::none_of(transfers.begin(), transfers.end(), [&](size_t transfer) {
                       return ancestor(transfer, index)
                              && std::get<TransferCircleAdmin>(*controlOf(transfer)).member == event->actor;
                   }))
                exclude(index);
        }
        if (retry)
            return std::nullopt;
        bool withinBudget = true;
        auto resolve = [&](auto&& write, bool original, bool restrictive) {
            std::vector<size_t> frontier;
            std::map<size_t, bool> values;
            for (size_t index = 0; index < ordered.size(); ++index) {
                if (!effective[index] || !nodes_.at(ordered[index]).event)
                    continue;
                const auto value = write(*nodes_.at(ordered[index]).event);
                if (value) {
                    values[index] = *value;
                    withinBudget = add(frontier, index) && withinBudget;
                }
            }
            if (frontier.empty())
                return original;
            return std::any_of(frontier.begin(),
                               frontier.end(),
                               [&](size_t index) { return values.at(index) == restrictive; })
                       ? restrictive
                       : !restrictive;
        };
        struct Registers
        {
            std::array<std::vector<size_t>, static_cast<size_t>(Permission::Count)> allow, deny;
        };
        std::map<std::pair<std::string, std::string>, Registers> rules;
        std::map<std::string, std::vector<size_t>> revisions;
        auto result = base;
        result.head = left.head;
        result.historyBase = checkpoint;
        // Concurrent transfers were excluded above, so the last one in topological order wins.
        for (const auto transfer : transfers)
            result.policy.admin = std::get<TransferCircleAdmin>(*controlOf(transfer)).member;
        result.policy.ended = result.policy.ended || !ends.empty();
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            const auto registration = change ? std::get_if<RegisterCircleChannel>(change) : nullptr;
            if (registration) {
                const auto [found, inserted] = result.policy.channels.emplace(registration->channel,
                                                                              registration->policy);
                if (inserted)
                    found->second.authorization = registration->authorization;
                else if (found->second.authorization != registration->authorization)
                    return std::nullopt;
            }
        }
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            if (event->requiredChannel && !result.policy.channels.contains(*event->requiredChannel)) {
                excluded.insert(ordered[index]);
                retry = true;
                continue;
            }
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            if (const auto archive = change ? std::get_if<ArchiveCircleChannel>(change) : nullptr)
                result.policy.channels.at(archive->channel).archived = true;
            if (const auto delegation = change ? std::get_if<SetCircleRoleDelegation>(change) : nullptr) {
                if (delegation->delegation
                    && std::any_of(delegation->delegation->channels.begin(),
                                   delegation->delegation->channels.end(),
                                   [&](const auto& channel) { return !result.policy.channels.contains(channel); })) {
                    excluded.insert(ordered[index]);
                    retry = true;
                }
            }
        }
        if (retry)
            return std::nullopt;
        std::map<std::string, std::vector<size_t>> membership;
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            if (event->lifecycle && !add(membership[event->member], index))
                return std::nullopt;
        }
        std::map<std::string, std::string> memberTrees;
        for (const auto& [member, frontier] : membership) {
            auto rank = [&](size_t index) {
                const auto state = *nodes_.at(ordered[index]).event->lifecycle;
                return state == CircleMemberState::BANNED   ? 3
                       : state == CircleMemberState::LEFT   ? 2
                       : state == CircleMemberState::ACTIVE ? 1
                                                            : 0;
            };
            auto winner = frontier.front();
            for (auto candidate : frontier) {
                if (rank(candidate) > rank(winner)
                    || (rank(candidate) == rank(winner) && ordered[candidate] < ordered[winner]))
                    winner = candidate;
            }
            const auto& event = *nodes_.at(ordered[winner]).event;
            const auto state = *event.lifecycle;
            auto& memberState = result.policy.members[member];
            if (!base.memberEpochs.contains(member) || base.memberEpochs.at(member) != event.epoch)
                memberState.roles.clear();
            memberState.state = state;
            result.memberEpochs[member] = event.epoch;
            result.memberTransitions[member] = ordered[winner];
            memberTrees[member] = ordered[winner];
        }
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index] || !event->authority || event->byAdmin)
                continue;
            const auto epoch = result.memberEpochs.find(event->actor);
            const auto transition = memberTrees.find(event->actor);
            if (epoch != result.memberEpochs.end() && epoch->second != event->actorEpoch
                && transition != memberTrees.end() && !ancestor(index, indices.at(transition->second))) {
                excluded.insert(ordered[index]);
                retry = true;
            }
        }
        if (retry)
            return std::nullopt;
        std::erase_if(result.devices, [&](const auto& device) {
            const auto& member = device.second;
            return result.policy.members.at(member).state != CircleMemberState::ACTIVE
                   || result.memberEpochs.at(member) != base.memberEpochs.at(member);
        });
        std::map<std::string, std::string> deviceTrees;
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index] || event->member.empty())
                continue;
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            if (!change
                || (!std::holds_alternative<JoinCircleMember>(*change)
                    && !std::holds_alternative<RegisterCircleDevice>(*change))
                || result.policy.members.at(event->member).state != CircleMemberState::ACTIVE
                || result.memberEpochs.at(event->member) != event->epoch)
                continue;
            const auto commit = lookupCommit(repo_, &nodes_.at(ordered[index]).oid);
            const auto author = commit ? git_commit_author(commit.get()) : nullptr;
            if (!author || !author->email)
                return std::nullopt;
            result.devices[author->email] = event->member;
            const auto found = deviceTrees.find(author->email);
            if (found == deviceTrees.end() || ordered[index] < found->second)
                deviceTrees[author->email] = ordered[index];
        }
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& delta = nodes_.at(ordered[index]).delta;
            if (!delta || !effective[index])
                continue;
            if (delta->role && !delta->channel)
                result.policy.roles.try_emplace(*delta->role);
            auto& registers = rules[{delta->channel.value_or(""), delta->role.value_or("")}];
            for (size_t permission = 0; permission < delta->allow.size(); ++permission) {
                if ((delta->allow[permission] >= 0 && !add(registers.allow[permission], index))
                    || (delta->deny[permission] >= 0 && !add(registers.deny[permission], index)))
                    return std::nullopt;
            }
            if (delta->changed()) {
                for (const auto& [uri, member] : base.policy.members) {
                    if ((!delta->role || member.roles.contains(*delta->role)) && !add(revisions[uri], index))
                        return std::nullopt;
                }
            }
        }
        std::map<std::pair<std::string, std::string>, bool> assignments;
        std::set<std::string> delegations;
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            if (!change)
                continue;
            if (const auto assignment = std::get_if<SetCircleRoleAssignment>(change)) {
                assignments.try_emplace({assignment->member, assignment->role});
            } else if (const auto delegation = std::get_if<SetCircleRoleDelegation>(change)) {
                delegations.insert(delegation->role);
            }
        }
        for (const auto& [key, unused] : assignments) {
            const auto& [member, role] = key;
            auto& roles = result.policy.members.at(member).roles;
            const auto assigned = resolve(
                [&](const HistoryEvent& event) -> std::optional<bool> {
                    const auto change = std::get_if<CirclePolicyChange>(&event.record.payload);
                    const auto assignment = change ? std::get_if<SetCircleRoleAssignment>(change) : nullptr;
                    if (assignment && assignment->member == member && assignment->role == role
                        && event.epoch == result.memberEpochs.at(member)
                        && result.policy.members.at(member).state == CircleMemberState::ACTIVE)
                        return assignment->assigned;
                    return std::nullopt;
                },
                roles.contains(role),
                false);
            if (assigned)
                roles.insert(role);
            else
                roles.erase(role);
        }
        for (const auto& role : delegations) {
            const auto original = base.policy.delegations.find(role);
            const auto present = resolve(
                [&](const HistoryEvent& event) -> std::optional<bool> {
                    const auto change = std::get_if<CirclePolicyChange>(&event.record.payload);
                    const auto delegation = change ? std::get_if<SetCircleRoleDelegation>(change) : nullptr;
                    return delegation && delegation->role == role
                               ? std::optional<bool>(delegation->delegation.has_value())
                               : std::nullopt;
                },
                original != base.policy.delegations.end(),
                false);
            if (!present) {
                result.policy.delegations.erase(role);
                continue;
            }
            auto& grant = result.policy.delegations[role];
            auto field = [&](auto pointer) {
                auto& values = grant.*pointer;
                auto universe = values;
                for (const auto& id : ordered) {
                    const auto& event = nodes_.at(id).event;
                    if (!event)
                        continue;
                    const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
                    const auto delegation = change ? std::get_if<SetCircleRoleDelegation>(change) : nullptr;
                    if (!delegation || delegation->role != role)
                        continue;
                    if (delegation->delegation) {
                        const auto& incoming = *delegation->delegation.*pointer;
                        universe.insert(incoming.begin(), incoming.end());
                    }
                    if (event->delegationBefore) {
                        const auto& incoming = *event->delegationBefore.*pointer;
                        universe.insert(incoming.begin(), incoming.end());
                    }
                }
                for (const auto& value : universe) {
                    const auto retained = resolve(
                        [&](const HistoryEvent& event) -> std::optional<bool> {
                            const auto change = std::get_if<CirclePolicyChange>(&event.record.payload);
                            const auto delegation = change ? std::get_if<SetCircleRoleDelegation>(change) : nullptr;
                            if (!delegation || delegation->role != role)
                                return std::nullopt;
                            if (!delegation->delegation)
                                return false;
                            const bool previous = event.delegationBefore
                                                  && (*event.delegationBefore.*pointer).contains(value);
                            const bool next = (*delegation->delegation.*pointer).contains(value);
                            return previous != next ? std::optional<bool>(next) : std::nullopt;
                        },
                        values.contains(value),
                        false);
                    if (retained)
                        values.insert(value);
                    else
                        values.erase(value);
                }
            };
            field(&RoleDelegation::permissions);
            field(&RoleDelegation::editableRoles);
            field(&RoleDelegation::assignableRoles);
            field(&RoleDelegation::channels);
            grant.circleWide = resolve(
                [&](const HistoryEvent& event) -> std::optional<bool> {
                    const auto change = std::get_if<CirclePolicyChange>(&event.record.payload);
                    const auto delegation = change ? std::get_if<SetCircleRoleDelegation>(change) : nullptr;
                    if (!delegation || delegation->role != role)
                        return std::nullopt;
                    if (!delegation->delegation)
                        return false;
                    const bool previous = event.delegationBefore && event.delegationBefore->circleWide;
                    return previous != delegation->delegation->circleWide
                               ? std::optional<bool>(delegation->delegation->circleWide)
                               : std::nullopt;
                },
                grant.circleWide,
                false);
        }
        if (!withinBudget)
            return std::nullopt;
        auto revises = [&](const HistoryEvent& event) {
            const auto change = std::get_if<CirclePolicyChange>(&event.record.payload);
            const auto assignment = change ? std::get_if<SetCircleRoleAssignment>(change) : nullptr;
            return !assignment
                   || (event.epoch == result.memberEpochs.at(assignment->member)
                       && result.policy.members.at(assignment->member).state == CircleMemberState::ACTIVE);
        };
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index] || !revises(*event) || event->member.empty())
                continue;
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            if ((event->lifecycle || (change && std::holds_alternative<SetCircleRoleAssignment>(*change)))
                && !add(revisions[event->member], index))
                return std::nullopt;
        }
        for (const auto& [key, registers] : rules) {
            const auto& [channel, role] = key;
            auto& rule = channel.empty() ? (role.empty() ? result.policy.defaults : result.policy.roles.at(role))
                                         : (role.empty() ? (result.policy.channels.at(channel).defaults
                                                                ? *result.policy.channels.at(channel).defaults
                                                                : result.policy.channels.at(channel).defaults.emplace(
                                                                      result.policy.defaults))
                                                         : result.policy.channels.at(channel)
                                                               .roles.try_emplace(role, result.policy.roles.at(role))
                                                               .first->second);
            for (size_t permission = 0; permission < registers.allow.size(); ++permission) {
                for (bool deny : {false, true}) {
                    const auto& frontier = deny ? registers.deny[permission] : registers.allow[permission];
                    if (frontier.empty())
                        continue;
                    auto winner = frontier.front();
                    for (auto candidate : frontier) {
                        const auto& first = *nodes_.at(ordered[winner]).delta;
                        const auto& second = *nodes_.at(ordered[candidate]).delta;
                        const auto firstValue = deny ? first.deny[permission] : first.allow[permission];
                        const auto secondValue = deny ? second.deny[permission] : second.allow[permission];
                        if ((firstValue != secondValue && secondValue == (deny ? 1 : 0))
                            || (firstValue == secondValue && ordered[candidate] < ordered[winner]))
                            winner = candidate;
                    }
                    const auto& delta = *nodes_.at(ordered[winner]).delta;
                    const bool value = deny ? delta.deny[permission] : delta.allow[permission];
                    auto& permissions = deny ? rule.deny : rule.allow;
                    if (value)
                        permissions.insert(static_cast<Permission>(permission));
                    else
                        permissions.erase(static_cast<Permission>(permission));
                }
            }
        }
        for (const auto& [key, registers] : rules) {
            const auto& [channel, role] = key;
            if (channel.empty())
                continue;
            const auto present = resolve(
                [&](const HistoryEvent& event) -> std::optional<bool> {
                    if (event.rule && event.rule->channel == std::optional<std::string>(channel)
                        && event.rule->role.value_or("") == role)
                        return event.rule->present == 1;
                    return std::nullopt;
                },
                true,
                true);
            if (!present) {
                if (role.empty())
                    result.policy.channels.at(channel).defaults.reset();
                else
                    result.policy.channels.at(channel).roles.erase(role);
            }
        }
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index] || !revises(*event))
                continue;
            for (const auto& member : event->affected) {
                if (!add(revisions[member], index))
                    return std::nullopt;
            }
            if (event->rule && event->rule->changed()) {
                for (const auto& [member, state] : result.policy.members) {
                    if ((!event->rule->role || state.roles.contains(*event->rule->role))
                        && !add(revisions[member], index))
                        return std::nullopt;
                }
            }
        }
        constexpr std::array governance {Permission::AddMember,
                                         Permission::BanUnbanMember,
                                         Permission::ManageChannels,
                                         Permission::ManageRoles,
                                         Permission::AssignRoles,
                                         Permission::ModerateConference};
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index] || event->byAdmin)
                continue;
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            if (change && (event->rule || std::holds_alternative<SetCircleRoleAssignment>(*change))) {
                auto previous = result.policy;
                if (event->rule) {
                    const auto& delta = *event->rule;
                    auto& rule = delta.channel
                                     ? (delta.role ? previous.channels.at(*delta.channel)
                                                         .roles.try_emplace(*delta.role, previous.roles.at(*delta.role))
                                                         .first->second
                                                   : (previous.channels.at(*delta.channel).defaults
                                                          ? *previous.channels.at(*delta.channel).defaults
                                                          : previous.channels.at(*delta.channel)
                                                                .defaults.emplace(previous.defaults)))
                                     : (delta.role ? previous.roles.at(*delta.role) : previous.defaults);
                    for (size_t permission = 0; permission < delta.allow.size(); ++permission) {
                        if (delta.allow[permission] == 0)
                            rule.allow.insert(static_cast<Permission>(permission));
                        if (delta.deny[permission] == 1)
                            rule.deny.erase(static_cast<Permission>(permission));
                    }
                } else {
                    const auto& assignment = std::get<SetCircleRoleAssignment>(*change);
                    auto& roles = previous.members.at(assignment.member).roles;
                    if (!assignment.assigned)
                        roles.insert(assignment.role);
                    else if (!event->targetRoles.contains(assignment.role))
                        roles.erase(assignment.role);
                }
                if (!previous.preservesGovernance(result.policy, event->actor)) {
                    excluded.insert(ordered[index]);
                    retry = true;
                }
                continue;
            }
            if (!event->proof || event->lifecycle != CircleMemberState::BANNED)
                continue;
            auto protectedPolicy = result.policy;
            auto& member = protectedPolicy.members.at(event->member);
            member.state = CircleMemberState::ACTIVE;
            member.roles = event->targetRoles;
            for (const auto& [key, unused] : assignments) {
                if (key.first != event->member)
                    continue;
                const bool assigned = resolve(
                    [&](const HistoryEvent& candidate) -> std::optional<bool> {
                        const auto change = std::get_if<CirclePolicyChange>(&candidate.record.payload);
                        const auto assignment = change ? std::get_if<SetCircleRoleAssignment>(change) : nullptr;
                        if (assignment && assignment->member == event->member && assignment->role == key.second
                            && candidate.epoch == event->previousEpoch)
                            return assignment->assigned;
                        return std::nullopt;
                    },
                    member.roles.contains(key.second),
                    false);
                if (assigned)
                    member.roles.insert(key.second);
                else
                    member.roles.erase(key.second);
            }
            auto governs = [&](std::optional<std::string_view> channel) {
                const auto permissions = protectedPolicy.permissions(event->member, channel);
                return std::any_of(governance.begin(), governance.end(), [&](auto permission) {
                    return permissions.contains(permission);
                });
            };
            bool protectedMember = governs(std::nullopt);
            for (const auto& [channel, policy] : protectedPolicy.channels)
                protectedMember = protectedMember || governs(channel);
            if (protectedMember) {
                excluded.insert(ordered[index]);
                retry = true;
            }
        }
        if (retry || !withinBudget)
            return std::nullopt;
        auto revisionId = [&](size_t index) {
            const auto& event = nodes_.at(ordered[index]).event;
            const auto resolution = event ? std::get_if<CircleModerationResolution>(&event->record.payload) : nullptr;
            return resolution ? resolution->vote : ordered[index];
        };
        for (const auto& [uri, frontier] : revisions) {
            if (frontier.size() == 1) {
                result.memberRevisions[uri] = revisionId(frontier.front());
                continue;
            }
            std::set<std::string> ids;
            for (auto index : frontier)
                ids.insert(revisionId(index));
            if (ids.size() == 1) {
                result.memberRevisions[uri] = *ids.begin();
                continue;
            }
            const auto previous = base.memberRevisions.find(uri);
            std::string text = "jami-circle-member-revision-v1\n" + base.circle + "\n"
                               + (previous == base.memberRevisions.end() ? std::string() : previous->second) + "\n";
            for (const auto& id : ids)
                text += id + "\n";
            std::array<uint8_t, 32> hash {};
            if (gnutls_hash_fast(GNUTLS_DIG_SHA256, text.data(), text.size(), hash.data()) < 0)
                return std::nullopt;
            result.memberRevisions[uri] = dht::PkId(hash.data(), hash.size()).toString();
        }
        std::set<std::string> consumed;
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            if (std::holds_alternative<CircleModerationVote>(event->record.payload))
                result.votes.emplace(ordered[index], *event->proof);
            else if (const auto resolution = std::get_if<CircleModerationResolution>(&event->record.payload))
                consumed.insert(resolution->vote);
        }
        std::set<std::tuple<std::string, CircleModerationAction, std::string>> principals;
        std::erase_if(result.votes, [&](const auto& entry) {
            const auto& [id, vote] = entry;
            return consumed.contains(id) || !freshVote(result, vote)
                   || !result.policy.moderate(vote.voter, vote.operation)
                   || !principals.emplace(vote.voter, vote.operation.action, vote.operation.member).second;
        });
        if (result.votes.size() > CircleRepository::MAX_PENDING_VOTES)
            return std::nullopt;
        const auto admin = result.policy.members.find(result.policy.admin);
        if (admin == result.policy.members.end() || admin->second.state != CircleMemberState::ACTIVE) {
            // A transfer to a member who did not stay active fails; then the previous ADMIN cannot leave.
            if (!transfers.empty()) {
                exclude(transfers.back());
            } else {
                for (size_t index = 0; index < ordered.size(); ++index) {
                    const auto& event = nodes_.at(ordered[index]).event;
                    if (event && effective[index] && event->lifecycle && event->member == result.policy.admin)
                        exclude(index);
                }
            }
            return std::nullopt;
        }
        if (!result.policy.isValid())
            return std::nullopt;
        std::vector<TreeEdit> edits;
        const auto source = lookupTree(repo_, &base.tree);
        if (!source)
            return std::nullopt;
        for (const auto& [member, state] : base.policy.members) {
            if (state.state == CircleMemberState::ACTIVE)
                edits.push_back({"members", member + ".crt", std::nullopt});
            else if (state.state == CircleMemberState::INVITED)
                edits.push_back({"invited", member, std::nullopt});
        }
        for (const auto& [device, member] : base.devices)
            edits.push_back({"devices", device + ".crt", std::nullopt});
        auto certificate = [&](const std::string& commitId, const std::string& directory, const std::string& name) {
            git_oid oid {};
            if (git_oid_fromstr(&oid, commitId.c_str()) < 0)
                return std::optional<std::string>();
            const auto commit = lookupCommit(repo_, &oid);
            const auto tree = commit ? lookupTree(repo_, git_commit_tree_id(commit.get())) : GitTree {};
            return tree ? readBlob(repo_, tree.get(), directory, name) : std::nullopt;
        };
        for (const auto& [member, state] : result.policy.members) {
            if (state.state == CircleMemberState::ACTIVE) {
                const auto contents = certificate(memberTrees.contains(member) ? memberTrees.at(member) : base.head,
                                                  "members",
                                                  member + ".crt");
                if (!contents)
                    return std::nullopt;
                edits.push_back({"members", member + ".crt", *contents});
            } else if (state.state == CircleMemberState::INVITED) {
                edits.push_back({"invited", member, std::string()});
            }
        }
        for (const auto& [device, member] : result.devices) {
            const auto contents = certificate(deviceTrees.contains(device) ? deviceTrees.at(device) : base.head,
                                              "devices",
                                              device + ".crt");
            if (!contents)
                return std::nullopt;
            edits.push_back({"devices", device + ".crt", *contents});
        }
        std::map<std::optional<std::string>, std::vector<size_t>> profiles;
        for (size_t index = 0; index < ordered.size(); ++index) {
            const auto& event = nodes_.at(ordered[index]).event;
            if (!event || !effective[index])
                continue;
            const auto change = std::get_if<CirclePolicyChange>(&event->record.payload);
            const auto profile = change ? std::get_if<SetCircleProfile>(change) : nullptr;
            if (profile && !add(profiles[profile->channel], index))
                return std::nullopt;
        }
        for (const auto& [channel, frontier] : profiles) {
            const auto winner = *std::min_element(frontier.begin(), frontier.end(), [&](size_t first, size_t second) {
                return ordered[first] < ordered[second];
            });
            const auto commit = lookupCommit(repo_, &nodes_.at(ordered[winner]).oid);
            const auto tree = commit ? lookupTree(repo_, git_commit_tree_id(commit.get())) : GitTree {};
            const auto contents = tree ? readProfile(repo_, tree.get(), channel) : std::nullopt;
            if (!contents)
                return std::nullopt;
            edits.push_back({channel ? "channels" : "", channel ? *channel + ".vcf" : "profile.vcf", *contents, true});
        }
        const auto tree = writeChanges(repo_, source.get(), edits);
        const auto rebuilt = tree ? lookupTree(repo_, &*tree) : GitTree {};
        if (!rebuilt || !validTreeShape(repo_, rebuilt.get(), result))
            return std::nullopt;
        for (const auto& [device, member] : result.devices) {
            if (!readSigner(repo_, rebuilt.get(), device, member))
                return std::nullopt;
        }
        result.tree = *tree;
        return result;
    }

    std::optional<VerifiedState> validateMerge(git_commit* commit,
                                               const VerifiedState& left,
                                               const VerifiedState& right) const
    {
        if (git_commit_parentcount(commit) != 2 || left.head == right.head
            || oidString(git_commit_parent_id(commit, 0)) != left.head
            || oidString(git_commit_parent_id(commit, 1)) != right.head)
            return std::nullopt;
        auto result = reconcile(left, right);
        const auto author = git_commit_author(commit);
        if (!result || !author || !author->email || !git_oid_equal(git_commit_tree_id(commit), &result->tree))
            return std::nullopt;
        const auto owner = result->devices.find(author->email);
        if (owner == result->devices.end()
            || result->policy.members.at(owner->second).state != CircleMemberState::ACTIVE)
            return std::nullopt;
        const auto tree = lookupTree(repo_, &result->tree);
        const auto signer = tree ? readSigner(repo_, tree.get(), author->email, owner->second) : std::nullopt;
        const auto record = signer ? verifyCommit(repo_, commit, *signer) : std::nullopt;
        const auto merge = record ? std::get_if<CirclePolicyMerge>(&record->payload) : nullptr;
        if (!merge || record->circle != circle_ || record->base != left.head || merge->other != right.head)
            return std::nullopt;
        result->head = oidString(git_commit_id(commit));
        return result;
    }

private:
    struct Node
    {
        git_oid oid {};
        std::vector<std::string> parents, children;
        std::string authorization;
        std::optional<RuleDelta> delta;
        std::optional<HistoryEvent> event;
        std::shared_ptr<const VerifiedState> checkpoint;
        size_t waiting {0}, users {0}, order {0};
    };

    git_repository* repo_;
    std::string circle_;
    size_t limit_;
    std::map<std::string, Node, std::less<>> nodes_;
    std::map<std::string, VerifiedState, std::less<>> states_;
    size_t checkpoints_ {0};
    mutable bool exhausted_ {false};
};

std::optional<VerifiedState>
validateHistory(git_repository* repo, std::string_view expectedCircle, const git_oid& head, size_t limit)
{
    HistoryValidator validator(repo, expectedCircle, limit);
    if (!validator.run({head}))
        return std::nullopt;
    const auto result = validator.state(oidString(&head));
    return result ? std::optional<VerifiedState>(*result) : std::nullopt;
}

class RecentStates
{
public:
    static constexpr size_t CAPACITY = 32;

    std::shared_ptr<const VerifiedState> find(std::string_view head) const
    {
        for (const auto& state : states_) {
            if (state->head == head)
                return state;
        }
        return {};
    }

    void add(const VerifiedState& state)
    {
        if (find(state.head))
            return;
        if (states_.size() == CAPACITY)
            states_.pop_front();
        auto copy = std::make_shared<VerifiedState>(state);
        copy->historyBase.reset();
        states_.push_back(std::move(copy));
    }

private:
    std::deque<std::shared_ptr<const VerifiedState>> states_;
};

struct Advance
{
    std::optional<VerifiedState> state;
    bool fallback {false};
};

// Replays only single-parent commits after a verified state; anything else needs a full replay.
Advance
advanceLinear(git_repository* repo, const VerifiedState& from, const git_oid& target, size_t limit, RecentStates& recent)
{
    git_oid head {};
    if (git_oid_fromstr(&head, from.head.c_str()) < 0)
        return {std::nullopt, true};
    if (git_oid_equal(&head, &target))
        return {from};
    if (git_graph_descendant_of(repo, &target, &head) != 1)
        return {std::nullopt, true};
    std::vector<GitCommit> chain;
    for (git_oid current = target; !git_oid_equal(&current, &head);) {
        if (chain.size() == limit)
            return {};
        auto commit = lookupCommit(repo, &current);
        if (!commit || git_commit_parentcount(commit.get()) != 1)
            return {std::nullopt, true};
        current = *git_commit_parent_id(commit.get(), 0);
        chain.push_back(std::move(commit));
    }
    std::optional<VerifiedState> state = from;
    for (auto commit = chain.rbegin(); commit != chain.rend(); ++commit) {
        const VerifiedState* authorization = nullptr;
        std::shared_ptr<const VerifiedState> known;
        const auto message = git_commit_message_raw(commit->get());
        const auto record = message ? CircleRecord::fromString(message) : std::nullopt;
        const auto change = record ? std::get_if<CirclePolicyChange>(&record->payload) : nullptr;
        if (const auto registration = change ? std::get_if<RegisterCircleChannel>(change) : nullptr) {
            known = recent.find(registration->authorization);
            authorization = registration->authorization == state->head ? &*state : known.get();
            if (!authorization)
                return {std::nullopt, true};
        }
        auto next = validateUpdate(repo, commit->get(), *state, nullptr, nullptr, authorization);
        if (!next)
            return {};
        state = std::move(next);
        if (chain.rend() - commit <= static_cast<std::ptrdiff_t>(RecentStates::CAPACITY))
            recent.add(*state);
    }
    return {std::move(state)};
}

std::optional<VerifiedState>
advance(git_repository* repo, const VerifiedState& from, const git_oid& target, size_t limit, RecentStates& recent)
{
    auto result = advanceLinear(repo, from, target, limit, recent);
    if (result.state || !result.fallback)
        return std::move(result.state);
    return validateHistory(repo, from.circle, target, limit);
}

using WireRule = std::tuple<std::vector<int>, std::vector<int>>;
using WireNames = std::set<std::string>;
using WireChannel = std::tuple<std::vector<WireRule>, std::map<std::string, WireRule>, bool, std::string>;
using WireDelegation = std::tuple<std::vector<int>, WireNames, WireNames, WireNames, bool>;
using WirePolicy = std::tuple<std::string,
                              WireRule,
                              std::map<std::string, std::tuple<int, WireNames>>,
                              std::map<std::string, WireRule>,
                              std::map<std::string, WireChannel>,
                              std::map<std::string, WireDelegation>,
                              bool>;
using WireVote = std::tuple<int, std::string, std::string, std::string, std::string>;
using WireMap = std::map<std::string, std::string>;
using WireState = std::tuple<unsigned,
                             std::string,
                             std::string,
                             std::string,
                             WirePolicy,
                             WireMap,
                             WireMap,
                             std::map<std::string, WireVote>,
                             WireMap,
                             WireMap>;

constexpr size_t MAX_STATE_SIZE = 64 * 1024 * 1024;

std::vector<int>
wire(const PermissionSet& permissions)
{
    std::vector<int> result;
    for (const auto permission : permissions)
        result.push_back(static_cast<int>(permission));
    return result;
}

WireRule
wire(const PermissionRule& rule)
{
    return {wire(rule.allow), wire(rule.deny)};
}

WireState
wire(const VerifiedState& state)
{
    WirePolicy policy;
    const auto& source = state.policy;
    std::get<0>(policy) = source.admin;
    std::get<1>(policy) = wire(source.defaults);
    for (const auto& [id, member] : source.members)
        std::get<2>(policy).emplace(id,
                                    std::tuple<int, WireNames> {static_cast<int>(member.state),
                                                                WireNames(member.roles.begin(), member.roles.end())});
    for (const auto& [id, rule] : source.roles)
        std::get<3>(policy).emplace(id, wire(rule));
    for (const auto& [id, channel] : source.channels) {
        std::vector<WireRule> defaults;
        if (channel.defaults)
            defaults.push_back(wire(*channel.defaults));
        std::map<std::string, WireRule> roles;
        for (const auto& [role, rule] : channel.roles)
            roles.emplace(role, wire(rule));
        std::get<4>(policy)
            .emplace(id, WireChannel {std::move(defaults), std::move(roles), channel.archived, channel.authorization});
    }
    for (const auto& [id, delegation] : source.delegations)
        std::get<5>(policy)
            .emplace(id,
                     WireDelegation {wire(delegation.permissions),
                                     WireNames(delegation.editableRoles.begin(), delegation.editableRoles.end()),
                                     WireNames(delegation.assignableRoles.begin(), delegation.assignableRoles.end()),
                                     WireNames(delegation.channels.begin(), delegation.channels.end()),
                                     delegation.circleWide});
    std::get<6>(policy) = source.ended;
    std::map<std::string, WireVote> votes;
    for (const auto& [id, vote] : state.votes)
        votes.emplace(id,
                      WireVote {static_cast<int>(vote.operation.action),
                                vote.operation.member,
                                vote.voter,
                                vote.voterRevision,
                                vote.targetRevision});
    return {CircleRepository::STATE_VERSION,
            state.circle,
            state.head,
            oidString(&state.tree),
            std::move(policy),
            WireMap(state.devices.begin(), state.devices.end()),
            WireMap(state.memberRevisions.begin(), state.memberRevisions.end()),
            std::move(votes),
            WireMap(state.memberEpochs.begin(), state.memberEpochs.end()),
            WireMap(state.memberTransitions.begin(), state.memberTransitions.end())};
}

std::optional<PermissionSet>
unwire(const std::vector<int>& values)
{
    PermissionSet result;
    for (const auto value : values) {
        if (value < 0 || value >= static_cast<int>(Permission::Count))
            return std::nullopt;
        result.insert(static_cast<Permission>(value));
    }
    return result;
}

std::optional<PermissionRule>
unwire(const WireRule& rule)
{
    auto allow = unwire(std::get<0>(rule));
    auto deny = unwire(std::get<1>(rule));
    if (!allow || !deny)
        return std::nullopt;
    return PermissionRule {std::move(*allow), std::move(*deny)};
}

std::optional<VerifiedState>
unwire(const WireState& value)
{
    const auto& [version, circle, head, tree, policy, devices, revisions, votes, epochs, transitions] = value;
    VerifiedState state;
    if (version != CircleRepository::STATE_VERSION || !isHexId(circle, 40) || !isHexId(head, 40)
        || git_oid_fromstr(&state.tree, tree.c_str()) < 0)
        return std::nullopt;
    state.circle = circle;
    state.head = head;
    auto& target = state.policy;
    target.admin = std::get<0>(policy);
    const auto defaults = unwire(std::get<1>(policy));
    if (!defaults)
        return std::nullopt;
    target.defaults = *defaults;
    for (const auto& [id, member] : std::get<2>(policy)) {
        const auto lifecycle = std::get<0>(member);
        if (lifecycle < 0 || lifecycle > static_cast<int>(CircleMemberState::BANNED))
            return std::nullopt;
        const auto& roles = std::get<1>(member);
        target.members.emplace(id,
                               CircleMember {static_cast<CircleMemberState>(lifecycle), {roles.begin(), roles.end()}});
    }
    for (const auto& [id, rule] : std::get<3>(policy)) {
        const auto parsed = unwire(rule);
        if (!parsed)
            return std::nullopt;
        target.roles.emplace(id, *parsed);
    }
    for (const auto& [id, channel] : std::get<4>(policy)) {
        const auto& [defaultRules, roleRules, archived, authorization] = channel;
        ChannelPolicy parsed;
        if (defaultRules.size() > 1)
            return std::nullopt;
        if (!defaultRules.empty()) {
            parsed.defaults = unwire(defaultRules.front());
            if (!parsed.defaults)
                return std::nullopt;
        }
        for (const auto& [role, rule] : roleRules) {
            const auto roleRule = unwire(rule);
            if (!roleRule)
                return std::nullopt;
            parsed.roles.emplace(role, *roleRule);
        }
        parsed.archived = archived;
        parsed.authorization = authorization;
        target.channels.emplace(id, std::move(parsed));
    }
    for (const auto& [id, delegation] : std::get<5>(policy)) {
        const auto& [permissions, editable, assignable, channels, circleWide] = delegation;
        const auto parsed = unwire(permissions);
        if (!parsed)
            return std::nullopt;
        target.delegations.emplace(id,
                                   RoleDelegation {*parsed,
                                                   {editable.begin(), editable.end()},
                                                   {assignable.begin(), assignable.end()},
                                                   {channels.begin(), channels.end()},
                                                   circleWide});
    }
    target.ended = std::get<6>(policy);
    if (!target.isValid())
        return std::nullopt;
    state.devices = {devices.begin(), devices.end()};
    state.memberRevisions = {revisions.begin(), revisions.end()};
    for (const auto& [id, vote] : votes) {
        const auto& [action, member, voter, voterRevision, targetRevision] = vote;
        if (action != static_cast<int>(CircleModerationAction::BAN_MEMBER)
            && action != static_cast<int>(CircleModerationAction::UNBAN_MEMBER))
            return std::nullopt;
        state.votes.emplace(id,
                            PendingVote {{static_cast<CircleModerationAction>(action), member},
                                         voter,
                                         voterRevision,
                                         targetRevision});
    }
    state.memberEpochs = {epochs.begin(), epochs.end()};
    state.memberTransitions = {transitions.begin(), transitions.end()};
    return state;
}

std::filesystem::path
statePath(git_repository* repo)
{
    return std::filesystem::path(git_repository_path(repo)) / CircleRepository::STATE_FILE;
}

// Local storage is trusted like other account data: a saved state replaces replaying its history.
void
saveState(git_repository* repo, const VerifiedState& state)
{
    try {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, wire(state));
        fileutils::writeAtomic(statePath(repo), std::string_view(buffer.data(), buffer.size()));
    } catch (const std::exception&) {
    }
}

std::optional<VerifiedState>
loadState(git_repository* repo, std::string_view circle)
{
    try {
        const auto data = fileutils::readBounded(statePath(repo), MAX_STATE_SIZE);
        if (!data)
            return std::nullopt;
        const auto handle = msgpack::unpack(data->data(), data->size());
        auto state = unwire(handle.get().as<WireState>());
        if (!state || state->circle != circle)
            return std::nullopt;
        return state;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

ChannelValidationResult validateChannelHistory(git_repository* circleRepo,
                                               const VerifiedState& circle,
                                               const RecentStates& recent,
                                               git_repository* repo,
                                               std::string_view channel,
                                               const git_oid& head,
                                               const std::vector<git_oid>& verified,
                                               size_t limit);

Permission
channelContentPermission(const CommitMessage& message)
{
    return !message.editedId.empty() ? (message.body.empty() ? Permission::Delete : Permission::Edit)
                                     : textPermission(message);
}

// The account that signed a channel record, read from the circle policy the record references.
std::optional<std::string>
recordSigner(git_repository* circleRepo, const CircleRecord& record, std::string_view device)
{
    git_oid base {};
    if (git_oid_fromstr(&base, record.base.c_str()) < 0)
        return std::nullopt;
    const auto policy = lookupCommit(circleRepo, &base);
    const auto tree = policy ? lookupTree(circleRepo, git_commit_tree_id(policy.get())) : GitTree {};
    const auto signer = tree ? readSigner(circleRepo, tree.get(), device) : std::nullopt;
    return signer ? std::optional<std::string>(signer->memberId) : std::nullopt;
}

// Returns whether verified content is an edit, a reaction, and its author account.
std::optional<std::tuple<bool, bool, std::string>>
verifiedContent(git_repository* circleRepo, git_repository* repo, std::string_view channel, const git_oid& oid)
{
    const auto commit = lookupCommit(repo, &oid);
    if (!commit || git_commit_parentcount(commit.get()) != 1)
        return std::nullopt;
    const auto text = git_commit_message_raw(commit.get());
    const auto record = text ? CircleRecord::fromString(text) : std::nullopt;
    const auto content = record ? std::get_if<CircleChannelContent>(&record->payload) : nullptr;
    const auto author = git_commit_author(commit.get());
    if (!content || content->channel != channel || !author || !author->email)
        return std::nullopt;
    const auto signer = recordSigner(circleRepo, *record, author->email);
    if (!signer)
        return std::nullopt;
    return std::tuple {!content->message.editedId.empty(), !content->message.reactTo.empty(), *signer};
}

// Validates the channel commits reachable from head but not from already verified heads.
ChannelValidationResult
validateChannelHistory(git_repository* circleRepo,
                       const VerifiedState& circle,
                       const RecentStates& recent,
                       git_repository* repo,
                       std::string_view channel,
                       const git_oid& head,
                       const std::vector<git_oid>& verified,
                       size_t limit)
{
    ChannelValidationResult result;
    result.head = oidString(&head);
    const auto registered = circle.policy.channels.find(channel);
    if (!isHexId(channel, 40))
        return result;
    if (registered == circle.policy.channels.end()) {
        result.status = ChannelValidationStatus::UNREGISTERED;
        return result;
    }
    struct Entry
    {
        git_oid oid {};
        std::string policy;
        std::string target;
        std::string actor;
        bool merge {false};
        bool edit {false};
        bool reaction {false};
    };
    std::vector<Entry> entries;
    std::map<std::string, size_t> indices;
    git_oid root {}, accepted {};
    const auto empty = emptyTree(repo);
    git_revwalk* walkerRaw = nullptr;
    if (git_oid_fromstr(&root, std::string(channel).c_str()) < 0 || git_oid_fromstr(&accepted, circle.head.c_str()) < 0
        || !empty || git_revwalk_new(&walkerRaw, repo) < 0)
        return result;
    GitRevWalker walker(walkerRaw);
    if (git_revwalk_push(walker.get(), &head) < 0)
        return result;
    for (const auto& oid : verified) {
        if (git_revwalk_hide(walker.get(), &oid) < 0)
            return result;
    }
    bool rootSeen = false;
    size_t seen = 0;
    git_oid current {};
    int walked = 0;
    while ((walked = git_revwalk_next(&current, walker.get())) == 0) {
        if (++seen > limit) {
            result.status = ChannelValidationStatus::LIMIT_EXCEEDED;
            return result;
        }
        const auto commit = lookupCommit(repo, &current);
        if (!commit || !git_oid_equal(git_commit_tree_id(commit.get()), &*empty))
            return result;
        const auto parents = git_commit_parentcount(commit.get());
        if (git_oid_equal(&current, &root)) {
            if (parents)
                return result;
            rootSeen = true;
            continue;
        }
        if (!parents || parents > 2)
            return result;
        const auto text = git_commit_message_raw(commit.get());
        const auto record = text ? CircleRecord::fromString(text) : std::nullopt;
        if (!record || record->circle != circle.circle)
            return result;
        const auto first = git_commit_parent_id(commit.get(), 0);
        Entry entry {current, record->base};
        if (parents == 1) {
            const auto content = std::get_if<CircleChannelContent>(&record->payload);
            if (!content || content->channel != channel || content->parent != oidString(first))
                return result;
            const auto& message = content->message;
            entry.edit = !message.editedId.empty();
            entry.reaction = !message.reactTo.empty();
            entry.target = entry.edit ? message.editedId : !message.replyTo.empty() ? message.replyTo : message.reactTo;
        } else {
            const auto merge = std::get_if<CircleChannelMerge>(&record->payload);
            const auto second = git_commit_parent_id(commit.get(), 1);
            if (!merge || merge->channel != channel || merge->parent != oidString(first)
                || merge->other != oidString(second) || git_oid_equal(first, second)
                || git_graph_descendant_of(repo, first, second) != 0
                || git_graph_descendant_of(repo, second, first) != 0)
                return result;
            entry.merge = true;
        }
        indices.emplace(oidString(&current), entries.size());
        entries.push_back(std::move(entry));
    }
    if (walked != GIT_ITEROVER || (!rootSeen && verified.empty()))
        return result;
    const auto proof = rootSeen ? rawChannelGenesis(repo, root) : std::nullopt;
    if (rootSeen && !proof)
        return result;
    std::map<std::string, std::vector<size_t>> policies;
    for (size_t index = 0; index < entries.size(); ++index)
        policies[entries[index].policy].push_back(index);
    bool rootVerified = !rootSeen;
    bool invalid = false;
    const auto& authorization = registered->second.authorization;
    const auto inspect = [&](const VerifiedState& policy) {
        if (!rootVerified && policy.head == authorization) {
            rootVerified = verifyChannelGenesis(circleRepo, *proof, channel, policy);
            if (!rootVerified) {
                invalid = true;
                return false;
            }
        }
        const auto needed = policies.find(policy.head);
        if (needed == policies.end())
            return true;
        const auto registration = policy.policy.channels.find(channel);
        if (registration == policy.policy.channels.end() || registration->second.authorization != authorization) {
            invalid = true;
            return false;
        }
        const auto tree = lookupTree(circleRepo, &policy.tree);
        for (auto index : needed->second) {
            auto& entry = entries[index];
            const auto commit = lookupCommit(repo, &entry.oid);
            const auto author = commit ? git_commit_author(commit.get()) : nullptr;
            const auto enrolled = author && author->email ? policy.devices.find(author->email) : policy.devices.end();
            const auto signer = enrolled != policy.devices.end() && tree
                                    ? readSigner(circleRepo, tree.get(), author->email, enrolled->second)
                                    : std::nullopt;
            const auto record = signer ? verifyCommit(repo, commit.get(), *signer) : std::nullopt;
            const auto content = record ? std::get_if<CircleChannelContent>(&record->payload) : nullptr;
            const auto merge = record ? std::get_if<CircleChannelMerge>(&record->payload) : nullptr;
            const auto permission = content ? channelContentPermission(content->message) : Permission::ReadChannel;
            if ((!content && !merge) || record->circle != circle.circle || record->base != policy.head
                || (content ? content->channel : merge->channel) != channel
                || !policy.policy.hasPermission(signer->memberId, permission, channel)) {
                invalid = true;
                return false;
            }
            entry.actor = signer->memberId;
        }
        policies.erase(needed);
        return true;
    };
    std::vector<std::string> wanted;
    for (const auto& [policy, unused] : policies)
        wanted.push_back(policy);
    if (!rootVerified)
        wanted.push_back(authorization);
    for (const auto& policy : wanted) {
        std::shared_ptr<const VerifiedState> known;
        if (policy != circle.head)
            known = recent.find(policy);
        const auto state = policy == circle.head ? &circle : known.get();
        git_oid oid {};
        if (!state || git_oid_fromstr(&oid, policy.c_str()) < 0
            || (policy != circle.head && git_graph_descendant_of(circleRepo, &accepted, &oid) != 1))
            continue;
        if (!inspect(*state))
            return result;
    }
    if (!policies.empty() || !rootVerified) {
        HistoryValidator validator(circleRepo, circle.circle, CircleRepository::DEFAULT_VALIDATION_LIMIT);
        if (!validator.run({accepted}, inspect)) {
            if (validator.exhausted()) {
                result.status = ChannelValidationStatus::LIMIT_EXCEEDED;
            } else if (!invalid) {
                result.status = ChannelValidationStatus::PENDING_DEPENDENCY;
                result.missingPolicies.push_back(circle.head);
            }
            return result;
        }
    }
    if (!rootVerified)
        return result;
    if (!policies.empty()) {
        result.status = ChannelValidationStatus::PENDING_DEPENDENCY;
        for (const auto& [policy, unused] : policies)
            result.missingPolicies.push_back(policy);
        return result;
    }
    for (const auto& entry : entries) {
        if (entry.target.empty())
            continue;
        git_oid target {};
        if (git_oid_fromstr(&target, entry.target.c_str()) < 0
            || git_graph_descendant_of(repo, &entry.oid, &target) != 1)
            return result;
        bool edit = false, reaction = false;
        std::string actor;
        if (const auto found = indices.find(entry.target); found != indices.end()) {
            const auto& original = entries[found->second];
            if (original.merge)
                return result;
            edit = original.edit;
            reaction = original.reaction;
            actor = original.actor;
        } else {
            const auto original = verifiedContent(circleRepo, repo, channel, target);
            if (!original)
                return result;
            std::tie(edit, reaction, actor) = *original;
        }
        if (edit || (entry.edit ? entry.actor != actor : reaction))
            return result;
    }
    result.status = ChannelValidationStatus::VALID;
    return result;
}

using WireChannelState = std::tuple<unsigned, std::string, std::string, std::string, std::string>;

void
saveChannelState(git_repository* repo, const VerifiedState& circle, std::string_view channel, std::string_view head)
{
    try {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer,
                      WireChannelState {CircleRepository::STATE_VERSION,
                                        circle.circle,
                                        std::string(channel),
                                        std::string(head),
                                        circle.head});
        fileutils::writeAtomic(statePath(repo), std::string_view(buffer.data(), buffer.size()));
    } catch (const std::exception&) {
    }
}

// A channel head verified against an accepted circle ancestor stays verified.
std::vector<git_oid>
verifiedChannelHeads(git_repository* circleRepo,
                     const VerifiedState& circle,
                     git_repository* repo,
                     std::string_view channel)
{
    try {
        const auto data = fileutils::readBounded(statePath(repo), 4096);
        if (!data)
            return {};
        const auto handle = msgpack::unpack(data->data(), data->size());
        const auto [version, savedCircle, savedChannel, head, circleHead] = handle.get().as<WireChannelState>();
        git_oid channelHead {}, policy {}, accepted {};
        if (version != CircleRepository::STATE_VERSION || savedCircle != circle.circle || savedChannel != channel
            || git_oid_fromstr(&channelHead, head.c_str()) < 0 || git_oid_fromstr(&policy, circleHead.c_str()) < 0
            || git_oid_fromstr(&accepted, circle.head.c_str()) < 0
            || (!git_oid_equal(&policy, &accepted) && git_graph_descendant_of(circleRepo, &accepted, &policy) != 1)
            || !lookupCommit(repo, &channelHead))
            return {};
        return {channelHead};
    } catch (const std::exception&) {
        return {};
    }
}

std::optional<git_oid>
writeTree(git_repository* repo, const dht::crypto::Identity& identity)
{
    using Builder = std::unique_ptr<git_treebuilder, decltype(&git_treebuilder_free)>;
    git_treebuilder* rootRaw = nullptr;
    if (git_treebuilder_new(&rootRaw, repo, nullptr) < 0)
        return std::nullopt;
    Builder root(rootRaw, git_treebuilder_free);
    for (auto directory : {"members", "devices"}) {
        const auto& cert = std::string_view(directory) == "members" ? identity.second->issuer : identity.second;
        const auto id = std::string_view(directory) == "members" ? cert->getId().toString()
                                                                 : cert->getLongId().toString();
        const auto contents = cert->toString(false);
        git_oid blob {}, subtree {};
        git_treebuilder* childRaw = nullptr;
        if (contents.size() > MAX_CERTIFICATE_SIZE
            || git_blob_create_frombuffer(&blob, repo, contents.data(), contents.size()) < 0
            || git_treebuilder_new(&childRaw, repo, nullptr) < 0)
            return std::nullopt;
        Builder child(childRaw, git_treebuilder_free);
        if (git_treebuilder_insert(nullptr, child.get(), (id + ".crt").c_str(), &blob, GIT_FILEMODE_BLOB) < 0
            || git_treebuilder_write(&subtree, child.get()) < 0
            || git_treebuilder_insert(nullptr, root.get(), directory, &subtree, GIT_FILEMODE_TREE) < 0)
            return std::nullopt;
    }
    git_oid tree {};
    if (git_treebuilder_write(&tree, root.get()) < 0)
        return std::nullopt;
    return tree;
}

std::optional<git_oid>
writeChanges(git_repository* repo, const git_tree* before, const std::vector<TreeEdit>& edits)
{
    if (edits.empty())
        return *git_tree_id(before);
    using Builder = std::unique_ptr<git_treebuilder, decltype(&git_treebuilder_free)>;
    git_treebuilder* rootRaw = nullptr;
    if (git_treebuilder_new(&rootRaw, repo, before) < 0)
        return std::nullopt;
    Builder root(rootRaw, git_treebuilder_free);
    std::map<std::string, std::vector<const TreeEdit*>, std::less<>> directories;
    for (const auto& edit : edits)
        directories[edit.directory].push_back(&edit);
    for (const auto& [name, changes] : directories) {
        const auto entry = name.empty() ? nullptr : git_tree_entry_byname(before, name.c_str());
        const auto source = entry ? lookupTree(repo, git_tree_entry_id(entry), MAX_DIRECTORY_SIZE) : GitTree {};
        if (entry && !source)
            return std::nullopt;
        git_treebuilder* childRaw = nullptr;
        if (!name.empty() && git_treebuilder_new(&childRaw, repo, source.get()) < 0)
            return std::nullopt;
        Builder child(childRaw, git_treebuilder_free);
        auto builder = name.empty() ? root.get() : child.get();
        for (const auto edit : changes) {
            const auto existing = git_treebuilder_get(builder, edit->name.c_str());
            if (edit->contents) {
                git_oid blob {};
                if ((existing && !edit->replace) || (!edit->replace && edit->contents->size() > MAX_CERTIFICATE_SIZE)
                    || git_blob_create_frombuffer(&blob, repo, edit->contents->data(), edit->contents->size()) < 0
                    || git_treebuilder_insert(nullptr, builder, edit->name.c_str(), &blob, GIT_FILEMODE_BLOB) < 0)
                    return std::nullopt;
            } else if (!existing || git_treebuilder_remove(builder, edit->name.c_str()) < 0) {
                return std::nullopt;
            }
        }
        if (name.empty())
            continue;
        if (!git_treebuilder_entrycount(child.get())) {
            if (git_treebuilder_remove(root.get(), name.c_str()) < 0)
                return std::nullopt;
        } else {
            git_oid tree {};
            if (git_treebuilder_write(&tree, child.get()) < 0
                || git_treebuilder_insert(nullptr, root.get(), name.c_str(), &tree, GIT_FILEMODE_TREE) < 0)
                return std::nullopt;
        }
    }
    git_oid result {};
    if (git_treebuilder_write(&result, root.get()) < 0)
        return std::nullopt;
    return result;
}

std::optional<git_oid>
writeCommit(git_repository* repo,
            const dht::crypto::Identity& identity,
            const git_oid& treeId,
            git_commit* parent,
            const std::string& message,
            git_commit* other = nullptr)
{
    const auto tree = git::lookupTree(repo, &treeId);
    const auto signature = git::makeSignature(identity.second->issuer->getId().toString(),
                                              identity.second->getLongId().toString());
    if (!tree || !signature)
        return std::nullopt;
    std::vector<const git_commit*> parents;
    if (parent)
        parents.push_back(parent);
    if (parent && other)
        parents.push_back(other);
    return git::createSignedCommit(repo, signature.get(), tree.get(), std::move(parents), message, *identity.first);
}

} // namespace

struct CircleRepository::Impl
{
    GitRepository repo;
    VerifiedState state;
    RecentStates recent;
    std::atomic<uint64_t> generation {0};
    mutable std::mutex mutex;

    Impl(GitRepository repository, VerifiedState verified, RecentStates known = {})
        : repo(std::move(repository))
        , state(std::move(verified))
        , recent(std::move(known))
    {
        recent.add(state);
    }

    void publish(VerifiedState next)
    {
        state.policy = std::move(next.policy);
        state.tree = next.tree;
        state.devices.swap(next.devices);
        state.memberRevisions.swap(next.memberRevisions);
        state.votes.swap(next.votes);
        state.historyBase.swap(next.historyBase);
        state.memberEpochs.swap(next.memberEpochs);
        state.memberTransitions.swap(next.memberTransitions);
        state.head.swap(next.head);
        recent.add(state);
        ++generation;
        saveState(repo.get(), state);
    }

    bool current() const
    {
        git_oid head {}, main {};
        return git_oid_fromstr(&head, state.head.c_str()) == 0
               && git_reference_name_to_id(&main, repo.get(), MAIN_REF) == 0 && git_oid_equal(&head, &main);
    }

    ChannelValidationResult checkChannel(git_repository* channelRepo,
                                         std::string_view channel,
                                         const git_oid& head,
                                         size_t limit) const
    {
        return validateChannelHistory(repo.get(),
                                      state,
                                      recent,
                                      channelRepo,
                                      channel,
                                      head,
                                      verifiedChannelHeads(repo.get(), state, channelRepo, channel),
                                      limit);
    }

    // Fast-forwards to other, or merges it when an identity may sign the merge.
    ChannelValidationResult updateChannel(const std::filesystem::path& path,
                                          std::string_view channel,
                                          std::string_view other,
                                          const dht::crypto::Identity* identity)
    {
        git_repository* raw = nullptr;
        git_oid previous {}, target {};
        if (!isHexId(other, 40) || git_oid_fromstr(&target, std::string(other).c_str()) < 0
            || git_repository_open_bare(&raw, path.string().c_str()) < 0)
            return {};
        GitRepository channelRepo(raw);
        if (git_reference_name_to_id(&previous, channelRepo.get(), MAIN_REF) < 0)
            return {};
        auto result = checkChannel(channelRepo.get(), channel, previous, DEFAULT_VALIDATION_LIMIT);
        if (result.status != ChannelValidationStatus::VALID)
            return result;
        saveChannelState(channelRepo.get(), state, channel, result.head);
        if (git_oid_equal(&previous, &target) || git_graph_descendant_of(channelRepo.get(), &previous, &target) == 1)
            return result;
        result = validateChannelHistory(repo.get(),
                                        state,
                                        recent,
                                        channelRepo.get(),
                                        channel,
                                        target,
                                        {previous},
                                        DEFAULT_VALIDATION_LIMIT);
        if (result.status != ChannelValidationStatus::VALID)
            return result;
        auto next = target;
        if (git_graph_descendant_of(channelRepo.get(), &target, &previous) != 1) {
            if (!identity) {
                result.status = ChannelValidationStatus::DIVERGED;
                return result;
            }
            const CircleRecord record {CircleChannelMerge {std::string(channel),
                                                           oidString(&previous),
                                                           std::string(other)},
                                       state.circle,
                                       state.head};
            const auto text = record.toString();
            const auto parent = lookupCommit(channelRepo.get(), &previous);
            const auto second = lookupCommit(channelRepo.get(), &target);
            const auto empty = emptyTree(channelRepo.get());
            const auto merge = text && parent && second && empty
                                   ? writeCommit(channelRepo.get(), *identity, *empty, parent.get(), *text, second.get())
                                   : std::nullopt;
            if (!merge)
                return {};
            result = validateChannelHistory(repo.get(),
                                            state,
                                            recent,
                                            channelRepo.get(),
                                            channel,
                                            *merge,
                                            {previous, target},
                                            DEFAULT_VALIDATION_LIMIT);
            if (result.status != ChannelValidationStatus::VALID || !current())
                return {};
            next = *merge;
        }
        git_reference* referenceRaw = nullptr;
        if (git_reference_create_matching(&referenceRaw, channelRepo.get(), MAIN_REF, &next, 1, &previous, "Update channel")
            < 0)
            return {};
        GitReference reference(referenceRaw);
        saveChannelState(channelRepo.get(), state, channel, result.head);
        return result;
    }
};

CircleRepository::CircleRepository(std::unique_ptr<Impl> impl)
    : pimpl_(std::move(impl))
{}

CircleRepository::~CircleRepository() = default;

std::unique_ptr<CircleRepository>
CircleRepository::create(const std::filesystem::path& path,
                         const dht::crypto::Identity& identity,
                         const PermissionRule& defaults)
{
    if (!ensureGit() || !validIdentity(identity))
        return {};
    const auto member = identity.second->issuer->getId().toString();
    std::array<uint8_t, 32> nonce {};
    if (gnutls_rnd(GNUTLS_RND_NONCE, nonce.data(), nonce.size()) < 0)
        return {};
    const CircleRecord record {CircleGenesis {member, defaults, dht::PkId(nonce.data(), nonce.size()).toString()},
                               {},
                               {}};
    const auto message = record.toString();
    if (!message)
        return {};
    std::error_code error;
    if (!std::filesystem::create_directory(path, error))
        return {};
    auto create = [&]() -> std::unique_ptr<CircleRepository> {
        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     error);
        git_repository* raw = nullptr;
        if (error || git_repository_init(&raw, path.string().c_str(), 1) < 0)
            return {};
        GitRepository repo(raw);
        const auto tree = writeTree(repo.get(), identity);
        if (!tree)
            return {};
        const auto commit = writeCommit(repo.get(), identity, *tree, nullptr, *message);
        if (!commit)
            return {};
        auto state = validateHistory(repo.get(), oidString(&*commit), *commit, 1);
        git_reference* referenceRaw = nullptr;
        if (!state || git_reference_create(&referenceRaw, repo.get(), MAIN_REF, &*commit, 0, "Create circle") < 0)
            return {};
        GitReference reference(referenceRaw);
        if (git_repository_set_head(repo.get(), MAIN_REF) < 0)
            return {};
        saveState(repo.get(), *state);
        return std::unique_ptr<CircleRepository>(
            new CircleRepository(std::make_unique<Impl>(std::move(repo), std::move(*state))));
    };
    std::unique_ptr<CircleRepository> result;
    try {
        result = create();
    } catch (const std::exception&) {
    }
    if (!result)
        std::filesystem::remove_all(path, error);
    return result;
}

std::unique_ptr<CircleRepository>
CircleRepository::open(const std::filesystem::path& path, std::string_view expectedCircle, size_t validationLimit)
{
    if (!ensureGit() || !isHexId(expectedCircle, 40))
        return {};
    try {
        git_repository* raw = nullptr;
        if (git_repository_open_bare(&raw, path.string().c_str()) < 0)
            return {};
        GitRepository repo(raw);
        git_oid head {};
        if (git_reference_name_to_id(&head, repo.get(), MAIN_REF) < 0)
            return {};
        RecentStates recent;
        std::optional<VerifiedState> state;
        const auto saved = loadState(repo.get(), expectedCircle);
        if (saved) {
            auto advanced = advanceLinear(repo.get(), *saved, head, validationLimit, recent);
            if (!advanced.state && !advanced.fallback)
                return {};
            state = std::move(advanced.state);
        }
        if (!state)
            state = validateHistory(repo.get(), expectedCircle, head, validationLimit);
        if (!state)
            return {};
        if (!saved || saved->head != state->head)
            saveState(repo.get(), *state);
        return std::unique_ptr<CircleRepository>(
            new CircleRepository(std::make_unique<Impl>(std::move(repo), std::move(*state), std::move(recent))));
    } catch (const std::exception&) {
        return {};
    }
}

uint64_t
CircleRepository::generation() const
{
    return pimpl_->generation;
}

std::optional<std::string>
CircleRepository::fastForward(std::string_view other, size_t validationLimit)
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        const auto& state = pimpl_->state;
        git_oid previous {}, target {}, current {};
        if (!isHexId(other, 40) || git_oid_fromstr(&previous, state.head.c_str()) < 0
            || git_oid_fromstr(&target, std::string(other).c_str()) < 0
            || git_reference_name_to_id(&current, pimpl_->repo.get(), MAIN_REF) < 0
            || !git_oid_equal(&previous, &current))
            return std::nullopt;
        if (git_oid_equal(&previous, &target) || git_graph_descendant_of(pimpl_->repo.get(), &previous, &target) == 1)
            return state.head;
        if (git_graph_descendant_of(pimpl_->repo.get(), &target, &previous) != 1)
            return std::nullopt;
        auto next = advance(pimpl_->repo.get(), state, target, validationLimit, pimpl_->recent);
        git_reference* raw = nullptr;
        if (!next
            || git_reference_create_matching(
                   &raw, pimpl_->repo.get(), MAIN_REF, &target, 1, &previous, "Fast-forward circle")
                   < 0)
            return std::nullopt;
        GitReference reference(raw);
        pimpl_->publish(std::move(*next));
        return state.head;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool
CircleRepository::reload()
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        git_oid current {};
        if (git_reference_name_to_id(&current, pimpl_->repo.get(), MAIN_REF) < 0)
            return false;
        if (oidString(&current) == pimpl_->state.head)
            return true;
        auto next = advance(pimpl_->repo.get(), pimpl_->state, current, DEFAULT_VALIDATION_LIMIT, pimpl_->recent);
        if (!next)
            return false;
        pimpl_->publish(std::move(*next));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

const std::string&
CircleRepository::id() const
{
    return pimpl_->state.circle;
}

std::string
CircleRepository::head() const
{
    std::lock_guard lock(pimpl_->mutex);
    return pimpl_->state.head;
}

CirclePolicy
CircleRepository::policy() const
{
    std::lock_guard lock(pimpl_->mutex);
    return pimpl_->state.policy;
}

std::string
CircleRepository::profile(std::optional<std::string_view> channel) const
{
    std::lock_guard lock(pimpl_->mutex);
    if (channel && !pimpl_->state.policy.channels.contains(*channel))
        return {};
    const auto tree = lookupTree(pimpl_->repo.get(), &pimpl_->state.tree);
    return tree ? readProfile(pimpl_->repo.get(),
                              tree.get(),
                              channel ? std::optional<std::string>(*channel) : std::nullopt)
                      .value_or(std::string())
                : std::string();
}

std::optional<std::string>
CircleRepository::updateProfile(const dht::crypto::Identity& identity,
                                std::string_view vcard,
                                std::optional<std::string_view> channel)
{
    std::lock_guard lock(pimpl_->mutex);
    return appendRecord(identity,
                        CircleRecord {CirclePolicyChange {SetCircleProfile {
                                          channel ? std::optional<std::string>(*channel) : std::nullopt}},
                                      pimpl_->state.circle,
                                      pimpl_->state.head},
                        vcard);
}

bool
CircleRepository::enrolled(std::string_view device) const
{
    std::lock_guard lock(pimpl_->mutex);
    return pimpl_->state.devices.find(std::string(device)) != pimpl_->state.devices.end();
}

bool
CircleRepository::authorizePeer(const dht::crypto::Certificate& certificate,
                                std::optional<std::string_view> channel) const
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        const auto& state = pimpl_->state;
        git_oid current {};
        if (!certificate.issuer || git_reference_name_to_id(&current, pimpl_->repo.get(), MAIN_REF) < 0
            || oidString(&current) != state.head)
            return false;
        const auto member = certificate.issuer->getId().toString();
        const auto device = certificate.getLongId().toString();
        const auto found = state.policy.members.find(member);
        const auto now = std::time(nullptr);
        if (found == state.policy.members.end() || !validAt(certificate, now) || !validAt(*certificate.issuer, now)
            || !isCertificateOfDevice(certificate, device)
            || !isDeviceOfMember(certificate, *certificate.issuer, member))
            return false;
        if (found->second.state == CircleMemberState::INVITED)
            return !channel;
        // Enrollment only authenticates signed commits: reading is granted to any device of the account.
        return found->second.state == CircleMemberState::ACTIVE
               && (!channel || state.policy.hasPermission(member, Permission::ReadChannel, *channel));
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<std::string>
CircleRepository::append(const dht::crypto::Identity& identity, const CirclePolicyChange& change)
{
    std::lock_guard lock(pimpl_->mutex);
    return appendRecord(identity, CircleRecord {change, pimpl_->state.circle, pimpl_->state.head});
}

std::optional<CircleChannelResult>
CircleRepository::createChannel(const std::filesystem::path& path,
                                const dht::crypto::Identity& identity,
                                const ChannelPolicy& policy)
{
    std::lock_guard lock(pimpl_->mutex);
    auto& state = pimpl_->state;
    if (!validIdentity(identity) || policy.archived || !policy.authorization.empty())
        return std::nullopt;
    const auto member = identity.second->issuer->getId().toString();
    if (!state.policy.apply(member, RegisterCircleChannel {std::string(40, '0'), state.head, policy, "pending"}))
        return std::nullopt;
    const auto device = identity.second->getLongId().toString();
    if (!state.devices.contains(device) || state.devices.at(device) != member)
        return std::nullopt;
    std::error_code error;
    if (!std::filesystem::create_directory(path, error))
        return std::nullopt;
    bool keep = false;
    std::optional<CircleChannelResult> result;
    try {
        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     error);
        git_repository* raw = nullptr;
        if (!error && git_repository_init(&raw, path.string().c_str(), 1) == 0) {
            GitRepository repo(raw);
            std::array<uint8_t, 32> nonce {};
            const auto tree = emptyTree(repo.get());
            if (tree && gnutls_rnd(GNUTLS_RND_NONCE, nonce.data(), nonce.size()) == 0) {
                const CircleRecord record {CircleChannelGenesis {member,
                                                                 dht::PkId(nonce.data(), nonce.size()).toString(),
                                                                 policy},
                                           state.circle,
                                           state.head};
                const auto text = record.toString();
                const auto commit = text ? writeCommit(repo.get(), identity, *tree, nullptr, *text) : std::nullopt;
                const auto proof = commit ? rawChannelGenesis(repo.get(), *commit) : std::nullopt;
                git_reference* referenceRaw = nullptr;
                if (proof && verifyChannelGenesis(pimpl_->repo.get(), *proof, oidString(&*commit), state)
                    && git_reference_create(&referenceRaw, repo.get(), MAIN_REF, &*commit, 0, "Create channel") == 0) {
                    GitReference reference(referenceRaw);
                    if (git_repository_set_head(repo.get(), MAIN_REF) == 0) {
                        keep = true;
                        result = CircleChannelResult {oidString(&*commit), std::nullopt};
                        result->registration = appendRecord(identity,
                                                            CircleRecord {CirclePolicyChange {
                                                                              RegisterCircleChannel {result->channel,
                                                                                                     state.head,
                                                                                                     policy,
                                                                                                     *proof}},
                                                                          state.circle,
                                                                          state.head});
                    }
                }
            }
        }
    } catch (const std::exception&) {
    }
    if (!keep)
        std::filesystem::remove_all(path, error);
    return result;
}

bool
CircleRepository::verifyChannel(const std::filesystem::path& path, std::string_view channel) const
{
    return validateChannel(path, channel).status == ChannelValidationStatus::VALID;
}

ChannelValidationResult
CircleRepository::validateChannel(const std::filesystem::path& path,
                                  std::string_view channel,
                                  size_t validationLimit) const
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        git_repository* raw = nullptr;
        if (git_repository_open_bare(&raw, path.string().c_str()) < 0)
            return {};
        GitRepository repo(raw);
        git_oid head {};
        if (git_reference_name_to_id(&head, repo.get(), MAIN_REF) < 0)
            return {};
        auto result = pimpl_->checkChannel(repo.get(), channel, head, validationLimit);
        if (result.status == ChannelValidationStatus::VALID)
            saveChannelState(repo.get(), pimpl_->state, channel, result.head);
        return result;
    } catch (const std::exception&) {
        return {};
    }
}

bool
CircleRepository::channelLog(const std::filesystem::path& path,
                             std::string_view channel,
                             const std::function<bool(CircleChannelEntry&&)>& visit,
                             std::string_view from,
                             std::string_view stop) const
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        git_repository* raw = nullptr;
        if (git_repository_open_bare(&raw, path.string().c_str()) < 0)
            return false;
        GitRepository repo(raw);
        git_oid head {}, start {}, hidden {};
        if (git_reference_name_to_id(&head, repo.get(), MAIN_REF) < 0
            || pimpl_->checkChannel(repo.get(), channel, head, DEFAULT_VALIDATION_LIMIT).status
                   != ChannelValidationStatus::VALID)
            return false;
        if (!from.empty()
            && (!isHexId(from, 40) || git_oid_fromstr(&start, std::string(from).c_str()) < 0
                || (!git_oid_equal(&start, &head) && git_graph_descendant_of(repo.get(), &head, &start) != 1)))
            return false;
        git_revwalk* walkRaw = nullptr;
        if (git_revwalk_new(&walkRaw, repo.get()) < 0)
            return false;
        GitRevWalker walk(walkRaw);
        git_revwalk_sorting(walk.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
        if (git_revwalk_push(walk.get(), &head) < 0
            || (!stop.empty()
                && (!isHexId(stop, 40) || git_oid_fromstr(&hidden, std::string(stop).c_str()) < 0
                    || git_revwalk_hide(walk.get(), &hidden) < 0)))
            return false;
        std::map<std::pair<std::string, std::string>, std::string> signers;
        bool started = from.empty();
        git_oid oid {};
        while (git_revwalk_next(&oid, walk.get()) == 0) {
            // Pages start from the head too, so that merged branches keep one order.
            if (!started && !git_oid_equal(&oid, &start))
                continue;
            started = true;
            const auto commit = lookupCommit(repo.get(), &oid);
            const auto text = commit ? git_commit_message_raw(commit.get()) : nullptr;
            const auto record = text ? CircleRecord::fromString(text) : std::nullopt;
            const auto author = commit ? git_commit_author(commit.get()) : nullptr;
            if (!record || !author || !author->email)
                return false;
            CircleChannelEntry entry;
            entry.id = oidString(&oid);
            for (unsigned index = 0; index < git_commit_parentcount(commit.get()); ++index)
                entry.parents.push_back(oidString(git_commit_parent_id(commit.get(), index)));
            entry.device = author->email;
            entry.timestamp = author->when.time;
            if (const auto content = std::get_if<CircleChannelContent>(&record->payload))
                entry.message = content->message;
            auto& signer = signers[{record->base, entry.device}];
            if (signer.empty())
                signer = recordSigner(pimpl_->repo.get(), *record, entry.device).value_or(std::string());
            if (signer.empty())
                return false;
            entry.author = signer;
            if (!visit(std::move(entry)))
                break;
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

ChannelValidationResult
CircleRepository::fastForwardChannel(const std::filesystem::path& path, std::string_view channel, std::string_view other)
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        return pimpl_->updateChannel(path, channel, other, nullptr);
    } catch (const std::exception&) {
        return {};
    }
}

ChannelValidationResult
CircleRepository::mergeChannel(const std::filesystem::path& path,
                               std::string_view channel,
                               const dht::crypto::Identity& identity,
                               std::string_view other)
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        const auto& state = pimpl_->state;
        if (!validIdentity(identity))
            return {};
        const auto device = identity.second->getLongId().toString();
        const auto member = identity.second->issuer->getId().toString();
        const auto enrolled = state.devices.find(device);
        if (enrolled == state.devices.end() || enrolled->second != member
            || !state.policy.hasPermission(member, Permission::ReadChannel, channel) || !pimpl_->current())
            return {};
        return pimpl_->updateChannel(path, channel, other, &identity);
    } catch (const std::exception&) {
        return {};
    }
}

std::optional<std::string>
CircleRepository::appendChannelMessage(const std::filesystem::path& path,
                                       std::string_view channel,
                                       const dht::crypto::Identity& identity,
                                       const CommitMessage& message)
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        const auto& state = pimpl_->state;
        if (!validIdentity(identity))
            return std::nullopt;
        const auto device = identity.second->getLongId().toString();
        const auto member = identity.second->issuer->getId().toString();
        const auto enrolled = state.devices.find(device);
        if (enrolled == state.devices.end() || enrolled->second != member || !pimpl_->current())
            return std::nullopt;
        git_repository* raw = nullptr;
        if (git_repository_open_bare(&raw, path.string().c_str()) < 0)
            return std::nullopt;
        GitRepository repo(raw);
        git_oid previous {};
        if (git_reference_name_to_id(&previous, repo.get(), MAIN_REF) < 0)
            return std::nullopt;
        const auto verified = pimpl_->checkChannel(repo.get(), channel, previous, DEFAULT_VALIDATION_LIMIT);
        if (verified.status != ChannelValidationStatus::VALID)
            return std::nullopt;
        const CircleRecord record {CircleChannelContent {std::string(channel), verified.head, message},
                                   state.circle,
                                   state.head};
        const auto text = record.toString();
        const auto parent = lookupCommit(repo.get(), &previous);
        const auto tree = parent ? *git_commit_tree_id(parent.get()) : git_oid {};
        const auto commit = text && parent ? writeCommit(repo.get(), identity, tree, parent.get(), *text)
                                           : std::nullopt;
        if (!commit)
            return std::nullopt;
        const auto checked = validateChannelHistory(pimpl_->repo.get(),
                                                    state,
                                                    pimpl_->recent,
                                                    repo.get(),
                                                    channel,
                                                    *commit,
                                                    {previous},
                                                    DEFAULT_VALIDATION_LIMIT);
        git_reference* referenceRaw = nullptr;
        if (checked.status != ChannelValidationStatus::VALID || !pimpl_->current()
            || git_reference_create_matching(
                   &referenceRaw, repo.get(), MAIN_REF, &*commit, 1, &previous, "Append channel content")
                   < 0)
            return std::nullopt;
        GitReference reference(referenceRaw);
        saveChannelState(repo.get(), state, channel, checked.head);
        return checked.head;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string>
CircleRepository::vote(const dht::crypto::Identity& identity, const CircleModerationVote& operation)
{
    std::lock_guard lock(pimpl_->mutex);
    return appendRecord(identity, CircleRecord {operation, pimpl_->state.circle, pimpl_->state.head});
}

std::optional<std::string>
CircleRepository::resolveVote(const dht::crypto::Identity& identity, std::string_view voteCommit)
{
    std::lock_guard lock(pimpl_->mutex);
    return appendRecord(identity,
                        CircleRecord {CircleModerationResolution {std::string(voteCommit)},
                                      pimpl_->state.circle,
                                      pimpl_->state.head});
}

std::optional<CircleModerationResult>
CircleRepository::moderate(const dht::crypto::Identity& identity, const CircleModerationVote& operation)
{
    auto commit = vote(identity, operation);
    if (!commit)
        return std::nullopt;
    auto resolution = resolveVote(identity, *commit);
    return CircleModerationResult {std::move(*commit), std::move(resolution)};
}

std::vector<std::string>
CircleRepository::pendingVotes() const
{
    std::lock_guard lock(pimpl_->mutex);
    std::vector<std::string> result;
    result.reserve(pimpl_->state.votes.size());
    for (const auto& [id, vote] : pimpl_->state.votes)
        result.push_back(id);
    return result;
}

std::optional<std::string>
CircleRepository::merge(const dht::crypto::Identity& identity, std::string_view otherCommit)
{
    std::lock_guard lock(pimpl_->mutex);
    try {
        auto& state = pimpl_->state;
        if (!validIdentity(identity) || !isHexId(otherCommit, 40))
            return std::nullopt;
        git_oid previous {}, other {}, current {};
        if (git_oid_fromstr(&previous, state.head.c_str()) < 0
            || git_oid_fromstr(&other, std::string(otherCommit).c_str()) < 0
            || git_reference_name_to_id(&current, pimpl_->repo.get(), MAIN_REF) < 0
            || !git_oid_equal(&previous, &current))
            return std::nullopt;
        const int otherAncestor = git_oid_equal(&previous, &other)
                                      ? 1
                                      : git_graph_descendant_of(pimpl_->repo.get(), &previous, &other);
        const int firstAncestor = git_oid_equal(&previous, &other)
                                      ? 1
                                      : git_graph_descendant_of(pimpl_->repo.get(), &other, &previous);
        if (otherAncestor < 0 || firstAncestor < 0)
            return std::nullopt;
        std::optional<HistoryValidator> validator;
        const VerifiedState* left = nullptr;
        const VerifiedState* right = nullptr;
        std::optional<VerifiedState> next;
        if (otherAncestor) {
            next = state;
        } else if (firstAncestor) {
            next = advance(pimpl_->repo.get(), state, other, DEFAULT_VALIDATION_LIMIT, pimpl_->recent);
        } else {
            validator.emplace(pimpl_->repo.get(), state.circle, DEFAULT_VALIDATION_LIMIT);
            if (!validator->run({previous, other}))
                return std::nullopt;
            left = validator->state(state.head);
            right = validator->state(otherCommit);
            if (!left || !right)
                return std::nullopt;
            next = validator->reconcile(*left, *right);
        }
        if (!next)
            return std::nullopt;
        const auto member = identity.second->issuer->getId().toString();
        const auto device = identity.second->getLongId().toString();
        const auto owner = next->devices.find(device);
        const auto active = next->policy.members.find(member);
        if (owner == next->devices.end() || owner->second != member || active == next->policy.members.end()
            || active->second.state != CircleMemberState::ACTIVE)
            return std::nullopt;
        const auto tree = lookupTree(pimpl_->repo.get(), &next->tree);
        const auto signer = tree ? readSigner(pimpl_->repo.get(), tree.get(), device, member) : std::nullopt;
        if (!signer || !validAt(*signer->device, std::time(nullptr)) || !validAt(*signer->member, std::time(nullptr)))
            return std::nullopt;
        if (otherAncestor)
            return state.head;
        git_oid target = other;
        if (!firstAncestor) {
            const CircleRecord record {CirclePolicyMerge {std::string(otherCommit)}, state.circle, state.head};
            const auto text = record.toString();
            const auto parent = lookupCommit(pimpl_->repo.get(), &previous);
            const auto second = lookupCommit(pimpl_->repo.get(), &other);
            if (!text || !parent || !second)
                return std::nullopt;
            const auto oid = writeCommit(pimpl_->repo.get(), identity, next->tree, parent.get(), *text, second.get());
            if (!oid)
                return std::nullopt;
            const auto commit = lookupCommit(pimpl_->repo.get(), &*oid);
            next = commit ? validator->validateMerge(commit.get(), *left, *right) : std::nullopt;
            if (!next)
                return std::nullopt;
            target = *oid;
        }
        std::optional<std::string> result(next->head);
        git_reference* raw = nullptr;
        if (git_reference_create_matching(&raw, pimpl_->repo.get(), MAIN_REF, &target, 1, &previous, "Merge circle") < 0)
            return std::nullopt;
        GitReference reference(raw);
        pimpl_->publish(std::move(*next));
        return result;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string>
CircleRepository::appendRecord(const dht::crypto::Identity& identity,
                               const CircleRecord& record,
                               std::optional<std::string_view> profile)
{
    try {
        auto& state = pimpl_->state;
        if (!validIdentity(identity))
            return std::nullopt;
        git_oid previous {}, current {};
        if (git_oid_fromstr(&previous, state.head.c_str()) < 0
            || git_reference_name_to_id(&current, pimpl_->repo.get(), MAIN_REF) < 0
            || !git_oid_equal(&previous, &current))
            return std::nullopt;
        const Signer signer {identity.second,
                             identity.second->issuer,
                             identity.second->getLongId().toString(),
                             identity.second->issuer->getId().toString(),
                             identity.second->toString(false),
                             identity.second->issuer->toString(false)};
        const auto message = record.toString();
        if (!message)
            return std::nullopt;
        const auto edits = recordEdits(state, record, signer, profile);
        const auto source = lookupTree(pimpl_->repo.get(), &state.tree);
        if (!edits || !source)
            return std::nullopt;
        const auto tree = writeChanges(pimpl_->repo.get(), source.get(), *edits);
        if (!tree)
            return std::nullopt;
        if (profile && git_oid_equal(&*tree, &state.tree))
            return state.head;
        auto parent = lookupCommit(pimpl_->repo.get(), &previous);
        if (!parent)
            return std::nullopt;
        const auto oid = writeCommit(pimpl_->repo.get(), identity, *tree, parent.get(), *message);
        if (!oid)
            return std::nullopt;
        auto commit = lookupCommit(pimpl_->repo.get(), &*oid);
        std::optional<VerifiedState> authorized;
        std::shared_ptr<const VerifiedState> known;
        const VerifiedState* authorization = nullptr;
        if (const auto change = std::get_if<CirclePolicyChange>(&record.payload)) {
            if (const auto registration = std::get_if<RegisterCircleChannel>(change)) {
                known = pimpl_->recent.find(registration->authorization);
                if (registration->authorization == state.head) {
                    authorization = &state;
                } else if (known) {
                    authorization = known.get();
                } else {
                    git_oid policy {};
                    if (git_oid_fromstr(&policy, registration->authorization.c_str()) < 0)
                        return std::nullopt;
                    authorized = validateHistory(pimpl_->repo.get(), state.circle, policy, DEFAULT_VALIDATION_LIMIT);
                    authorization = authorized ? &*authorized : nullptr;
                }
            }
        }
        auto next = commit ? validateUpdate(pimpl_->repo.get(), commit.get(), state, nullptr, nullptr, authorization)
                           : std::nullopt;
        if (!next)
            return std::nullopt;
        std::optional<std::string> result(oidString(&*oid));
        git_reference* raw = nullptr;
        if (git_reference_create_matching(&raw, pimpl_->repo.get(), MAIN_REF, &*oid, 1, &previous, "Update circle") < 0)
            return std::nullopt;
        GitReference reference(raw);
        pimpl_->publish(std::move(*next));
        return result;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace jami
