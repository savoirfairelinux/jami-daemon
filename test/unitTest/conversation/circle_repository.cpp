#include "jamidht/swarm_certificate.h"
#include "jamidht/circlerepository.h"
#include "jamidht/circle_module.h"
#include "jamidht/git_def.h"
#include "jamidht/circle_record.h"
#include "../../test_runner.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <array>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>

namespace jami {
namespace test {

// Existing cases check full-replay determinism, so they reopen without the saved state.
std::unique_ptr<CircleRepository>
openReplayed(const std::filesystem::path& path,
             std::string_view circle,
             size_t limit = CircleRepository::DEFAULT_VALIDATION_LIMIT)
{
    std::error_code error;
    std::filesystem::remove(path / CircleRepository::STATE_FILE, error);
    return CircleRepository::open(path, circle, limit);
}

ChannelValidationResult
validateReplayed(const CircleRepository& repository,
                 const std::filesystem::path& path,
                 std::string_view channel,
                 size_t limit = CircleRepository::DEFAULT_VALIDATION_LIMIT)
{
    std::error_code error;
    std::filesystem::remove(path / CircleRepository::STATE_FILE, error);
    return repository.validateChannel(path, channel, limit);
}

bool
verifyReplayed(const CircleRepository& repository, const std::filesystem::path& path, std::string_view channel)
{
    return validateReplayed(repository, path, channel).status == ChannelValidationStatus::VALID;
}

class CircleRepositoryTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "CircleRepository"; }
    void setUp() override;
    void tearDown() override;

private:
    enum class Signature { VALID, MISSING, INVALID, MALFORMED, EMPTY, ALTERED_BODY };
    struct TamperOptions
    {
        Signature signature {Signature::VALID};
        std::optional<std::string> message;
        std::optional<git_oid> tree;
        std::optional<git_time_t> when;
        const dht::crypto::Identity* signer {nullptr};
        std::string device;
        bool root {false};
        bool merge {false};
        std::optional<std::string> otherParent;
        std::optional<std::filesystem::path> repository;
        std::optional<std::string> parent;
    };

    std::filesystem::path path_;
    std::filesystem::path peerPath_;
    void setHead(std::string_view commit);
    static void setMain(const std::filesystem::path& repository, std::string_view commit);
    static std::optional<std::string> fetchFrom(const std::filesystem::path& repository,
                                                const std::filesystem::path& source,
                                                std::string_view ref);
    std::string tamper(const CircleRepository& repository, const TamperOptions& options);
    git_oid changedTree(const CircleRepository& repository, const dht::crypto::Certificate* device);
    git_oid invitationTree(const CircleRepository& repository,
                           const std::string& member,
                           std::string_view contents,
                           git_filemode_t mode,
                           bool oversized = false);
    static const dht::crypto::Identity& identity();
    static const dht::crypto::Identity& accountIdentity();
    static dht::crypto::Identity addMember(CircleRepository& repository, const std::string& name);
    static std::string channelProof(const std::filesystem::path& path, std::string_view channel);
    static std::string channelHead(const std::filesystem::path& path);
    CirclePolicy mergeBothWays(const std::string& circle, const std::string& left, const std::string& right);
    void testDeviceCertificateBinding();
    void testUnverifiedIssuerChainRejected();
    void testCreateAndReopen();
    void testProfiles();
    void testProfileAuthority();
    void testProfileMerge();
    void testChannelProfileMerge();
    void testProfileTreeBinding();
    void testSavedStateReopen();
    void testIncrementalFastForward();
    void testIncrementalRejectsInvalidCommit();
    void testReloadAdoptsExternalWriter();
    void testAppendSignedOperation();
    void testForeignSignerRejected();
    void testUniqueGenesis();
    void testUnsignedCommitRejected();
    void testInvalidSignatureRejected();
    void testOriginalBufferTamperingRejected();
    void testWrongAuthorRejected();
    void testTreeChangesRejected();
    void testForgedDeviceRejected();
    void testCrossCircleBaseRejected();
    void testCertificateTimeRejected();
    void testConcurrentAppends();
    void testStaleWriterRejected();
    void testExistingDirectoryPreserved();
    void testInvalidIdentityRejected();
    void testUnsupportedMergeRejected();
    void testOversizedCommitRejected();
    void testAdditionalDeviceRegistration();
    void testInviteJoinAndLeave();
    void testSignedDesignation();
    void testDelegatedInvitation();
    void testLeaveRemovesAllDevices();
    void testMalformedInvitationTrees();
    void testJoinCannotSmuggleTreeChanges();
    void testRegistrationCannotSmuggleTreeChanges();
    void testLeaveRequiresCertificateRemoval();
    void testModerationVoteResolution();
    void testVoteSurvivesUnrelatedUpdates();
    void testModerationAuthority();
    void testPeerModerationProtected();
    void testPeerDegradationReplay();
    void testPromotionInvalidatesPeerVote();
    void testVoterRevisionInvalidatesVotes();
    void testTargetRevisionInvalidatesVotes();
    void testUnrelatedMemberUpdatePreservesVote();
    void testDeviceEnrollmentPreservesVote();
    void testResolutionReplayAndCycles();
    void testModerationRequiresTreeEdits();
    void testVoteCannotChangeTree();
    void testInvitedAndLeftMemberModeration();
    void testVoteSnapshotBinding();
    void testIndependentVotesCoexist();
    void testSignedPolicyMerge();
    void testPolicyMergeFastForward();
    void testConcurrentPermissionComposition();
    void testRestrictivePolicyConflicts();
    void testMergeTopologyConvergence();
    void testMemberSignsPolicyMerge();
    void testMembershipAndPolicyMerge();
    void testDelegatedRuleDivergence();
    void testPolicyMergeTampering();
    void testMergePreservesUnrelatedVotes();
    void testMergeInvalidatesAffectedVotes();
    void testMembershipFastForward();
    void testConcurrentMembershipMerge();
    void testConcurrentRoleAssignments();
    void testRevokedDelegatedEditMerge();
    void testConcurrentDeviceEnrollment();
    void testLeaveBeatsStaleEnrollment();
    void testRejoinDropsOldAssignments();
    void testConcurrentVoteMerge();
    void testDuplicateResolutionMerge();
    void testRevokedModeratorMerge();
    void testPromotionBanMerge();
    void testAdminTransferThenLeave();
    void testCircleEnd();
    void testConcurrentAdminTransfers();
    void testTransferDefeatsModeratorBan();
    void testTransferSuppressesConcurrentAdminWork();
    void testTransferToLeavingMember();
    void testChannelLog();
    void testRuntimeRouteQueries();
    void testDelegationConflictMerge();
    void testConcurrentPeerDegradation();
    void testAuthorityRegrantCannotRevive();
    void testDelegationExpansionPreservesEdit();
    void testMembershipTopologyConvergence();
    void testAssignmentIntentVoteRevision();
    void testCreateRegisteredChannel();
    void testPrivateDelegatedChannel();
    void testChannelProofRejected();
    void testOrphanChannelRegistration();
    void testConcurrentChannelRegistration();
    void testArchivedChannelMerge();
    void testSignedChannelRuleMerge();
    void testScopedChannelEditorMerge();
    void testChannelGenesisTreeRejected();
    void testRevokedChannelRegistration();
    void testConcurrentSameChannelRegistration();
    void testSignedChannelText();
    void testChannelMessagePermissions();
    void testHistoricalChannelContent();
    void testPendingChannelPolicy();
    void testChannelContentTampering();
    void testChannelMerge();
    void testChannelMergeRules();
    void testChannelFetchedUpdates();
    void testIncrementalChannelValidation();
    void testChannelEditAuthorBinding();
    void testChannelContentWrongTree();
    void testChannelReadPrerequisite();
    void testCurrentRuntimeAuthorization();
    void testMountedRuntimeSessions();
    void testRuntimeRemountInvalidatesChannels();
    void testRuntimeSessionLimit();
    void testRuntimeGrantCache();
    void testRuntimePinnedChannel();
    void testRuntimeFetchAndClone();

    CPPUNIT_TEST_SUITE(CircleRepositoryTest);
    CPPUNIT_TEST(testDeviceCertificateBinding);
    CPPUNIT_TEST(testUnverifiedIssuerChainRejected);
    CPPUNIT_TEST(testCreateAndReopen);
    CPPUNIT_TEST(testProfiles);
    CPPUNIT_TEST(testProfileAuthority);
    CPPUNIT_TEST(testProfileMerge);
    CPPUNIT_TEST(testChannelProfileMerge);
    CPPUNIT_TEST(testProfileTreeBinding);
    CPPUNIT_TEST(testSavedStateReopen);
    CPPUNIT_TEST(testIncrementalFastForward);
    CPPUNIT_TEST(testIncrementalRejectsInvalidCommit);
    CPPUNIT_TEST(testReloadAdoptsExternalWriter);
    CPPUNIT_TEST(testAppendSignedOperation);
    CPPUNIT_TEST(testForeignSignerRejected);
    CPPUNIT_TEST(testUniqueGenesis);
    CPPUNIT_TEST(testUnsignedCommitRejected);
    CPPUNIT_TEST(testInvalidSignatureRejected);
    CPPUNIT_TEST(testOriginalBufferTamperingRejected);
    CPPUNIT_TEST(testWrongAuthorRejected);
    CPPUNIT_TEST(testTreeChangesRejected);
    CPPUNIT_TEST(testForgedDeviceRejected);
    CPPUNIT_TEST(testCrossCircleBaseRejected);
    CPPUNIT_TEST(testCertificateTimeRejected);
    CPPUNIT_TEST(testConcurrentAppends);
    CPPUNIT_TEST(testStaleWriterRejected);
    CPPUNIT_TEST(testExistingDirectoryPreserved);
    CPPUNIT_TEST(testInvalidIdentityRejected);
    CPPUNIT_TEST(testUnsupportedMergeRejected);
    CPPUNIT_TEST(testOversizedCommitRejected);
    CPPUNIT_TEST(testAdditionalDeviceRegistration);
    CPPUNIT_TEST(testInviteJoinAndLeave);
    CPPUNIT_TEST(testSignedDesignation);
    CPPUNIT_TEST(testDelegatedInvitation);
    CPPUNIT_TEST(testLeaveRemovesAllDevices);
    CPPUNIT_TEST(testMalformedInvitationTrees);
    CPPUNIT_TEST(testJoinCannotSmuggleTreeChanges);
    CPPUNIT_TEST(testRegistrationCannotSmuggleTreeChanges);
    CPPUNIT_TEST(testLeaveRequiresCertificateRemoval);
    CPPUNIT_TEST(testModerationVoteResolution);
    CPPUNIT_TEST(testVoteSurvivesUnrelatedUpdates);
    CPPUNIT_TEST(testModerationAuthority);
    CPPUNIT_TEST(testPeerModerationProtected);
    CPPUNIT_TEST(testPeerDegradationReplay);
    CPPUNIT_TEST(testPromotionInvalidatesPeerVote);
    CPPUNIT_TEST(testVoterRevisionInvalidatesVotes);
    CPPUNIT_TEST(testTargetRevisionInvalidatesVotes);
    CPPUNIT_TEST(testUnrelatedMemberUpdatePreservesVote);
    CPPUNIT_TEST(testDeviceEnrollmentPreservesVote);
    CPPUNIT_TEST(testResolutionReplayAndCycles);
    CPPUNIT_TEST(testModerationRequiresTreeEdits);
    CPPUNIT_TEST(testVoteCannotChangeTree);
    CPPUNIT_TEST(testInvitedAndLeftMemberModeration);
    CPPUNIT_TEST(testVoteSnapshotBinding);
    CPPUNIT_TEST(testIndependentVotesCoexist);
    CPPUNIT_TEST(testSignedPolicyMerge);
    CPPUNIT_TEST(testPolicyMergeFastForward);
    CPPUNIT_TEST(testConcurrentPermissionComposition);
    CPPUNIT_TEST(testRestrictivePolicyConflicts);
    CPPUNIT_TEST(testMergeTopologyConvergence);
    CPPUNIT_TEST(testMemberSignsPolicyMerge);
    CPPUNIT_TEST(testMembershipAndPolicyMerge);
    CPPUNIT_TEST(testDelegatedRuleDivergence);
    CPPUNIT_TEST(testPolicyMergeTampering);
    CPPUNIT_TEST(testMergePreservesUnrelatedVotes);
    CPPUNIT_TEST(testMergeInvalidatesAffectedVotes);
    CPPUNIT_TEST(testMembershipFastForward);
    CPPUNIT_TEST(testConcurrentMembershipMerge);
    CPPUNIT_TEST(testConcurrentRoleAssignments);
    CPPUNIT_TEST(testRevokedDelegatedEditMerge);
    CPPUNIT_TEST(testConcurrentDeviceEnrollment);
    CPPUNIT_TEST(testLeaveBeatsStaleEnrollment);
    CPPUNIT_TEST(testRejoinDropsOldAssignments);
    CPPUNIT_TEST(testConcurrentVoteMerge);
    CPPUNIT_TEST(testDuplicateResolutionMerge);
    CPPUNIT_TEST(testRevokedModeratorMerge);
    CPPUNIT_TEST(testPromotionBanMerge);
    CPPUNIT_TEST(testAdminTransferThenLeave);
    CPPUNIT_TEST(testCircleEnd);
    CPPUNIT_TEST(testConcurrentAdminTransfers);
    CPPUNIT_TEST(testTransferDefeatsModeratorBan);
    CPPUNIT_TEST(testTransferSuppressesConcurrentAdminWork);
    CPPUNIT_TEST(testTransferToLeavingMember);
    CPPUNIT_TEST(testChannelLog);
    CPPUNIT_TEST(testRuntimeRouteQueries);
    CPPUNIT_TEST(testDelegationConflictMerge);
    CPPUNIT_TEST(testConcurrentPeerDegradation);
    CPPUNIT_TEST(testAuthorityRegrantCannotRevive);
    CPPUNIT_TEST(testDelegationExpansionPreservesEdit);
    CPPUNIT_TEST(testMembershipTopologyConvergence);
    CPPUNIT_TEST(testAssignmentIntentVoteRevision);
    CPPUNIT_TEST(testCreateRegisteredChannel);
    CPPUNIT_TEST(testPrivateDelegatedChannel);
    CPPUNIT_TEST(testChannelProofRejected);
    CPPUNIT_TEST(testOrphanChannelRegistration);
    CPPUNIT_TEST(testConcurrentChannelRegistration);
    CPPUNIT_TEST(testArchivedChannelMerge);
    CPPUNIT_TEST(testSignedChannelRuleMerge);
    CPPUNIT_TEST(testScopedChannelEditorMerge);
    CPPUNIT_TEST(testChannelGenesisTreeRejected);
    CPPUNIT_TEST(testRevokedChannelRegistration);
    CPPUNIT_TEST(testConcurrentSameChannelRegistration);
    CPPUNIT_TEST(testSignedChannelText);
    CPPUNIT_TEST(testChannelMessagePermissions);
    CPPUNIT_TEST(testHistoricalChannelContent);
    CPPUNIT_TEST(testPendingChannelPolicy);
    CPPUNIT_TEST(testChannelContentTampering);
    CPPUNIT_TEST(testChannelMerge);
    CPPUNIT_TEST(testChannelMergeRules);
    CPPUNIT_TEST(testChannelFetchedUpdates);
    CPPUNIT_TEST(testIncrementalChannelValidation);
    CPPUNIT_TEST(testChannelEditAuthorBinding);
    CPPUNIT_TEST(testChannelContentWrongTree);
    CPPUNIT_TEST(testChannelReadPrerequisite);
    CPPUNIT_TEST(testCurrentRuntimeAuthorization);
    CPPUNIT_TEST(testMountedRuntimeSessions);
    CPPUNIT_TEST(testRuntimeRemountInvalidatesChannels);
    CPPUNIT_TEST(testRuntimeSessionLimit);
    CPPUNIT_TEST(testRuntimeGrantCache);
    CPPUNIT_TEST(testRuntimePinnedChannel);
    CPPUNIT_TEST(testRuntimeFetchAndClone);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CircleRepositoryTest, CircleRepositoryTest::name());

void
CircleRepositoryTest::testProfiles()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    CPPUNIT_ASSERT(repository->profile().empty());
    const std::string vcard = "BEGIN:VCARD\nVERSION:2.1\nFN:Studio\nPHOTO;ENCODING=BASE64:"
                              + std::string(256 * 1024, 'A') + "\nEND:VCARD\n";
    CPPUNIT_ASSERT(repository->updateProfile(identity(), vcard));
    CPPUNIT_ASSERT_EQUAL(vcard, repository->profile());
    auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened);
    CPPUNIT_ASSERT_EQUAL(vcard, reopened->profile());
    const std::string renamed = "BEGIN:VCARD\nFN:Design studio\nEND:VCARD\n";
    CPPUNIT_ASSERT(reopened->updateProfile(identity(), renamed));
    CPPUNIT_ASSERT_EQUAL(renamed, reopened->profile());
    const auto channel = reopened->createChannel(peerPath_, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(reopened->updateProfile(identity(), renamed, channel->channel));
    CPPUNIT_ASSERT_EQUAL(renamed, reopened->profile(channel->channel));
    CPPUNIT_ASSERT_EQUAL(renamed, reopened->profile());
    CPPUNIT_ASSERT(!reopened->updateProfile(identity(), vcard, std::string(40, 'f')));
    reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened);
    CPPUNIT_ASSERT_EQUAL(renamed, reopened->profile(channel->channel));
}

void
CircleRepositoryTest::testProfileAuthority()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto member = addMember(*repository, "profile-editor");
    const auto uri = member.second->issuer->getId().toString();
    const std::string vcard = "BEGIN:VCARD\nFN:Studio\nEND:VCARD\n";
    CPPUNIT_ASSERT(!repository->updateProfile(member, vcard));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::UpdateProfile}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {uri, "editor"}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, uri});
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(repository->updateProfile(member, vcard));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    const auto channel = repository->createChannel(peerPath_, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(!repository->updateProfile(member, vcard, channel->channel));
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRole {"manager", {{Permission::ManageChannels, Permission::ReadChannel}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {uri, "manager"}));
    CPPUNIT_ASSERT(repository->updateProfile(member, vcard, channel->channel));
    CPPUNIT_ASSERT(repository->append(identity(), EndCircle {}));
    CPPUNIT_ASSERT(!repository->updateProfile(identity(), vcard + "\n"));
    CPPUNIT_ASSERT(!repository->updateProfile(identity(), vcard + "\n", channel->channel));
    CPPUNIT_ASSERT(openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testProfileMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto member = addMember(*repository, "profile-editor");
    const auto uri = member.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::UpdateProfile}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {uri, "editor"}));
    const auto base = repository->head();
    const std::string first = "BEGIN:VCARD\nFN:First\nEND:VCARD\n";
    const std::string second = "BEGIN:VCARD\nFN:Second\nEND:VCARD\n";
    const auto left = repository->updateProfile(member, first);
    CPPUNIT_ASSERT(left);
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto right = other->updateProfile(identity(), second);
    CPPUNIT_ASSERT(right);
    const auto expected = *left < *right ? first : second;
    setHead(*left);
    CPPUNIT_ASSERT(repository->merge(identity(), *right));
    CPPUNIT_ASSERT_EQUAL(expected, repository->profile());
    setHead(*right);
    CPPUNIT_ASSERT(other->merge(identity(), *left));
    CPPUNIT_ASSERT_EQUAL(expected, other->profile());
    CPPUNIT_ASSERT_EQUAL(expected, openReplayed(path_, circle)->profile());
    CPPUNIT_ASSERT(other->updateProfile(identity(), first));
    CPPUNIT_ASSERT_EQUAL(first, openReplayed(path_, circle)->profile());
    setHead(base);
    other = openReplayed(path_, circle);
    const auto revoke = other->append(identity(), SetCircleRole {"editor", {}});
    CPPUNIT_ASSERT(revoke);
    setHead(*left);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository->merge(identity(), *revoke));
    CPPUNIT_ASSERT(repository->profile().empty());
    CPPUNIT_ASSERT(openReplayed(path_, circle)->profile().empty());
}

void
CircleRepositoryTest::testProfileTreeBinding()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto base = repository->head();
    const SetCircleProfile profile {};
    const auto record = CircleRecord {CirclePolicyChange {profile}, circle, base}.toString();
    CPPUNIT_ASSERT(record && CircleRecord::fromString(*record));
    CPPUNIT_ASSERT(!circlePolicyChangeFromJson(R"({"type":"set-profile","channel":"../profile"})"));
    CPPUNIT_ASSERT(!circlePolicyChangeFromJson(R"({"type":"set-profile","channel":null,"avatar":"x"})"));
    TamperOptions missing;
    missing.message = record;
    tamper(*repository, missing);
    CPPUNIT_ASSERT(!openReplayed(path_, circle));
    setHead(base);
    CPPUNIT_ASSERT(repository->updateProfile(identity(), "BEGIN:VCARD\nFN:Studio\nEND:VCARD\n"));
    TamperOptions smuggled;
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path_.string().c_str()) == 0);
    GitRepository git(raw);
    git_oid head {};
    CPPUNIT_ASSERT(git_oid_fromstr(&head, repository->head().c_str()) == 0);
    git_commit* commitRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&commitRaw, git.get(), &head) == 0);
    GitCommit commit(commitRaw);
    smuggled.tree = *git_commit_tree_id(commit.get());
    smuggled.parent = base;
    smuggled.message = CircleRecord {CirclePolicyChange {SetCircleRole {"reader", {}}}, circle, base}.toString();
    tamper(*repository, smuggled);
    CPPUNIT_ASSERT(!openReplayed(path_, circle));
}

void
CircleRepositoryTest::testChannelProfileMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto channel = repository->createChannel(peerPath_, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const std::string first = "BEGIN:VCARD\nFN:general\nEND:VCARD\n";
    const std::string second = "BEGIN:VCARD\nFN:ideas\nEND:VCARD\n";
    CPPUNIT_ASSERT(repository->updateProfile(identity(), first));
    const auto base = repository->head();
    const auto left = repository->updateProfile(identity(), first, channel->channel);
    CPPUNIT_ASSERT(left);
    setHead(base);
    auto other = openReplayed(path_, circle);
    const auto right = other->updateProfile(identity(), second, channel->channel);
    CPPUNIT_ASSERT(right);
    const auto expected = *left < *right ? first : second;
    setHead(*left);
    CPPUNIT_ASSERT(repository->merge(identity(), *right));
    CPPUNIT_ASSERT_EQUAL(expected, repository->profile(channel->channel));
    CPPUNIT_ASSERT_EQUAL(first, repository->profile());
    setHead(*right);
    CPPUNIT_ASSERT(other->merge(identity(), *left));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened);
    CPPUNIT_ASSERT_EQUAL(expected, reopened->profile(channel->channel));
    CPPUNIT_ASSERT_EQUAL(first, reopened->profile());
}

const dht::crypto::Identity&
CircleRepositoryTest::identity()
{
    static const auto identity = dht::crypto::generateIdentity("circle-device", accountIdentity(), 2048, false);
    return identity;
}

const dht::crypto::Identity&
CircleRepositoryTest::accountIdentity()
{
    static const auto account = [] {
        const auto authority = dht::crypto::generateIdentity("circle-authority", {}, 2048, true);
        return dht::crypto::generateIdentity("circle-account", authority, 2048, true);
    }();
    return account;
}

void
CircleRepositoryTest::setUp()
{
    path_ = std::filesystem::temp_directory_path() / ("jami-circle-test-" + dht::InfoHash::getRandom().toString());
    peerPath_ = path_;
    peerPath_ += "-peer";
}

void
CircleRepositoryTest::tearDown()
{
    std::error_code error;
    std::filesystem::remove_all(peerPath_, error);
    std::filesystem::remove_all(path_, error);
    CPPUNIT_ASSERT(!error);
}

std::optional<std::string>
CircleRepositoryTest::fetchFrom(const std::filesystem::path& repository,
                                const std::filesystem::path& source,
                                std::string_view ref)
{
    git_repository* raw = nullptr;
    if (git_repository_open_bare(&raw, repository.string().c_str()) < 0)
        return std::nullopt;
    GitRepository repo(raw);
    git_remote* remoteRaw = nullptr;
    if (git_remote_create_anonymous(&remoteRaw, repo.get(), source.string().c_str()) < 0)
        return std::nullopt;
    std::unique_ptr<git_remote, decltype(&git_remote_free)> remote(remoteRaw, git_remote_free);
    auto spec = "+refs/heads/main:" + std::string(ref);
    char* specs[] {spec.data()};
    const git_strarray refspecs {specs, 1};
    git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
    git_oid head {};
    if (git_remote_fetch(remote.get(), &refspecs, &options, "test fetch") < 0
        || git_reference_name_to_id(&head, repo.get(), std::string(ref).c_str()) < 0)
        return std::nullopt;
    return git_oid_tostr_s(&head);
}

void
CircleRepositoryTest::testSavedStateReopen()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    const auto policy = repository->policy();
    repository.reset();
    // A budget of one commit cannot replay two: the saved state needs no replay at all.
    CPPUNIT_ASSERT(CircleRepository::open(path_, circle, 1));
    CPPUNIT_ASSERT(!openReplayed(path_, circle, 1));
    auto reopened = CircleRepository::open(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->head() == *first && reopened->policy() == policy);
    CPPUNIT_ASSERT(reopened->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}}));
    const auto latest = CircleRepository::open(path_, circle, 1);
    CPPUNIT_ASSERT(latest && latest->head() == reopened->head() && latest->policy() == reopened->policy());
}

void
CircleRepositoryTest::testIncrementalFastForward()
{
    auto writer = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(writer);
    const auto circle = writer->id();
    CPPUNIT_ASSERT(writer->append(identity(), SetCircleRole {"base", {}}));
    std::filesystem::copy(path_, peerPath_, std::filesystem::copy_options::recursive);
    auto reader = CircleRepository::open(peerPath_, circle);
    CPPUNIT_ASSERT(reader);
    const auto generation = reader->generation();
    const auto first = writer->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    const auto second = writer->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}});
    CPPUNIT_ASSERT(first && second);
    const auto fetched = fetchFrom(peerPath_, path_, "refs/jami/incoming");
    CPPUNIT_ASSERT(fetched == second);
    CPPUNIT_ASSERT(!reader->fastForward(*fetched, 1));
    CPPUNIT_ASSERT(reader->fastForward(*fetched, 2) == second);
    CPPUNIT_ASSERT(reader->policy() == writer->policy() && reader->generation() > generation);
    CPPUNIT_ASSERT(reader->fastForward(*first) == second);
    const auto local = reader->append(identity(), SetCircleRole {"left", {}});
    const auto remote = writer->append(identity(), SetCircleRole {"right", {}});
    CPPUNIT_ASSERT(local && remote);
    CPPUNIT_ASSERT(fetchFrom(peerPath_, path_, "refs/jami/incoming") == remote);
    CPPUNIT_ASSERT(!reader->fastForward(*remote));
    CPPUNIT_ASSERT(reader->head() == *local);
    CPPUNIT_ASSERT(!reader->fastForward(std::string(40, '1')));
}

void
CircleRepositoryTest::testIncrementalRejectsInvalidCommit()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    const auto head = repository->head();
    TamperOptions options;
    options.signature = Signature::INVALID;
    tamper(*repository, options);
    CPPUNIT_ASSERT(!repository->reload());
    CPPUNIT_ASSERT(repository->head() == head);
    CPPUNIT_ASSERT(!CircleRepository::open(path_, circle));
    setHead(head);
    CPPUNIT_ASSERT(CircleRepository::open(path_, circle));
    std::ofstream(path_ / CircleRepository::STATE_FILE, std::ios::trunc) << "corrupted";
    const auto reopened = CircleRepository::open(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->head() == head && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testReloadAdoptsExternalWriter()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto other = CircleRepository::open(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto generation = repository->generation();
    const auto first = other->append(identity(), SetCircleRole {"writer", {}});
    CPPUNIT_ASSERT(first);
    CPPUNIT_ASSERT(!repository->authorizePeer(*identity().second));
    CPPUNIT_ASSERT(repository->reload());
    CPPUNIT_ASSERT(repository->head() == *first && repository->policy() == other->policy());
    CPPUNIT_ASSERT(repository->generation() > generation);
    CPPUNIT_ASSERT(repository->authorizePeer(*identity().second));
    setHead(circle);
    CPPUNIT_ASSERT(repository->reload() && repository->head() == circle);
}

void
CircleRepositoryTest::testCreateAndReopen()
{
    auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto id = repository->id();
    CPPUNIT_ASSERT(id.size() == 40 && repository->head() == id);
    CPPUNIT_ASSERT(repository->policy().admin == identity().second->issuer->getId().toString());
    repository.reset();
    repository = openReplayed(path_, id);
    CPPUNIT_ASSERT(repository && repository->policy().isValid());
    CPPUNIT_ASSERT(!openReplayed(path_, std::string(40, 'a')));
    CPPUNIT_ASSERT(!openReplayed(path_, "invalid"));
}

void
CircleRepositoryTest::testAppendSignedOperation()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto id = repository->id();
    const auto commit = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(commit && *commit != id && repository->head() == *commit);
    const auto reopened = openReplayed(path_, id);
    CPPUNIT_ASSERT(reopened && reopened->head() == *commit);
    CPPUNIT_ASSERT(reopened->policy() == repository->policy());
    CPPUNIT_ASSERT(!openReplayed(path_, id, 1));
    CPPUNIT_ASSERT(openReplayed(path_, id, 2));
}

void
CircleRepositoryTest::testForeignSignerRejected()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto before = repository->head();
    const auto foreignAccount = dht::crypto::generateIdentity("foreign-account", {}, 2048, true);
    const auto foreign = dht::crypto::generateIdentity("foreign-device", foreignAccount, 2048, false);
    CPPUNIT_ASSERT(!repository->append(foreign, SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->head() == before);
    CPPUNIT_ASSERT(!repository->append(identity(), SetCircleRole {"../role", {}}));
    CPPUNIT_ASSERT(repository->head() == before);
}

void
CircleRepositoryTest::testUniqueGenesis()
{
    const auto first = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(first);
    const auto second = CircleRepository::create(path_ / "another", identity());
    CPPUNIT_ASSERT(second && first->id() != second->id());
}

void
CircleRepositoryTest::testUnsignedCommitRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    TamperOptions options;
    options.signature = Signature::MISSING;
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testInvalidSignatureRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    for (auto signature : {Signature::INVALID, Signature::MALFORMED, Signature::EMPTY}) {
        TamperOptions options;
        options.signature = signature;
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    }
}

void
CircleRepositoryTest::testOriginalBufferTamperingRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    TamperOptions options;
    options.signature = Signature::ALTERED_BODY;
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testWrongAuthorRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    TamperOptions options;
    options.device = std::string(64, 'a');
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testTreeChangesRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    TamperOptions options;
    options.tree = changedTree(*repository, nullptr);
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    options.root = true;
    const auto forgedRoot = tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, forgedRoot));
}

void
CircleRepositoryTest::testForgedDeviceRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto rogueAccount = dht::crypto::generateIdentity("rogue-account", {}, 2048, true);
    auto rogue = dht::crypto::generateIdentity("rogue-device", rogueAccount, 2048, false);
    rogue.second->issuer = identity().second->issuer;
    TamperOptions options;
    options.root = true;
    options.signer = &rogue;
    options.tree = changedTree(*repository, rogue.second.get());
    const auto forgedRoot = tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, forgedRoot));
}

void
CircleRepositoryTest::testCrossCircleBaseRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    CircleRecord record {CirclePolicyChange {SetCircleRole {"writer", {}}}, repository->id(), repository->head()};
    for (auto field : {&record.circle, &record.base}) {
        const auto previous = *field;
        *field = std::string(40, 'a');
        TamperOptions options;
        options.message = record.toString();
        CPPUNIT_ASSERT(options.message);
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
        *field = previous;
    }
}

void
CircleRepositoryTest::testCertificateTimeRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    for (auto when : {gnutls_x509_crt_get_activation_time(identity().second->cert) - 1,
                      gnutls_x509_crt_get_expiration_time(identity().second->cert) + 1}) {
        TamperOptions options;
        options.when = when;
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    }
}

void
CircleRepositoryTest::testConcurrentAppends()
{
    auto first = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(first);
    auto second = openReplayed(path_, first->id());
    CPPUNIT_ASSERT(second);
    std::array<CircleRepository*, 2> stores {first.get(), second.get()};
    std::array<std::optional<std::string>, 2> results;
    std::array<std::thread, 2> threads;
    std::mutex mutex;
    std::condition_variable ready;
    bool start = false;
    for (size_t index = 0; index < threads.size(); ++index) {
        threads[index] = std::thread([&, index] {
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&] { return start; });
            }
            results[index] = stores[index]->append(identity(), SetCircleRole {"writer" + std::to_string(index), {}});
        });
    }
    {
        std::lock_guard lock(mutex);
        start = true;
    }
    ready.notify_all();
    for (auto& thread : threads)
        thread.join();
    CPPUNIT_ASSERT(results[0].has_value() != results[1].has_value());
    const auto reopened = openReplayed(path_, first->id());
    CPPUNIT_ASSERT(reopened && reopened->policy().roles.size() == 1);
    CPPUNIT_ASSERT(reopened->head() == *results[results[0] ? 0 : 1]);
}

void
CircleRepositoryTest::testStaleWriterRejected()
{
    const auto first = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(first);
    const auto stale = openReplayed(path_, first->id());
    CPPUNIT_ASSERT(stale);
    CPPUNIT_ASSERT(first->append(identity(), SetCircleRole {"first", {}}));
    CPPUNIT_ASSERT(!stale->append(identity(), SetCircleRole {"second", {}}));
    CPPUNIT_ASSERT(stale->head() == stale->id());
    auto fresh = openReplayed(path_, first->id());
    CPPUNIT_ASSERT(fresh && fresh->append(identity(), SetCircleRole {"second", {}}));
    const auto reopened = openReplayed(path_, first->id());
    CPPUNIT_ASSERT(reopened && reopened->policy().roles.size() == 2);
}

void
CircleRepositoryTest::testExistingDirectoryPreserved()
{
    CPPUNIT_ASSERT(std::filesystem::create_directory(path_));
    {
        std::ofstream marker(path_ / "keep");
        marker << "keep";
    }
    CPPUNIT_ASSERT(!CircleRepository::create(path_, identity()));
    CPPUNIT_ASSERT(std::filesystem::exists(path_ / "keep"));
    CPPUNIT_ASSERT(!std::filesystem::exists(path_ / "config"));
}

void
CircleRepositoryTest::testInvalidIdentityRejected()
{
    CPPUNIT_ASSERT(!CircleRepository::create(path_, {}));
    CPPUNIT_ASSERT(!std::filesystem::exists(path_));
    auto mismatch = identity();
    mismatch.first = dht::crypto::generateIdentity("mismatch", {}, 2048, true).first;
    CPPUNIT_ASSERT(!CircleRepository::create(path_, mismatch));
    CPPUNIT_ASSERT(!std::filesystem::exists(path_));
    CPPUNIT_ASSERT(!CircleRepository::create(path_, identity(), {{Permission::Count}, {}}));
    CPPUNIT_ASSERT(!std::filesystem::exists(path_));
}

void
CircleRepositoryTest::testUnsupportedMergeRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    TamperOptions options;
    options.merge = true;
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testOversizedCommitRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    TamperOptions options;
    options.message = std::string(CircleRecord::MAX_SIZE + 32 * 1024, ' ');
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

std::string
CircleRepositoryTest::tamper(const CircleRepository& repository, const TamperOptions& options)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, options.repository.value_or(path_).string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid previous {};
    CPPUNIT_ASSERT(git_oid_fromstr(&previous, options.parent.value_or(repository.head()).c_str()) == 0);
    git_commit* parentRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&parentRaw, repo.get(), &previous) == 0);
    GitCommit parent(parentRaw);
    git_tree* treeRaw = nullptr;
    CPPUNIT_ASSERT(
        git_tree_lookup(&treeRaw, repo.get(), options.tree ? &*options.tree : git_commit_tree_id(parent.get())) == 0);
    GitTree tree(treeRaw);
    const auto& signer = options.signer ? *options.signer : identity();
    const auto device = options.device.empty() ? signer.second->getLongId().toString() : options.device;
    git_signature* signatureRaw = nullptr;
    CPPUNIT_ASSERT(
        git_signature_new(&signatureRaw, "test", device.c_str(), options.when.value_or(std::time(nullptr)), 0) == 0);
    GitSignature signature(signatureRaw);
    const CircleRecord record {CirclePolicyChange {SetCircleRole {"writer", {}}}, repository.id(), repository.head()};
    const auto message = options.message.value_or(options.root ? std::string(git_commit_message_raw(parent.get()))
                                                               : *record.toString());
    GitCommit second;
    if (options.otherParent) {
        git_oid oid {};
        CPPUNIT_ASSERT(git_oid_fromstr(&oid, options.otherParent->c_str()) == 0);
        git_commit* raw = nullptr;
        CPPUNIT_ASSERT(git_commit_lookup(&raw, repo.get(), &oid) == 0);
        second.reset(raw);
    }
    const git_commit* parents[] {parent.get(), second ? second.get() : parent.get()};
    git_buf buffer {};
    GitBuf guard(&buffer);
    CPPUNIT_ASSERT(git_commit_create_buffer(&buffer,
                                            repo.get(),
                                            signature.get(),
                                            signature.get(),
                                            nullptr,
                                            message.c_str(),
                                            tree.get(),
                                            options.root    ? 0
                                            : options.merge ? 2
                                                            : 1,
                                            options.root ? nullptr : parents)
                   == 0);
    git_oid oid {};
    if (options.signature == Signature::MISSING) {
        git_odb* odbRaw = nullptr;
        CPPUNIT_ASSERT(git_repository_odb(&odbRaw, repo.get()) == 0);
        GitOdb odb(odbRaw);
        CPPUNIT_ASSERT(git_odb_write(&oid, odb.get(), buffer.ptr, buffer.size, GIT_OBJECT_COMMIT) == 0);
    } else {
        const auto bytes = signer.first->sign(reinterpret_cast<const uint8_t*>(buffer.ptr), buffer.size);
        gnutls_datum_t input {const_cast<unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size())},
            encoded {};
        CPPUNIT_ASSERT(gnutls_base64_encode2(&input, &encoded) == 0);
        std::unique_ptr<unsigned char, decltype(gnutls_free)> encodedGuard(encoded.data, gnutls_free);
        const auto text = options.signature == Signature::INVALID     ? std::string("AAAA")
                          : options.signature == Signature::MALFORMED ? std::string("%%%")
                          : options.signature == Signature::EMPTY
                              ? std::string("====")
                              : std::string(reinterpret_cast<const char*>(encoded.data), encoded.size);
        std::string contents(buffer.ptr, buffer.size);
        if (options.signature == Signature::ALTERED_BODY) {
            const auto position = contents.find("writer");
            CPPUNIT_ASSERT(position != std::string::npos);
            contents.replace(position, 6, "hacker");
        }
        CPPUNIT_ASSERT(git_commit_create_with_signature(&oid, repo.get(), contents.c_str(), text.c_str(), "signature")
                       == 0);
    }
    git_reference* referenceRaw = nullptr;
    CPPUNIT_ASSERT(git_reference_create(&referenceRaw, repo.get(), "refs/heads/main", &oid, 1, "test") == 0);
    GitReference reference(referenceRaw);
    return git_oid_tostr_s(&oid);
}

git_oid
CircleRepositoryTest::changedTree(const CircleRepository& repository, const dht::crypto::Certificate* device)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path_.string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid head {};
    CPPUNIT_ASSERT(git_oid_fromstr(&head, repository.head().c_str()) == 0);
    git_commit* commitRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&commitRaw, repo.get(), &head) == 0);
    GitCommit commit(commitRaw);
    git_tree* treeRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_tree(&treeRaw, commit.get()) == 0);
    GitTree tree(treeRaw);
    using Builder = std::unique_ptr<git_treebuilder, decltype(&git_treebuilder_free)>;
    git_treebuilder* rootRaw = nullptr;
    CPPUNIT_ASSERT(git_treebuilder_new(&rootRaw, repo.get(), tree.get()) == 0);
    Builder root(rootRaw, git_treebuilder_free);
    const auto contents = device ? device->toString(true) : std::string("unexpected");
    git_oid blob {};
    CPPUNIT_ASSERT(git_blob_create_frombuffer(&blob, repo.get(), contents.data(), contents.size()) == 0);
    if (device) {
        git_treebuilder* childRaw = nullptr;
        CPPUNIT_ASSERT(git_treebuilder_new(&childRaw, repo.get(), nullptr) == 0);
        Builder child(childRaw, git_treebuilder_free);
        const auto name = device->getLongId().toString() + ".crt";
        CPPUNIT_ASSERT(git_treebuilder_insert(nullptr, child.get(), name.c_str(), &blob, GIT_FILEMODE_BLOB) == 0);
        git_oid childId {};
        CPPUNIT_ASSERT(git_treebuilder_write(&childId, child.get()) == 0);
        CPPUNIT_ASSERT(git_treebuilder_insert(nullptr, root.get(), "devices", &childId, GIT_FILEMODE_TREE) == 0);
    } else {
        CPPUNIT_ASSERT(git_treebuilder_insert(nullptr, root.get(), "unexpected", &blob, GIT_FILEMODE_BLOB) == 0);
    }
    git_oid result {};
    CPPUNIT_ASSERT(git_treebuilder_write(&result, root.get()) == 0);
    return result;
}

void
CircleRepositoryTest::testAdditionalDeviceRegistration()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto device = dht::crypto::generateIdentity("second-device", accountIdentity(), 2048, false);
    const auto member = accountIdentity().second->getId().toString();
    const auto deviceId = device.second->getLongId().toString();
    CPPUNIT_ASSERT(!repository->append(device, SetCircleRole {"unregistered", {}}));
    CPPUNIT_ASSERT(
        !repository->append(device, RegisterCircleDevice {member, identity().second->getLongId().toString()}));
    CPPUNIT_ASSERT(repository->append(device, RegisterCircleDevice {member, deviceId}));
    CPPUNIT_ASSERT(!repository->append(device, RegisterCircleDevice {member, deviceId}));
    CPPUNIT_ASSERT(repository->append(device, SetCircleRole {"from-new-device", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"from-founder-device", {}}));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testInviteJoinAndLeave()
{
    const auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("member-account", {}, 2048, true);
    const auto device = dht::crypto::generateIdentity("member-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(!repository->append(device, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::INVITED);
    CPPUNIT_ASSERT(!repository->append(device, RegisterCircleDevice {member, device.second->getLongId().toString()}));
    CPPUNIT_ASSERT(!repository->append(identity(), JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(!repository->append(device, SetCircleRole {"admin-role", {}}));
    CPPUNIT_ASSERT(repository->append(device, LeaveCircleMember {member}));
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::LEFT);
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testSignedDesignation()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("designator-account", {}, 2048, true);
    const auto device = dht::crypto::generateIdentity("designator-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"designator", {{Permission::AssignRoles}, {}}}));
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRoleDelegation {"designator", RoleDelegation {{}, {}, {"moderator"}, {}, true}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "designator"}));
    CPPUNIT_ASSERT(!repository->policy().hasPermission(member, Permission::BanUnbanMember));
    CPPUNIT_ASSERT(repository->append(device, SetCircleRoleAssignment {member, "moderator"}));
    CPPUNIT_ASSERT(repository->policy().hasPermission(member, Permission::BanUnbanMember));
    CPPUNIT_ASSERT(!repository->append(device, SetCircleRole {"moderator", {}}));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testDelegatedInvitation()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("inviter-account", {}, 2048, true);
    const auto device = dht::crypto::generateIdentity("inviter-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    const auto otherAccount = dht::crypto::generateIdentity("invitee-account", {}, 2048, true);
    const auto other = dht::crypto::generateIdentity("invitee-device", otherAccount, 2048, false);
    const auto invitee = otherAccount.second->getId().toString();
    CPPUNIT_ASSERT(!repository->append(device, InviteCircleMember {invitee}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"inviter", {{Permission::AddMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "inviter"}));
    CPPUNIT_ASSERT(repository->append(device, InviteCircleMember {invitee}));
    CPPUNIT_ASSERT(repository->append(other, JoinCircleMember {invitee}));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testLeaveRemovesAllDevices()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("leaving-account", {}, 2048, true);
    const auto first = dht::crypto::generateIdentity("leaving-device", account, 2048, false);
    const auto second = dht::crypto::generateIdentity("leaving-second-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(first, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(second, RegisterCircleDevice {member, second.second->getLongId().toString()}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    CPPUNIT_ASSERT(repository->append(second, LeaveCircleMember {member}));
    CPPUNIT_ASSERT(repository->policy().members.at(member).roles.empty());
    for (const auto& device : {first, second}) {
        CPPUNIT_ASSERT(!repository->append(device, JoinCircleMember {member}));
        CPPUNIT_ASSERT(
            !repository->append(device, RegisterCircleDevice {member, device.second->getLongId().toString()}));
    }
    auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path_.string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_oid_fromstr(&oid, repository->head().c_str()) == 0);
    git_commit* commitRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&commitRaw, repo.get(), &oid) == 0);
    GitCommit commit(commitRaw);
    git_tree* treeRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_tree(&treeRaw, commit.get()) == 0);
    GitTree tree(treeRaw);
    for (const auto& device : {first, second}) {
        git_tree_entry* entry = nullptr;
        const auto path = "devices/" + device.second->getLongId().toString() + ".crt";
        CPPUNIT_ASSERT(git_tree_entry_bypath(&entry, tree.get(), path.c_str()) == GIT_ENOTFOUND);
    }
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(first, JoinCircleMember {member}));
    CPPUNIT_ASSERT(!repository->policy().hasPermission(member, Permission::BanUnbanMember));
    reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testMalformedInvitationTrees()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string member(40, 'a');
    const CircleRecord record {CirclePolicyChange {InviteCircleMember {member}}, repository->id(), repository->head()};
    const auto message = record.toString();
    CPPUNIT_ASSERT(message);
    for (auto mode : {GIT_FILEMODE_BLOB, GIT_FILEMODE_BLOB_EXECUTABLE, GIT_FILEMODE_LINK}) {
        TamperOptions options;
        options.message = message;
        options.tree = invitationTree(*repository, member, "unexpected", mode);
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    }
    TamperOptions options;
    options.message = message;
    options.tree = invitationTree(*repository, std::string(40, 'b'), "", GIT_FILEMODE_BLOB);
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    options.tree = invitationTree(*repository, member, "", GIT_FILEMODE_BLOB, true);
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testJoinCannotSmuggleTreeChanges()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("joining-account", {}, 2048, true);
    const auto device = dht::crypto::generateIdentity("joining-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    const auto invited = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(invited);
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    const CircleRecord record {CirclePolicyChange {JoinCircleMember {member}}, invited->id(), invited->head()};
    TamperOptions options;
    options.signer = &device;
    options.message = record.toString();
    options.tree = changedTree(*repository, nullptr);
    tamper(*invited, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    options.signer = &identity();
    tamper(*invited, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testRegistrationCannotSmuggleTreeChanges()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto before = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(before);
    const auto device = dht::crypto::generateIdentity("registered-device", accountIdentity(), 2048, false);
    const auto member = accountIdentity().second->getId().toString();
    const auto deviceId = device.second->getLongId().toString();
    CPPUNIT_ASSERT(repository->append(device, RegisterCircleDevice {member, deviceId}));
    const CircleRecord record {CirclePolicyChange {RegisterCircleDevice {member, deviceId}},
                               before->id(),
                               before->head()};
    TamperOptions options;
    options.signer = &device;
    options.message = record.toString();
    options.tree = changedTree(*repository, nullptr);
    tamper(*before, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testLeaveRequiresCertificateRemoval()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("departing-account", {}, 2048, true);
    const auto device = dht::crypto::generateIdentity("departing-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    const CircleRecord record {CirclePolicyChange {LeaveCircleMember {member}}, repository->id(), repository->head()};
    TamperOptions options;
    options.signer = &device;
    options.message = record.toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

git_oid
CircleRepositoryTest::invitationTree(const CircleRepository& repository,
                                     const std::string& member,
                                     std::string_view contents,
                                     git_filemode_t mode,
                                     bool oversized)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path_.string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid head {};
    CPPUNIT_ASSERT(git_oid_fromstr(&head, repository.head().c_str()) == 0);
    git_commit* commitRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&commitRaw, repo.get(), &head) == 0);
    GitCommit commit(commitRaw);
    git_tree* treeRaw = nullptr;
    CPPUNIT_ASSERT(git_commit_tree(&treeRaw, commit.get()) == 0);
    GitTree tree(treeRaw);
    using Builder = std::unique_ptr<git_treebuilder, decltype(&git_treebuilder_free)>;
    git_treebuilder* rootRaw = nullptr;
    CPPUNIT_ASSERT(git_treebuilder_new(&rootRaw, repo.get(), tree.get()) == 0);
    Builder root(rootRaw, git_treebuilder_free);
    git_oid childId {};
    if (oversized) {
        git_odb* odbRaw = nullptr;
        CPPUNIT_ASSERT(git_repository_odb(&odbRaw, repo.get()) == 0);
        GitOdb odb(odbRaw);
        const std::string huge(4 * 1024 * 1024 + 1, 'x');
        CPPUNIT_ASSERT(git_odb_write(&childId, odb.get(), huge.data(), huge.size(), GIT_OBJECT_TREE) == 0);
    } else {
        git_treebuilder* childRaw = nullptr;
        CPPUNIT_ASSERT(git_treebuilder_new(&childRaw, repo.get(), nullptr) == 0);
        Builder child(childRaw, git_treebuilder_free);
        git_oid blob {};
        CPPUNIT_ASSERT(git_blob_create_frombuffer(&blob, repo.get(), contents.data(), contents.size()) == 0);
        CPPUNIT_ASSERT(git_treebuilder_insert(nullptr, child.get(), member.c_str(), &blob, mode) == 0);
        CPPUNIT_ASSERT(git_treebuilder_write(&childId, child.get()) == 0);
    }
    CPPUNIT_ASSERT(git_treebuilder_insert(nullptr, root.get(), "invited", &childId, GIT_FILEMODE_TREE) == 0);
    git_oid result {};
    CPPUNIT_ASSERT(git_treebuilder_write(&result, root.get()) == 0);
    return result;
}

void
CircleRepositoryTest::testModerationVoteResolution()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("moderated-account", {}, 2048, true);
    const auto device = dht::crypto::generateIdentity("moderated-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(vote && repository->pendingVotes() == std::vector<std::string> {*vote});
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(!repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member}));
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *vote));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::BANNED);
    CPPUNIT_ASSERT(repository->policy().members.at(member).roles.empty());
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *vote));
    CPPUNIT_ASSERT(!repository->append(device, JoinCircleMember {member}));
    const auto unban = repository->moderate(identity(), {CircleModerationAction::UNBAN_MEMBER, member});
    CPPUNIT_ASSERT(unban && unban->resolution && repository->pendingVotes().empty());
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::INVITED);
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    CPPUNIT_ASSERT(!repository->policy().hasPermission(member, Permission::BanUnbanMember));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy() && reopened->pendingVotes().empty());
}

void
CircleRepositoryTest::testVoteSurvivesUnrelatedUpdates()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"unused-role", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->pendingVotes() == repository->pendingVotes());
    CPPUNIT_ASSERT(reopened->resolveVote(identity(), *vote));
    CPPUNIT_ASSERT(reopened->policy().members.at(target).state == CircleMemberState::BANNED);
}

void
CircleRepositoryTest::testModerationAuthority()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto memberDevice = addMember(*repository, "ordinary");
    const auto member = memberDevice.second->issuer->getId().toString();
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto original = repository->head();
    CPPUNIT_ASSERT(!repository->vote(memberDevice, {CircleModerationAction::BAN_MEMBER, target}));
    CPPUNIT_ASSERT(!repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, repository->policy().admin}));
    CPPUNIT_ASSERT(repository->head() == original);
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(!repository->resolveVote(memberDevice, *vote));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    const auto competing = repository->vote(memberDevice, {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(competing && repository->pendingVotes().size() == 2);
    CPPUNIT_ASSERT(repository->resolveVote(memberDevice, *vote));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *competing));
    CPPUNIT_ASSERT(!repository->vote(memberDevice, {CircleModerationAction::BAN_MEMBER, member}));
    CPPUNIT_ASSERT(!repository->vote(memberDevice, {CircleModerationAction::BAN_MEMBER, repository->policy().admin}));
}

void
CircleRepositoryTest::testPeerModerationProtected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto moderator = addMember(*repository, "moderator");
    const auto peer = addMember(*repository, "peer");
    const auto actor = moderator.second->issuer->getId().toString();
    const auto target = peer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "moderator"}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {target, "moderator"}));
    const CircleModerationVote ban {CircleModerationAction::BAN_MEMBER, target};
    const auto original = repository->head();
    CPPUNIT_ASSERT(!repository->vote(moderator, ban));
    CPPUNIT_ASSERT(!repository->moderate(moderator, ban));
    CPPUNIT_ASSERT(repository->head() == original && repository->pendingVotes().empty());
    TamperOptions options;
    options.signer = &moderator;
    options.message = CircleRecord {ban, repository->id(), original}.toString();
    CPPUNIT_ASSERT(options.message);
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    setHead(original);
    const auto vote = repository->vote(identity(), ban);
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(!repository->resolveVote(moderator, *vote));
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *vote));
    CPPUNIT_ASSERT(repository->policy().members.at(target).state == CircleMemberState::BANNED);
    const auto unban = repository->moderate(moderator, {CircleModerationAction::UNBAN_MEMBER, target});
    CPPUNIT_ASSERT(unban && unban->resolution);
    CPPUNIT_ASSERT(repository->append(peer, JoinCircleMember {target}));
    CPPUNIT_ASSERT(repository->policy().members.at(target).roles.empty());
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testPeerDegradationReplay()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto designator = addMember(*repository, "designator");
    const auto peer = addMember(*repository, "peer");
    const auto actor = designator.second->issuer->getId().toString();
    const auto target = peer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"restricted", {{}, {Permission::BanUnbanMember}}}));
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRole {"designator", {{Permission::AssignRoles, Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(),
                                      SetCircleRoleDelegation {"designator",
                                                               RoleDelegation {{Permission::BanUnbanMember},
                                                                               {"moderator"},
                                                                               {"moderator", "restricted"},
                                                                               {},
                                                                               true}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "designator"}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {target, "moderator"}));
    const auto original = repository->head();
    const std::array<CirclePolicyChange, 3> changes {SetCircleRoleAssignment {target, "moderator", false},
                                                     SetCircleRoleAssignment {target, "restricted"},
                                                     SetCircleRole {"moderator", {}}};
    for (const auto& change : changes) {
        CPPUNIT_ASSERT(!repository->append(designator, change));
        CPPUNIT_ASSERT(repository->head() == original);
        TamperOptions options;
        options.signer = &designator;
        options.message = CircleRecord {change, repository->id(), original}.toString();
        CPPUNIT_ASSERT(options.message);
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
        setHead(original);
    }
    CPPUNIT_ASSERT(repository->append(identity(), changes.front()));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
    CPPUNIT_ASSERT(!reopened->policy().hasPermission(target, Permission::BanUnbanMember));
}

void
CircleRepositoryTest::testPromotionInvalidatesPeerVote()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto moderator = addMember(*repository, "moderator");
    const auto peer = addMember(*repository, "peer");
    const auto actor = moderator.second->issuer->getId().toString();
    const auto target = peer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "moderator"}));
    const CircleModerationVote ban {CircleModerationAction::BAN_MEMBER, target};
    const auto vote = repository->vote(moderator, ban);
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {target, "moderator"}));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(!repository->resolveVote(moderator, *vote));
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *vote));
    CPPUNIT_ASSERT(!repository->vote(moderator, ban));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->pendingVotes().empty());
    CPPUNIT_ASSERT(reopened->policy().members.at(target).state == CircleMemberState::ACTIVE);
}

void
CircleRepositoryTest::testVoterRevisionInvalidatesVotes()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto voter = addMember(*repository, "voter");
    const auto member = voter.second->issuer->getId().toString();
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    const auto vote = repository->vote(voter, {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator", false}));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *vote));
    const auto second = repository->vote(voter, {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(second);
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRole {"moderator", {{Permission::BanUnbanMember, Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *second));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->pendingVotes().empty());
}

void
CircleRepositoryTest::testTargetRevisionInvalidatesVotes()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto target = addMember(*repository, "target");
    const auto member = target.second->issuer->getId().toString();
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}}));
    CPPUNIT_ASSERT(repository->pendingVotes().size() == 1);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "reader"}));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "reader", false}));
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *vote));
    const auto second = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(second);
    CPPUNIT_ASSERT(repository->append(target, LeaveCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(target, JoinCircleMember {member}));
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *second));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->pendingVotes().empty());
}

void
CircleRepositoryTest::testUnrelatedMemberUpdatePreservesVote()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto other = addMember(*repository, "other");
    const auto member = other.second->issuer->getId().toString();
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "reader"}));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *vote));
}

void
CircleRepositoryTest::testDeviceEnrollmentPreservesVote()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    const auto device = dht::crypto::generateIdentity("another-device", accountIdentity(), 2048, false);
    CPPUNIT_ASSERT(repository->append(device,
                                      RegisterCircleDevice {accountIdentity().second->getId().toString(),
                                                            device.second->getLongId().toString()}));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    CPPUNIT_ASSERT(!repository->vote(device, {CircleModerationAction::BAN_MEMBER, target}));
    CPPUNIT_ASSERT(repository->resolveVote(device, *vote));
}

void
CircleRepositoryTest::testResolutionReplayAndCycles()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto first = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(first && first->resolution);
    const auto unban = repository->moderate(identity(), {CircleModerationAction::UNBAN_MEMBER, target});
    CPPUNIT_ASSERT(unban && unban->resolution);
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), first->vote));
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), unban->vote));
    const auto second = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(second && second->resolution && first->vote != second->vote);
    const CircleRecord replay {CircleModerationResolution {first->vote}, repository->id(), repository->head()};
    TamperOptions options;
    options.message = replay.toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testModerationRequiresTreeEdits()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto target = addMember(*repository, "target");
    const auto member = target.second->issuer->getId().toString();
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(vote);
    const CircleRecord resolution {CircleModerationResolution {*vote}, repository->id(), repository->head()};
    TamperOptions options;
    options.message = resolution.toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testVoteCannotChangeTree()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const CircleRecord vote {CircleModerationVote {CircleModerationAction::BAN_MEMBER, target},
                             repository->id(),
                             repository->head()};
    TamperOptions options;
    options.message = vote.toString();
    options.tree = changedTree(*repository, nullptr);
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
    const CircleRecord missing {CircleModerationResolution {std::string(40, 'b')}, repository->id(), repository->head()};
    options.tree.reset();
    options.message = missing.toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
}

void
CircleRepositoryTest::testInvitedAndLeftMemberModeration()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("target-account", {}, 2048, true);
    const auto target = dht::crypto::generateIdentity("target-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    auto result = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(result && result->resolution);
    CPPUNIT_ASSERT(!repository->append(target, JoinCircleMember {member}));
    result = repository->moderate(identity(), {CircleModerationAction::UNBAN_MEMBER, member});
    CPPUNIT_ASSERT(result && result->resolution);
    CPPUNIT_ASSERT(repository->append(target, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(target, LeaveCircleMember {member}));
    result = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(result && result->resolution);
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy().members.at(member).state == CircleMemberState::BANNED);
}

void
CircleRepositoryTest::testVoteSnapshotBinding()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    CircleRecord record {CircleModerationVote {CircleModerationAction::BAN_MEMBER, target},
                         repository->id(),
                         repository->head()};
    for (auto field : {&record.circle, &record.base}) {
        const auto previous = *field;
        *field = std::string(40, 'c');
        TamperOptions options;
        options.message = record.toString();
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, repository->id()));
        *field = previous;
    }
}

dht::crypto::Identity
CircleRepositoryTest::addMember(CircleRepository& repository, const std::string& name)
{
    const auto account = dht::crypto::generateIdentity(name + "-account", {}, 2048, true);
    auto device = dht::crypto::generateIdentity(name + "-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository.append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository.append(device, JoinCircleMember {member}));
    return device;
}

void
CircleRepositoryTest::testIndependentVotesCoexist()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string first(40, 'a'), second(40, 'b');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {first}));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {second}));
    const auto firstVote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, first});
    const auto secondVote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, second});
    CPPUNIT_ASSERT(firstVote && secondVote && repository->pendingVotes().size() == 2);
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *firstVote));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*secondVote});
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *secondVote));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->pendingVotes().empty());
    CPPUNIT_ASSERT(reopened->policy().members.at(first).state == CircleMemberState::BANNED);
    CPPUNIT_ASSERT(reopened->policy().members.at(second).state == CircleMemberState::BANNED);
}

void
CircleRepositoryTest::testSignedPolicyMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    const auto merge = repository->merge(identity(), *second);
    CPPUNIT_ASSERT(merge && *merge != *first && *merge != *second);
    const auto expected = repository->policy();
    CPPUNIT_ASSERT(expected.roles.size() == 2);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->head() == *merge && repository->policy() == expected);
    setHead(*second);
    const auto reversed = other->merge(identity(), *first);
    CPPUNIT_ASSERT(reversed && other->policy() == expected);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->policy() == expected);
}

void
CircleRepositoryTest::testPolicyMergeFastForward()
{
    const auto first = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(first);
    auto stale = openReplayed(path_, first->id());
    CPPUNIT_ASSERT(stale);
    const auto commit = first->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}});
    CPPUNIT_ASSERT(commit);
    setHead(stale->head());
    CPPUNIT_ASSERT(stale->merge(identity(), *commit) == commit);
    CPPUNIT_ASSERT(stale->policy() == first->policy());
    CPPUNIT_ASSERT(stale->merge(identity(), first->id()) == commit);
    CPPUNIT_ASSERT(stale->merge(identity(), *commit) == commit);
}

void
CircleRepositoryTest::setHead(std::string_view commit)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path_.string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_oid_fromstr(&oid, std::string(commit).c_str()) == 0);
    git_reference* referenceRaw = nullptr;
    CPPUNIT_ASSERT(git_reference_create(&referenceRaw, repo.get(), "refs/heads/main", &oid, 1, "test branch") == 0);
    GitReference reference(referenceRaw);
}

void
CircleRepositoryTest::testConcurrentPermissionComposition()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), SetCircleRole {"writer", {{Permission::File}, {}}});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    CPPUNIT_ASSERT(repository->merge(identity(), *second));
    const PermissionSet combined {Permission::Text, Permission::File};
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow == combined);
    const auto state = repository->policy();
    setHead(*second);
    CPPUNIT_ASSERT(other->merge(identity(), *first));
    CPPUNIT_ASSERT(other->policy() == state);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == state);
    CPPUNIT_ASSERT(!openReplayed(path_, circle, 3));
    CPPUNIT_ASSERT(openReplayed(path_, circle, 4));
}

void
CircleRepositoryTest::testRestrictivePolicyConflicts()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto grant = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(grant);
    const auto revoke = repository->append(identity(), SetCircleRole {"writer", {}});
    CPPUNIT_ASSERT(revoke);
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto concurrent = other->append(identity(),
                                          SetCircleRole {"writer", {{Permission::Text, Permission::File}, {}}});
    CPPUNIT_ASSERT(concurrent);
    setHead(*revoke);
    CPPUNIT_ASSERT(repository->merge(identity(), *concurrent));
    CPPUNIT_ASSERT(!repository->policy().roles.at("writer").allow.contains(Permission::Text));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow.contains(Permission::File));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow.contains(Permission::Text));
    const auto base = repository->head();
    const auto deny = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {Permission::Text}}});
    CPPUNIT_ASSERT(deny);
    const auto removeDeny = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(removeDeny);
    setHead(base);
    other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto concurrentDeny = other->append(identity(),
                                              SetCircleRole {"writer",
                                                             {{Permission::Text},
                                                              {Permission::Text, Permission::File}}});
    CPPUNIT_ASSERT(concurrentDeny);
    setHead(*removeDeny);
    CPPUNIT_ASSERT(repository->merge(identity(), *concurrentDeny));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").deny.contains(Permission::Text));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").deny.empty());
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testMergeTopologyConvergence()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    std::array<std::string, 3> branches;
    const std::array permissions {Permission::Text, Permission::File, Permission::Reaction};
    for (size_t index = 0; index < branches.size(); ++index) {
        setHead(circle);
        repository = openReplayed(path_, circle);
        CPPUNIT_ASSERT(repository);
        const auto commit = repository->append(identity(), SetCircleRole {"writer", {{permissions[index]}, {}}});
        CPPUNIT_ASSERT(commit);
        branches[index] = *commit;
    }
    setHead(branches[0]);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->merge(identity(), branches[1]));
    CPPUNIT_ASSERT(repository->merge(identity(), branches[2]));
    const auto expected = repository->policy();
    setHead(branches[2]);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->merge(identity(), branches[1]));
    CPPUNIT_ASSERT(repository->merge(identity(), branches[0]));
    CPPUNIT_ASSERT(repository->policy() == expected);
    const PermissionSet all {Permission::Text, Permission::File, Permission::Reaction};
    CPPUNIT_ASSERT(expected.roles.at("writer").allow == all);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
}

void
CircleRepositoryTest::testMemberSignsPolicyMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto member = addMember(*repository, "merge-member");
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto first = repository->append(identity(), SetCircleDefaults {{{Permission::ReadChannel}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), SetCircleDefaults {{{Permission::Text}, {}}});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    const auto outsiderAccount = dht::crypto::generateIdentity("outsider", {}, 2048, true);
    const auto outsider = dht::crypto::generateIdentity("outsider-device", outsiderAccount, 2048, false);
    CPPUNIT_ASSERT(!repository->merge(outsider, *second));
    const auto merged = repository->merge(member, *second);
    CPPUNIT_ASSERT(merged);
    const PermissionSet defaults {Permission::ReadChannel, Permission::Text};
    CPPUNIT_ASSERT(repository->policy().defaults.allow == defaults);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testMembershipAndPolicyMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), InviteCircleMember {std::string(40, 'a')});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    CPPUNIT_ASSERT(repository->merge(identity(), *second));
    CPPUNIT_ASSERT(repository->policy().members.at(std::string(40, 'a')).state == CircleMemberState::INVITED);
    CPPUNIT_ASSERT(repository->policy().roles.contains("writer"));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
    CPPUNIT_ASSERT(!repository->merge(identity(), "invalid"));
    CPPUNIT_ASSERT(!repository->merge(identity(), std::string(40, 'f')));
    setHead(*second);
    CPPUNIT_ASSERT(!repository->merge(identity(), *second));
}

void
CircleRepositoryTest::testDelegatedRuleDivergence()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto editor = addMember(*repository, "editor");
    const auto member = editor.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRoleDelegation {"editor",
                                                    RoleDelegation {{Permission::Text}, {"writer"}, {}, {}, true}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor"}));
    const auto base = repository->head();
    const auto circle = repository->id();
    const auto first = repository->append(identity(), SetCircleRole {"other-role", {{Permission::File}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(editor, SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    CPPUNIT_ASSERT(repository->merge(identity(), *second));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow.contains(Permission::Text));
    CPPUNIT_ASSERT(repository->policy().roles.contains("other-role"));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testPolicyMergeTampering()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}});
    CPPUNIT_ASSERT(second);
    const CircleRecord record {CirclePolicyMerge {*second}, circle, *first};
    for (auto signature : {Signature::MISSING, Signature::INVALID, Signature::VALID}) {
        TamperOptions options;
        options.merge = true;
        options.otherParent = second;
        options.message = record.toString();
        options.signature = signature;
        if (signature == Signature::VALID)
            options.tree = changedTree(*repository, nullptr);
        tamper(*repository, options);
        CPPUNIT_ASSERT(!openReplayed(path_, circle));
    }
    TamperOptions options;
    options.merge = true;
    options.otherParent = second;
    options.message = CircleRecord {CirclePolicyMerge {*first}, circle, *second}.toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, circle));
}

void
CircleRepositoryTest::testMergePreservesUnrelatedVotes()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    const auto base = repository->head();
    const auto first = repository->append(identity(), SetCircleRole {"first", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), SetCircleRole {"second", {{Permission::File}, {}}});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    CPPUNIT_ASSERT(repository->merge(identity(), *second));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->pendingVotes() == std::vector<std::string> {*vote});
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *vote));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy().members.at(target).state == CircleMemberState::BANNED);
}

void
CircleRepositoryTest::testMergeInvalidatesAffectedVotes()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto target = addMember(*repository, "merge-target");
    const auto member = target.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(vote);
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto first = repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(first);
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->append(identity(), SetCircleRole {"writer", {{Permission::File}, {}}});
    CPPUNIT_ASSERT(second);
    setHead(*first);
    CPPUNIT_ASSERT(repository->merge(identity(), *second));
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    CPPUNIT_ASSERT(!repository->resolveVote(identity(), *vote));
    const auto expected = repository->policy();
    setHead(*second);
    CPPUNIT_ASSERT(other->merge(identity(), *first));
    CPPUNIT_ASSERT(other->policy() == expected && other->pendingVotes().empty());
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->pendingVotes().empty());
    CPPUNIT_ASSERT(!reopened->resolveVote(identity(), *vote));
}

void
CircleRepositoryTest::testConcurrentMembershipMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = addMember(*repository, "first-branch-member");
    const auto left = repository->head();
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = addMember(*other, "second-branch-member");
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    const auto expected = repository->policy();
    CPPUNIT_ASSERT(expected.members.at(first.second->issuer->getId().toString()).state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(expected.members.at(second.second->issuer->getId().toString()).state == CircleMemberState::ACTIVE);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->policy() == expected);
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
}

void
CircleRepositoryTest::testConcurrentRoleAssignments()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto device = addMember(*repository, "assignee");
    const auto member = device.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "reader", false}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    CPPUNIT_ASSERT(other->append(identity(), SetCircleRoleAssignment {member, "reader"}));
    CPPUNIT_ASSERT(other->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    const std::set<std::string, std::less<>> expectedRoles {"writer"};
    CPPUNIT_ASSERT(repository->policy().members.at(member).roles == expectedRoles);
    const auto expected = repository->policy();
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    CPPUNIT_ASSERT(other->append(identity(), SetCircleRoleAssignment {member, "reader"}));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy().hasPermission(member, Permission::ReadChannel));
}

void
CircleRepositoryTest::testRevokedDelegatedEditMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto editor = addMember(*repository, "revoked-editor");
    const auto member = editor.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRoleDelegation {"editor",
                                                    RoleDelegation {{Permission::Text}, {"writer"}, {}, {}, true}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor", false}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(editor, SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow.empty());
    CPPUNIT_ASSERT(!repository->policy().hasPermission(member, Permission::ManageRoles));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testConcurrentDeviceEnrollment()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto first = dht::crypto::generateIdentity("first-new-device", accountIdentity(), 2048, false);
    const auto second = dht::crypto::generateIdentity("second-new-device", accountIdentity(), 2048, false);
    const auto member = accountIdentity().second->getId().toString();
    CPPUNIT_ASSERT(repository->append(first, RegisterCircleDevice {member, first.second->getLongId().toString()}));
    const auto left = repository->head();
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(second, RegisterCircleDevice {member, second.second->getLongId().toString()}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository);
    CPPUNIT_ASSERT(repository->append(first, SetCircleRole {"first", {}}));
    CPPUNIT_ASSERT(repository->append(second, SetCircleRole {"second", {}}));
}

void
CircleRepositoryTest::testLeaveBeatsStaleEnrollment()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("leaving-account", {}, 2048, true);
    const auto first = dht::crypto::generateIdentity("original-device", account, 2048, false);
    const auto second = dht::crypto::generateIdentity("late-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(first, JoinCircleMember {member}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(first, LeaveCircleMember {member}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(second, RegisterCircleDevice {member, second.second->getLongId().toString()}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::LEFT);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository);
    CPPUNIT_ASSERT(!repository->append(second, RegisterCircleDevice {member, second.second->getLongId().toString()}));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(first, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(second, RegisterCircleDevice {member, second.second->getLongId().toString()}));
}

void
CircleRepositoryTest::testRejoinDropsOldAssignments()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto device = addMember(*repository, "rejoining-member");
    const auto member = device.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "reader"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(device, LeaveCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->append(device, JoinCircleMember {member}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(repository->policy().members.at(member).roles.empty());
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testConcurrentVoteMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string first(40, 'a'), second(40, 'b');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {first}));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {second}));
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto firstVote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, first});
    CPPUNIT_ASSERT(firstVote);
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto secondVote = other->vote(identity(), {CircleModerationAction::BAN_MEMBER, second});
    CPPUNIT_ASSERT(secondVote);
    setHead(*firstVote);
    CPPUNIT_ASSERT(repository->merge(identity(), *secondVote));
    CPPUNIT_ASSERT(repository->pendingVotes().size() == 2);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->resolveVote(identity(), *firstVote));
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *secondVote));
}

void
CircleRepositoryTest::testDuplicateResolutionMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto moderator = addMember(*repository, "second-resolver");
    const auto member = moderator.second->issuer->getId().toString();
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    const auto circle = repository->id();
    CPPUNIT_ASSERT(repository->resolveVote(identity(), *vote));
    const auto left = repository->head();
    setHead(*vote);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->resolveVote(moderator, *vote));
    const auto right = other->head();
    CPPUNIT_ASSERT(left != right);
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().members.at(target).state == CircleMemberState::BANNED);
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
    CPPUNIT_ASSERT(!reopened->resolveVote(identity(), *vote));
}

void
CircleRepositoryTest::testRevokedModeratorMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto moderator = addMember(*repository, "revoked-moderator");
    const auto member = moderator.second->issuer->getId().toString();
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "moderator"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto removed = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(removed && removed->resolution);
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto stale = other->moderate(moderator, {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(stale && stale->resolution);
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().members.at(member).state == CircleMemberState::BANNED);
    CPPUNIT_ASSERT(repository->policy().members.at(target).state == CircleMemberState::INVITED);
    CPPUNIT_ASSERT(repository->pendingVotes().empty());
    const auto expected = repository->policy();
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
}

void
CircleRepositoryTest::testPromotionBanMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto moderator = addMember(*repository, "moderator");
    const auto peer = addMember(*repository, "promoted-peer");
    const auto actor = moderator.second->issuer->getId().toString();
    const auto target = peer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "moderator"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {target, "moderator"}));
    const auto left = repository->head();
    for (const auto& resolver : {moderator, identity()}) {
        setHead(base);
        auto other = openReplayed(path_, circle);
        CPPUNIT_ASSERT(other);
        const auto ban = other->moderate(resolver, {CircleModerationAction::BAN_MEMBER, target});
        CPPUNIT_ASSERT(ban && ban->resolution);
        const auto right = other->head();
        const auto expectedState = resolver.second->getLongId() == identity().second->getLongId()
                                       ? CircleMemberState::BANNED
                                       : CircleMemberState::ACTIVE;
        CircleMember expectedMember {expectedState, {}};
        if (expectedState == CircleMemberState::ACTIVE)
            expectedMember.roles.insert("moderator");
        std::optional<CirclePolicy> expectedPolicy;
        for (bool reversed : {false, true}) {
            const auto& first = reversed ? right : left;
            const auto& second = reversed ? left : right;
            setHead(first);
            repository = openReplayed(path_, circle);
            CPPUNIT_ASSERT(repository && repository->merge(identity(), second));
            const auto policy = repository->policy();
            CPPUNIT_ASSERT(policy.members.at(target) == expectedMember);
            if (expectedPolicy)
                CPPUNIT_ASSERT(policy == *expectedPolicy);
            else
                expectedPolicy = policy;
            CPPUNIT_ASSERT(repository->pendingVotes().empty());
            CPPUNIT_ASSERT(repository->authorizePeer(*peer.second) == (expectedState == CircleMemberState::ACTIVE));
            CPPUNIT_ASSERT(repository->authorizePeer(*moderator.second));
            CPPUNIT_ASSERT(repository->authorizePeer(*identity().second));
            const auto reopened = openReplayed(path_, circle);
            CPPUNIT_ASSERT(reopened && reopened->policy() == *expectedPolicy);
            CPPUNIT_ASSERT(reopened->pendingVotes().empty());
            CPPUNIT_ASSERT(reopened->authorizePeer(*peer.second) == (expectedState == CircleMemberState::ACTIVE));
            CPPUNIT_ASSERT(reopened->authorizePeer(*moderator.second));
            CPPUNIT_ASSERT(reopened->authorizePeer(*identity().second));
        }
    }
}

CirclePolicy
CircleRepositoryTest::mergeBothWays(const std::string& circle, const std::string& left, const std::string& right)
{
    std::optional<CirclePolicy> expected;
    for (bool reversed : {false, true}) {
        setHead(reversed ? right : left);
        const auto repository = openReplayed(path_, circle);
        CPPUNIT_ASSERT(repository && repository->merge(identity(), reversed ? left : right));
        const auto policy = repository->policy();
        CPPUNIT_ASSERT(!expected || policy == *expected);
        expected = policy;
        const auto reopened = openReplayed(path_, circle);
        CPPUNIT_ASSERT(reopened && reopened->policy() == policy);
    }
    return *expected;
}

void
CircleRepositoryTest::testAdminTransferThenLeave()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto successor = addMember(*repository, "successor");
    const auto next = successor.second->issuer->getId().toString();
    const auto founder = identity().second->issuer->getId().toString();
    CPPUNIT_ASSERT(!repository->append(identity(), LeaveCircleMember {founder}));
    CPPUNIT_ASSERT(!repository->append(successor, TransferCircleAdmin {founder}));
    CPPUNIT_ASSERT(repository->append(identity(), TransferCircleAdmin {next}));
    CPPUNIT_ASSERT(repository->policy().admin == next);
    CPPUNIT_ASSERT(!repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), LeaveCircleMember {founder}));
    CPPUNIT_ASSERT(!repository->authorizePeer(*identity().second));
    CPPUNIT_ASSERT(repository->append(successor, SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
    const auto saved = CircleRepository::open(path_, repository->id());
    CPPUNIT_ASSERT(saved && saved->policy() == repository->policy());
}

void
CircleRepositoryTest::testCircleEnd()
{
    auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel, Permission::Text}, {}});
    CPPUNIT_ASSERT(repository);
    const auto reader = addMember(*repository, "reader");
    const auto readerId = reader.second->issuer->getId().toString();
    const auto channelPath = path_ / "ended.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, identity(), CommitMessage::text("before")));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto concurrent = repository->head();
    setHead(base);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository);
    CPPUNIT_ASSERT(!repository->append(reader, EndCircle {}));
    CPPUNIT_ASSERT(repository->append(identity(), EndCircle {}));
    const auto ended = repository->head();
    CPPUNIT_ASSERT(repository->policy().ended);
    CPPUNIT_ASSERT(!repository->append(identity(), SetCircleRole {"other", {}}));
    CPPUNIT_ASSERT(!repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, readerId}));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, identity(), CommitMessage::text("after")));
    CPPUNIT_ASSERT(!repository->createChannel(path_ / "late.git", identity()));
    CPPUNIT_ASSERT(repository->authorizePeer(*reader.second, channel->channel));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    const auto policy = mergeBothWays(circle, ended, concurrent);
    CPPUNIT_ASSERT(policy.ended && !policy.roles.contains("writer"));
}

void
CircleRepositoryTest::testConcurrentAdminTransfers()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto first = addMember(*repository, "first-successor").second->issuer->getId().toString();
    const auto second = addMember(*repository, "second-successor").second->issuer->getId().toString();
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto left = repository->append(identity(), TransferCircleAdmin {first});
    setHead(base);
    const auto other = openReplayed(path_, circle);
    const auto right = other ? other->append(identity(), TransferCircleAdmin {second}) : std::nullopt;
    CPPUNIT_ASSERT(left && right);
    const auto policy = mergeBothWays(circle, *left, *right);
    CPPUNIT_ASSERT(policy.admin == (*left < *right ? first : second));
}

void
CircleRepositoryTest::testTransferDefeatsModeratorBan()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto moderator = addMember(*repository, "moderator");
    const auto successor = addMember(*repository, "successor").second->issuer->getId().toString();
    const auto moderatorId = moderator.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"moderator", {{Permission::BanUnbanMember}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {moderatorId, "moderator"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto left = repository->append(identity(), TransferCircleAdmin {successor});
    setHead(base);
    const auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(left && other);
    const auto ban = other->moderate(moderator, {CircleModerationAction::BAN_MEMBER, successor});
    CPPUNIT_ASSERT(ban && ban->resolution);
    const auto policy = mergeBothWays(circle, *left, other->head());
    CPPUNIT_ASSERT(policy.admin == successor);
    CPPUNIT_ASSERT(policy.members.at(successor).state == CircleMemberState::ACTIVE);
}

void
CircleRepositoryTest::testTransferSuppressesConcurrentAdminWork()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto successor = addMember(*repository, "successor").second->issuer->getId().toString();
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto left = repository->append(identity(), TransferCircleAdmin {successor});
    setHead(base);
    const auto other = openReplayed(path_, circle);
    const auto right = other ? other->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}})
                             : std::nullopt;
    CPPUNIT_ASSERT(left && right);
    const auto policy = mergeBothWays(circle, *left, *right);
    CPPUNIT_ASSERT(policy.admin == successor && !policy.roles.contains("reader"));
}

void
CircleRepositoryTest::testTransferToLeavingMember()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto successor = addMember(*repository, "successor");
    const auto next = successor.second->issuer->getId().toString();
    const auto founder = identity().second->issuer->getId().toString();
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), TransferCircleAdmin {next}));
    const auto left = repository->append(identity(), LeaveCircleMember {founder});
    setHead(base);
    const auto other = openReplayed(path_, circle);
    const auto right = other ? other->append(successor, LeaveCircleMember {next}) : std::nullopt;
    CPPUNIT_ASSERT(left && right);
    const auto policy = mergeBothWays(circle, *left, *right);
    CPPUNIT_ASSERT(policy.admin == founder);
    CPPUNIT_ASSERT(policy.members.at(founder).state == CircleMemberState::ACTIVE);
    CPPUNIT_ASSERT(policy.members.at(next).state == CircleMemberState::LEFT);
}

void
CircleRepositoryTest::testChannelLog()
{
    const auto repository = CircleRepository::create(path_,
                                                     identity(),
                                                     {{Permission::ReadChannel,
                                                       Permission::Text,
                                                       Permission::TextReply,
                                                       Permission::Reaction},
                                                      {}});
    CPPUNIT_ASSERT(repository);
    const auto writer = addMember(*repository, "writer");
    const auto channelPath = path_ / "log.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto first = repository->appendChannelMessage(channelPath,
                                                        channel->channel,
                                                        identity(),
                                                        CommitMessage::text("first"));
    CPPUNIT_ASSERT(first);
    auto reply = CommitMessage::text("second");
    reply.replyTo = *first;
    const auto second = repository->appendChannelMessage(channelPath, channel->channel, writer, reply);
    const auto third = repository->appendChannelMessage(channelPath,
                                                        channel->channel,
                                                        identity(),
                                                        CommitMessage::reaction("+1", *first));
    CPPUNIT_ASSERT(second && third);
    const auto founder = identity().second->issuer->getId().toString();
    const auto read = [&](std::string_view id,
                          std::string_view from = {},
                          std::string_view stop = {},
                          size_t limit = 5) -> std::optional<std::vector<CircleChannelEntry>> {
        std::vector<CircleChannelEntry> entries;
        const auto visit = [&](CircleChannelEntry&& entry) {
            entries.emplace_back(std::move(entry));
            return entries.size() < limit;
        };
        if (!repository->channelLog(channelPath, id, visit, from, stop))
            return std::nullopt;
        return entries;
    };

    const auto log = read(channel->channel);
    CPPUNIT_ASSERT(log && log->size() == 4);
    CPPUNIT_ASSERT((*log)[0].id == *third && (*log)[1].id == *second && (*log)[2].id == *first);
    CPPUNIT_ASSERT((*log)[3].id == channel->channel && !(*log)[3].message && (*log)[3].parents.empty());
    CPPUNIT_ASSERT((*log)[1].author == writer.second->issuer->getId().toString());
    CPPUNIT_ASSERT((*log)[1].device == writer.second->getLongId().toString());
    CPPUNIT_ASSERT((*log)[1].message->body == "second" && (*log)[1].message->replyTo == *first);
    CPPUNIT_ASSERT((*log)[0].author == founder && (*log)[0].message->reactTo == *first);
    CPPUNIT_ASSERT((*log)[0].parents == std::vector<std::string> {*second});

    const auto page = read(channel->channel, *second, channel->channel);
    CPPUNIT_ASSERT(page && page->size() == 2 && (*page)[0].id == *second && (*page)[1].id == *first);
    const auto limited = read(channel->channel, {}, {}, 1);
    CPPUNIT_ASSERT(limited && limited->size() == 1 && (*limited)[0].id == *third);
    CPPUNIT_ASSERT(!read(channel->channel, std::string(40, 'a')));
    CPPUNIT_ASSERT(!read(channel->channel, "../main"));
    CPPUNIT_ASSERT(!read(std::string(40, 'b')));
}

void
CircleRepositoryTest::testDelegationConflictMerge()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    const RoleDelegation original {{Permission::Text, Permission::File}, {"writer"}, {}, {}, true};
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleDelegation {"editor", original}));
    const auto circle = repository->id();
    const auto base = repository->head();
    auto reduced = original;
    reduced.permissions.erase(Permission::File);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleDelegation {"editor", reduced}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    auto expanded = original;
    expanded.permissions.insert(Permission::Reaction);
    CPPUNIT_ASSERT(other && other->append(identity(), SetCircleRoleDelegation {"editor", expanded}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    const auto grant = repository->policy().delegations.at("editor");
    const PermissionSet expected {Permission::Text, Permission::Reaction};
    CPPUNIT_ASSERT(grant.permissions == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testConcurrentPeerDegradation()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto editor = addMember(*repository, "peer-editor");
    const auto peer = addMember(*repository, "protected-peer");
    const auto actor = editor.second->issuer->getId().toString();
    const auto target = peer.second->issuer->getId().toString();
    for (const auto role : {"first-guard", "second-guard"}) {
        CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {role, {{Permission::BanUnbanMember}, {}}}));
        CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {target, role}));
    }
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(),
                                      SetCircleRoleDelegation {"editor",
                                                               RoleDelegation {{Permission::BanUnbanMember},
                                                                               {"first-guard", "second-guard"},
                                                                               {},
                                                                               {},
                                                                               true}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "editor"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(editor, SetCircleRole {"first-guard", {}}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(editor, SetCircleRole {"second-guard", {}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().hasPermission(target, Permission::BanUnbanMember));
    const auto expected = repository->policy();
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
}

void
CircleRepositoryTest::testAuthorityRegrantCannotRevive()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto editor = addMember(*repository, "regranted-editor");
    const auto member = editor.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(
        repository->append(identity(),
                           SetCircleRoleDelegation {"editor",
                                                    RoleDelegation {{Permission::Text}, {"writer"}, {}, {}, true}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor", false}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor"}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(editor, SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow.empty());
    CPPUNIT_ASSERT(repository->policy().hasPermission(member, Permission::ManageRoles));
    CPPUNIT_ASSERT(repository->append(editor, SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy().roles.at("writer").allow.contains(Permission::Text));
}

void
CircleRepositoryTest::testDelegationExpansionPreservesEdit()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto editor = addMember(*repository, "expanded-editor");
    const auto member = editor.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    const RoleDelegation original {{Permission::Text}, {"writer"}, {}, {}, true};
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleDelegation {"editor", original}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "editor"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    auto expanded = original;
    expanded.permissions.insert(Permission::File);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleDelegation {"editor", expanded}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(editor, SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().roles.at("writer").allow.contains(Permission::Text));
    CPPUNIT_ASSERT(repository->policy().delegations.at("editor").permissions.contains(Permission::File));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testMembershipTopologyConvergence()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const std::string target(40, 'a');
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {target}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, target});
    CPPUNIT_ASSERT(vote);
    const auto circle = repository->id();
    const auto base = repository->head();
    std::array<std::string, 3> branches;
    for (size_t index = 0; index < branches.size(); ++index) {
        setHead(base);
        repository = openReplayed(path_, circle);
        CPPUNIT_ASSERT(repository);
        addMember(*repository, "topology-member-" + std::to_string(index));
        branches[index] = repository->head();
    }
    setHead(branches[0]);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->merge(identity(), branches[1]));
    CPPUNIT_ASSERT(repository->merge(identity(), branches[2]));
    const auto expected = repository->policy();
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    setHead(branches[2]);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && repository->merge(identity(), branches[1]));
    CPPUNIT_ASSERT(repository->merge(identity(), branches[0]));
    CPPUNIT_ASSERT(repository->policy() == expected);
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
    CPPUNIT_ASSERT(reopened->resolveVote(identity(), *vote));
}

void
CircleRepositoryTest::testAssignmentIntentVoteRevision()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto device = addMember(*repository, "intent-target");
    const auto member = device.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"reader", {{Permission::ReadChannel}, {}}}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "reader", false}));
    const auto vote = repository->vote(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(vote);
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(identity(), SetCircleRole {"unrelated", {}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->pendingVotes() == std::vector<std::string> {*vote});
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->resolveVote(identity(), *vote));
}

void
CircleRepositoryTest::testCreateRegisteredChannel()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "channel.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(repository->policy().channels.contains(channel->channel));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
    CPPUNIT_ASSERT(verifyReplayed(*reopened, channelPath, channel->channel));
    CPPUNIT_ASSERT(!openReplayed(channelPath, channel->channel));
    CPPUNIT_ASSERT(!repository->createChannel(channelPath, identity()));
}

void
CircleRepositoryTest::testPrivateDelegatedChannel()
{
    const auto repository = CircleRepository::create(path_,
                                                     identity(),
                                                     {{Permission::ReadChannel, Permission::Text}, {}});
    CPPUNIT_ASSERT(repository);
    const auto manager = addMember(*repository, "channel-manager");
    const auto reader = addMember(*repository, "channel-reader");
    const auto actor = manager.second->issuer->getId().toString();
    const auto member = reader.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"manager", {{Permission::ManageChannels}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "manager"}));
    ChannelPolicy policy;
    policy.defaults = PermissionRule {};
    policy.roles["manager"] = {{Permission::ReadChannel, Permission::ManageChannels}, {}};
    CPPUNIT_ASSERT(!repository->createChannel(path_ / "ordinary.git", reader, policy));
    CPPUNIT_ASSERT(!std::filesystem::exists(path_ / "ordinary.git"));
    const auto channelPath = path_ / "private.git";
    const auto channel = repository->createChannel(channelPath, manager, policy);
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(!repository->policy().hasPermission(member, Permission::ReadChannel, channel->channel));
    CPPUNIT_ASSERT(repository->policy().hasPermission(actor, Permission::ReadChannel, channel->channel));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    auto expanded = policy;
    expanded.roles.at("manager").allow.insert(Permission::File);
    CPPUNIT_ASSERT(!repository->createChannel(path_ / "expanded.git", manager, expanded));
    CPPUNIT_ASSERT(!std::filesystem::exists(path_ / "expanded.git"));
    CPPUNIT_ASSERT(repository->append(manager, ArchiveCircleChannel {channel->channel}));
    CPPUNIT_ASSERT(repository->policy().channels.at(channel->channel).archived);
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && verifyReplayed(*reopened, channelPath, channel->channel));
}

std::string
CircleRepositoryTest::channelProof(const std::filesystem::path& path, std::string_view channel)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path.string().c_str()) == 0);
    GitRepository repository(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_oid_fromstr(&oid, std::string(channel).c_str()) == 0);
    git_odb* odbRaw = nullptr;
    CPPUNIT_ASSERT(git_repository_odb(&odbRaw, repository.get()) == 0);
    GitOdb odb(odbRaw);
    git_odb_object* objectRaw = nullptr;
    CPPUNIT_ASSERT(git_odb_read(&objectRaw, odb.get(), &oid) == 0);
    std::unique_ptr<git_odb_object, decltype(&git_odb_object_free)> object(objectRaw, git_odb_object_free);
    return std::string(static_cast<const char*>(git_odb_object_data(object.get())), git_odb_object_size(object.get()));
}

void
CircleRepositoryTest::testChannelProofRejected()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleDefaults {{{Permission::ReadChannel}, {}}}));
    const auto authorization = repository->head();
    const auto circle = repository->id();
    const auto channelPath = path_ / "proof.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const RegisterCircleChannel original {channel->channel,
                                          authorization,
                                          {},
                                          channelProof(channelPath, channel->channel)};
    setHead(authorization);
    repository = openReplayed(path_, circle);
    CPPUNIT_ASSERT(repository && !verifyReplayed(*repository, channelPath, channel->channel));
    auto changed = original;
    changed.policy.defaults = PermissionRule {};
    CPPUNIT_ASSERT(!repository->append(identity(), changed));
    changed = original;
    changed.authorization = circle;
    CPPUNIT_ASSERT(!repository->append(identity(), changed));
    changed = original;
    auto position = changed.genesis.find("\"nonce\":\"");
    CPPUNIT_ASSERT(position != std::string::npos);
    position += std::string("\"nonce\":\"").size();
    changed.genesis[position] = changed.genesis[position] == 'a' ? 'b' : 'a';
    git_oid oid {};
    CPPUNIT_ASSERT(git_odb_hash(&oid, changed.genesis.data(), changed.genesis.size(), GIT_OBJECT_COMMIT) == 0);
    changed.channel = git_oid_tostr_s(&oid);
    CPPUNIT_ASSERT(!repository->append(identity(), changed));
    TamperOptions options;
    options.message = CircleRecord {CirclePolicyChange {changed}, circle, authorization}.toString();
    CPPUNIT_ASSERT(options.message);
    tamper(*repository, options);
    CPPUNIT_ASSERT(!openReplayed(path_, circle));
    setHead(authorization);
    CPPUNIT_ASSERT(repository->append(identity(), original));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
}

void
CircleRepositoryTest::testOrphanChannelRegistration()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto stale = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(stale);
    const auto authorization = stale->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"intervening", {}}));
    const auto channelPath = path_ / "orphan.git";
    const auto channel = stale->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && !channel->registration);
    CPPUNIT_ASSERT(!verifyReplayed(*repository, channelPath, channel->channel));
    CPPUNIT_ASSERT(repository->append(identity(),
                                      RegisterCircleChannel {channel->channel,
                                                             authorization,
                                                             {},
                                                             channelProof(channelPath, channel->channel)}));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && verifyReplayed(*reopened, channelPath, channel->channel));
}

void
CircleRepositoryTest::testConcurrentChannelRegistration()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto firstPath = path_ / "first.git", secondPath = path_ / "second.git";
    const auto first = repository->createChannel(firstPath, identity());
    CPPUNIT_ASSERT(first && first->registration);
    const auto left = repository->head();
    setHead(circle);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto second = other->createChannel(secondPath, identity());
    CPPUNIT_ASSERT(second && second->registration && first->channel != second->channel);
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().channels.size() == 2);
    CPPUNIT_ASSERT(verifyReplayed(*repository, firstPath, first->channel));
    CPPUNIT_ASSERT(verifyReplayed(*repository, secondPath, second->channel));
    const auto expected = repository->policy();
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
}

void
CircleRepositoryTest::testArchivedChannelMerge()
{
    auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel, Permission::Text}, {}});
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "archive.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), ArchiveCircleChannel {channel->channel}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other
                   && other->append(identity(),
                                    SetCircleChannelRule {channel->channel,
                                                          std::nullopt,
                                                          PermissionRule {{Permission::ReadChannel,
                                                                           Permission::Text,
                                                                           Permission::File},
                                                                          {}}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().channels.at(channel->channel).archived);
    const auto actor = repository->policy().admin;
    CPPUNIT_ASSERT(repository->policy().hasPermission(actor, Permission::ReadChannel, channel->channel));
    CPPUNIT_ASSERT(!repository->policy().hasPermission(actor, Permission::Text, channel->channel));
    CPPUNIT_ASSERT(!repository->policy().hasPermission(actor, Permission::File, channel->channel));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testSignedChannelRuleMerge()
{
    auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto reader = addMember(*repository, "channel-assignee");
    const auto member = reader.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto channel = repository->createChannel(path_ / "rules.git", identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(),
                                      SetCircleChannelRule {channel->channel,
                                                            "writer",
                                                            PermissionRule {{Permission::Text, Permission::File}, {}}}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(
        other
        && other->append(identity(),
                         SetCircleChannelRule {channel->channel,
                                               "writer",
                                               PermissionRule {{Permission::Text, Permission::Reaction}, {}}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    for (auto permission : {Permission::Text, Permission::File, Permission::Reaction})
        CPPUNIT_ASSERT(repository->policy().hasPermission(member, permission, channel->channel));
    const auto expected = repository->policy();
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == expected);
}

void
CircleRepositoryTest::testScopedChannelEditorMerge()
{
    auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto editor = addMember(*repository, "channel-editor");
    const auto reader = addMember(*repository, "channel-reader");
    const auto actor = editor.second->issuer->getId().toString();
    const auto member = reader.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"editor", {{Permission::ManageRoles}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {actor, "editor"}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto channel = repository->createChannel(path_ / "scoped.git", identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(repository->append(
        identity(),
        SetCircleRoleDelegation {"editor",
                                 RoleDelegation {{Permission::File}, {"writer"}, {}, {channel->channel}, false}}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(editor,
                                      SetCircleChannelRule {channel->channel,
                                                            "writer",
                                                            PermissionRule {{Permission::Text, Permission::File}, {}}}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(
        other
        && other->append(identity(),
                         SetCircleChannelRule {channel->channel,
                                               "writer",
                                               PermissionRule {{Permission::Text, Permission::Reaction}, {}}}));
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().hasPermission(member, Permission::File, channel->channel));
    CPPUNIT_ASSERT(repository->policy().hasPermission(member, Permission::Reaction, channel->channel));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testChannelGenesisTreeRejected()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto base = repository->head();
    TamperOptions options;
    options.root = true;
    options.message = CircleRecord {CircleChannelGenesis {repository->policy().admin, std::string(64, 'a'), {}},
                                    repository->id(),
                                    base}
                          .toString();
    CPPUNIT_ASSERT(options.message);
    const auto channel = tamper(*repository, options);
    const auto proof = channelProof(path_, channel);
    setHead(base);
    CPPUNIT_ASSERT(!repository->append(identity(), RegisterCircleChannel {channel, base, {}, proof}));
    CPPUNIT_ASSERT(repository->head() == base);
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && reopened->policy().channels.empty());
}

void
CircleRepositoryTest::testRevokedChannelRegistration()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto manager = addMember(*repository, "revoked-channel-manager");
    const auto member = manager.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"manager", {{Permission::ManageChannels}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "manager"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "manager", false}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other);
    const auto channelPath = path_ / "revoked.git";
    const auto channel = other->createChannel(channelPath, manager);
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto right = other->head();
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(!repository->policy().channels.contains(channel->channel));
    CPPUNIT_ASSERT(!verifyReplayed(*repository, channelPath, channel->channel));
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && reopened->policy() == repository->policy());
}

void
CircleRepositoryTest::testConcurrentSameChannelRegistration()
{
    auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto manager = addMember(*repository, "other-registrar");
    const auto member = manager.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"manager", {{Permission::ManageChannels}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "manager"}));
    const auto circle = repository->id();
    const auto base = repository->head();
    const auto channelPath = path_ / "shared-genesis.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other
                   && other->append(manager,
                                    RegisterCircleChannel {channel->channel,
                                                           base,
                                                           {},
                                                           channelProof(channelPath, channel->channel)}));
    const auto right = other->head();
    CPPUNIT_ASSERT(left != right);
    setHead(left);
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(repository->policy().channels.size() == 1);
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    const auto expected = repository->policy();
    setHead(right);
    CPPUNIT_ASSERT(other->merge(identity(), left));
    CPPUNIT_ASSERT(other->policy() == expected);
    const auto reopened = openReplayed(path_, circle);
    CPPUNIT_ASSERT(reopened && verifyReplayed(*reopened, channelPath, channel->channel));
}

void
CircleRepositoryTest::testSignedChannelText()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "messages.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto text = repository->appendChannelMessage(channelPath,
                                                       channel->channel,
                                                       identity(),
                                                       CommitMessage::text("hello"));
    CPPUNIT_ASSERT(text && *text != channel->channel);
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    const auto reply = repository->appendChannelMessage(channelPath,
                                                        channel->channel,
                                                        identity(),
                                                        CommitMessage::text("reply", *text));
    CPPUNIT_ASSERT(reply);
    CPPUNIT_ASSERT(repository->appendChannelMessage(channelPath,
                                                    channel->channel,
                                                    identity(),
                                                    CommitMessage::edit("edited", *text)));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && verifyReplayed(*reopened, channelPath, channel->channel));
    CPPUNIT_ASSERT(validateReplayed(*reopened, channelPath, channel->channel, 1).status
                   == ChannelValidationStatus::LIMIT_EXCEEDED);
}

void
CircleRepositoryTest::testChannelMessagePermissions()
{
    const auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto writer = addMember(*repository, "message-writer");
    const auto member = writer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto channelPath = path_ / "permissions.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto original = repository->appendChannelMessage(channelPath,
                                                           channel->channel,
                                                           identity(),
                                                           CommitMessage::text("original"));
    CPPUNIT_ASSERT(original);
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("denied")));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto own = repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("own"));
    CPPUNIT_ASSERT(own);
    CPPUNIT_ASSERT(!repository->appendChannelMessage(channelPath,
                                                     channel->channel,
                                                     writer,
                                                     CommitMessage::text("reply", *original)));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::TextReply}, {}}}));
    CPPUNIT_ASSERT(repository->appendChannelMessage(channelPath,
                                                    channel->channel,
                                                    writer,
                                                    CommitMessage::text("reply", *original)));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("plain")));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Reaction}, {}}}));
    const auto reaction = repository->appendChannelMessage(channelPath,
                                                           channel->channel,
                                                           writer,
                                                           CommitMessage::reaction("yes", *original));
    CPPUNIT_ASSERT(reaction);
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::edit("edited", *own)));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Edit}, {}}}));
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::edit("edited", *own)));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::edit("", *own)));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Delete}, {}}}));
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::edit("", *own)));
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::edit("", *reaction)));
    const auto reopened = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(reopened && verifyReplayed(*reopened, channelPath, channel->channel));
}

void
CircleRepositoryTest::testHistoricalChannelContent()
{
    const auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto writer = addMember(*repository, "historical-writer");
    const auto member = writer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto channelPath = path_ / "historical.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("before")));
    const auto head = channelHead(channelPath);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("after")));
    const auto ban = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(ban && ban->resolution);
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("banned")));
    const auto stale = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(stale);
    CPPUNIT_ASSERT(repository->append(identity(), ArchiveCircleChannel {channel->channel}));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, identity(), CommitMessage::text("archived")));
    CPPUNIT_ASSERT(
        !stale->appendChannelMessage(channelPath, channel->channel, identity(), CommitMessage::text("stale")));
    CPPUNIT_ASSERT(channelHead(channelPath) == head);
}

void
CircleRepositoryTest::testPendingChannelPolicy()
{
    auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto writer = addMember(*repository, "pending-writer");
    const auto member = writer.second->issuer->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"writer", {}}));
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRoleAssignment {member, "writer"}));
    const auto channelPath = path_ / "pending.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"unrelated", {}}));
    const auto left = repository->head();
    setHead(base);
    auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(identity(), SetCircleRole {"writer", {{Permission::Text}, {}}}));
    const auto right = other->head();
    CPPUNIT_ASSERT(
        other->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("offline branch")));
    setHead(left);
    const auto pending = validateReplayed(*repository, channelPath, channel->channel);
    CPPUNIT_ASSERT(pending.status == ChannelValidationStatus::PENDING_DEPENDENCY);
    CPPUNIT_ASSERT(pending.missingPolicies == std::vector<std::string> {right});
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, identity(), CommitMessage::text("not yet")));
    CPPUNIT_ASSERT(repository->merge(identity(), right));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("accepted")));
}

std::string
CircleRepositoryTest::channelHead(const std::filesystem::path& path)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, path.string().c_str()) == 0);
    GitRepository repository(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_reference_name_to_id(&oid, repository.get(), "refs/heads/main") == 0);
    return git_oid_tostr_s(&oid);
}

void
CircleRepositoryTest::setMain(const std::filesystem::path& repository, std::string_view commit)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, repository.string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_oid_fromstr(&oid, std::string(commit).c_str()) == 0);
    git_reference* referenceRaw = nullptr;
    CPPUNIT_ASSERT(git_reference_create(&referenceRaw, repo.get(), "refs/heads/main", &oid, 1, "test") == 0);
    GitReference reference(referenceRaw);
}

void
CircleRepositoryTest::testChannelMerge()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "merged.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto& id = channel->channel;
    const auto left = repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("left"));
    CPPUNIT_ASSERT(left);
    setMain(channelPath, id);
    const auto right = repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("right"));
    CPPUNIT_ASSERT(right);
    CPPUNIT_ASSERT(!repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("early", *left)));
    CPPUNIT_ASSERT(repository->fastForwardChannel(channelPath, id, *left).status == ChannelValidationStatus::DIVERGED);
    CPPUNIT_ASSERT(channelHead(channelPath) == *right);
    const auto merged = repository->mergeChannel(channelPath, id, identity(), *left);
    CPPUNIT_ASSERT(merged.status == ChannelValidationStatus::VALID && merged.head == channelHead(channelPath));
    CPPUNIT_ASSERT(merged.head != *left && merged.head != *right);
    CPPUNIT_ASSERT(repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("late", *left)));
    CPPUNIT_ASSERT(repository->mergeChannel(channelPath, id, identity(), *left).status
                   == ChannelValidationStatus::VALID);
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, id));
}

void
CircleRepositoryTest::testChannelMergeRules()
{
    const auto repository = CircleRepository::create(path_,
                                                     identity(),
                                                     {{Permission::ReadChannel, Permission::Text}, {}});
    CPPUNIT_ASSERT(repository);
    const auto outsider = addMember(*repository, "merge-outsider");
    const auto channelPath = path_ / "rules.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto& id = channel->channel;
    const auto left = repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("left"));
    setMain(channelPath, id);
    const auto right = repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("right"));
    CPPUNIT_ASSERT(left && right);
    const auto forge = [&](const std::string& parent,
                           const std::string& other,
                           const std::string& recordParent,
                           const std::string& recordOther,
                           const dht::crypto::Identity* signer) {
        TamperOptions options;
        options.repository = channelPath;
        options.parent = parent;
        options.otherParent = other;
        options.merge = true;
        options.signer = signer;
        options.message = CircleRecord {CircleChannelMerge {id, recordParent, recordOther},
                                        repository->id(),
                                        repository->head()}
                              .toString();
        tamper(*repository, options);
        return validateReplayed(*repository, channelPath, id).status;
    };
    CPPUNIT_ASSERT(forge(*right, *left, *right, *left, nullptr) == ChannelValidationStatus::VALID);
    CPPUNIT_ASSERT(forge(*right, *left, *left, *right, nullptr) == ChannelValidationStatus::INVALID);
    CPPUNIT_ASSERT(forge(*right, id, *right, id, nullptr) == ChannelValidationStatus::INVALID);
    CPPUNIT_ASSERT(forge(*right, *left, *right, *left, &outsider) == ChannelValidationStatus::VALID);
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleChannelRule {id, std::nullopt, PermissionRule {}}));
    CPPUNIT_ASSERT(forge(*right, *left, *right, *left, &outsider) == ChannelValidationStatus::INVALID);
    setMain(channelPath, *right);
    CPPUNIT_ASSERT(repository->mergeChannel(channelPath, id, outsider, *left).status != ChannelValidationStatus::VALID);
    CPPUNIT_ASSERT(channelHead(channelPath) == *right);
}

void
CircleRepositoryTest::testChannelFetchedUpdates()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "origin.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto& id = channel->channel;
    std::filesystem::copy(channelPath, peerPath_, std::filesystem::copy_options::recursive);
    const auto first = repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("first"));
    auto fetched = fetchFrom(peerPath_, channelPath, "refs/jami/incoming");
    CPPUNIT_ASSERT(first && fetched == first);
    auto result = repository->fastForwardChannel(peerPath_, id, *fetched);
    CPPUNIT_ASSERT(result.status == ChannelValidationStatus::VALID && channelHead(peerPath_) == *first);
    const auto local = repository->appendChannelMessage(peerPath_, id, identity(), CommitMessage::text("local"));
    const auto remote = repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text("remote"));
    fetched = fetchFrom(peerPath_, channelPath, "refs/jami/incoming");
    CPPUNIT_ASSERT(local && remote && fetched == remote);
    CPPUNIT_ASSERT(repository->fastForwardChannel(peerPath_, id, *fetched).status == ChannelValidationStatus::DIVERGED);
    CPPUNIT_ASSERT(channelHead(peerPath_) == *local);
    result = repository->mergeChannel(peerPath_, id, identity(), *fetched);
    CPPUNIT_ASSERT(result.status == ChannelValidationStatus::VALID && channelHead(peerPath_) == result.head);
    CPPUNIT_ASSERT(verifyReplayed(*repository, peerPath_, id));
}

void
CircleRepositoryTest::testIncrementalChannelValidation()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "incremental.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto& id = channel->channel;
    for (const auto* body : {"one", "two", "three"})
        CPPUNIT_ASSERT(repository->appendChannelMessage(channelPath, id, identity(), CommitMessage::text(body)));
    CPPUNIT_ASSERT(repository->validateChannel(channelPath, id, 1).status == ChannelValidationStatus::VALID);
    CPPUNIT_ASSERT(validateReplayed(*repository, channelPath, id, 1).status == ChannelValidationStatus::LIMIT_EXCEEDED);
    CPPUNIT_ASSERT(repository->verifyChannel(channelPath, id));
    const auto previous = channelHead(channelPath);
    TamperOptions options;
    options.repository = channelPath;
    options.parent = previous;
    options.signature = Signature::INVALID;
    options.message = CircleRecord {CircleChannelContent {id, previous, CommitMessage::text("forged")},
                                    repository->id(),
                                    repository->head()}
                          .toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(repository->validateChannel(channelPath, id).status == ChannelValidationStatus::INVALID);
    setMain(channelPath, previous);
    CPPUNIT_ASSERT(repository->validateChannel(channelPath, id, 1).status == ChannelValidationStatus::VALID);
}

void
CircleRepositoryTest::testChannelContentTampering()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "tampered.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const CircleRecord original {CircleChannelContent {channel->channel,
                                                       channel->channel,
                                                       CommitMessage::text("writer")},
                                 repository->id(),
                                 repository->head()};
    for (auto signature :
         {Signature::MISSING, Signature::INVALID, Signature::MALFORMED, Signature::EMPTY, Signature::ALTERED_BODY}) {
        TamperOptions options;
        options.repository = channelPath;
        options.parent = channel->channel;
        options.message = original.toString();
        options.signature = signature;
        tamper(*repository, options);
        CPPUNIT_ASSERT(validateReplayed(*repository, channelPath, channel->channel).status
                       == ChannelValidationStatus::INVALID);
    }
    for (auto field : {&CircleRecord::circle, &CircleRecord::base}) {
        auto record = original;
        record.*field = std::string(40, 'f');
        TamperOptions options;
        options.repository = channelPath;
        options.parent = channel->channel;
        options.message = record.toString();
        tamper(*repository, options);
        const auto result = validateReplayed(*repository, channelPath, channel->channel);
        CPPUNIT_ASSERT(result.status
                       == (field == &CircleRecord::base ? ChannelValidationStatus::PENDING_DEPENDENCY
                                                        : ChannelValidationStatus::INVALID));
    }
    auto record = original;
    std::get<CircleChannelContent>(record.payload).parent = std::string(40, 'a');
    TamperOptions options;
    options.repository = channelPath;
    options.parent = channel->channel;
    options.message = record.toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(!verifyReplayed(*repository, channelPath, channel->channel));
}

void
CircleRepositoryTest::testChannelEditAuthorBinding()
{
    const auto repository = CircleRepository::create(path_,
                                                     identity(),
                                                     {{Permission::ReadChannel, Permission::Text, Permission::Edit},
                                                      {}});
    CPPUNIT_ASSERT(repository);
    const auto writer = addMember(*repository, "other-editor");
    const auto channelPath = path_ / "authors.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto original = repository->appendChannelMessage(channelPath,
                                                           channel->channel,
                                                           identity(),
                                                           CommitMessage::text("owner"));
    CPPUNIT_ASSERT(original);
    CPPUNIT_ASSERT(!repository->appendChannelMessage(channelPath,
                                                     channel->channel,
                                                     writer,
                                                     CommitMessage::edit("forged", *original)));
    TamperOptions options;
    options.repository = channelPath;
    options.parent = *original;
    options.signer = &writer;
    options.message = CircleRecord {CircleChannelContent {channel->channel,
                                                          *original,
                                                          CommitMessage::edit("forged", *original)},
                                    repository->id(),
                                    repository->head()}
                          .toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(validateReplayed(*repository, channelPath, channel->channel).status
                   == ChannelValidationStatus::INVALID);
}

void
CircleRepositoryTest::testChannelContentWrongTree()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "tree.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open_bare(&raw, channelPath.string().c_str()) == 0);
    GitRepository repo(raw);
    git_oid blob {}, tree {};
    CPPUNIT_ASSERT(git_blob_create_frombuffer(&blob, repo.get(), "extra", 5) == 0);
    git_treebuilder* builderRaw = nullptr;
    CPPUNIT_ASSERT(git_treebuilder_new(&builderRaw, repo.get(), nullptr) == 0);
    std::unique_ptr<git_treebuilder, decltype(&git_treebuilder_free)> builder(builderRaw, git_treebuilder_free);
    CPPUNIT_ASSERT(git_treebuilder_insert(nullptr, builder.get(), "forbidden", &blob, GIT_FILEMODE_BLOB) == 0);
    CPPUNIT_ASSERT(git_treebuilder_write(&tree, builder.get()) == 0);
    TamperOptions options;
    options.repository = channelPath;
    options.parent = channel->channel;
    options.tree = tree;
    options.message = CircleRecord {CircleChannelContent {channel->channel,
                                                          channel->channel,
                                                          CommitMessage::text("extra tree")},
                                    repository->id(),
                                    repository->head()}
                          .toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(validateReplayed(*repository, channelPath, channel->channel).status
                   == ChannelValidationStatus::INVALID);
}

void
CircleRepositoryTest::testChannelReadPrerequisite()
{
    const auto repository = CircleRepository::create(path_,
                                                     identity(),
                                                     {{Permission::ReadChannel, Permission::Text}, {}});
    CPPUNIT_ASSERT(repository);
    const auto writer = addMember(*repository, "unreadable-writer");
    const auto channelPath = path_ / "unreadable.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(
        repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("before")));
    CPPUNIT_ASSERT(repository->append(identity(),
                                      SetCircleChannelRule {channel->channel,
                                                            std::nullopt,
                                                            PermissionRule {{Permission::Text}, {}}}));
    CPPUNIT_ASSERT(verifyReplayed(*repository, channelPath, channel->channel));
    CPPUNIT_ASSERT(
        !repository->appendChannelMessage(channelPath, channel->channel, writer, CommitMessage::text("denied")));
    const auto previous = channelHead(channelPath);
    TamperOptions options;
    options.repository = channelPath;
    options.parent = previous;
    options.signer = &writer;
    options.message = CircleRecord {CircleChannelContent {channel->channel, previous, CommitMessage::text("forged")},
                                    repository->id(),
                                    repository->head()}
                          .toString();
    tamper(*repository, options);
    CPPUNIT_ASSERT(validateReplayed(*repository, channelPath, channel->channel).status
                   == ChannelValidationStatus::INVALID);
}

void
CircleRepositoryTest::testCurrentRuntimeAuthorization()
{
    const auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("runtime-account", {}, 2048, true);
    const auto first = dht::crypto::generateIdentity("runtime-device", account, 2048, false);
    const auto second = dht::crypto::generateIdentity("unknown-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    const auto channel = repository->createChannel(path_ / "runtime.git", identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CPPUNIT_ASSERT(!repository->authorizePeer(*first.second));
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    CPPUNIT_ASSERT(repository->authorizePeer(*first.second));
    CPPUNIT_ASSERT(!repository->authorizePeer(*first.second, channel->channel));
    CPPUNIT_ASSERT(repository->append(first, JoinCircleMember {member}));
    CPPUNIT_ASSERT(repository->authorizePeer(*first.second, channel->channel));
    // Other devices of an active account read without enrolling; enrollment only signs commits.
    CPPUNIT_ASSERT(repository->authorizePeer(*second.second));
    CPPUNIT_ASSERT(repository->authorizePeer(*second.second, channel->channel));
    CPPUNIT_ASSERT(repository->append(second, RegisterCircleDevice {member, second.second->getLongId().toString()}));
    CPPUNIT_ASSERT(repository->authorizePeer(*second.second));
    CPPUNIT_ASSERT(
        repository->append(identity(), SetCircleChannelRule {channel->channel, std::nullopt, PermissionRule {}}));
    CPPUNIT_ASSERT(repository->authorizePeer(*first.second));
    CPPUNIT_ASSERT(!repository->authorizePeer(*first.second, channel->channel));
    const auto stale = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(stale && stale->authorizePeer(*first.second));
    const auto ban = repository->moderate(identity(), {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(ban && ban->resolution);
    CPPUNIT_ASSERT(!repository->authorizePeer(*first.second));
    CPPUNIT_ASSERT(!stale->authorizePeer(*first.second));
}

void
CircleRepositoryTest::testMountedRuntimeSessions()
{
    const auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "mounted.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto account = dht::crypto::generateIdentity("mounted-peer", {}, 2048, true);
    const auto peer = dht::crypto::generateIdentity("mounted-peer-device", account, 2048, false);
    const auto member = account.second->getId().toString();
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {member}));
    bool denied = false;
    CircleModule runtime(identity(),
                         [&](const auto& cert) { return !denied || cert.getLongId() != peer.second->getLongId(); });
    CPPUNIT_ASSERT(runtime.mountCircle(path_, repository->id()));
    CPPUNIT_ASSERT(runtime.mountChannel(channelPath, repository->id(), channel->channel));
    const auto metadata = "circles/v1/" + repository->id();
    const auto content = "channels/v1/" + channel->channel;
    CPPUNIT_ASSERT(runtime.authorize(metadata, *peer.second));
    CPPUNIT_ASSERT(!runtime.authorize(content, *peer.second));
    unsigned metadataClosed = 0, contentClosed = 0;
    const auto circleSession = runtime.open(metadata, peer.second, [&] { ++metadataClosed; });
    CPPUNIT_ASSERT(circleSession && circleSession->authorized());
    CPPUNIT_ASSERT(repository->append(peer, JoinCircleMember {member}));
    CPPUNIT_ASSERT(runtime.refresh(repository->id()));
    const auto channelSession = runtime.open(content, peer.second, [&] { ++contentClosed; });
    CPPUNIT_ASSERT(channelSession && channelSession->authorized());
    CPPUNIT_ASSERT(!runtime.authorize("circles/v2/" + repository->id(), *peer.second));
    CPPUNIT_ASSERT(!runtime.authorize(content + "/extra", *peer.second));
    CPPUNIT_ASSERT(
        repository->append(identity(), SetCircleChannelRule {channel->channel, std::nullopt, PermissionRule {}}));
    CPPUNIT_ASSERT(runtime.refresh(repository->id()));
    CPPUNIT_ASSERT(contentClosed == 1 && metadataClosed == 0);
    CPPUNIT_ASSERT(!channelSession->authorized());
    CPPUNIT_ASSERT(circleSession->authorized());
    denied = true;
    runtime.revalidate();
    CPPUNIT_ASSERT(metadataClosed == 1);
    CPPUNIT_ASSERT(!circleSession->authorized());
}

void
CircleRepositoryTest::testRuntimeRemountInvalidatesChannels()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "remounted.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto circle = repository->id();
    const auto base = repository->head();
    CPPUNIT_ASSERT(repository->append(identity(), SetCircleRole {"left", {}}));
    CPPUNIT_ASSERT(repository->appendChannelMessage(channelPath,
                                                    channel->channel,
                                                    identity(),
                                                    CommitMessage::text("branch content")));
    CircleModule runtime(identity(), [](const auto&) { return true; });
    CPPUNIT_ASSERT(runtime.mountCircle(path_, circle));
    CPPUNIT_ASSERT(runtime.mountChannel(channelPath, circle, channel->channel));
    const auto route = "channels/v1/" + channel->channel;
    CPPUNIT_ASSERT(runtime.authorize(route, *identity().second));
    setHead(base);
    const auto other = openReplayed(path_, circle);
    CPPUNIT_ASSERT(other && other->append(identity(), SetCircleRole {"right", {}}));
    CPPUNIT_ASSERT(runtime.mountCircle(path_, circle));
    CPPUNIT_ASSERT(!runtime.authorize(route, *identity().second));
    CPPUNIT_ASSERT(runtime.refresh(circle));
    CPPUNIT_ASSERT(!runtime.authorize(route, *identity().second));
    CPPUNIT_ASSERT(runtime.authorize("circles/v1/" + circle, *identity().second));
}

void
CircleRepositoryTest::testRuntimeSessionLimit()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto account = dht::crypto::generateIdentity("quota-account", {}, 2048, true);
    CPPUNIT_ASSERT(repository->append(identity(), InviteCircleMember {account.second->getId().toString()}));
    CircleModule runtime(identity(), [](const auto&) { return true; });
    CPPUNIT_ASSERT(runtime.mountCircle(path_, repository->id()));
    const auto route = "circles/v1/" + repository->id();
    unsigned replaced = 0;
    const auto first = runtime.open(route, identity().second, [&] { ++replaced; });
    const auto second = runtime.open(route, identity().second, [] {});
    CPPUNIT_ASSERT(first && second && replaced == 1 && !first->authorized() && second->authorized());
    std::vector<std::shared_ptr<CircleSession>> sessions;
    for (size_t index = 0; index < CircleModule::MAX_ACCOUNT_SESSIONS; ++index) {
        const auto device = dht::crypto::generateIdentity("quota-device", account, 2048, false);
        const auto session = runtime.open(route, device.second, [] {});
        CPPUNIT_ASSERT(session);
        sessions.push_back(session);
    }
    const auto extra = dht::crypto::generateIdentity("quota-extra", account, 2048, false);
    CPPUNIT_ASSERT(!runtime.open(route, extra.second, [] {}));
    CPPUNIT_ASSERT(second->authorized());
    sessions.pop_back();
    const auto accepted = runtime.open(route, extra.second, [] {});
    CPPUNIT_ASSERT(accepted);
    sessions.push_back(accepted);
    runtime.unmount(repository->id());
    CPPUNIT_ASSERT(
        std::none_of(sessions.begin(), sessions.end(), [](const auto& session) { return session->authorized(); }));
    CPPUNIT_ASSERT(!second->authorized());
}

void
CircleRepositoryTest::testRuntimeGrantCache()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    std::atomic<unsigned> checks {0};
    CircleModule runtime(identity(), [&](const auto&) {
        ++checks;
        return true;
    });
    CPPUNIT_ASSERT(runtime.mountCircle(path_, circle));
    const auto route = "circles/v1/" + circle;
    const auto session = runtime.open(route, identity().second, [] {});
    CPPUNIT_ASSERT(session && session->grant() == circle);
    const auto before = checks.load();
    for (int index = 0; index < 100; ++index)
        CPPUNIT_ASSERT(session->grant() == circle);
    CPPUNIT_ASSERT(checks == before);
    const auto first = runtime.circle(circle)->append(identity(), SetCircleRole {"writer", {}});
    CPPUNIT_ASSERT(first && session->grant() == *first && checks > before);
    const auto other = CircleRepository::open(path_, circle);
    const auto second = other ? other->append(identity(), SetCircleRole {"reader", {}}) : std::nullopt;
    CPPUNIT_ASSERT(second && session->grant() == *first);
    runtime.revalidate();
    CPPUNIT_ASSERT(session->grant() == *second);
    CircleModule expiring(identity(), [](const auto&) { return true; }, std::chrono::seconds(0));
    CPPUNIT_ASSERT(expiring.mountCircle(path_, circle));
    const auto fresh = expiring.open(route, identity().second, [] {});
    CPPUNIT_ASSERT(fresh && fresh->grant() == *second);
    const auto third = other->append(identity(), SetCircleRole {"third", {}});
    CPPUNIT_ASSERT(third && fresh->grant() == *third);
}

void
CircleRepositoryTest::testRuntimePinnedChannel()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto channelPath = path_ / "pinned.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    CircleModule runtime(identity(), [](const auto&) { return true; });
    CPPUNIT_ASSERT(runtime.mountCircle(path_, repository->id()));
    CPPUNIT_ASSERT(runtime.mountChannel(channelPath, repository->id(), channel->channel));
    const auto session = runtime.open("channels/v1/" + channel->channel, identity().second, [] {});
    CPPUNIT_ASSERT(session && session->grant() == channel->channel);
    const auto message = runtime.appendChannelMessage(channel->channel, CommitMessage::text("pinned"));
    CPPUNIT_ASSERT(message && session->grant() == *message);
    setMain(channelPath, channel->channel);
    runtime.revalidate();
    CPPUNIT_ASSERT(session->grant() == *message);
}

void
CircleRepositoryTest::testRuntimeFetchAndClone()
{
    const auto repository = CircleRepository::create(path_,
                                                     identity(),
                                                     {{Permission::ReadChannel, Permission::Text}, {}});
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto member = addMember(*repository, "fetching-member");
    const auto channelPath = path_ / "fetched.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto& id = channel->channel;
    CircleModule origin(identity(), [](const auto&) { return true; });
    CPPUNIT_ASSERT(origin.mountCircle(path_, circle) && origin.mountChannel(channelPath, circle, id));
    CircleModule peer(member, [](const auto&) { return true; });
    const auto circleRoute = "circles/v1/" + circle;
    const auto channelRoute = "channels/v1/" + id;
    const auto from = [](const std::filesystem::path& source) {
        return [source](const std::filesystem::path& repository, std::string_view ref) {
            return fetchFrom(repository, source, ref);
        };
    };
    const auto peerChannel = peerPath_ / "fetched.git";
    CPPUNIT_ASSERT(!peer.clone(channelRoute, peerChannel, circle, from(channelPath)));
    CPPUNIT_ASSERT(peer.clone(circleRoute, peerPath_, circle, from(path_)));
    CPPUNIT_ASSERT(peer.circle(circle)->head() == origin.circle(circle)->head());
    CPPUNIT_ASSERT(!peer.clone(circleRoute, peerPath_, circle, from(path_)));
    CPPUNIT_ASSERT(peer.clone(channelRoute, peerChannel, circle, from(channelPath)));
    const auto update = origin.circle(circle)->append(identity(), SetCircleRole {"writer", {}});
    const auto first = origin.appendChannelMessage(id, CommitMessage::text("first"));
    CPPUNIT_ASSERT(update && first);
    const auto updated = peer.fetch(circleRoute, from(path_));
    CPPUNIT_ASSERT(updated.status == CircleFetchStatus::UPDATED && updated.head == *update);
    CPPUNIT_ASSERT(peer.fetch(circleRoute, from(path_)).status == CircleFetchStatus::UNCHANGED);
    const auto fetched = peer.fetch(channelRoute, from(channelPath));
    CPPUNIT_ASSERT(fetched.status == CircleFetchStatus::UPDATED && fetched.head == *first);
    const auto local = peer.circle(circle)->append(identity(), SetCircleRole {"left", {}});
    const auto remote = origin.circle(circle)->append(identity(), SetCircleRole {"right", {}});
    const auto merged = peer.fetch(circleRoute, from(path_));
    CPPUNIT_ASSERT(local && remote && merged.status == CircleFetchStatus::UPDATED);
    CPPUNIT_ASSERT(merged.head != *local && merged.head != *remote);
    const auto mine = peer.appendChannelMessage(id, CommitMessage::text("mine"));
    const auto theirs = origin.appendChannelMessage(id, CommitMessage::text("theirs"));
    const auto joined = peer.fetch(channelRoute, from(channelPath));
    CPPUNIT_ASSERT(mine && theirs && joined.status == CircleFetchStatus::UPDATED);
    CPPUNIT_ASSERT(joined.head != *mine && joined.head != *theirs);
    CPPUNIT_ASSERT(verifyReplayed(*peer.circle(circle), peerChannel, id));
    const auto session = peer.open(circleRoute, identity().second, [] {});
    CPPUNIT_ASSERT(session && session->grant() == merged.head);

    // Content referencing a policy the peer has not accepted yet waits for the circle.
    const auto later = origin.circle(circle)->append(identity(), SetCircleRole {"later", {}});
    const auto dependent = origin.appendChannelMessage(id, CommitMessage::text("dependent"));
    CPPUNIT_ASSERT(later && dependent);
    CPPUNIT_ASSERT(peer.fetch(channelRoute, from(channelPath)).status == CircleFetchStatus::PENDING_DEPENDENCY);
    CPPUNIT_ASSERT(!peer.accepted(channelRoute, *dependent) && peer.accepted(channelRoute, *first));
    CPPUNIT_ASSERT(peer.fetch(circleRoute, from(path_)).status == CircleFetchStatus::UPDATED);
    CPPUNIT_ASSERT(peer.fetch(channelRoute, from(channelPath)).status == CircleFetchStatus::UPDATED);
    CPPUNIT_ASSERT(peer.accepted(channelRoute, *dependent) && peer.accepted(circleRoute, *later));
}

void
CircleRepositoryTest::testRuntimeRouteQueries()
{
    const auto repository = CircleRepository::create(path_, identity(), {{Permission::ReadChannel}, {}});
    CPPUNIT_ASSERT(repository);
    const auto circle = repository->id();
    const auto member = addMember(*repository, "departing-member");
    const auto memberId = member.second->issuer->getId().toString();
    const auto channelPath = path_ / "queries.git";
    const auto channel = repository->createChannel(channelPath, identity());
    CPPUNIT_ASSERT(channel && channel->registration);
    const auto circleRoute = "circles/v1/" + circle;
    const auto channelRoute = "channels/v1/" + channel->channel;
    CircleModule runtime(identity(), [](const auto&) { return true; });
    CPPUNIT_ASSERT(runtime.mountCircle(path_, circle) && runtime.mountChannel(channelPath, circle, channel->channel));
    CPPUNIT_ASSERT(runtime.circles() == std::vector<std::string> {circle});
    CPPUNIT_ASSERT(runtime.channels(circle) == std::vector<std::string> {channel->channel});
    CPPUNIT_ASSERT(runtime.path(circleRoute) == std::filesystem::canonical(path_));
    CPPUNIT_ASSERT(runtime.head(circleRoute) == repository->head());
    CPPUNIT_ASSERT(runtime.head(channelRoute) == channel->channel);
    CPPUNIT_ASSERT(runtime.accepted(circleRoute, circle) && runtime.accepted(circleRoute, repository->head()));
    CPPUNIT_ASSERT(!runtime.accepted(circleRoute, std::string(40, 'a')) && !runtime.accepted("circles", circle));
    runtime.unmountChannel(channel->channel);
    CPPUNIT_ASSERT(runtime.channels(circle).empty() && !runtime.head(channelRoute));

    CircleModule departing(member, [](const auto&) { return true; });
    CPPUNIT_ASSERT(departing.mountCircle(path_, circle));
    CPPUNIT_ASSERT(departing.mountChannel(channelPath, circle, channel->channel));
    CPPUNIT_ASSERT(repository->append(member, LeaveCircleMember {memberId}));
    CPPUNIT_ASSERT(!departing.authorize(circleRoute, *identity().second));
    CPPUNIT_ASSERT(departing.depart(circle));
    CPPUNIT_ASSERT(departing.authorize(circleRoute, *identity().second));
    CPPUNIT_ASSERT(departing.channels(circle).empty());
    CPPUNIT_ASSERT(!departing.mountChannel(channelPath, circle, channel->channel));
    CircleModule restored(member, [](const auto&) { return true; });
    CPPUNIT_ASSERT(!restored.mountCircle(path_, circle));
    CPPUNIT_ASSERT(restored.mountCircle(path_, circle, true) && restored.authorize(circleRoute, *identity().second));
}

void
CircleRepositoryTest::testMembershipFastForward()
{
    const auto repository = CircleRepository::create(path_, identity());
    CPPUNIT_ASSERT(repository);
    const auto before = openReplayed(path_, repository->id());
    CPPUNIT_ASSERT(before);
    const auto member = addMember(*repository, "fast-forward-member");
    const auto head = repository->head();
    setHead(before->head());
    CPPUNIT_ASSERT(before->merge(identity(), head) == std::optional<std::string>(head));
    CPPUNIT_ASSERT(before->policy() == repository->policy());
    CPPUNIT_ASSERT(before->policy().members.at(member.second->issuer->getId().toString()).state
                   == CircleMemberState::ACTIVE);
}

void
CircleRepositoryTest::testDeviceCertificateBinding()
{
    const auto& device = identity();
    const auto& account = *device.second->issuer;
    const auto member = account.getId().toString();
    CPPUNIT_ASSERT(isCertificateOfDevice(*device.second, device.second->getLongId().toString()));
    CPPUNIT_ASSERT(!isCertificateOfDevice(*device.second, std::string(64, 'a')));
    CPPUNIT_ASSERT(isDeviceOfMember(*device.second, account, member));
    CPPUNIT_ASSERT(!isDeviceOfMember(*device.second, account, std::string(40, 'a')));
}

void
CircleRepositoryTest::testUnverifiedIssuerChainRejected()
{
    const auto& account = identity().second->issuer;
    const auto rogueAccount = dht::crypto::generateIdentity("rogue-account", {}, 2048, true);
    auto rogueDevice = dht::crypto::generateIdentity("rogue-device", rogueAccount, 2048, false);
    rogueDevice.second->issuer = account;
    CPPUNIT_ASSERT(!isDeviceOfMember(*rogueDevice.second, *account, account->getId().toString()));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CircleRepositoryTest::name())
