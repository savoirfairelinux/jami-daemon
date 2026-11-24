#include "circle_record.h"

#include <json/json.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <type_traits>

namespace jami {
namespace {

std::optional<Json::Value>
parseStrict(std::string_view text, size_t limit)
{
    if (text.empty() || text.size() > limit || text.find('\0') != std::string_view::npos)
        return std::nullopt;
    Json::CharReaderBuilder builder;
    Json::CharReaderBuilder::strictMode(&builder.settings_);
    builder["collectComments"] = false;
    builder["rejectDupKeys"] = true;
    builder["stackLimit"] = 32;
    builder["skipBom"] = false;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value value;
    std::string error;
    try {
        if (!reader->parse(text.data(), text.data() + text.size(), &value, &error))
            return std::nullopt;
    } catch (const Json::Exception&) {
        return std::nullopt;
    }
    return value;
}

std::string
writeCompact(const Json::Value& value)
{
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    writer["commentStyle"] = "None";
    return Json::writeString(writer, value);
}

bool
hasFields(const Json::Value& value, std::initializer_list<const char*> fields)
{
    if (!value.isObject() || value.size() != fields.size())
        return false;
    return std::all_of(fields.begin(), fields.end(), [&](const char* field) { return value.isMember(field); });
}

bool
isHash(const Json::Value& value, size_t length = 40)
{
    if (!value.isString())
        return false;
    const auto text = value.asString();
    return text.size() == length && std::all_of(text.begin(), text.end(), [](char character) {
               return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
           });
}

bool
isRoleId(const Json::Value& value)
{
    if (!value.isString())
        return false;
    const auto text = value.asString();
    return !text.empty() && text.size() <= CircleRecord::MAX_ROLE_ID_SIZE && text != "." && text != ".."
           && std::all_of(text.begin(), text.end(), [](char character) {
                  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')
                         || (character >= '0' && character <= '9') || character == '-' || character == '_'
                         || character == '.';
              });
}

template<typename Set, typename Parse>
bool
readSet(const Json::Value& value, Set& result, size_t limit, Parse parse)
{
    if (!value.isArray() || value.size() > limit)
        return false;
    for (const auto& entry : value) {
        if (!entry.isString())
            return false;
        auto parsed = parse(entry);
        if (!parsed || !result.insert(std::move(*parsed)).second)
            return false;
    }
    return true;
}

bool
readPermissions(const Json::Value& value, PermissionSet& result)
{
    return readSet(value, result, static_cast<size_t>(Permission::Count), [](const Json::Value& entry) {
        return permissionFromName(entry.asString());
    });
}

bool
readIds(const Json::Value& value, std::set<std::string, std::less<>>& result, bool hashes)
{
    return readSet(value,
                   result,
                   CircleRecord::MAX_SCOPE_SIZE,
                   [&](const Json::Value& entry) -> std::optional<std::string> {
                       if (hashes ? isHash(entry) : isRoleId(entry))
                           return entry.asString();
                       return std::nullopt;
                   });
}

std::optional<PermissionRule>
readRule(const Json::Value& value)
{
    PermissionRule rule;
    if (!hasFields(value, {"allow", "deny"}) || !readPermissions(value["allow"], rule.allow)
        || !readPermissions(value["deny"], rule.deny))
        return std::nullopt;
    return rule;
}

std::optional<RoleDelegation>
readDelegation(const Json::Value& value)
{
    RoleDelegation result;
    if (!hasFields(value, {"permissions", "editable-roles", "assignable-roles", "channels", "circle-wide"})
        || !readPermissions(value["permissions"], result.permissions)
        || !readIds(value["editable-roles"], result.editableRoles, false)
        || !readIds(value["assignable-roles"], result.assignableRoles, false)
        || !readIds(value["channels"], result.channels, true) || !value["circle-wide"].isBool())
        return std::nullopt;
    result.circleWide = value["circle-wide"].asBool();
    return result;
}

std::optional<CirclePolicyChange> readOperation(const Json::Value& value);

std::optional<ChannelPolicy>
readChannelPolicy(const Json::Value& value)
{
    if (!hasFields(value, {"defaults", "roles"}) || !value["roles"].isObject()
        || value["roles"].size() > CircleRecord::MAX_SCOPE_SIZE)
        return std::nullopt;
    ChannelPolicy result;
    if (!value["defaults"].isNull()) {
        result.defaults = readRule(value["defaults"]);
        if (!result.defaults)
            return std::nullopt;
    }
    for (const auto& role : value["roles"].getMemberNames()) {
        const auto rule = readRule(value["roles"][role]);
        if (!isRoleId(Json::Value(role)) || !rule)
            return std::nullopt;
        result.roles.emplace(role, *rule);
    }
    return result;
}

std::optional<CirclePolicyChange>
readOperation(const Json::Value& value)
{
    if (!value.isObject() || !value["type"].isString())
        return std::nullopt;
    auto type = value["type"].asString();
    if (type == "set-defaults" && hasFields(value, {"type", "rule"})) {
        if (auto rule = readRule(value["rule"]))
            return SetCircleDefaults {*rule};
    } else if (type == "set-role" && hasFields(value, {"type", "role", "rule"}) && isRoleId(value["role"])) {
        if (auto rule = readRule(value["rule"]))
            return SetCircleRole {value["role"].asString(), *rule};
    } else if (type == "set-role-assignment" && hasFields(value, {"type", "member", "role", "assigned"})
               && isHash(value["member"]) && isRoleId(value["role"]) && value["assigned"].isBool()) {
        return SetCircleRoleAssignment {value["member"].asString(),
                                        value["role"].asString(),
                                        value["assigned"].asBool()};
    } else if (type == "set-channel-rule" && hasFields(value, {"type", "channel", "role", "rule"})
               && isHash(value["channel"])) {
        SetCircleChannelRule operation {value["channel"].asString(), std::nullopt, std::nullopt};
        if (!value["role"].isNull()) {
            if (!isRoleId(value["role"]))
                return std::nullopt;
            operation.role = value["role"].asString();
        }
        if (!value["rule"].isNull()) {
            operation.rule = readRule(value["rule"]);
            if (!operation.rule)
                return std::nullopt;
        }
        return operation;
    } else if (type == "set-role-delegation" && hasFields(value, {"type", "role", "delegation"})
               && isRoleId(value["role"])) {
        SetCircleRoleDelegation operation {value["role"].asString(), std::nullopt};
        if (!value["delegation"].isNull()) {
            operation.delegation = readDelegation(value["delegation"]);
            if (!operation.delegation)
                return std::nullopt;
        }
        return operation;
    } else if (hasFields(value, {"type", "member"}) && isHash(value["member"])) {
        if (type == "invite-member")
            return InviteCircleMember {value["member"].asString()};
        if (type == "join-member")
            return JoinCircleMember {value["member"].asString()};
        if (type == "leave-member")
            return LeaveCircleMember {value["member"].asString()};
        if (type == "transfer-admin")
            return TransferCircleAdmin {value["member"].asString()};
    } else if (type == "register-device" && hasFields(value, {"type", "member", "device"}) && isHash(value["member"])
               && isHash(value["device"], 64)) {
        return RegisterCircleDevice {value["member"].asString(), value["device"].asString()};
    } else if (type == "register-channel" && hasFields(value, {"type", "channel", "authorization", "policy", "genesis"})
               && isHash(value["channel"]) && isHash(value["authorization"]) && value["genesis"].isString()) {
        RegisterCircleChannel operation {value["channel"].asString(),
                                         value["authorization"].asString(),
                                         {},
                                         value["genesis"].asString()};
        if (operation.genesis.empty() || operation.genesis.size() > CircleRecord::MAX_CHANNEL_PROOF_SIZE
            || operation.genesis.find('\0') != std::string::npos)
            return std::nullopt;
        const auto policy = readChannelPolicy(value["policy"]);
        if (!policy)
            return std::nullopt;
        operation.policy = *policy;
        return operation;
    } else if (type == "archive-channel" && hasFields(value, {"type", "channel"}) && isHash(value["channel"])) {
        return ArchiveCircleChannel {value["channel"].asString()};
    } else if (type == "end-circle" && hasFields(value, {"type"})) {
        return EndCircle {};
    } else if (type == "set-profile" && hasFields(value, {"type", "channel"})
               && (value["channel"].isNull() || isHash(value["channel"]))) {
        return SetCircleProfile {value["channel"].isNull() ? std::nullopt
                                                           : std::optional<std::string>(value["channel"].asString())};
    }
    return std::nullopt;
}

std::optional<CircleRecord>
readRecord(const Json::Value& value)
{
    if (hasFields(value, {"type", "version", "kind", "circle", "policy", "channel", "parent", "message"})
        && value["type"] == "jami-circle-channel" && value["kind"] == "content" && value["version"].isUInt()
        && value["version"].type() != Json::realValue && value["version"].asUInt() == CircleRecord::VERSION
        && isHash(value["circle"]) && isHash(value["policy"]) && isHash(value["channel"]) && isHash(value["parent"])) {
        const auto& message = value["message"];
        if (message[CommitKey::TYPE] != CommitType::TEXT || !message[CommitKey::BODY].isString())
            return std::nullopt;
        const auto body = message[CommitKey::BODY].asString();
        if (body.size() > CircleRecord::MAX_CHANNEL_BODY_SIZE || body.find('\0') != std::string::npos)
            return std::nullopt;
        CommitMessage parsed = CommitMessage::text(body);
        if (hasFields(message, {CommitKey::TYPE, CommitKey::BODY, CommitKey::REPLY_TO})
            && isHash(message[CommitKey::REPLY_TO]))
            parsed.replyTo = message[CommitKey::REPLY_TO].asString();
        else if (hasFields(message, {CommitKey::TYPE, CommitKey::BODY, CommitKey::REACT_TO})
                 && isHash(message[CommitKey::REACT_TO]))
            parsed.reactTo = message[CommitKey::REACT_TO].asString();
        else if (hasFields(message, {CommitKey::TYPE, CommitKey::BODY, CommitKey::EDIT})
                 && isHash(message[CommitKey::EDIT]))
            parsed.editedId = message[CommitKey::EDIT].asString();
        else if (!hasFields(message, {CommitKey::TYPE, CommitKey::BODY}))
            return std::nullopt;
        if (body.empty() && parsed.editedId.empty())
            return std::nullopt;
        return CircleRecord {CircleChannelContent {value["channel"].asString(),
                                                   value["parent"].asString(),
                                                   std::move(parsed)},
                             value["circle"].asString(),
                             value["policy"].asString()};
    }
    if (hasFields(value, {"type", "version", "kind", "circle", "policy", "channel", "parent", "other"})
        && value["type"] == "jami-circle-channel" && value["kind"] == "merge" && value["version"].isUInt()
        && value["version"].type() != Json::realValue && value["version"].asUInt() == CircleRecord::VERSION
        && isHash(value["circle"]) && isHash(value["policy"]) && isHash(value["channel"]) && isHash(value["parent"])
        && isHash(value["other"]) && value["parent"] != value["other"]) {
        return CircleRecord {CircleChannelMerge {value["channel"].asString(),
                                                 value["parent"].asString(),
                                                 value["other"].asString()},
                             value["circle"].asString(),
                             value["policy"].asString()};
    }
    if (hasFields(value, {"type", "version", "kind", "circle", "policy", "creator", "nonce", "access"})
        && value["type"] == "jami-circle-channel" && value["kind"] == "genesis" && value["version"].isUInt()
        && value["version"].type() != Json::realValue && value["version"].asUInt() == CircleRecord::VERSION
        && isHash(value["circle"]) && isHash(value["policy"]) && isHash(value["creator"])
        && isHash(value["nonce"], 64)) {
        const auto access = readChannelPolicy(value["access"]);
        if (!access)
            return std::nullopt;
        return CircleRecord {CircleChannelGenesis {value["creator"].asString(), value["nonce"].asString(), *access},
                             value["circle"].asString(),
                             value["policy"].asString()};
    }
    if (!value.isObject() || value["type"] != "jami-circle" || !value["version"].isUInt()
        || value["version"].type() == Json::realValue || value["version"].asUInt() != CircleRecord::VERSION
        || !value["kind"].isString())
        return std::nullopt;
    if (value["kind"] == "genesis" && hasFields(value, {"type", "version", "kind", "admin", "defaults", "nonce"})
        && isHash(value["admin"]) && isHash(value["nonce"], 64)) {
        if (auto rule = readRule(value["defaults"]))
            return CircleRecord {CircleGenesis {value["admin"].asString(), *rule, value["nonce"].asString()}, {}, {}};
    } else if (value["kind"] == "update" && hasFields(value, {"type", "version", "kind", "circle", "base", "operation"})
               && isHash(value["circle"]) && isHash(value["base"])) {
        if (auto operation = readOperation(value["operation"]))
            return CircleRecord {*operation, value["circle"].asString(), value["base"].asString()};
    } else if (value["kind"] == "vote"
               && hasFields(value, {"type", "version", "kind", "circle", "base", "action", "member"})
               && isHash(value["circle"]) && isHash(value["base"]) && isHash(value["member"])
               && value["action"].isString()) {
        if (value["action"] == "ban-member")
            return CircleRecord {CircleModerationVote {CircleModerationAction::BAN_MEMBER, value["member"].asString()},
                                 value["circle"].asString(),
                                 value["base"].asString()};
        if (value["action"] == "unban-member")
            return CircleRecord {CircleModerationVote {CircleModerationAction::UNBAN_MEMBER, value["member"].asString()},
                                 value["circle"].asString(),
                                 value["base"].asString()};
    } else if (value["kind"] == "resolution" && hasFields(value, {"type", "version", "kind", "circle", "base", "vote"})
               && isHash(value["circle"]) && isHash(value["base"]) && isHash(value["vote"])) {
        return CircleRecord {CircleModerationResolution {value["vote"].asString()},
                             value["circle"].asString(),
                             value["base"].asString()};
    } else if (value["kind"] == "merge" && hasFields(value, {"type", "version", "kind", "circle", "base", "other"})
               && isHash(value["circle"]) && isHash(value["base"]) && isHash(value["other"])
               && value["base"] != value["other"]) {
        return CircleRecord {CirclePolicyMerge {value["other"].asString()},
                             value["circle"].asString(),
                             value["base"].asString()};
    }
    return std::nullopt;
}

template<typename Set>
Json::Value
writeSet(const Set& values)
{
    std::set<std::string> names;
    for (const auto& value : values) {
        if constexpr (std::is_same_v<typename Set::value_type, Permission>)
            names.emplace(permissionWireName(value));
        else
            names.emplace(value);
    }
    Json::Value result(Json::arrayValue);
    for (const auto& name : names)
        result.append(name);
    return result;
}

Json::Value
writeRule(const PermissionRule& rule)
{
    Json::Value value(Json::objectValue);
    value["allow"] = writeSet(rule.allow);
    value["deny"] = writeSet(rule.deny);
    return value;
}

Json::Value
writeDelegation(const RoleDelegation& delegation)
{
    Json::Value value(Json::objectValue);
    value["permissions"] = writeSet(delegation.permissions);
    value["editable-roles"] = writeSet(delegation.editableRoles);
    value["assignable-roles"] = writeSet(delegation.assignableRoles);
    value["channels"] = writeSet(delegation.channels);
    value["circle-wide"] = delegation.circleWide;
    return value;
}

Json::Value
writeChannelPolicy(const ChannelPolicy& policy)
{
    Json::Value value(Json::objectValue);
    value["defaults"] = policy.defaults ? writeRule(*policy.defaults) : Json::Value(Json::nullValue);
    value["roles"] = Json::Value(Json::objectValue);
    for (const auto& [role, rule] : policy.roles)
        value["roles"][role] = writeRule(rule);
    return value;
}

Json::Value
writeOperation(const CirclePolicyChange& change)
{
    return std::visit(
        [](const auto& operation) {
            using Operation = std::decay_t<decltype(operation)>;
            Json::Value value(Json::objectValue);
            if constexpr (std::is_same_v<Operation, SetCircleDefaults>) {
                value["type"] = "set-defaults";
                value["rule"] = writeRule(operation.rule);
            } else if constexpr (std::is_same_v<Operation, SetCircleRole>) {
                value["type"] = "set-role";
                value["role"] = operation.role;
                value["rule"] = writeRule(operation.rule);
            } else if constexpr (std::is_same_v<Operation, SetCircleRoleAssignment>) {
                value["type"] = "set-role-assignment";
                value["member"] = operation.member;
                value["role"] = operation.role;
                value["assigned"] = operation.assigned;
            } else if constexpr (std::is_same_v<Operation, SetCircleChannelRule>) {
                value["type"] = "set-channel-rule";
                value["channel"] = operation.channel;
                value["role"] = operation.role ? Json::Value(*operation.role) : Json::Value(Json::nullValue);
                value["rule"] = operation.rule ? writeRule(*operation.rule) : Json::Value(Json::nullValue);
            } else if constexpr (std::is_same_v<Operation, SetCircleRoleDelegation>) {
                value["type"] = "set-role-delegation";
                value["role"] = operation.role;
                value["delegation"] = operation.delegation ? writeDelegation(*operation.delegation)
                                                           : Json::Value(Json::nullValue);
            } else if constexpr (std::is_same_v<Operation, InviteCircleMember>) {
                value["type"] = "invite-member";
                value["member"] = operation.member;
            } else if constexpr (std::is_same_v<Operation, JoinCircleMember>) {
                value["type"] = "join-member";
                value["member"] = operation.member;
            } else if constexpr (std::is_same_v<Operation, LeaveCircleMember>) {
                value["type"] = "leave-member";
                value["member"] = operation.member;
            } else if constexpr (std::is_same_v<Operation, RegisterCircleDevice>) {
                value["type"] = "register-device";
                value["member"] = operation.member;
                value["device"] = operation.device;
            } else if constexpr (std::is_same_v<Operation, RegisterCircleChannel>) {
                if (operation.policy.archived || !operation.policy.authorization.empty())
                    return Json::Value();
                value["type"] = "register-channel";
                value["channel"] = operation.channel;
                value["authorization"] = operation.authorization;
                value["genesis"] = operation.genesis;
                value["policy"] = writeChannelPolicy(operation.policy);
            } else if constexpr (std::is_same_v<Operation, ArchiveCircleChannel>) {
                value["type"] = "archive-channel";
                value["channel"] = operation.channel;
            } else if constexpr (std::is_same_v<Operation, TransferCircleAdmin>) {
                value["type"] = "transfer-admin";
                value["member"] = operation.member;
            } else if constexpr (std::is_same_v<Operation, EndCircle>) {
                value["type"] = "end-circle";
            } else if constexpr (std::is_same_v<Operation, SetCircleProfile>) {
                value["type"] = "set-profile";
                value["channel"] = operation.channel ? Json::Value(*operation.channel) : Json::Value(Json::nullValue);
            }
            return value;
        },
        change);
}

} // namespace

std::optional<std::string>
CircleRecord::toString() const
{
    Json::Value value(Json::objectValue);
    value["type"] = "jami-circle";
    value["version"] = VERSION;
    if (const auto* content = std::get_if<CircleChannelContent>(&payload)) {
        const auto& message = content->message;
        if (!message.action.empty() || !message.uri.empty() || !message.confId.empty() || !message.device.empty()
            || !message.duration.empty() || !message.reason.empty() || !message.to.empty() || !message.tid.empty()
            || !message.displayName.empty() || message.totalSize != -1 || !message.sha3sum.empty() || message.mode != -1
            || !message.invited.empty() || !message.mimeType.empty() || !message.parent.empty())
            return std::nullopt;
        value["type"] = "jami-circle-channel";
        value["kind"] = "content";
        value["circle"] = circle;
        value["policy"] = base;
        value["channel"] = content->channel;
        value["parent"] = content->parent;
        value["message"][CommitKey::TYPE] = message.type;
        value["message"][CommitKey::BODY] = message.body;
        if (!message.replyTo.empty())
            value["message"][CommitKey::REPLY_TO] = message.replyTo;
        if (!message.reactTo.empty())
            value["message"][CommitKey::REACT_TO] = message.reactTo;
        if (!message.editedId.empty())
            value["message"][CommitKey::EDIT] = message.editedId;
    } else if (const auto* merge = std::get_if<CircleChannelMerge>(&payload)) {
        value["type"] = "jami-circle-channel";
        value["kind"] = "merge";
        value["circle"] = circle;
        value["policy"] = base;
        value["channel"] = merge->channel;
        value["parent"] = merge->parent;
        value["other"] = merge->other;
    } else if (const auto* channel = std::get_if<CircleChannelGenesis>(&payload)) {
        if (channel->access.archived || !channel->access.authorization.empty())
            return std::nullopt;
        value["type"] = "jami-circle-channel";
        value["kind"] = "genesis";
        value["circle"] = circle;
        value["policy"] = base;
        value["creator"] = channel->creator;
        value["nonce"] = channel->nonce;
        value["access"] = writeChannelPolicy(channel->access);
    } else if (const auto* genesis = std::get_if<CircleGenesis>(&payload)) {
        if (!circle.empty() || !base.empty())
            return std::nullopt;
        value["kind"] = "genesis";
        value["admin"] = genesis->admin;
        value["defaults"] = writeRule(genesis->defaults);
        value["nonce"] = genesis->nonce;
    } else {
        value["circle"] = circle;
        value["base"] = base;
        if (const auto* change = std::get_if<CirclePolicyChange>(&payload)) {
            if (change->valueless_by_exception())
                return std::nullopt;
            value["kind"] = "update";
            value["operation"] = writeOperation(*change);
        } else if (const auto* vote = std::get_if<CircleModerationVote>(&payload)) {
            value["kind"] = "vote";
            value["action"] = vote->action == CircleModerationAction::BAN_MEMBER     ? "ban-member"
                              : vote->action == CircleModerationAction::UNBAN_MEMBER ? "unban-member"
                                                                                     : "";
            value["member"] = vote->member;
        } else if (const auto* resolution = std::get_if<CircleModerationResolution>(&payload)) {
            value["kind"] = "resolution";
            value["vote"] = resolution->vote;
        } else if (const auto* merge = std::get_if<CirclePolicyMerge>(&payload)) {
            value["kind"] = "merge";
            value["other"] = merge->other;
        } else {
            return std::nullopt;
        }
    }
    if (!readRecord(value))
        return std::nullopt;
    auto text = writeCompact(value);
    if (text.size() > MAX_SIZE)
        return std::nullopt;
    return text;
}

std::optional<CircleRecord>
CircleRecord::fromString(std::string_view text)
{
    const auto value = parseStrict(text, MAX_SIZE);
    try {
        return value ? readRecord(*value) : std::nullopt;
    } catch (const Json::Exception&) {
        return std::nullopt;
    }
}

std::string
circlePolicyToJson(const CirclePolicy& policy)
{
    Json::Value value(Json::objectValue);
    value["admin"] = policy.admin;
    value["ended"] = policy.ended;
    value["defaults"] = writeRule(policy.defaults);
    value["roles"] = Json::Value(Json::objectValue);
    for (const auto& [role, rule] : policy.roles)
        value["roles"][role] = writeRule(rule);
    value["members"] = Json::Value(Json::objectValue);
    for (const auto& [uri, member] : policy.members) {
        auto& entry = value["members"][uri];
        entry["state"] = member.state == CircleMemberState::INVITED  ? "invited"
                         : member.state == CircleMemberState::ACTIVE ? "active"
                         : member.state == CircleMemberState::LEFT   ? "left"
                                                                     : "banned";
        entry["roles"] = writeSet(member.roles);
    }
    value["channels"] = Json::Value(Json::objectValue);
    for (const auto& [id, channel] : policy.channels) {
        auto entry = writeChannelPolicy(channel);
        entry["archived"] = channel.archived;
        value["channels"][id] = std::move(entry);
    }
    value["delegations"] = Json::Value(Json::objectValue);
    for (const auto& [role, delegation] : policy.delegations)
        value["delegations"][role] = writeDelegation(delegation);
    return writeCompact(value);
}

std::optional<CirclePolicyChange>
circlePolicyChangeFromJson(std::string_view text)
{
    const auto value = parseStrict(text, CircleRecord::MAX_SIZE);
    try {
        return value ? readOperation(*value) : std::nullopt;
    } catch (const Json::Exception&) {
        return std::nullopt;
    }
}

std::optional<PermissionRule>
circleRuleFromJson(std::string_view text)
{
    const auto value = parseStrict(text, CircleRecord::MAX_SIZE);
    try {
        return value ? readRule(*value) : std::nullopt;
    } catch (const Json::Exception&) {
        return std::nullopt;
    }
}

std::optional<ChannelPolicy>
channelPolicyFromJson(std::string_view text)
{
    const auto value = parseStrict(text, CircleRecord::MAX_SIZE);
    try {
        return value ? readChannelPolicy(*value) : std::nullopt;
    } catch (const Json::Exception&) {
        return std::nullopt;
    }
}

std::optional<CirclePolicy>
CircleRecord::initialPolicy(std::string_view authenticatedActor) const
{
    const auto* genesis = std::get_if<CircleGenesis>(&payload);
    if (!genesis || authenticatedActor != genesis->admin || !toString())
        return std::nullopt;
    CirclePolicy policy;
    policy.admin = genesis->admin;
    policy.defaults = genesis->defaults;
    policy.members.emplace(genesis->admin, CircleMember {CircleMemberState::ACTIVE, {}});
    return policy;
}

std::optional<CirclePolicy>
CircleRecord::apply(const CirclePolicy& policy,
                    std::string_view authenticatedActor,
                    std::string_view circleId,
                    std::string_view baseCommit) const
{
    const auto* change = std::get_if<CirclePolicyChange>(&payload);
    if (!change || circle != circleId || base != baseCommit || !toString())
        return std::nullopt;
    return policy.apply(authenticatedActor, *change);
}

} // namespace jami
