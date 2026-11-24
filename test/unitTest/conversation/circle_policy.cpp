#include "jamidht/circle_policy.h"
#include "jamidht/circle_record.h"
#include "../../test_runner.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <json/json.h>

#include <array>
#include <sstream>
#include <utility>

namespace jami {
namespace test {

class CirclePolicyTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "CirclePolicy"; }

private:
    static CirclePolicy makePolicy();
    static CirclePolicy makeEditorPolicy();
    void testSnapshotValidation();
    void testInvalidSnapshotIdentity();
    void testInvalidSnapshotReferences();
    void testInvalidSnapshotPermissions();
    void testAdminPolicyChanges();
    void testDesignationIsSeparate();
    void testPolicyChangesDenied();
    void testInvalidDelegations();
    void testDelegatedRoleEdits();
    void testDelegatedChannelEdits();
    void testManagementRequiresScope();
    void testScopesNotCombined();
    void testIndirectGainsDenied();
    void testPreStateAuthority();
    void testDelegationRevocation();
    void testDefaultDeny();
    void testDefaultAndRoleGrants();
    void testDenyWins();
    void testChannelRuleReplacesRole();
    void testPrivateChannel();
    void testReadRequired();
    void testInactiveMembersDenied();
    void testUnknownReferencesDenied();
    void testAdminBypassesDenies();
    void testUnknownPermissionsDenied();
    void testRoleOrderIndependent();
    void testLargePolicy();
    void testConversationPermissionsUnchanged();
    void testPermissionWireNames();
    void testCircleGenesisRecord();
    void testCircleUpdateRecord();
    void testCircleRecordStrictParsing();
    void testCircleRecordOperations();
    void testCircleRecordSchema();
    void testCircleOperationSchema();
    void testCircleRecordLimits();
    void testCircleRecordCanonicalForm();
    void testCircleRecordReplay();
    void testMemberLifecycle();
    void testMembershipAuthority();
    void testMembershipRecords();
    void testAdminTransfer();
    void testCircleEnd();
    void testAdminLifecycleRecords();
    void testPolicyJson();
    void testMemberModeration();
    void testModerationAuthority();
    void testPeerModerationProtected();
    void testPeerRoleAssignments();
    void testPeerRoleEdits();
    void testPeerChannelEdits();
    void testPeerDelegationPreserved();
    void testChannelRegistration();
    void testChannelArchive();
    void testChannelRecords();
    void testChannelContentRecords();
    void testModerationRecords();
    void testPolicyMergeRecords();
    void testChannelMergeRecords();

    CPPUNIT_TEST_SUITE(CirclePolicyTest);
    CPPUNIT_TEST(testSnapshotValidation);
    CPPUNIT_TEST(testInvalidSnapshotIdentity);
    CPPUNIT_TEST(testInvalidSnapshotReferences);
    CPPUNIT_TEST(testInvalidSnapshotPermissions);
    CPPUNIT_TEST(testAdminPolicyChanges);
    CPPUNIT_TEST(testDesignationIsSeparate);
    CPPUNIT_TEST(testPolicyChangesDenied);
    CPPUNIT_TEST(testInvalidDelegations);
    CPPUNIT_TEST(testDelegatedRoleEdits);
    CPPUNIT_TEST(testDelegatedChannelEdits);
    CPPUNIT_TEST(testManagementRequiresScope);
    CPPUNIT_TEST(testScopesNotCombined);
    CPPUNIT_TEST(testIndirectGainsDenied);
    CPPUNIT_TEST(testPreStateAuthority);
    CPPUNIT_TEST(testDelegationRevocation);
    CPPUNIT_TEST(testDefaultDeny);
    CPPUNIT_TEST(testDefaultAndRoleGrants);
    CPPUNIT_TEST(testDenyWins);
    CPPUNIT_TEST(testChannelRuleReplacesRole);
    CPPUNIT_TEST(testPrivateChannel);
    CPPUNIT_TEST(testReadRequired);
    CPPUNIT_TEST(testInactiveMembersDenied);
    CPPUNIT_TEST(testUnknownReferencesDenied);
    CPPUNIT_TEST(testAdminBypassesDenies);
    CPPUNIT_TEST(testUnknownPermissionsDenied);
    CPPUNIT_TEST(testRoleOrderIndependent);
    CPPUNIT_TEST(testLargePolicy);
    CPPUNIT_TEST(testConversationPermissionsUnchanged);
    CPPUNIT_TEST(testPermissionWireNames);
    CPPUNIT_TEST(testCircleGenesisRecord);
    CPPUNIT_TEST(testCircleUpdateRecord);
    CPPUNIT_TEST(testCircleRecordStrictParsing);
    CPPUNIT_TEST(testCircleRecordOperations);
    CPPUNIT_TEST(testCircleRecordSchema);
    CPPUNIT_TEST(testCircleOperationSchema);
    CPPUNIT_TEST(testCircleRecordLimits);
    CPPUNIT_TEST(testCircleRecordCanonicalForm);
    CPPUNIT_TEST(testCircleRecordReplay);
    CPPUNIT_TEST(testMemberLifecycle);
    CPPUNIT_TEST(testMembershipAuthority);
    CPPUNIT_TEST(testMembershipRecords);
    CPPUNIT_TEST(testAdminTransfer);
    CPPUNIT_TEST(testCircleEnd);
    CPPUNIT_TEST(testAdminLifecycleRecords);
    CPPUNIT_TEST(testPolicyJson);
    CPPUNIT_TEST(testMemberModeration);
    CPPUNIT_TEST(testModerationAuthority);
    CPPUNIT_TEST(testPeerModerationProtected);
    CPPUNIT_TEST(testPeerRoleAssignments);
    CPPUNIT_TEST(testPeerRoleEdits);
    CPPUNIT_TEST(testPeerChannelEdits);
    CPPUNIT_TEST(testPeerDelegationPreserved);
    CPPUNIT_TEST(testChannelRegistration);
    CPPUNIT_TEST(testChannelArchive);
    CPPUNIT_TEST(testChannelRecords);
    CPPUNIT_TEST(testChannelContentRecords);
    CPPUNIT_TEST(testModerationRecords);
    CPPUNIT_TEST(testPolicyMergeRecords);
    CPPUNIT_TEST(testChannelMergeRecords);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CirclePolicyTest, CirclePolicyTest::name());

CirclePolicy
CirclePolicyTest::makePolicy()
{
    CirclePolicy policy;
    policy.admin = "alice";
    policy.members.emplace("alice", CircleMember {CircleMemberState::ACTIVE, {}});
    policy.members.emplace("bob", CircleMember {CircleMemberState::ACTIVE, {}});
    policy.members.emplace("carol", CircleMember {CircleMemberState::ACTIVE, {}});
    policy.channels.emplace("general", ChannelPolicy {});
    return policy;
}

void
CirclePolicyTest::testSnapshotValidation()
{
    auto policy = makePolicy();
    CPPUNIT_ASSERT(policy.isValid());
    policy.roles["writer"].allow = {Permission::ReadChannel, Permission::Text};
    policy.members.at("bob").roles = {"writer"};
    policy.channels.at("general").roles["writer"].deny = {Permission::Text};
    policy.channels.at("general").defaults = PermissionRule {};
    for (auto state :
         {CircleMemberState::INVITED, CircleMemberState::ACTIVE, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy.members.at("bob").state = state;
        CPPUNIT_ASSERT(policy.isValid());
    }
}

void
CirclePolicyTest::testInvalidSnapshotIdentity()
{
    const auto valid = makePolicy();
    auto policy = valid;
    policy.admin.clear();
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.members.erase("alice");
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.members.at("alice").state = CircleMemberState::LEFT;
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.members.at("bob").state = static_cast<CircleMemberState>(999);
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.members.emplace("", CircleMember {});
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.roles.emplace("", PermissionRule {});
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.channels.emplace("", ChannelPolicy {});
    CPPUNIT_ASSERT(!policy.isValid());
}

void
CirclePolicyTest::testInvalidSnapshotReferences()
{
    auto policy = makePolicy();
    policy.members.at("alice").roles = {"unknown"};
    CPPUNIT_ASSERT(!policy.isValid());
    policy.members.at("alice").roles.clear();
    policy.channels.at("general").roles["unknown"] = {};
    CPPUNIT_ASSERT(!policy.isValid());
    policy.roles["unknown"] = {};
    CPPUNIT_ASSERT(policy.isValid());
}

void
CirclePolicyTest::testInvalidSnapshotPermissions()
{
    const auto valid = makePolicy();
    auto policy = valid;
    policy.defaults.deny = {Permission::Count};
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.roles["unused"].allow = {static_cast<Permission>(999)};
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.roles["unused"].deny = {static_cast<Permission>(-1)};
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.channels.at("general").defaults = PermissionRule {{Permission::Count}, {}};
    CPPUNIT_ASSERT(!policy.isValid());
    policy = valid;
    policy.roles["unused"] = {};
    policy.channels.at("general").roles["unused"].deny = {Permission::Count};
    CPPUNIT_ASSERT(!policy.isValid());
}

void
CirclePolicyTest::testAdminPolicyChanges()
{
    const auto policy = makePolicy();
    auto next = policy.apply("alice", SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}});
    CPPUNIT_ASSERT(next && next->isValid());
    CPPUNIT_ASSERT(!policy.roles.contains("moderator"));
    next = next->apply("alice", SetCircleRoleAssignment {"bob", "moderator"});
    CPPUNIT_ASSERT(next && next->hasPermission("bob", Permission::BanUnbanMember));
    next = next->apply("alice", SetCircleDefaults {{{Permission::ReadChannel}, {}}});
    CPPUNIT_ASSERT(next && next->hasPermission("bob", Permission::ReadChannel, "general"));
    next = next->apply("alice", SetCircleChannelRule {"general", std::nullopt, PermissionRule {}});
    CPPUNIT_ASSERT(next && next->permissions("bob", "general").empty());
    next = next->apply("alice", SetCircleChannelRule {"general", std::nullopt, std::nullopt});
    CPPUNIT_ASSERT(next && next->hasPermission("bob", Permission::ReadChannel, "general"));
}

void
CirclePolicyTest::testDesignationIsSeparate()
{
    auto policy = makePolicy();
    policy.roles["moderator"].allow = {Permission::BanUnbanMember};
    policy.roles["designator"].allow = {Permission::AssignRoles};
    policy.members.at("bob").roles = {"moderator"};
    policy.members.at("carol").roles = {"designator"};
    auto approved = policy.apply("alice",
                                 SetCircleRoleDelegation {"designator",
                                                          RoleDelegation {{}, {}, {"moderator"}, {}, true}});
    CPPUNIT_ASSERT(approved);
    CPPUNIT_ASSERT(!approved->apply("bob", SetCircleRoleAssignment {"carol", "moderator"}));
    CPPUNIT_ASSERT(!approved->hasPermission("carol", Permission::BanUnbanMember));
    auto assigned = approved->apply("carol", SetCircleRoleAssignment {"carol", "moderator"});
    CPPUNIT_ASSERT(assigned && assigned->hasPermission("carol", Permission::BanUnbanMember));
    CPPUNIT_ASSERT(!approved->apply("carol", SetCircleRoleAssignment {"bob", "designator"}));
    CPPUNIT_ASSERT(!approved->apply("carol", SetCircleRole {"moderator", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(!assigned->apply("carol", SetCircleRoleAssignment {"bob", "moderator", false}));
    auto removed = assigned->apply("alice", SetCircleRoleAssignment {"bob", "moderator", false});
    CPPUNIT_ASSERT(removed && !removed->hasPermission("bob", Permission::BanUnbanMember));
}

void
CirclePolicyTest::testPolicyChangesDenied()
{
    const auto policy = makePolicy();
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleDefaults {{{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(!policy.apply("unknown", SetCircleRole {"role", {}}));
    CPPUNIT_ASSERT(!policy.apply("alice", SetCircleRole {"", {}}));
    CPPUNIT_ASSERT(!policy.apply("alice", SetCircleRole {"invalid", {{Permission::Count}, {}}}));
    CPPUNIT_ASSERT(!policy.apply("alice", SetCircleRoleAssignment {"bob", "unknown"}));
    CPPUNIT_ASSERT(!policy.apply("alice", SetCircleChannelRule {"unknown", std::nullopt, PermissionRule {}}));
    CPPUNIT_ASSERT(!policy.apply("alice", SetCircleRoleDelegation {"unknown", RoleDelegation {}}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRoleDelegation {"role", RoleDelegation {}}));
    auto invalid = policy;
    invalid.members.at("alice").state = CircleMemberState::LEFT;
    CPPUNIT_ASSERT(!invalid.apply("bob", SetCircleRole {"role", {}}));
    CPPUNIT_ASSERT(policy == makePolicy());
}

void
CirclePolicyTest::testInvalidDelegations()
{
    const auto valid = makeEditorPolicy();
    CPPUNIT_ASSERT(valid.isValid());
    auto invalid = valid;
    invalid.delegations["unknown"] = {};
    CPPUNIT_ASSERT(!invalid.isValid());
    invalid = valid;
    invalid.delegations.at("editor").permissions.insert(Permission::Count);
    CPPUNIT_ASSERT(!invalid.isValid());
    invalid = valid;
    invalid.delegations.at("editor").editableRoles.insert("unknown");
    CPPUNIT_ASSERT(!invalid.isValid());
    invalid = valid;
    invalid.delegations.at("editor").assignableRoles.insert("unknown");
    CPPUNIT_ASSERT(!invalid.isValid());
    invalid = valid;
    invalid.delegations.at("editor").channels.insert("unknown");
    CPPUNIT_ASSERT(!invalid.isValid());
    CPPUNIT_ASSERT(!valid.apply("alice", SetCircleRoleDelegation {"unknown", std::nullopt}));
}

CirclePolicy
CirclePolicyTest::makeEditorPolicy()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel};
    policy.roles["writer"].allow = {Permission::Text};
    policy.roles["editor"].allow = {Permission::ManageRoles};
    policy.members.at("bob").roles = {"editor"};
    policy.members.at("carol").roles = {"writer"};
    policy.channels.emplace("private", ChannelPolicy {});
    policy.delegations["editor"] = {{Permission::Text, Permission::File}, {"writer"}, {}, {"general"}, true};
    return policy;
}

void
CirclePolicyTest::testDelegatedRoleEdits()
{
    const auto policy = makeEditorPolicy();
    auto next = policy.apply("bob", SetCircleRole {"writer", {{Permission::Text, Permission::File}, {}}});
    CPPUNIT_ASSERT(next && next->hasPermission("carol", Permission::File, "private"));
    CPPUNIT_ASSERT(!policy.hasPermission("carol", Permission::File, "private"));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"writer", {{Permission::Reaction}, {}}}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"editor", {{Permission::AssignRoles}, {}}}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"new-role", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleDefaults {policy.defaults}));
    CPPUNIT_ASSERT(policy == makeEditorPolicy());
}

void
CirclePolicyTest::testDelegatedChannelEdits()
{
    const auto policy = makeEditorPolicy();
    auto next = policy.apply("bob", SetCircleChannelRule {"general", "writer", PermissionRule {{Permission::File}, {}}});
    CPPUNIT_ASSERT(next && next->hasPermission("carol", Permission::File, "general"));
    CPPUNIT_ASSERT(!next->hasPermission("carol", Permission::Text, "general"));
    CPPUNIT_ASSERT(next->hasPermission("carol", Permission::Text, "private"));
    next = next->apply("bob", SetCircleChannelRule {"general", "writer", std::nullopt});
    CPPUNIT_ASSERT(next && next->hasPermission("carol", Permission::Text, "general"));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"private", "writer", PermissionRule {}}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", "editor", PermissionRule {}}));
    auto defaults = policy.defaults;
    defaults.allow.insert(Permission::File);
    next = policy.apply("bob", SetCircleChannelRule {"general", std::nullopt, defaults});
    CPPUNIT_ASSERT(next && next->hasPermission("carol", Permission::File, "general"));
    CPPUNIT_ASSERT(!next->hasPermission("carol", Permission::File, "private"));
}

void
CirclePolicyTest::testManagementRequiresScope()
{
    const CirclePolicyChange edit = SetCircleRole {"writer", {{Permission::Text, Permission::File}, {}}};
    auto policy = makeEditorPolicy();
    policy.delegations.clear();
    CPPUNIT_ASSERT(!policy.apply("bob", edit));
    policy = makeEditorPolicy();
    policy.roles.at("editor").allow.clear();
    CPPUNIT_ASSERT(!policy.apply("bob", edit));
    policy = makeEditorPolicy();
    policy.delegations.at("editor").circleWide = false;
    CPPUNIT_ASSERT(!policy.apply("bob", edit));
    CPPUNIT_ASSERT(policy.apply("bob", SetCircleChannelRule {"general", "writer", PermissionRule {}}));
    for (auto state : {CircleMemberState::INVITED, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy = makeEditorPolicy();
        policy.members.at("bob").state = state;
        CPPUNIT_ASSERT(!policy.apply("bob", edit));
    }
}

void
CirclePolicyTest::testScopesNotCombined()
{
    auto policy = makeEditorPolicy();
    policy.delegations.at("editor").permissions = {Permission::Text};
    policy.roles["channel-editor"].allow = {Permission::ManageRoles};
    policy.members.at("bob").roles.insert("channel-editor");
    policy.delegations["channel-editor"] = {{Permission::Text, Permission::File}, {"writer"}, {}, {"private"}, false};
    const PermissionRule files {{Permission::Text, Permission::File}, {}};
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"writer", files}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", "writer", files}));
    CPPUNIT_ASSERT(policy.apply("bob", SetCircleChannelRule {"private", "writer", files}));
}

void
CirclePolicyTest::testIndirectGainsDenied()
{
    auto policy = makeEditorPolicy();
    policy.roles.at("writer").deny = {Permission::ReadChannel};
    policy.delegations.at("editor").permissions = {Permission::ReadChannel};
    const PermissionRule unlocked {{Permission::Text}, {}};
    CPPUNIT_ASSERT(policy.permissions("carol", "general").empty());
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"writer", unlocked}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", "writer", unlocked}));
    for (auto state : {CircleMemberState::INVITED, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy.members.at("carol").state = state;
        CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"writer", unlocked}));
        CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", "writer", unlocked}));
    }
    policy.members.at("carol").state = CircleMemberState::ACTIVE;
    policy.delegations.at("editor").permissions.insert(Permission::Text);
    auto next = policy.apply("bob", SetCircleRole {"writer", unlocked});
    CPPUNIT_ASSERT(next && next->hasPermission("carol", Permission::Text, "general"));
    policy.roles.at("writer").deny.clear();
    policy.channels.at("general").roles["writer"] = {{Permission::Text}, {Permission::ReadChannel}};
    policy.delegations.at("editor").permissions = {Permission::ReadChannel};
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", "writer", std::nullopt}));
}

void
CirclePolicyTest::testPreStateAuthority()
{
    auto policy = makeEditorPolicy();
    policy.members.at("bob").roles.insert("writer");
    policy.roles.at("editor").allow.clear();
    policy.delegations.at("editor").permissions.insert(Permission::ManageRoles);
    const auto before = policy;
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRole {"writer", {{Permission::Text, Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(policy == before);
    policy.roles.at("editor").allow = {Permission::ManageRoles};
    policy.delegations.at("editor").editableRoles.insert("editor");
    auto next = policy.apply("bob", SetCircleRole {"editor", {}});
    CPPUNIT_ASSERT(next && !next->hasPermission("bob", Permission::ManageRoles));
    CPPUNIT_ASSERT(!next->apply("bob", SetCircleRole {"editor", policy.roles.at("editor")}));
}

void
CirclePolicyTest::testDelegationRevocation()
{
    const auto policy = makeEditorPolicy();
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRoleDelegation {"editor", std::nullopt}));
    auto next = policy.apply("alice", SetCircleRoleDelegation {"editor", std::nullopt});
    CPPUNIT_ASSERT(next && next->hasPermission("bob", Permission::ManageRoles));
    CPPUNIT_ASSERT(!next->apply("bob", SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(!next->apply("bob", SetCircleChannelRule {"general", "writer", PermissionRule {}}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRoleDelegation {"editor", policy.delegations.at("editor")}));
}

void
CirclePolicyTest::testPermissionWireNames()
{
    CPPUNIT_ASSERT(permissionWireName(Permission::ReadChannel) == "read-channel");
    CPPUNIT_ASSERT(permissionFromName("assign-roles") == Permission::AssignRoles);
    for (int value = 0; value < static_cast<int>(Permission::Count); ++value) {
        auto permission = static_cast<Permission>(value);
        CPPUNIT_ASSERT(!permissionWireName(permission).empty());
        CPPUNIT_ASSERT(permissionFromName(permissionWireName(permission)) == permission);
    }
    CPPUNIT_ASSERT(permissionWireName(Permission::Count).empty());
    CPPUNIT_ASSERT(permissionWireName(static_cast<Permission>(-1)).empty());
    for (auto name : {"", "Text", "0", "count", "unknown"})
        CPPUNIT_ASSERT(!permissionFromName(name));
}

void
CirclePolicyTest::testCircleGenesisRecord()
{
    CircleRecord record {CircleGenesis {std::string(40, 'a'),
                                        {{Permission::ReadChannel, Permission::Text}, {}},
                                        std::string(64, 'd')},
                         {},
                         {}};
    const auto text = record.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded);
    const auto& genesis = std::get<CircleGenesis>(decoded->payload);
    CPPUNIT_ASSERT_EQUAL(std::string(40, 'a'), genesis.admin);
    CPPUNIT_ASSERT(genesis.defaults.allow == PermissionSet({Permission::ReadChannel, Permission::Text}));
    CPPUNIT_ASSERT(decoded->toString() == text);
    CPPUNIT_ASSERT(text->find("read-channel") != std::string::npos);
    record.circle = std::string(40, 'b');
    CPPUNIT_ASSERT(!record.toString());
}

void
CirclePolicyTest::testCircleUpdateRecord()
{
    CircleRecord record {CirclePolicyChange {SetCircleRole {"writer", {{Permission::Text}, {Permission::File}}}},
                         std::string(40, 'a'),
                         std::string(40, 'b')};
    const auto text = record.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded && decoded->toString() == text);
    const auto& change = std::get<CirclePolicyChange>(decoded->payload);
    const auto& role = std::get<SetCircleRole>(change);
    CPPUNIT_ASSERT_EQUAL(std::string("writer"), role.role);
    CPPUNIT_ASSERT(role.rule.allow == PermissionSet {Permission::Text});
    CPPUNIT_ASSERT(role.rule.deny == PermissionSet {Permission::File});
    record.base = "invalid";
    CPPUNIT_ASSERT(!record.toString());
}

void
CirclePolicyTest::testCircleRecordStrictParsing()
{
    CircleRecord record {CircleGenesis {std::string(40, 'a'), {}, std::string(64, 'd')}, {}, {}};
    const auto text = record.toString();
    CPPUNIT_ASSERT(text);
    for (const auto& malformed : {std::string(),
                                  std::string("null"),
                                  std::string("[]"),
                                  *text + "{}",
                                  "/*comment*/" + *text,
                                  text->substr(0, text->size() - 1) + ",\"version\":1}",
                                  text->substr(0, text->size() - 1) + ",\"actor\":\"someone\"}",
                                  *text + std::string("\0{}", 3)})
        CPPUNIT_ASSERT(!CircleRecord::fromString(malformed));
    CPPUNIT_ASSERT(!CircleRecord::fromString(std::string(CircleRecord::MAX_SIZE + 1, ' ')));
}

void
CirclePolicyTest::testCircleRecordOperations()
{
    const std::string id(40, 'a');
    const RoleDelegation delegation {{Permission::Text}, {"writer"}, {"moderator"}, {id}, true};
    const std::array<CirclePolicyChange, 10> changes {
        SetCircleDefaults {{{Permission::ReadChannel}, {}}},
        SetCircleRole {"writer", {{Permission::Text}, {Permission::File}}},
        SetCircleRoleAssignment {id, "moderator", true},
        SetCircleRoleAssignment {id, "moderator", false},
        SetCircleChannelRule {id, std::nullopt, PermissionRule {}},
        SetCircleChannelRule {id, std::nullopt, std::nullopt},
        SetCircleChannelRule {id, "writer", PermissionRule {{Permission::Text}, {}}},
        SetCircleChannelRule {id, "writer", std::nullopt},
        SetCircleRoleDelegation {"designator", delegation},
        SetCircleRoleDelegation {"designator", std::nullopt},
    };
    for (const auto& change : changes) {
        const CircleRecord record {change, id, id};
        const auto text = record.toString();
        CPPUNIT_ASSERT(text);
        const auto decoded = CircleRecord::fromString(*text);
        CPPUNIT_ASSERT(decoded && decoded->toString() == text);
        CPPUNIT_ASSERT(std::get<CirclePolicyChange>(decoded->payload).index() == change.index());
    }
    const auto empty = CircleRecord {changes[4], id, id}.toString();
    const auto inherit = CircleRecord {changes[5], id, id}.toString();
    CPPUNIT_ASSERT(empty != inherit);
    const auto assign = CircleRecord::fromString(*CircleRecord {changes[3], id, id}.toString());
    CPPUNIT_ASSERT(!std::get<SetCircleRoleAssignment>(std::get<CirclePolicyChange>(assign->payload)).assigned);
}

void
CirclePolicyTest::testCircleRecordSchema()
{
    const auto text = CircleRecord {CircleGenesis {std::string(40, 'a'), {}, std::string(64, 'd')}, {}, {}}.toString();
    CPPUNIT_ASSERT(text);
    Json::Value valid;
    std::istringstream(*text) >> valid;
    Json::StreamWriterBuilder writer;
    auto reject = [&](const Json::Value& value) {
        CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(writer, value)));
    };
    for (const auto& version :
         {Json::Value(2), Json::Value(-1), Json::Value(1.0), Json::Value("1"), Json::Value(true), Json::Value()}) {
        auto invalid = valid;
        invalid["version"] = version;
        reject(invalid);
    }
    for (auto field : {"type", "version", "kind", "admin", "defaults", "nonce"}) {
        auto invalid = valid;
        invalid.removeMember(field);
        reject(invalid);
    }
    for (auto hash : {std::string(39, 'a'), std::string(41, 'a'), std::string(40, 'A'), std::string(40, 'g')}) {
        auto invalid = valid;
        invalid["admin"] = hash;
        reject(invalid);
    }
    for (auto nonce : {Json::Value(),
                       Json::Value(1),
                       Json::Value(std::string(63, 'a')),
                       Json::Value(std::string(65, 'a')),
                       Json::Value(std::string(64, 'A'))}) {
        auto invalid = valid;
        invalid["nonce"] = nonce;
        reject(invalid);
    }
    auto invalid = valid;
    invalid["kind"] = "future";
    reject(invalid);
    invalid = valid;
    invalid["defaults"]["allow"].append("future-permission");
    reject(invalid);
    invalid = valid;
    invalid["defaults"]["allow"].append("text");
    invalid["defaults"]["allow"].append("text");
    reject(invalid);
    invalid = valid;
    invalid["defaults"]["allow"].append(0);
    reject(invalid);
    invalid = valid;
    invalid["defaults"]["deny"] = "text";
    reject(invalid);
    invalid = valid;
    invalid["defaults"]["unknown"] = true;
    reject(invalid);
}

void
CirclePolicyTest::testCircleOperationSchema()
{
    const std::string id(40, 'a');
    const auto text = CircleRecord {CirclePolicyChange {SetCircleRoleAssignment {id, "writer"}}, id, id}.toString();
    Json::Value valid;
    std::istringstream(*text) >> valid;
    Json::StreamWriterBuilder writer;
    auto reject = [&](const Json::Value& value) {
        CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(writer, value)));
    };
    for (auto field : {"circle", "base", "operation"}) {
        auto invalid = valid;
        invalid.removeMember(field);
        reject(invalid);
    }
    for (auto field : {"type", "member", "role", "assigned"}) {
        auto invalid = valid;
        invalid["operation"].removeMember(field);
        reject(invalid);
    }
    auto invalid = valid;
    invalid["operation"]["assigned"] = 1;
    reject(invalid);
    invalid = valid;
    invalid["operation"]["type"] = "future-operation";
    reject(invalid);
    invalid = valid;
    invalid["operation"]["actor"] = id;
    reject(invalid);
    for (auto role : {std::string(),
                      std::string("."),
                      std::string(".."),
                      std::string("../role"),
                      std::string("role/name"),
                      std::string("role name"),
                      std::string("role\0name", 9),
                      std::string(CircleRecord::MAX_ROLE_ID_SIZE + 1, 'r')}) {
        invalid = valid;
        invalid["operation"]["role"] = role;
        reject(invalid);
    }
    const auto delegation
        = CircleRecord {CirclePolicyChange {SetCircleRoleDelegation {"editor", RoleDelegation {}}}, id, id}.toString();
    std::istringstream(*delegation) >> valid;
    invalid = valid;
    invalid["operation"]["delegation"]["channels"].append("invalid");
    reject(invalid);
    invalid = valid;
    invalid["operation"]["delegation"]["editable-roles"].append("writer");
    invalid["operation"]["delegation"]["editable-roles"].append("writer");
    reject(invalid);
    invalid = valid;
    invalid["operation"]["delegation"]["circle-wide"] = "true";
    reject(invalid);
}

void
CirclePolicyTest::testCircleRecordLimits()
{
    const std::string id(40, 'a');
    RoleDelegation delegation;
    for (size_t index = 0; index < CircleRecord::MAX_SCOPE_SIZE; ++index)
        delegation.editableRoles.insert("role" + std::to_string(index));
    CircleRecord record {CirclePolicyChange {SetCircleRoleDelegation {"editor", delegation}}, id, id};
    auto text = record.toString();
    CPPUNIT_ASSERT(text && CircleRecord::fromString(*text));
    auto& change = std::get<CirclePolicyChange>(record.payload);
    std::get<SetCircleRoleDelegation>(change).delegation->editableRoles.insert("excess");
    CPPUNIT_ASSERT(!record.toString());
    record.payload = CirclePolicyChange {SetCircleRole {std::string(CircleRecord::MAX_ROLE_ID_SIZE, 'r'), {}}};
    CPPUNIT_ASSERT(record.toString());
    text = CircleRecord {CircleGenesis {id, {}, std::string(64, 'd')}, {}, {}}.toString();
    CPPUNIT_ASSERT(text);
    const auto padded = *text + std::string(CircleRecord::MAX_SIZE - text->size(), ' ');
    CPPUNIT_ASSERT(CircleRecord::fromString(padded));
    CPPUNIT_ASSERT(!CircleRecord::fromString(padded + " "));
}

void
CirclePolicyTest::testCircleRecordCanonicalForm()
{
    const std::string id(40, 'a');
    const std::string nonce(64, 'd');
    const CircleRecord record {CircleGenesis {id,
                                              {{Permission::Text, Permission::File, Permission::ReadChannel}, {}},
                                              nonce},
                               {},
                               {}};
    const auto canonical = record.toString();
    CPPUNIT_ASSERT(canonical);
    CPPUNIT_ASSERT_EQUAL("{\"admin\":\"" + id
                             + "\",\"defaults\":{\"allow\":[\"file\",\"read-channel\",\"text\"],\"deny\":[]},"
                               "\"kind\":\"genesis\",\"nonce\":\""
                             + nonce + "\",\"type\":\"jami-circle\",\"version\":1}",
                         *canonical);
    const auto reordered = "{\"version\":1,\"type\":\"jami-circle\",\"kind\":\"genesis\",\"defaults\":{\"deny\":[],"
                           "\"allow\":[\"text\",\"read-channel\",\"file\"]},\"admin\":\""
                           + id + "\",\"nonce\":\"" + nonce + "\"}";
    const auto decoded = CircleRecord::fromString(reordered);
    CPPUNIT_ASSERT(decoded && decoded->toString() == canonical);
    const auto duplicate = "{\"type\":\"jami-circle\",\"version\":1,\"kind\":\"genesis\",\"admin\":\"" + id
                           + "\",\"nonce\":\"" + nonce + "\",\"defaults\":{\"allow\":[],\"allow\":[],\"deny\":[]}}";
    CPPUNIT_ASSERT(!CircleRecord::fromString(duplicate));
    CPPUNIT_ASSERT(!CircleRecord::fromString(*canonical + " trailing"));
    CPPUNIT_ASSERT(!CircleRecord::fromString("\xef\xbb\xbf" + *canonical));
}

void
CirclePolicyTest::testCircleRecordReplay()
{
    const std::string admin(40, 'a');
    const std::string circle(40, 'b');
    const std::string base(40, 'c');
    const CircleRecord genesis {CircleGenesis {admin, {{Permission::ReadChannel}, {}}, std::string(64, 'd')}, {}, {}};
    const auto initial = genesis.initialPolicy(admin);
    CPPUNIT_ASSERT(initial && initial->isValid());
    CPPUNIT_ASSERT(initial->members.size() == 1);
    CPPUNIT_ASSERT(!genesis.initialPolicy(circle));
    CPPUNIT_ASSERT(!genesis.initialPolicy(""));
    const CircleRecord update {CirclePolicyChange {SetCircleRole {"writer", {{Permission::Text}, {}}}}, circle, base};
    const auto text = update.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded);
    const auto next = decoded->apply(*initial, admin, circle, base);
    CPPUNIT_ASSERT(next && next->roles.contains("writer"));
    CPPUNIT_ASSERT(!initial->roles.contains("writer"));
    CPPUNIT_ASSERT(!decoded->apply(*initial, circle, circle, base));
    CPPUNIT_ASSERT(!decoded->apply(*initial, admin, base, base));
    CPPUNIT_ASSERT(!decoded->apply(*initial, admin, circle, circle));
    CPPUNIT_ASSERT(!decoded->initialPolicy(admin));
    CPPUNIT_ASSERT(!genesis.apply(*initial, admin, circle, base));
    const CircleRecord invalid {CirclePolicyChange {SetCircleRole {"../role", {}}}, circle, base};
    CPPUNIT_ASSERT(!invalid.apply(*initial, admin, circle, base));
}

void
CirclePolicyTest::testMemberLifecycle()
{
    const auto initial = makePolicy();
    auto policy = initial.apply("alice", InviteCircleMember {"dave"});
    CPPUNIT_ASSERT(policy && policy->members.at("dave").state == CircleMemberState::INVITED);
    CPPUNIT_ASSERT(policy->permissions("dave").empty());
    CPPUNIT_ASSERT(!policy->apply("dave", SetCircleRole {"moderator", {}}));
    CPPUNIT_ASSERT(!policy->apply("alice", JoinCircleMember {"dave"}));
    policy = policy->apply("dave", JoinCircleMember {"dave"});
    CPPUNIT_ASSERT(policy && policy->members.at("dave").state == CircleMemberState::ACTIVE);
    policy->roles["moderator"].allow = {Permission::BanUnbanMember};
    policy = policy->apply("alice", SetCircleRoleAssignment {"dave", "moderator"});
    CPPUNIT_ASSERT(policy && policy->hasPermission("dave", Permission::BanUnbanMember));
    CPPUNIT_ASSERT(!policy->apply("alice", LeaveCircleMember {"dave"}));
    CPPUNIT_ASSERT(!policy->apply("alice", LeaveCircleMember {"alice"}));
    policy = policy->apply("dave", LeaveCircleMember {"dave"});
    CPPUNIT_ASSERT(policy && policy->members.at("dave").state == CircleMemberState::LEFT);
    CPPUNIT_ASSERT(policy->members.at("dave").roles.empty());
    CPPUNIT_ASSERT(!policy->apply("dave", JoinCircleMember {"dave"}));
    policy = policy->apply("alice", InviteCircleMember {"dave"});
    CPPUNIT_ASSERT(policy);
    policy = policy->apply("dave", JoinCircleMember {"dave"});
    CPPUNIT_ASSERT(policy && !policy->hasPermission("dave", Permission::BanUnbanMember));
    CPPUNIT_ASSERT(initial == makePolicy());
}

void
CirclePolicyTest::testMembershipAuthority()
{
    auto policy = makePolicy();
    CPPUNIT_ASSERT(!policy.apply("bob", InviteCircleMember {"dave"}));
    CPPUNIT_ASSERT(!policy.apply("alice", InviteCircleMember {""}));
    CPPUNIT_ASSERT(!policy.apply("alice", InviteCircleMember {"bob"}));
    CPPUNIT_ASSERT(!policy.apply("unknown", JoinCircleMember {"unknown"}));
    policy.defaults.allow.insert(Permission::AddMember);
    const auto invited = policy.apply("bob", InviteCircleMember {"dave"});
    CPPUNIT_ASSERT(invited && !invited->apply("dave", InviteCircleMember {"someone"}));
    CPPUNIT_ASSERT(!invited->apply("alice", InviteCircleMember {"dave"}));
    CPPUNIT_ASSERT(policy.apply("bob", RegisterCircleDevice {"bob", "device"}));
    CPPUNIT_ASSERT(!policy.apply("bob", RegisterCircleDevice {"alice", "device"}));
    CPPUNIT_ASSERT(!policy.apply("bob", RegisterCircleDevice {"bob", ""}));
    for (auto state : {CircleMemberState::INVITED, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy.members.at("carol").state = state;
        CPPUNIT_ASSERT(!policy.apply("carol", RegisterCircleDevice {"carol", "device"}));
    }
    CPPUNIT_ASSERT(!policy.apply("alice", InviteCircleMember {"carol"}));
}

void
CirclePolicyTest::testMembershipRecords()
{
    const std::string member(40, 'a');
    const std::string device(64, 'b');
    const std::array<CirclePolicyChange, 4> changes {
        InviteCircleMember {member},
        JoinCircleMember {member},
        LeaveCircleMember {member},
        RegisterCircleDevice {member, device},
    };
    for (const auto& change : changes) {
        const CircleRecord record {change, member, member};
        const auto text = record.toString();
        CPPUNIT_ASSERT(text);
        const auto decoded = CircleRecord::fromString(*text);
        CPPUNIT_ASSERT(decoded && decoded->toString() == text);
        CPPUNIT_ASSERT(std::get<CirclePolicyChange>(decoded->payload).index() == change.index());
        Json::Value value;
        std::istringstream(*text) >> value;
        value["operation"]["member"] = "../member";
        CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    }
    CircleRecord record {CirclePolicyChange {RegisterCircleDevice {member, member}}, member, member};
    CPPUNIT_ASSERT(!record.toString());
    record.payload = CirclePolicyChange {RegisterCircleDevice {member, device}};
    const auto text = record.toString();
    CPPUNIT_ASSERT(text);
    Json::Value value;
    std::istringstream(*text) >> value;
    value["operation"]["device"] = std::string(64, 'A');
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    value["operation"].removeMember("device");
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
}

void
CirclePolicyTest::testAdminTransfer()
{
    auto policy = makePolicy();
    policy.members.emplace("dave", CircleMember {CircleMemberState::INVITED, {}});
    CPPUNIT_ASSERT(!policy.apply("alice", LeaveCircleMember {"alice"}));
    CPPUNIT_ASSERT(!policy.apply("bob", TransferCircleAdmin {"carol"}));
    CPPUNIT_ASSERT(!policy.apply("alice", TransferCircleAdmin {"alice"}));
    CPPUNIT_ASSERT(!policy.apply("alice", TransferCircleAdmin {"dave"}));
    CPPUNIT_ASSERT(!policy.apply("alice", TransferCircleAdmin {"unknown"}));
    const auto transferred = policy.apply("alice", TransferCircleAdmin {"bob"});
    CPPUNIT_ASSERT(transferred && transferred->admin == "bob");
    CPPUNIT_ASSERT(transferred->members.at("alice").state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(!transferred->hasPermission("alice", Permission::AddMember));
    CPPUNIT_ASSERT(transferred->hasPermission("bob", Permission::AddMember));
    CPPUNIT_ASSERT(!transferred->apply("alice", SetCircleDefaults {}));
    const auto left = transferred->apply("alice", LeaveCircleMember {"alice"});
    CPPUNIT_ASSERT(left && left->members.at("alice").state == CircleMemberState::LEFT);
    CPPUNIT_ASSERT(!transferred->apply("bob", LeaveCircleMember {"bob"}));
}

void
CirclePolicyTest::testCircleEnd()
{
    auto policy = makePolicy();
    policy.roles["writer"].allow = {Permission::ReadChannel, Permission::Text, Permission::AddMember};
    policy.members.at("bob").roles.insert("writer");
    CPPUNIT_ASSERT(!policy.apply("bob", EndCircle {}));
    const auto ended = policy.apply("alice", EndCircle {});
    CPPUNIT_ASSERT(ended && ended->ended && ended->isValid());
    for (const auto* member : {"alice", "bob"}) {
        CPPUNIT_ASSERT(ended->hasPermission(member, Permission::ReadChannel, "general"));
        CPPUNIT_ASSERT(!ended->hasPermission(member, Permission::Text, "general"));
        CPPUNIT_ASSERT(!ended->hasPermission(member, Permission::AddMember));
    }
    CPPUNIT_ASSERT(!ended->apply("alice", SetCircleDefaults {}));
    CPPUNIT_ASSERT(!ended->apply("alice", EndCircle {}));
    CPPUNIT_ASSERT(!ended->apply("alice", TransferCircleAdmin {"bob"}));
    CPPUNIT_ASSERT(!ended->apply("bob", LeaveCircleMember {"bob"}));
    CPPUNIT_ASSERT(!ended->moderate("alice", {CircleModerationAction::BAN_MEMBER, "bob"}));
}

void
CirclePolicyTest::testAdminLifecycleRecords()
{
    const std::string member(40, 'a');
    for (const CirclePolicyChange& change :
         {CirclePolicyChange {TransferCircleAdmin {member}}, CirclePolicyChange {EndCircle {}}}) {
        const CircleRecord record {change, member, member};
        const auto text = record.toString();
        CPPUNIT_ASSERT(text);
        const auto decoded = CircleRecord::fromString(*text);
        CPPUNIT_ASSERT(decoded && decoded->toString() == text);
        CPPUNIT_ASSERT(std::get<CirclePolicyChange>(decoded->payload).index() == change.index());
        Json::Value value;
        std::istringstream(*text) >> value;
        value["operation"]["extra"] = true;
        CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    }
    Json::Value value;
    std::istringstream(*CircleRecord {TransferCircleAdmin {member}, member, member}.toString()) >> value;
    value["operation"]["member"] = "../member";
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
}

void
CirclePolicyTest::testPolicyJson()
{
    auto policy = makePolicy();
    policy.roles["writer"].allow = {Permission::ReadChannel, Permission::Text};
    policy.members.at("bob").roles.insert("writer");
    policy.members.at("carol").state = CircleMemberState::BANNED;
    policy.channels.at("general").archived = true;
    policy.delegations["writer"] = RoleDelegation {{Permission::Text}, {"writer"}, {}, {"general"}, true};
    Json::Value value;
    std::istringstream(circlePolicyToJson(policy)) >> value;
    CPPUNIT_ASSERT(value["admin"] == "alice" && value["ended"] == false);
    CPPUNIT_ASSERT(value["members"]["bob"]["state"] == "active" && value["members"]["bob"]["roles"][0] == "writer");
    CPPUNIT_ASSERT(value["members"]["carol"]["state"] == "banned");
    CPPUNIT_ASSERT(value["roles"]["writer"]["allow"].size() == 2);
    CPPUNIT_ASSERT(value["channels"]["general"]["archived"] == true);
    CPPUNIT_ASSERT(value["delegations"]["writer"]["circle-wide"] == true);

    const auto change = circlePolicyChangeFromJson(
        R"({"type":"set-role","role":"writer","rule":{"allow":["text"],"deny":[]}})");
    CPPUNIT_ASSERT(change && std::holds_alternative<SetCircleRole>(*change));
    CPPUNIT_ASSERT(std::get<SetCircleRole>(*change).rule.allow == PermissionSet {Permission::Text});
    CPPUNIT_ASSERT(!circlePolicyChangeFromJson(R"({"type":"set-role","role":"writer"})"));
    CPPUNIT_ASSERT(
        !circlePolicyChangeFromJson(R"({"type":"set-role","role":"writer","rule":{"allow":[],"deny":[]},"extra":1})"));
    CPPUNIT_ASSERT(!circlePolicyChangeFromJson("not json"));
    CPPUNIT_ASSERT(!circlePolicyChangeFromJson(""));
}

void
CirclePolicyTest::testMemberModeration()
{
    auto initial = makePolicy();
    initial.roles["moderator"].allow = {Permission::BanUnbanMember};
    initial.members.at("bob").roles.insert("moderator");
    const CircleModerationVote ban {CircleModerationAction::BAN_MEMBER, "bob"};
    auto policy = initial.moderate("alice", ban);
    CPPUNIT_ASSERT(policy && policy->members.at("bob").state == CircleMemberState::BANNED);
    CPPUNIT_ASSERT(policy->members.at("bob").roles.empty());
    CPPUNIT_ASSERT(policy->permissions("bob").empty());
    CPPUNIT_ASSERT(initial.members.at("bob").state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(!policy->moderate("alice", ban));
    CPPUNIT_ASSERT(!policy->apply("alice", InviteCircleMember {"bob"}));
    CPPUNIT_ASSERT(!policy->apply("bob", JoinCircleMember {"bob"}));
    policy = policy->moderate("alice", {CircleModerationAction::UNBAN_MEMBER, "bob"});
    CPPUNIT_ASSERT(policy && policy->members.at("bob").state == CircleMemberState::INVITED);
    CPPUNIT_ASSERT(policy->permissions("bob").empty());
    policy = policy->apply("bob", JoinCircleMember {"bob"});
    CPPUNIT_ASSERT(policy && !policy->hasPermission("bob", Permission::BanUnbanMember));
}

void
CirclePolicyTest::testModerationAuthority()
{
    auto policy = makePolicy();
    const CircleModerationVote ban {CircleModerationAction::BAN_MEMBER, "carol"};
    CPPUNIT_ASSERT(!policy.moderate("bob", ban));
    CPPUNIT_ASSERT(!policy.moderate("unknown", ban));
    CPPUNIT_ASSERT(!policy.moderate("alice", {CircleModerationAction::BAN_MEMBER, "alice"}));
    CPPUNIT_ASSERT(!policy.moderate("alice", {CircleModerationAction::BAN_MEMBER, "unknown"}));
    CPPUNIT_ASSERT(!policy.moderate("alice", {CircleModerationAction::UNBAN_MEMBER, "bob"}));
    CPPUNIT_ASSERT(!policy.moderate("alice", {static_cast<CircleModerationAction>(999), "bob"}));
    policy.roles["moderator"].allow = {Permission::BanUnbanMember};
    policy.members.at("bob").roles.insert("moderator");
    CPPUNIT_ASSERT(policy.moderate("bob", ban));
    CPPUNIT_ASSERT(!policy.moderate("bob", {CircleModerationAction::BAN_MEMBER, "bob"}));
    CPPUNIT_ASSERT(!policy.moderate("bob", {CircleModerationAction::BAN_MEMBER, "alice"}));
    for (auto state : {CircleMemberState::INVITED, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy.members.at("bob").state = state;
        CPPUNIT_ASSERT(!policy.moderate("bob", ban));
    }
    for (auto state : {CircleMemberState::INVITED, CircleMemberState::LEFT}) {
        policy.members.at("carol").state = state;
        CPPUNIT_ASSERT(policy.moderate("alice", ban));
    }
}

void
CirclePolicyTest::testPeerModerationProtected()
{
    auto policy = makePolicy();
    policy.roles["moderator"].allow = {Permission::BanUnbanMember};
    policy.members.at("bob").roles = {"moderator"};
    policy.members.at("carol").roles = {"peer"};
    const CircleModerationVote ban {CircleModerationAction::BAN_MEMBER, "carol"};
    for (auto permission : {Permission::AddMember,
                            Permission::BanUnbanMember,
                            Permission::ManageChannels,
                            Permission::ManageRoles,
                            Permission::AssignRoles,
                            Permission::ModerateConference}) {
        policy.roles["peer"].allow = {permission};
        CPPUNIT_ASSERT(!policy.moderate("bob", ban));
        CPPUNIT_ASSERT(policy.moderate("alice", ban));
        policy.roles.at("peer").allow.clear();
        policy.channels.at("general").roles["peer"] = {{Permission::ReadChannel, permission}, {}};
        CPPUNIT_ASSERT(!policy.moderate("bob", ban));
        CPPUNIT_ASSERT(policy.moderate("alice", ban));
        policy.channels.at("general").roles.clear();
    }
    policy.roles.at("peer").allow = {Permission::Text};
    CPPUNIT_ASSERT(policy.moderate("bob", ban));
    CPPUNIT_ASSERT(policy.members.at("carol").state == CircleMemberState::ACTIVE);
}

void
CirclePolicyTest::testPeerRoleAssignments()
{
    auto policy = makePolicy();
    policy.roles["moderator"].allow = {Permission::BanUnbanMember};
    policy.roles["designator"].allow = {Permission::AssignRoles};
    policy.roles["writer"].allow = {Permission::Text};
    policy.roles["restricted"].deny = {Permission::BanUnbanMember};
    policy.delegations["designator"] = {{}, {}, {"moderator", "designator", "writer", "restricted"}, {}, true};
    policy.members.at("bob").roles = {"designator"};
    policy.members.at("carol").roles = {"moderator"};
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRoleAssignment {"carol", "moderator", false}));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleRoleAssignment {"carol", "restricted"}));
    CPPUNIT_ASSERT(policy.apply("alice", SetCircleRoleAssignment {"carol", "moderator", false}));
    CPPUNIT_ASSERT(policy.apply("bob", SetCircleRoleAssignment {"carol", "writer"}));
    CPPUNIT_ASSERT(policy.apply("bob", SetCircleRoleAssignment {"bob", "designator", false}));
    CPPUNIT_ASSERT(policy.apply("bob", LeaveCircleMember {"bob"}));
    policy.roles["backup"].allow = {Permission::BanUnbanMember};
    policy.members.at("carol").roles.insert("backup");
    CPPUNIT_ASSERT(policy.apply("bob", SetCircleRoleAssignment {"carol", "moderator", false}));
}

void
CirclePolicyTest::testPeerRoleEdits()
{
    auto policy = makeEditorPolicy();
    policy.roles.at("writer").allow.insert(Permission::BanUnbanMember);
    policy.delegations.at("editor").permissions.insert(Permission::BanUnbanMember);
    const CirclePolicyChange remove = SetCircleRole {"writer", {{Permission::Text}, {}}};
    CPPUNIT_ASSERT(!policy.apply("bob", remove));
    CPPUNIT_ASSERT(policy.apply("alice", remove));
    CPPUNIT_ASSERT(
        !policy.apply("bob",
                      SetCircleRole {"writer",
                                     {{Permission::Text, Permission::BanUnbanMember}, {Permission::BanUnbanMember}}}));
    CPPUNIT_ASSERT(policy.apply("bob", SetCircleRole {"writer", {{Permission::BanUnbanMember}, {}}}));
    policy.members.at("bob").roles.insert("writer");
    CPPUNIT_ASSERT(!policy.apply("bob", remove));
    policy.members.at("carol").roles.clear();
    CPPUNIT_ASSERT(policy.apply("bob", remove));
}

void
CirclePolicyTest::testPeerChannelEdits()
{
    auto policy = makeEditorPolicy();
    policy.delegations.at("editor").permissions = {Permission::ReadChannel, Permission::BanUnbanMember};
    policy.channels.at("general").roles["writer"] = {{Permission::ReadChannel, Permission::BanUnbanMember}, {}};
    const CirclePolicyChange remove = SetCircleChannelRule {"general", "writer", std::nullopt};
    CPPUNIT_ASSERT(!policy.apply("bob", remove));
    CPPUNIT_ASSERT(policy.apply("alice", remove));
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", "writer", PermissionRule {}}));
    CPPUNIT_ASSERT(
        !policy.apply("bob",
                      SetCircleChannelRule {"general",
                                            "writer",
                                            PermissionRule {{Permission::BanUnbanMember}, {Permission::ReadChannel}}}));
    policy.channels.at("general").roles["writer"] = {{Permission::BanUnbanMember}, {}};
    CPPUNIT_ASSERT(!policy.apply("bob", SetCircleChannelRule {"general", std::nullopt, PermissionRule {}}));
    CPPUNIT_ASSERT(policy.apply("alice", SetCircleChannelRule {"general", std::nullopt, PermissionRule {}}));
}

void
CirclePolicyTest::testPeerDelegationPreserved()
{
    for (auto permission : {Permission::AssignRoles, Permission::ManageRoles}) {
        auto policy = makePolicy();
        policy.roles["designator"].allow = {Permission::AssignRoles};
        policy.roles["command"].allow = {permission};
        policy.roles["grant"] = {};
        policy.roles["writer"] = {};
        policy.roles["reader"] = {};
        policy.roles["first-backup"] = {};
        policy.roles["second-backup"] = {};
        policy.members.at("bob").roles = {"designator"};
        policy.members.at("carol").roles = {"command", "grant"};
        policy.delegations["designator"] = {{}, {}, {"grant"}, {}, true};
        policy.delegations["grant"] = {{Permission::Text}, {"writer", "reader"}, {"writer", "reader"}, {}, true};
        const CirclePolicyChange remove = SetCircleRoleAssignment {"carol", "grant", false};
        CPPUNIT_ASSERT(!policy.apply("bob", remove));
        CPPUNIT_ASSERT(policy.apply("alice", remove));
        policy.delegations["first-backup"] = {{Permission::Text}, {"writer"}, {"writer"}, {}, true};
        policy.delegations["second-backup"] = {{Permission::Text}, {"reader"}, {"reader"}, {}, true};
        policy.members.at("carol").roles.insert("first-backup");
        policy.members.at("carol").roles.insert("second-backup");
        CPPUNIT_ASSERT(!policy.apply("bob", remove));
        policy.delegations["first-backup"] = policy.delegations.at("grant");
        CPPUNIT_ASSERT(policy.apply("bob", remove));
        if (permission == Permission::ManageRoles) {
            policy.delegations.at("grant").circleWide = false;
            policy.delegations.at("grant").channels = {"general"};
            policy.defaults.allow = {Permission::ReadChannel};
            CPPUNIT_ASSERT(!policy.apply("bob", remove));
            policy.delegations.at("first-backup").channels = {"general"};
            CPPUNIT_ASSERT(policy.apply("bob", remove));
        }
    }
}

void
CirclePolicyTest::testChannelRegistration()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text};
    policy.roles["manager"].allow = {Permission::ManageChannels};
    policy.members.at("bob").roles = {"manager"};
    const RegisterCircleChannel registration {"new-channel", "authorization", {}};
    CPPUNIT_ASSERT(!policy.apply("carol", registration));
    auto next = policy.apply("bob", registration);
    CPPUNIT_ASSERT(next && next->hasPermission("carol", Permission::Text, "new-channel"));
    CPPUNIT_ASSERT(!next->apply("alice", registration));
    auto privateChannel = registration;
    privateChannel.policy.defaults = PermissionRule {};
    next = policy.apply("bob", privateChannel);
    CPPUNIT_ASSERT(next && !next->hasPermission("carol", Permission::ReadChannel, "new-channel"));
    auto expanded = registration;
    expanded.policy.defaults = PermissionRule {{Permission::ReadChannel, Permission::File}, {}};
    CPPUNIT_ASSERT(!policy.apply("bob", expanded));
    CPPUNIT_ASSERT(policy.apply("alice", expanded));
    policy.roles["latent"].allow = {Permission::File};
    policy.roles.at("latent").deny = {Permission::ReadChannel};
    policy.members.at("carol").roles = {"latent"};
    expanded = registration;
    expanded.policy.roles["latent"] = {{Permission::File}, {}};
    for (auto state :
         {CircleMemberState::ACTIVE, CircleMemberState::INVITED, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy.members.at("carol").state = state;
        CPPUNIT_ASSERT(!policy.apply("bob", expanded));
    }
}

void
CirclePolicyTest::testChannelArchive()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text, Permission::File};
    policy.roles["manager"].allow = {Permission::ManageChannels};
    policy.members.at("bob").roles = {"manager"};
    CPPUNIT_ASSERT(!policy.apply("carol", ArchiveCircleChannel {"general"}));
    auto next = policy.apply("bob", ArchiveCircleChannel {"general"});
    CPPUNIT_ASSERT(next && next->channels.at("general").archived);
    for (const auto actor : {"alice", "bob", "carol"}) {
        CPPUNIT_ASSERT(next->hasPermission(actor, Permission::ReadChannel, "general"));
        CPPUNIT_ASSERT(!next->hasPermission(actor, Permission::Text, "general"));
        CPPUNIT_ASSERT(!next->hasPermission(actor, Permission::File, "general"));
    }
    CPPUNIT_ASSERT(next->hasPermission("bob", Permission::ManageChannels, "general"));
    CPPUNIT_ASSERT(!next->apply("alice", ArchiveCircleChannel {"general"}));
    CPPUNIT_ASSERT(!next->apply("alice", RegisterCircleChannel {"general", "authorization", {}}));
    CPPUNIT_ASSERT(!policy.apply("alice", ArchiveCircleChannel {"unknown"}));
}

void
CirclePolicyTest::testChannelRecords()
{
    const std::string circle(40, 'a'), authorization(40, 'b'), channel(40, 'c'), creator(40, 'd');
    const CircleRecord genesis {CircleChannelGenesis {creator, std::string(64, 'e')}, circle, authorization};
    const auto text = genesis.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded && std::holds_alternative<CircleChannelGenesis>(decoded->payload));
    CPPUNIT_ASSERT(decoded->circle == circle && decoded->base == authorization);
    CPPUNIT_ASSERT(!decoded->initialPolicy(creator));
    const RegisterCircleChannel registration {channel, authorization, {}, "signed-git-genesis"};
    const CircleRecord record {CirclePolicyChange {registration}, circle, authorization};
    CPPUNIT_ASSERT(record.toString());
    const auto roundTrip = CircleRecord::fromString(*record.toString());
    CPPUNIT_ASSERT(roundTrip);
    const auto& operation = std::get<RegisterCircleChannel>(std::get<CirclePolicyChange>(roundTrip->payload));
    CPPUNIT_ASSERT(operation.channel == channel && operation.authorization == authorization);
    CPPUNIT_ASSERT(operation.genesis == registration.genesis);
    CPPUNIT_ASSERT(
        (CircleRecord {CirclePolicyChange {ArchiveCircleChannel {channel}}, circle, authorization}).toString());
    Json::Value value;
    std::istringstream input(*record.toString());
    input >> value;
    value["operation"]["genesis"] = std::string(CircleRecord::MAX_CHANNEL_PROOF_SIZE + 1, 'x');
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    value["operation"]["genesis"] = "proof";
    value["operation"]["policy"]["archived"] = true;
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
}

void
CirclePolicyTest::testChannelContentRecords()
{
    const std::string circle(40, 'a'), policy(40, 'b'), channel(40, 'c'), parent(40, 'd'), target(40, 'e');
    for (const auto& message : {CommitMessage::text("hello"),
                                CommitMessage::text("reply", target),
                                CommitMessage::reaction("yes", target),
                                CommitMessage::edit("edited", target),
                                CommitMessage::edit("", target)}) {
        const CircleRecord record {CircleChannelContent {channel, parent, message}, circle, policy};
        const auto encoded = record.toString();
        CPPUNIT_ASSERT(encoded);
        const auto decoded = CircleRecord::fromString(*encoded);
        CPPUNIT_ASSERT(decoded && decoded->circle == circle && decoded->base == policy);
        const auto& content = std::get<CircleChannelContent>(decoded->payload);
        CPPUNIT_ASSERT(content.channel == channel && content.parent == parent);
        CPPUNIT_ASSERT(content.message.body == message.body);
        CPPUNIT_ASSERT(content.message.replyTo == message.replyTo && content.message.reactTo == message.reactTo);
        CPPUNIT_ASSERT(content.message.editedId == message.editedId);
    }
    CircleRecord record {CircleChannelContent {channel, parent, CommitMessage::text("hello")}, circle, policy};
    auto& message = std::get<CircleChannelContent>(record.payload).message;
    message.replyTo = target;
    message.reactTo = target;
    CPPUNIT_ASSERT(!record.toString());
    message = CommitMessage::text("");
    CPPUNIT_ASSERT(!record.toString());
    message = CommitMessage::text(std::string(CircleRecord::MAX_CHANNEL_BODY_SIZE + 1, 'x'));
    CPPUNIT_ASSERT(!record.toString());
    message = CommitMessage::text("hello");
    message.uri = target;
    CPPUNIT_ASSERT(!record.toString());
}

void
CirclePolicyTest::testModerationRecords()
{
    const std::string id(40, 'a');
    for (auto action : {CircleModerationAction::BAN_MEMBER, CircleModerationAction::UNBAN_MEMBER}) {
        const CircleRecord record {CircleModerationVote {action, id}, id, id};
        const auto text = record.toString();
        CPPUNIT_ASSERT(text);
        const auto decoded = CircleRecord::fromString(*text);
        CPPUNIT_ASSERT(decoded && decoded->toString() == text);
        CPPUNIT_ASSERT(std::get<CircleModerationVote>(decoded->payload).action == action);
        Json::Value value;
        std::istringstream(*text) >> value;
        value["action"] = "ban-device";
        CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
        value["action"] = "ban-member";
        value["member"] = std::string(64, 'a');
        CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    }
    CircleRecord resolution {CircleModerationResolution {id}, id, id};
    const auto text = resolution.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded && decoded->toString() == text);
    CPPUNIT_ASSERT(std::get<CircleModerationResolution>(decoded->payload).vote == id);
    Json::Value value;
    std::istringstream(*text) >> value;
    value["member"] = id;
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    std::get<CircleModerationResolution>(resolution.payload).vote = "invalid";
    CPPUNIT_ASSERT(!resolution.toString());
    const CircleRecord invalid {CircleModerationVote {static_cast<CircleModerationAction>(999), id}, id, id};
    CPPUNIT_ASSERT(!invalid.toString());
}

void
CirclePolicyTest::testPolicyMergeRecords()
{
    const std::string circle(40, 'a'), first(40, 'b'), second(40, 'c');
    CircleRecord record {CirclePolicyMerge {second}, circle, first};
    const auto text = record.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded && decoded->toString() == text);
    CPPUNIT_ASSERT(std::get<CirclePolicyMerge>(decoded->payload).other == second);
    Json::Value value;
    std::istringstream(*text) >> value;
    value["policy"] = Json::Value(Json::objectValue);
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    std::get<CirclePolicyMerge>(record.payload).other = first;
    CPPUNIT_ASSERT(!record.toString());
    std::get<CirclePolicyMerge>(record.payload).other = "invalid";
    CPPUNIT_ASSERT(!record.toString());
}

void
CirclePolicyTest::testChannelMergeRecords()
{
    const std::string circle(40, 'a'), policy(40, 'b'), channel(40, 'c'), first(40, 'd'), second(40, 'e');
    CircleRecord record {CircleChannelMerge {channel, first, second}, circle, policy};
    const auto text = record.toString();
    CPPUNIT_ASSERT(text);
    const auto decoded = CircleRecord::fromString(*text);
    CPPUNIT_ASSERT(decoded && decoded->toString() == text && decoded->base == policy);
    const auto& merge = std::get<CircleChannelMerge>(decoded->payload);
    CPPUNIT_ASSERT(merge.channel == channel && merge.parent == first && merge.other == second);
    Json::Value value;
    std::istringstream(*text) >> value;
    CPPUNIT_ASSERT(value["type"] == "jami-circle-channel" && value["kind"] == "merge");
    value["message"] = Json::Value(Json::objectValue);
    CPPUNIT_ASSERT(!CircleRecord::fromString(Json::writeString(Json::StreamWriterBuilder {}, value)));
    std::get<CircleChannelMerge>(record.payload).other = first;
    CPPUNIT_ASSERT(!record.toString());
    std::get<CircleChannelMerge>(record.payload).other = "invalid";
    CPPUNIT_ASSERT(!record.toString());
}

void
CirclePolicyTest::testDefaultDeny()
{
    auto policy = makePolicy();
    CPPUNIT_ASSERT(policy.permissions("bob").empty());
    CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
    CPPUNIT_ASSERT(!policy.hasPermission("bob", Permission::Text));
}

void
CirclePolicyTest::testDefaultAndRoleGrants()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel};
    policy.roles["writer"].allow = {Permission::Text};
    policy.roles["host"].allow = {Permission::HostConference};
    policy.members.at("bob").roles = {"writer", "host"};
    const PermissionSet expected {Permission::ReadChannel, Permission::Text, Permission::HostConference};
    CPPUNIT_ASSERT(policy.permissions("bob", "general") == expected);
    CPPUNIT_ASSERT(policy.permissions("carol", "general") == policy.defaults.allow);
}

void
CirclePolicyTest::testDenyWins()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text};
    policy.roles["writer"].allow = {Permission::Text, Permission::File};
    policy.roles["restricted"].deny = {Permission::Text, Permission::File};
    policy.members.at("bob").roles = {"writer", "restricted"};
    CPPUNIT_ASSERT(policy.permissions("bob", "general") == PermissionSet {Permission::ReadChannel});
    policy.defaults.deny = {Permission::ReadChannel};
    CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
}

void
CirclePolicyTest::testChannelRuleReplacesRole()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel};
    policy.roles["writer"] = {{Permission::Text}, {Permission::File}};
    policy.members.at("bob").roles = {"writer"};
    policy.channels.at("general").roles["writer"] = {{Permission::File}, {}};
    const PermissionSet expected {Permission::ReadChannel, Permission::File};
    CPPUNIT_ASSERT(policy.permissions("bob", "general") == expected);
    CPPUNIT_ASSERT(policy.hasPermission("bob", Permission::Text));
    policy.channels.at("general").roles["writer"] = {};
    CPPUNIT_ASSERT(policy.permissions("bob", "general") == PermissionSet {Permission::ReadChannel});
}

void
CirclePolicyTest::testPrivateChannel()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text};
    policy.roles["private"].allow = {Permission::ReadChannel, Permission::Text};
    policy.members.at("bob").roles = {"private"};
    policy.channels["private"].defaults = PermissionRule {};
    CPPUNIT_ASSERT(policy.hasPermission("carol", Permission::Text, "general"));
    CPPUNIT_ASSERT(policy.permissions("carol", "private").empty());
    CPPUNIT_ASSERT(policy.hasPermission("bob", Permission::Text, "private"));
    policy.channels.at("private").defaults->deny = {Permission::ReadChannel};
    CPPUNIT_ASSERT(policy.permissions("bob", "private").empty());
}

void
CirclePolicyTest::testReadRequired()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::Text, Permission::File, Permission::HostConference};
    CPPUNIT_ASSERT(!policy.permissions("bob").empty());
    CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
    policy.defaults.allow.insert(Permission::ReadChannel);
    CPPUNIT_ASSERT(policy.hasPermission("bob", Permission::HostConference, "general"));
}

void
CirclePolicyTest::testInactiveMembersDenied()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text};
    for (auto state : {CircleMemberState::INVITED, CircleMemberState::LEFT, CircleMemberState::BANNED}) {
        policy.members.at("bob").state = state;
        CPPUNIT_ASSERT(policy.permissions("bob").empty());
        CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
    }
    policy.members.at("bob").state = CircleMemberState::ACTIVE;
    policy.members.at("alice").state = CircleMemberState::BANNED;
    CPPUNIT_ASSERT(policy.permissions("alice").empty());
    CPPUNIT_ASSERT(policy.permissions("bob").empty());
}

void
CirclePolicyTest::testUnknownReferencesDenied()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text};
    CPPUNIT_ASSERT(policy.permissions("").empty());
    CPPUNIT_ASSERT(policy.permissions("unknown").empty());
    CPPUNIT_ASSERT(policy.permissions("bob", "unknown").empty());
    CPPUNIT_ASSERT(policy.permissions("alice", "unknown").empty());
    policy.members.at("bob").roles = {"unknown"};
    policy.channels.at("general").roles["unknown"].allow = {Permission::ReadChannel, Permission::Text};
    CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
    policy.admin = "unknown";
    CPPUNIT_ASSERT(policy.permissions("carol").empty());
}

void
CirclePolicyTest::testAdminBypassesDenies()
{
    auto policy = makePolicy();
    policy.defaults.deny = {Permission::ReadChannel, Permission::Text, Permission::ManageRoles};
    for (int value = 0; value < static_cast<int>(Permission::Count); ++value)
        CPPUNIT_ASSERT(policy.hasPermission("alice", static_cast<Permission>(value), "general"));
    CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
    policy.roles["ADMIN"].allow = {Permission::ReadChannel};
    policy.members.at("bob").roles = {"ADMIN"};
    CPPUNIT_ASSERT(!policy.hasPermission("bob", Permission::ManageRoles));
}

void
CirclePolicyTest::testUnknownPermissionsDenied()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel, Permission::Text};
    for (auto permission : {static_cast<Permission>(-1), Permission::Count, static_cast<Permission>(999)}) {
        CPPUNIT_ASSERT(!policy.hasPermission("alice", permission));
        CPPUNIT_ASSERT(!policy.hasPermission("bob", permission));
        policy.roles["invalid"].allow = {permission};
        policy.members.at("bob").roles = {"invalid"};
        CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
    }
    policy.roles["invalid"] = {{}, {Permission::Count}};
    CPPUNIT_ASSERT(policy.permissions("bob", "general").empty());
}

void
CirclePolicyTest::testRoleOrderIndependent()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel};
    policy.roles["first"] = {{Permission::Text, Permission::File}, {Permission::Reaction}};
    policy.roles["second"] = {{Permission::Reaction}, {Permission::Text}};
    policy.members.at("bob").roles = {"first", "second"};
    auto expected = policy.permissions("bob", "general");
    std::swap(policy.roles.at("first"), policy.roles.at("second"));
    CPPUNIT_ASSERT(policy.permissions("bob", "general") == expected);
}

void
CirclePolicyTest::testLargePolicy()
{
    auto policy = makePolicy();
    policy.defaults.allow = {Permission::ReadChannel};
    policy.roles["writer"].allow = {Permission::Text};
    for (size_t index = 0; index < 5000; ++index)
        policy.members.emplace("member" + std::to_string(index), CircleMember {CircleMemberState::ACTIVE, {"writer"}});
    for (size_t index = 0; index < 500; ++index)
        policy.channels.emplace("channel" + std::to_string(index), ChannelPolicy {});
    for (auto member : {"member0", "member4999"}) {
        CPPUNIT_ASSERT(policy.hasPermission(member, Permission::Text, "channel0"));
        CPPUNIT_ASSERT(policy.hasPermission(member, Permission::Text, "channel499"));
    }
    policy.roles.at("writer").deny = {Permission::Text};
    CPPUNIT_ASSERT(!policy.hasPermission("member4999", Permission::Text, "channel499"));
}

void
CirclePolicyTest::testConversationPermissionsUnchanged()
{
    static_assert(static_cast<int>(Permission::Text) == 0);
    static_assert(static_cast<int>(Permission::UpdateProfile) == 18);
    static_assert(static_cast<int>(ConversationMode::FEED) == 5);

    struct ModePermissions
    {
        ConversationMode mode;
        PermissionSet members;
        PermissionSet admins;
    };
    const PermissionSet conversation {Permission::Text,
                                      Permission::TextReply,
                                      Permission::File,
                                      Permission::FileReply,
                                      Permission::Reaction,
                                      Permission::Edit,
                                      Permission::Delete,
                                      Permission::DeleteFile,
                                      Permission::CreateDocument,
                                      Permission::DeleteDocument,
                                      Permission::Leave,
                                      Permission::AddMember};
    auto oneToOne = conversation;
    oneToOne.insert(Permission::Call);
    auto group = conversation;
    group.insert(Permission::HostConference);
    const PermissionSet document {Permission::UpdateDocument,
                                  Permission::AddDocumentAttachment,
                                  Permission::Leave,
                                  Permission::AddMember};
    const PermissionSet feed {Permission::TextReply,
                              Permission::FileReply,
                              Permission::Reaction,
                              Permission::Edit,
                              Permission::Delete,
                              Permission::DeleteFile};
    auto feedAdmins = feed;
    feedAdmins.insert(Permission::Text);
    feedAdmins.insert(Permission::File);
    feedAdmins.insert(Permission::AddMember);
    auto feedMembers = feed;
    feedMembers.insert(Permission::Leave);
    auto admins = [](PermissionSet permissions) {
        permissions.insert(Permission::BanUnbanMember);
        permissions.insert(Permission::UpdateProfile);
        return permissions;
    };
    const std::array cases {
        ModePermissions {ConversationMode::ONE_TO_ONE, oneToOne, oneToOne},
        ModePermissions {ConversationMode::INVITES_ONLY, group, admins(group)},
        ModePermissions {ConversationMode::DOCUMENT, document, admins(document)},
        ModePermissions {ConversationMode::FEED, feedMembers, admins(feedAdmins)},
        ModePermissions {ConversationMode::ADMIN_INVITES_ONLY, {}, {}},
        ModePermissions {ConversationMode::PUBLIC, {}, {}},
        ModePermissions {static_cast<ConversationMode>(999), {}, {}},
    };
    for (const auto& entry : cases) {
        for (auto role : {MemberRole::ADMIN,
                          MemberRole::MEMBER,
                          MemberRole::INVITED,
                          MemberRole::BANNED,
                          MemberRole::LEFT,
                          static_cast<MemberRole>(999)}) {
            for (int value = -1; value <= static_cast<int>(Permission::Count); ++value) {
                auto permission = static_cast<Permission>(value);
                bool expected = role == MemberRole::ADMIN    ? entry.admins.contains(permission)
                                : role == MemberRole::MEMBER ? entry.members.contains(permission)
                                                             : false;
                if (role == MemberRole::INVITED && permission == Permission::Join)
                    expected = entry.mode == ConversationMode::ONE_TO_ONE
                               || entry.mode == ConversationMode::INVITES_ONLY
                               || entry.mode == ConversationMode::DOCUMENT || entry.mode == ConversationMode::FEED;
                CPPUNIT_ASSERT_EQUAL(expected, jami::hasPermission(entry.mode, role, permission));
            }
        }
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CirclePolicyTest::name())
