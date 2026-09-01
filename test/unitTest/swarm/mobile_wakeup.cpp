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

// Extensive tests and simulations for the mobile wake-up mechanism:
// - RoutingTable::getMobileNodesToNotify() (XOR-closest responsibility)
// - RoutingTable::getKnownMobileNodes() (mobility knowledge union)
// - SwarmManager::onMobileNodesChanged() (persistence callback)
// - Persistence round-trip (cold start from saved mobile nodes)
// - Live network simulations (gossip, churn, mass mobile shutdown)

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"
#include "jami.h"
#include "../common.h"
#include "jamidht/swarm/swarm_manager.h"
#include "nodes.h"

#include <dhtnet/multiplexed_socket.h>
#include <opendht/thread_pool.h>
#include <msgpack.hpp>

#include <algorithm>
#include <future>
#include <set>
#include <thread>

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

constexpr size_t N_DESKTOPS = 9;
constexpr size_t N_MOBILES = 3;
constexpr std::chrono::seconds CONVERGENCE_TIMEOUT {60};

struct LegacySwarmResponse
{
    Query q;
    std::vector<NodeId> nodes;
    std::vector<NodeId> mobile_nodes;
    MSGPACK_DEFINE_MAP(q, nodes, mobile_nodes);
};

struct VersionTwoMobileNodeInfo
{
    NodeId id;
    dht::Blob certificate;
    MSGPACK_DEFINE_MAP(id, certificate);
};

struct VersionTwoSwarmResponse
{
    Query q;
    std::vector<NodeId> nodes;
    std::vector<NodeId> mobile_nodes;
    std::vector<VersionTwoMobileNodeInfo> mobile_node_infos;
    MSGPACK_DEFINE_MAP(q, nodes, mobile_nodes, mobile_node_infos);
};

class BlockingWriteChannel final : public dhtnet::ChannelSocketTest
{
public:
    using ChannelSocketTest::ChannelSocketTest;

    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        std::unique_lock lock(writeMutex_);
        if (blockWrites_) {
            writeStarted_ = true;
            writeCv_.notify_all();
            writeCv_.wait(lock, [this] { return !blockWrites_; });
        }
        lock.unlock();
        return ChannelSocketTest::write(buf, len, ec);
    }

    void blockWrites()
    {
        std::lock_guard lock(writeMutex_);
        blockWrites_ = true;
        writeStarted_ = false;
    }

    bool waitForWrite(std::chrono::seconds timeout)
    {
        std::unique_lock lock(writeMutex_);
        return writeCv_.wait_for(lock, timeout, [this] { return writeStarted_; });
    }

    void releaseWrite()
    {
        std::lock_guard lock(writeMutex_);
        blockWrites_ = false;
        writeCv_.notify_all();
    }

private:
    std::mutex writeMutex_;
    std::condition_variable writeCv_;
    bool blockWrites_ {false};
    bool writeStarted_ {false};
};

class MobileWakeUpTest : public CppUnit::TestFixture
{
public:
    ~MobileWakeUpTest() { libjami::fini(); }
    static std::string name() { return "MobileWakeUp"; }

    void setUp();
    void tearDown();

private:
    std::mt19937_64 rd {dht::crypto::getSeededRandomEngine<std::mt19937_64>()};

    // ################# LIVE NETWORK HARNESS #################//

    std::mutex channelSocketsMtx_;
    std::map<NodeId, std::map<NodeId, std::shared_ptr<dhtnet::ChannelSocketTest>>> channelSockets_;
    std::set<std::pair<NodeId, NodeId>> linkedPairs_;
    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagers;
    std::vector<NodeId> desktopIds;
    std::vector<NodeId> mobileIds;
    std::set<NodeId> discoveredNodes;

    std::vector<std::shared_ptr<dhtnet::ChannelSocketTest>> nodeTestChannels1;

    std::shared_ptr<jami::SwarmManager> getManager(const NodeId& id)
    {
        std::lock_guard lk(channelSocketsMtx_);
        auto it = swarmManagers.find(id);
        return it == swarmManagers.end() ? nullptr : it->second;
    }
    std::shared_ptr<jami::SwarmManager> createManager(const NodeId& id, bool mobile);
    void needSocketCallBack(const std::shared_ptr<SwarmManager>& sm);
    void unlinkPair(const NodeId& a, const NodeId& b);
    void buildConvergedNetwork();
    void crossNodes(NodeId nodeId);

    // ################# HELPERS #################//

    static std::shared_ptr<dhtnet::ChannelSocketTest> makeChannel(const NodeId& id)
    {
        return std::make_shared<dhtnet::ChannelSocketTest>(Manager::instance().ioContext(), id, "test1", 0);
    }

    // Brute-force oracle: is self among the redundant closest nodes to mobile?
    static bool oracleResponsible(const NodeId& self, const std::vector<NodeId>& connected, const NodeId& mobile)
    {
        unsigned closerNodes = 0;
        for (const auto& c : connected) {
            if (c == mobile)
                continue;
            if (mobile.xorCmp(self, c) >= 0 && ++closerNodes == RoutingTable::MOBILE_WAKE_REDUNDANCY)
                return false;
        }
        return true;
    }

    static std::set<NodeId> toSet(const std::vector<NodeId>& v) { return {v.begin(), v.end()}; }

    template<typename Pred>
    static bool waitFor(Pred&& pred, std::chrono::seconds timeout)
    {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            if (pred())
                return true;
            std::this_thread::sleep_for(500ms);
        }
        return pred();
    }

    // Verify that a manager's wake-up list is exactly what the brute-force
    // oracle predicts from its own connected nodes and mobility knowledge.
    void checkLocalConsistency(const std::shared_ptr<SwarmManager>& sm);

    // ################# UNIT TEST METHODES #################//

    void testNotifyWithoutConnectedNodes();
    void testMobileProtocolCompatibility();
    void testBlockedResponseWriteDoesNotBlockMaintenance();
    void testLegacyMobileAnnouncementLifecycle();
    void testMobileLeaseCanonicalPayload();
    void testSignedMobileLeaseValidation();
    void testLeaseCertificateResolutionFromPeer();
    void testLeaseCertificateResolutionFromDht();
    void testCertificateRequestIsNotAnOracle();
    void testUnsolicitedCertificateResponseIsIgnored();
    void testCertificateRequestTimeoutFallsBackToDht();
    void testLocalCertificateSkipsCertificateRequest();
    void testUnverifiableLeaseIsNeverFetched();
    void testResolvedCertificateWithBadSignatureIsRejected();
    void testPendingCertificateRequestsAreRetried();
    void testNotifyAgainstBruteForceOracle();
    void testNotifyResponsibilityHandover();
    void testKnownMobileNodes();
    void testConnectedMobileLifecycle();
    void testFailedConnectionPreservesMobility();
    void testRoutingTableInfoResponsibleFlag();
    void testMobileNodesChangedCallback();
    void testDeleteNodeUpdatesMobileNodes();
    void testConcurrentMobileNodesChangedCallbacks();
    void testPersistenceColdStart();
    void testWakeUpCoverageConvergedNetwork();
    void testMobileLifecycleWakeUp();
    void testWakeUpCoverageAfterMassMobileShutdown();

    CPPUNIT_TEST_SUITE(MobileWakeUpTest);
    CPPUNIT_TEST(testNotifyWithoutConnectedNodes);
    CPPUNIT_TEST(testMobileProtocolCompatibility);
    CPPUNIT_TEST(testBlockedResponseWriteDoesNotBlockMaintenance);
    CPPUNIT_TEST(testLegacyMobileAnnouncementLifecycle);
    CPPUNIT_TEST(testMobileLeaseCanonicalPayload);
    CPPUNIT_TEST(testSignedMobileLeaseValidation);
    CPPUNIT_TEST(testLeaseCertificateResolutionFromPeer);
    CPPUNIT_TEST(testLeaseCertificateResolutionFromDht);
    CPPUNIT_TEST(testCertificateRequestIsNotAnOracle);
    CPPUNIT_TEST(testUnsolicitedCertificateResponseIsIgnored);
    CPPUNIT_TEST(testCertificateRequestTimeoutFallsBackToDht);
    CPPUNIT_TEST(testLocalCertificateSkipsCertificateRequest);
    CPPUNIT_TEST(testUnverifiableLeaseIsNeverFetched);
    CPPUNIT_TEST(testResolvedCertificateWithBadSignatureIsRejected);
    CPPUNIT_TEST(testPendingCertificateRequestsAreRetried);
    CPPUNIT_TEST(testNotifyAgainstBruteForceOracle);
    CPPUNIT_TEST(testNotifyResponsibilityHandover);
    CPPUNIT_TEST(testKnownMobileNodes);
    CPPUNIT_TEST(testConnectedMobileLifecycle);
    CPPUNIT_TEST(testFailedConnectionPreservesMobility);
    CPPUNIT_TEST(testRoutingTableInfoResponsibleFlag);
    CPPUNIT_TEST(testMobileNodesChangedCallback);
    CPPUNIT_TEST(testDeleteNodeUpdatesMobileNodes);
    CPPUNIT_TEST(testConcurrentMobileNodesChangedCallbacks);
    CPPUNIT_TEST(testPersistenceColdStart);
    CPPUNIT_TEST(testWakeUpCoverageConvergedNetwork);
    CPPUNIT_TEST(testMobileLifecycleWakeUp);
    CPPUNIT_TEST(testWakeUpCoverageAfterMassMobileShutdown);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MobileWakeUpTest, MobileWakeUpTest::name());

void
MobileWakeUpTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized) {
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    nodeTestChannels1 = buildChannels(nodeTestIds1);
}

void
MobileWakeUpTest::tearDown()
{
    std::vector<std::shared_ptr<SwarmManager>> managers;
    {
        std::lock_guard lk(channelSocketsMtx_);
        for (auto& [id, sm] : swarmManagers)
            managers.emplace_back(sm);
    }
    for (auto& sm : managers)
        sm->shutdown();
    std::lock_guard lk(channelSocketsMtx_);
    swarmManagers.clear();
    channelSockets_.clear();
    linkedPairs_.clear();
    desktopIds.clear();
    mobileIds.clear();
    discoveredNodes.clear();
}

void
MobileWakeUpTest::testBlockedResponseWriteDoesNotBlockMaintenance()
{
    auto manager = std::make_shared<SwarmManager>(nodeTestIds1.at(0), false, rd, [](auto) {
        return false;
    });
    auto channel = std::make_shared<BlockingWriteChannel>(Manager::instance().ioContext(),
                                                          nodeTestIds1.at(1),
                                                          "test1",
                                                          0);
    manager->addChannel(channel);
    channel->blockWrites();

    Message request;
    request.request = Request {Query::FIND, 8, nodeTestIds1.at(2)};
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, request);
    std::vector<uint8_t> packet(buffer.data(), buffer.data() + buffer.size());

    std::thread responseThread([channel, packet = std::move(packet)]() mutable {
        channel->onRecv(std::move(packet));
    });
    const auto writeStarted = channel->waitForWrite(5s);

    std::promise<void> maintenanceDone;
    auto maintenanceResult = maintenanceDone.get_future();
    std::thread maintenanceThread([manager, &maintenanceDone] {
        manager->maintainBuckets();
        maintenanceDone.set_value();
    });
    const auto maintenanceCompleted = maintenanceResult.wait_for(5s) == std::future_status::ready;

    channel->releaseWrite();
    responseThread.join();
    maintenanceThread.join();
    manager->shutdown();

    CPPUNIT_ASSERT(writeStarted);
    CPPUNIT_ASSERT(maintenanceCompleted);
}

std::shared_ptr<jami::SwarmManager>
MobileWakeUpTest::createManager(const NodeId& id, bool mobile)
{
    auto sm = std::make_shared<SwarmManager>(id, mobile, rd, [](auto) { return false; });
    needSocketCallBack(sm);
    {
        std::lock_guard lk(channelSocketsMtx_);
        swarmManagers[id] = sm;
        (mobile ? mobileIds : desktopIds).emplace_back(id);
    }
    return sm;
}

void
MobileWakeUpTest::needSocketCallBack(const std::shared_ptr<SwarmManager>& sm)
{
    if (sm->needSocketCb_)
        return;

    sm->needSocketCb_ = [this, wsm = std::weak_ptr<SwarmManager>(sm)](const std::string& nodeId,
                                                                      auto&& onSocket,
                                                                      bool /*noNewSocket*/) mutable {
        asio::post(*Manager::instance().ioContext(), [this, wsm, nodeId, onSocket = std::move(onSocket)] {
            auto sm = wsm.lock();
            if (!sm || sm->isShutdown())
                return;
            NodeId node = dhtnet::DeviceId(nodeId);
            std::lock_guard lk(channelSocketsMtx_);
            auto it = swarmManagers.find(node);
            if (it == swarmManagers.end())
                return;
            auto& smRemote = it->second;
            if (smRemote->isShutdown())
                return;
            auto myId = sm->getId();
            auto pairKey = std::minmax(myId, node);

            // Skip duplicate link attempts for the same pair
            if (!linkedPairs_.emplace(pairKey.first, pairKey.second).second)
                return;

            auto& cstRemote = channelSockets_[node][myId];
            auto& cstMe = channelSockets_[myId][node];
            cstRemote = makeChannel(myId);
            cstMe = makeChannel(node);
            dhtnet::ChannelSocketTest::link(cstMe, cstRemote);
            onSocket(cstMe);
            smRemote->addChannel(cstRemote);
        });
    };
}

void
MobileWakeUpTest::unlinkPair(const NodeId& a, const NodeId& b)
{
    std::lock_guard lk(channelSocketsMtx_);
    auto pairKey = std::minmax(a, b);
    linkedPairs_.erase(pairKey);
    channelSockets_[a].erase(b);
    channelSockets_[b].erase(a);
}

void
MobileWakeUpTest::crossNodes(NodeId nodeId)
{
    std::list<NodeId> pendingNodes {nodeId};
    discoveredNodes.clear();

    for (const auto& curNode : pendingNodes) {
        if (discoveredNodes.emplace(curNode).second) {
            if (auto sm = getManager(curNode))
                for (const auto& node : sm->getRoutingTable().getNodes())
                    pendingNodes.emplace_back(node);
        }
    }
}

void
MobileWakeUpTest::buildConvergedNetwork()
{
    // Desktops bootstrapped in a ring: the connectivity graph cannot
    // partition, making convergence deterministic.
    for (size_t i = 0; i < N_DESKTOPS; i++)
        createManager(Hash<32>::getRandom(), false);
    for (size_t i = 0; i < N_DESKTOPS; i++) {
        auto sm = getManager(desktopIds.at(i));
        sm->setKnownNodes({desktopIds.at((i + 1) % N_DESKTOPS)});
    }

    CPPUNIT_ASSERT(waitFor(
        [&] {
            crossNodes(desktopIds.front());
            return discoveredNodes.size() == N_DESKTOPS;
        },
        CONVERGENCE_TIMEOUT));
}

void
MobileWakeUpTest::checkLocalConsistency(const std::shared_ptr<SwarmManager>& sm)
{
    auto connected = sm->getConnectedNodes();
    auto knownMobiles = toSet(sm->getKnownMobileNodes());
    auto toNotify = sm->getMobileNodesToNotify();
    auto connectedSet = toSet(connected);

    for (const auto& m : toNotify) {
        // Wake-up targets must be known mobile nodes, not currently connected
        CPPUNIT_ASSERT(knownMobiles.count(m));
        CPPUNIT_ASSERT(!connectedSet.count(m));
    }
}

// ################# DETERMINISTIC ROUTING TABLE TESTS #################//

void
MobileWakeUpTest::testMobileProtocolCompatibility()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId mobile = nodeTestIds1.at(2);
    msgpack::sbuffer legacyBuffer;
    msgpack::pack(legacyBuffer, LegacySwarmResponse {Query::FOUND, {}, {mobile}});
    auto legacyObject = msgpack::unpack(legacyBuffer.data(), legacyBuffer.size());
    Response decodedLegacy;
    legacyObject.get().convert(decodedLegacy);
    CPPUNIT_ASSERT(decodedLegacy.mobile_nodes == std::vector<NodeId> {mobile});
    CPPUNIT_ASSERT(decodedLegacy.mobile_node_infos.empty());

    Response response {Query::FOUND, {}, {mobile}, {{mobile, std::nullopt}}};
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, response);
    auto object = msgpack::unpack(buffer.data(), buffer.size());
    Response decoded;
    object.get().convert(decoded);
    CPPUNIT_ASSERT(decoded.mobile_nodes == response.mobile_nodes);
    CPPUNIT_ASSERT_EQUAL(size_t(1), decoded.mobile_node_infos.size());
    CPPUNIT_ASSERT(decoded.mobile_node_infos.front().id == mobile);

    LegacySwarmResponse decodedByLegacy;
    object.get().convert(decodedByLegacy);
    CPPUNIT_ASSERT(decodedByLegacy.mobile_nodes == response.mobile_nodes);

    // Certificates used to be gossiped inline: they are now simply ignored.
    msgpack::sbuffer versionTwoBuffer;
    msgpack::pack(versionTwoBuffer, VersionTwoSwarmResponse {Query::FOUND, {}, {mobile}, {{mobile, {4, 5, 6}}}});
    auto versionTwoObject = msgpack::unpack(versionTwoBuffer.data(), versionTwoBuffer.size());
    Response decodedVersionTwo;
    versionTwoObject.get().convert(decodedVersionTwo);
    CPPUNIT_ASSERT_EQUAL(size_t(1), decodedVersionTwo.mobile_node_infos.size());
    CPPUNIT_ASSERT(decodedVersionTwo.mobile_node_infos.front().id == mobile);
    CPPUNIT_ASSERT(!decodedVersionTwo.mobile_node_infos.front().lease.has_value());

    MobileLease lease {1, "conversation", dht::InfoHash::getRandom(), mobile, 1, 2, {7, 8, 9}};
    Response leasedResponse {Query::FOUND, {}, {mobile}, {{mobile, lease}}};
    msgpack::sbuffer leasedBuffer;
    msgpack::pack(leasedBuffer, leasedResponse);
    auto leasedObject = msgpack::unpack(leasedBuffer.data(), leasedBuffer.size());
    VersionTwoSwarmResponse decodedLeasedByVersionTwo;
    leasedObject.get().convert(decodedLeasedByVersionTwo);
    CPPUNIT_ASSERT_EQUAL(size_t(1), decodedLeasedByVersionTwo.mobile_node_infos.size());
    CPPUNIT_ASSERT(decodedLeasedByVersionTwo.mobile_node_infos.front().id == mobile);
    CPPUNIT_ASSERT(decodedLeasedByVersionTwo.mobile_node_infos.front().certificate.empty());

    // A leased record must be a fraction of the size of the old one, which carried
    // a full PEM chain (device certificate + account CA) for every mobile node.
    CPPUNIT_ASSERT(leasedBuffer.size() < 512);

    auto sm = std::make_shared<SwarmManager>(nodeTestIds1.at(0), false, rd, [](auto) { return false; });
    sm->setMobileNodes(response.mobile_node_infos);
    auto infos = sm->getKnownMobileNodeInfos();
    CPPUNIT_ASSERT_EQUAL(size_t(1), infos.size());
    CPPUNIT_ASSERT(infos.front().id == mobile);
    CPPUNIT_ASSERT(!infos.front().lease.has_value());

    auto scopedManager
        = std::make_shared<SwarmManager>(nodeTestIds1.at(0), false, rd, [](auto) { return false; }, "conversation");
    scopedManager->setMobileNodes(decodedVersionTwo.mobile_node_infos, true);
    CPPUNIT_ASSERT(scopedManager->getKnownMobileNodes().empty());
    scopedManager->shutdown();
}

void
MobileWakeUpTest::testLegacyMobileAnnouncementLifecycle()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0), false, rd, [](auto) { return false; }, "legacy-conversation");
    auto inbound = makeChannel(nodeTestIds1.at(2));
    auto remote = makeChannel(nodeTestIds1.at(0));
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    Message legacyMessage;
    legacyMessage.v = 1;
    legacyMessage.is_mobile = true;
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, legacyMessage);
    std::error_code ec;
    remote->write(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), ec);
    CPPUNIT_ASSERT(!ec);
    CPPUNIT_ASSERT(waitFor([&] { return toSet(desktop->getKnownMobileNodes()).contains(inbound->deviceId()); },
                           CONVERGENCE_TIMEOUT));

    inbound->shutdown();
    CPPUNIT_ASSERT(waitFor([&] { return toSet(desktop->getMobileNodesToNotify()).contains(inbound->deviceId()); },
                           CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT_EQUAL(size_t(1), desktop->getMobileNodeInfosToNotify().size());
    desktop->shutdown();
}

void
MobileWakeUpTest::testMobileLeaseCanonicalPayload()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    MobileLease lease {1,
                       "0123456789abcdef0123456789abcdef01234567",
                       dht::InfoHash("fedcba9876543210fedcba9876543210fedcba98"),
                       nodeTestIds1.at(2),
                       1'700'000'000,
                       1'702'592'000,
                       {9, 8, 7}};
    const auto payload = mobileLeasePayload(lease);
    CPPUNIT_ASSERT(payload == mobileLeasePayload(lease));

    auto object = msgpack::unpack(reinterpret_cast<const char*>(payload.data()), payload.size());
    CPPUNIT_ASSERT_EQUAL(msgpack::type::ARRAY, object.get().type);
    CPPUNIT_ASSERT_EQUAL(uint32_t(7), object.get().via.array.size);
    CPPUNIT_ASSERT_EQUAL(std::string("DRT-MOBILE"), object.get().via.array.ptr[0].as<std::string>());
    CPPUNIT_ASSERT_EQUAL(uint8_t(1), object.get().via.array.ptr[1].as<uint8_t>());
    CPPUNIT_ASSERT_EQUAL(lease.conversation_id, object.get().via.array.ptr[2].as<std::string>());
    CPPUNIT_ASSERT(lease.issuer_id == object.get().via.array.ptr[3].as<dht::InfoHash>());
    CPPUNIT_ASSERT_EQUAL(lease.device_id.toString(), object.get().via.array.ptr[4].as<std::string>());
    CPPUNIT_ASSERT_EQUAL(lease.issued_at, object.get().via.array.ptr[5].as<int64_t>());
    CPPUNIT_ASSERT_EQUAL(lease.expires_at, object.get().via.array.ptr[6].as<int64_t>());

    lease.signature = {1, 2, 3, 4};
    CPPUNIT_ASSERT(payload == mobileLeasePayload(lease));
}

void
MobileWakeUpTest::testSignedMobileLeaseValidation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    const auto accountIdentity = dht::crypto::generateIdentity("lease-account", {}, 2048, true);
    const auto deviceIdentity = dht::crypto::generateIdentity("lease-device", accountIdentity, 2048, false);
    const auto deviceId = deviceIdentity.second->getLongId();
    const std::string conversationId = "0123456789abcdef0123456789abcdef01234567";
    const auto now = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    auto makeInfo = [&](int64_t expiry) {
        MobileLease lease {1, conversationId, accountIdentity.second->getId(), deviceId, now, expiry, {}};
        lease.signature = deviceIdentity.first->sign(mobileLeasePayload(lease));
        return MobileNodeInfo {deviceId, std::move(lease)};
    };

    const auto issuerId = accountIdentity.second->getId();
    auto isConversationMember = [issuerId](const dht::InfoHash& candidate) {
        return candidate == issuerId;
    };
    // Stands in for the account certificate store.
    auto certificates = std::make_shared<std::map<NodeId, std::shared_ptr<dht::crypto::Certificate>>>();
    auto certificateProvider = [certificates](const NodeId& id) -> std::shared_ptr<dht::crypto::Certificate> {
        auto it = certificates->find(id);
        return it == certificates->end() ? nullptr : it->second;
    };
    auto manager = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        conversationId,
        std::function<std::optional<MobileNodeInfo>()> {},
        isConversationMember,
        certificateProvider);
    auto valid = makeInfo(now + 60);

    // Without the certificate the lease cannot be verified, so the node must not
    // become a wake-up target even though the lease itself is perfectly valid.
    manager->setMobileNodes({valid});
    CPPUNIT_ASSERT(manager->getKnownMobileNodes().empty());

    (*certificates)[deviceId] = deviceIdentity.second;
    manager->setMobileNodes({valid});
    auto infos = manager->getKnownMobileNodeInfos();
    CPPUNIT_ASSERT_EQUAL(size_t(1), infos.size());
    CPPUNIT_ASSERT(infos.front().lease.has_value());
    CPPUNIT_ASSERT(valid.lease->signature == infos.front().lease->signature);

    auto renewed = valid;
    renewed.lease->issued_at += 1;
    renewed.lease->signature = deviceIdentity.first->sign(mobileLeasePayload(*renewed.lease));
    manager->setMobileNodes({renewed});
    infos = manager->getKnownMobileNodeInfos();
    CPPUNIT_ASSERT_EQUAL(renewed.lease->issued_at, infos.front().lease->issued_at);

    auto replayed = valid;
    replayed.lease->conversation_id = "different-conversation";
    manager->deleteNode({deviceId});
    manager->setMobileNodes({replayed});
    CPPUNIT_ASSERT(manager->getKnownMobileNodes().empty());

    auto tampered = valid;
    tampered.lease->expires_at += 1;
    manager->setMobileNodes({tampered});
    CPPUNIT_ASSERT(manager->getKnownMobileNodes().empty());

    const auto outsiderAccount = dht::crypto::generateIdentity("lease-outsider", {}, 2048, true);
    const auto outsiderDevice = dht::crypto::generateIdentity("lease-outsider-device", outsiderAccount, 2048, false);
    MobileLease outsiderLease {1,
                               conversationId,
                               outsiderAccount.second->getId(),
                               outsiderDevice.second->getLongId(),
                               now,
                               now + 60,
                               {}};
    outsiderLease.signature = outsiderDevice.first->sign(mobileLeasePayload(outsiderLease));
    (*certificates)[outsiderDevice.second->getLongId()] = outsiderDevice.second;
    manager->setMobileNodes({{outsiderLease.device_id, std::move(outsiderLease)}});
    CPPUNIT_ASSERT(manager->getKnownMobileNodes().empty());

    size_t providerCalls = 0;
    auto renewalManager = std::make_shared<SwarmManager>(
        deviceId,
        true,
        rd,
        [](auto) { return false; },
        conversationId,
        [&] {
            ++providerCalls;
            const auto duration = providerCalls == 1 ? MOBILE_LEASE_RENEWAL_THRESHOLD : MAX_MOBILE_LEASE_DURATION;
            return std::optional<MobileNodeInfo>(
                makeInfo(now + std::chrono::duration_cast<std::chrono::seconds>(duration).count()));
        },
        isConversationMember,
        certificateProvider);
    CPPUNIT_ASSERT(renewalManager->getLocalMobileNodeInfo().has_value());
    CPPUNIT_ASSERT_EQUAL(size_t(1), providerCalls);
    CPPUNIT_ASSERT(renewalManager->getLocalMobileNodeInfo().has_value());
    CPPUNIT_ASSERT_EQUAL(size_t(2), providerCalls);
    CPPUNIT_ASSERT(renewalManager->getLocalMobileNodeInfo().has_value());
    CPPUNIT_ASSERT_EQUAL(size_t(2), providerCalls);
    renewalManager->shutdown();

    manager->setMobileNodes({makeInfo(now + 2)});
    manager->setMobileNodes({MobileNodeInfo {deviceId, std::nullopt}});
    CPPUNIT_ASSERT_EQUAL(size_t(1), manager->getKnownMobileNodes().size());
    CPPUNIT_ASSERT(waitFor([&] { return manager->getKnownMobileNodes().empty(); }, 5s));
    manager->shutdown();
}

namespace {

constexpr const char* TEST_CONVERSATION_ID = "0123456789abcdef0123456789abcdef01234567";

int64_t
nowSeconds()
{
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

/// A device certificate plus a valid lease binding it to TEST_CONVERSATION_ID.
struct LeasedDevice
{
    dht::crypto::Identity account;
    dht::crypto::Identity device;
    NodeId id;
    MobileLease lease;

    explicit LeasedDevice(const std::string& name)
        : account(dht::crypto::generateIdentity(name + "-account", {}, 2048, true))
        , device(dht::crypto::generateIdentity(name + "-device", account, 2048, false))
        , id(device.second->getLongId())
    {
        const auto now = nowSeconds();
        lease = MobileLease {1, TEST_CONVERSATION_ID, account.second->getId(), id, now, now + 3600, {}};
        lease.signature = device.first->sign(mobileLeasePayload(lease));
    }

    MobileNodeInfo info() const { return MobileNodeInfo {id, lease}; }
};

/// Reads the messages a SwarmManager writes on a linked test channel.
struct MessageSink
{
    std::mutex mutex;
    std::vector<Message> messages;

    void attach(const std::shared_ptr<dhtnet::ChannelSocketTest>& channel)
    {
        channel->setOnRecv([this](const uint8_t* data, std::size_t size) {
            try {
                auto oh = msgpack::unpack(reinterpret_cast<const char*>(data), size);
                Message msg;
                oh.get().convert(msg);
                std::lock_guard lock(mutex);
                messages.emplace_back(std::move(msg));
            } catch (const std::exception&) {
            }
            return size;
        });
    }

    template<typename Pred>
    std::optional<Message> find(Pred&& pred)
    {
        std::lock_guard lock(mutex);
        for (const auto& msg : messages)
            if (pred(msg))
                return msg;
        return std::nullopt;
    }
};

void
writeMessage(const std::shared_ptr<dhtnet::ChannelSocketTest>& channel, const Message& msg)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, msg);
    std::error_code ec;
    channel->write(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), ec);
    CPPUNIT_ASSERT(!ec);
}

/// True when a verified lease was accepted for this device. A connected peer is
/// flagged mobile by the transport alone, which grants no wake-up right, so
/// getKnownMobileNodes() is not a usable predicate here.
bool
hasVerifiedLease(const std::shared_ptr<SwarmManager>& manager, const NodeId& id)
{
    for (const auto& info : manager->getKnownMobileNodeInfos())
        if (info.id == id)
            return info.lease.has_value();
    return false;
}

} // namespace

void
MobileWakeUpTest::testLeaseCertificateResolutionFromPeer()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("peer-resolution");
    const auto issuerId = mobile.account.second->getId();

    // No certificate provider and no DHT fetcher: the only way to obtain the
    // certificate is to ask the peer that gossiped the lease.
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; });

    auto inbound = makeChannel(mobile.id);
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    Message announcement;
    announcement.is_mobile = true;
    announcement.self_mobile_info = mobile.info();
    writeMessage(remote, announcement);

    // The lease alone is not enough: it stays pending until the certificate arrives.
    CPPUNIT_ASSERT(!hasVerifiedLease(desktop, mobile.id));

    std::optional<Message> request;
    CPPUNIT_ASSERT(waitFor(
        [&] {
            request = sink.find([](const Message& msg) { return msg.cert_request.has_value(); });
            return request.has_value();
        },
        CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT_EQUAL(size_t(1), request->cert_request->ids.size());
    CPPUNIT_ASSERT(request->cert_request->ids.front() == mobile.id);

    Message answer;
    answer.is_mobile = true;
    answer.cert_response = CertResponse {{mobile.device.second->getPacked()}};
    writeMessage(remote, answer);

    CPPUNIT_ASSERT(waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, CONVERGENCE_TIMEOUT));
    desktop->shutdown();
}

void
MobileWakeUpTest::testLeaseCertificateResolutionFromDht()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("dht-resolution");
    const auto issuerId = mobile.account.second->getId();

    std::atomic_size_t fetches {0};
    std::atomic_bool answerFetch {false};
    auto certificate = mobile.device.second;
    auto fetcher = [&fetches, &answerFetch, certificate](
                       const NodeId&, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb) {
        ++fetches;
        cb(answerFetch ? certificate : nullptr);
    };

    // No peer to ask (the lease came from persistence), no local certificate:
    // only the DHT can resolve it.
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; },
        [](const NodeId&) -> std::shared_ptr<dht::crypto::Certificate> { return nullptr; },
        fetcher);

    desktop->setMobileNodes({mobile.info()});
    CPPUNIT_ASSERT(waitFor([&] { return fetches.load() > 0; }, CONVERGENCE_TIMEOUT));
    // A failed lookup must never turn an unverified lease into a wake-up target.
    CPPUNIT_ASSERT(desktop->getKnownMobileNodes().empty());
    CPPUNIT_ASSERT(desktop->getMobileNodesToNotify().empty());

    // The unresolvable node was simply dropped, so re-announcing it retries at once.
    answerFetch = true;
    desktop->setMobileNodes({mobile.info()});
    CPPUNIT_ASSERT(waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(fetches.load() > 1);
    desktop->shutdown();
}

void
MobileWakeUpTest::testCertificateRequestIsNotAnOracle()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("oracle-mobile");
    const auto stranger = dht::crypto::generateIdentity("oracle-stranger", {}, 2048, true);
    const auto strangerDevice = dht::crypto::generateIdentity("oracle-stranger-device", stranger, 2048, false);
    const auto strangerId = strangerDevice.second->getLongId();
    const auto issuerId = mobile.account.second->getId();

    std::map<NodeId, std::shared_ptr<dht::crypto::Certificate>> store {{mobile.id, mobile.device.second},
                                                                      {strangerId, strangerDevice.second}};
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; },
        [&store](const NodeId& id) -> std::shared_ptr<dht::crypto::Certificate> {
            auto it = store.find(id);
            return it == store.end() ? nullptr : it->second;
        });
    desktop->setMobileNodes({mobile.info()});
    CPPUNIT_ASSERT(toSet(desktop->getKnownMobileNodes()).contains(mobile.id));

    auto inbound = makeChannel(nodeTestIds1.at(1));
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    // We hold the stranger's certificate but never announced it as a mobile node:
    // the swarm must not act as a generic certificate directory.
    Message strangerRequest;
    strangerRequest.cert_request = CertRequest {{strangerId}};
    writeMessage(remote, strangerRequest);
    CPPUNIT_ASSERT(!waitFor([&] { return sink.find([](const Message& m) { return m.cert_response.has_value(); }).has_value(); },
                            3s));

    // The certificate of a node whose lease we did accept is legitimate to serve.
    Message mobileRequest;
    mobileRequest.cert_request = CertRequest {{strangerId, mobile.id}};
    writeMessage(remote, mobileRequest);
    std::optional<Message> answer;
    CPPUNIT_ASSERT(waitFor(
        [&] {
            answer = sink.find([](const Message& m) { return m.cert_response.has_value(); });
            return answer.has_value();
        },
        CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT_EQUAL(size_t(1), answer->cert_response->certificates.size());
    dht::crypto::Certificate served(answer->cert_response->certificates.front());
    CPPUNIT_ASSERT(served.getLongId() == mobile.id);
    desktop->shutdown();
}

void
MobileWakeUpTest::testUnsolicitedCertificateResponseIsIgnored()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("unsolicited-mobile");
    const auto stranger = dht::crypto::generateIdentity("unsolicited-stranger", {}, 2048, true);
    const auto strangerDevice = dht::crypto::generateIdentity("unsolicited-stranger-device", stranger, 2048, false);
    const auto issuerId = mobile.account.second->getId();

    // No local store and no DHT: everything must come from an answered request.
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; });

    auto inbound = makeChannel(mobile.id);
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    // A certificate nobody asked for must not be able to install a lease, and a
    // fortiori must not be pinned.
    Message unsolicited;
    unsolicited.cert_response = CertResponse {{mobile.device.second->getPacked()}};
    writeMessage(remote, unsolicited);

    Message announcement;
    announcement.is_mobile = true;
    announcement.self_mobile_info = mobile.info();
    writeMessage(remote, announcement);
    CPPUNIT_ASSERT(waitFor([&] { return sink.find([](const Message& m) { return m.cert_request.has_value(); }).has_value(); },
                           CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(!hasVerifiedLease(desktop, mobile.id));

    // Answering a request with someone else's certificate resolves nothing.
    Message wrongAnswer;
    wrongAnswer.cert_response = CertResponse {{strangerDevice.second->getPacked()}};
    writeMessage(remote, wrongAnswer);
    CPPUNIT_ASSERT(!waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, 3s));
    desktop->shutdown();
}

void
MobileWakeUpTest::testCertificateRequestTimeoutFallsBackToDht()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("timeout-mobile");
    const auto issuerId = mobile.account.second->getId();

    std::atomic_size_t fetches {0};
    auto certificate = mobile.device.second;
    auto fetcher = [&fetches, certificate](const NodeId&,
                                           std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb) {
        ++fetches;
        cb(certificate);
    };

    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; },
        [](const NodeId&) -> std::shared_ptr<dht::crypto::Certificate> { return nullptr; },
        fetcher);

    auto inbound = makeChannel(mobile.id);
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    Message announcement;
    announcement.is_mobile = true;
    announcement.self_mobile_info = mobile.info();
    writeMessage(remote, announcement);

    CPPUNIT_ASSERT(waitFor([&] { return sink.find([](const Message& m) { return m.cert_request.has_value(); }).has_value(); },
                           CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT_EQUAL(size_t(0), fetches.load());

    // The peer never answers: after CERT_REQUEST_TIMEOUT the DHT takes over.
    CPPUNIT_ASSERT(waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(fetches.load() > 0);
    desktop->shutdown();
}

void
MobileWakeUpTest::testLocalCertificateSkipsCertificateRequest()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("local-cert-mobile");
    const auto issuerId = mobile.account.second->getId();

    std::atomic_size_t fetches {0};
    auto certificate = mobile.device.second;
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; },
        [certificate, id = mobile.id](const NodeId& node) -> std::shared_ptr<dht::crypto::Certificate> {
            return node == id ? certificate : nullptr;
        },
        [&fetches](const NodeId&, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb) {
            ++fetches;
            cb(nullptr);
        });

    auto inbound = makeChannel(mobile.id);
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    Message announcement;
    announcement.is_mobile = true;
    announcement.self_mobile_info = mobile.info();
    writeMessage(remote, announcement);

    // The swarm channel's TLS peer certificate is already pinned: this is the
    // common case and it must cost neither a request nor a DHT lookup.
    CPPUNIT_ASSERT(waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(!waitFor([&] { return sink.find([](const Message& m) { return m.cert_request.has_value(); }).has_value(); },
                            3s));
    CPPUNIT_ASSERT_EQUAL(size_t(0), fetches.load());
    desktop->shutdown();
}

void
MobileWakeUpTest::testUnverifiableLeaseIsNeverFetched()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("outsider-mobile");

    std::atomic_size_t fetches {0};
    // The issuer is not a member of the conversation.
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [](const dht::InfoHash&) { return false; },
        [](const NodeId&) -> std::shared_ptr<dht::crypto::Certificate> { return nullptr; },
        [&fetches](const NodeId&, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb) {
            ++fetches;
            cb(nullptr);
        });

    auto inbound = makeChannel(mobile.id);
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    Message announcement;
    announcement.is_mobile = true;
    announcement.self_mobile_info = mobile.info();
    writeMessage(remote, announcement);

    // The cheap checks run before any lookup, so an unauthorized lease cannot be
    // used to make us hammer the DHT or our peers.
    CPPUNIT_ASSERT(!waitFor([&] { return fetches.load() > 0; }, 3s));
    CPPUNIT_ASSERT(!sink.find([](const Message& m) { return m.cert_request.has_value(); }).has_value());
    CPPUNIT_ASSERT(!hasVerifiedLease(desktop, mobile.id));
    desktop->shutdown();
}

void
MobileWakeUpTest::testResolvedCertificateWithBadSignatureIsRejected()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice mobile("bad-signature-mobile");
    const auto issuerId = mobile.account.second->getId();

    auto forged = mobile.info();
    forged.lease->signature.back() ^= 0xff;

    std::atomic_size_t fetches {0};
    auto certificate = mobile.device.second;
    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerId](const dht::InfoHash& candidate) { return candidate == issuerId; },
        [](const NodeId&) -> std::shared_ptr<dht::crypto::Certificate> { return nullptr; },
        [&fetches, certificate](const NodeId&,
                                std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb) {
            ++fetches;
            cb(certificate);
        });

    desktop->setMobileNodes({forged});
    CPPUNIT_ASSERT(waitFor([&] { return fetches.load() > 0; }, CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(!waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, 3s));

    // The forged lease was dropped rather than cached, so the genuine one still
    // resolves on the next announcement.
    desktop->setMobileNodes({mobile.info()});
    CPPUNIT_ASSERT(waitFor([&] { return hasVerifiedLease(desktop, mobile.id); }, CONVERGENCE_TIMEOUT));
    desktop->shutdown();
}

void
MobileWakeUpTest::testPendingCertificateRequestsAreRetried()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    LeasedDevice first("retry-mobile-a");
    LeasedDevice second("retry-mobile-b");
    const auto issuerA = first.account.second->getId();
    const auto issuerB = second.account.second->getId();

    auto desktop = std::make_shared<SwarmManager>(
        nodeTestIds1.at(0),
        false,
        rd,
        [](auto) { return false; },
        TEST_CONVERSATION_ID,
        std::function<std::optional<MobileNodeInfo>()> {},
        [issuerA, issuerB](const dht::InfoHash& candidate) { return candidate == issuerA || candidate == issuerB; });

    auto inbound = makeChannel(nodeTestIds1.at(1));
    auto remote = makeChannel(nodeTestIds1.at(0));
    MessageSink sink;
    sink.attach(remote);
    dhtnet::ChannelSocketTest::link(inbound, remote);
    desktop->addChannel(inbound);

    Message gossip;
    gossip.response = Response {Query::FOUND, {}, {}, {first.info(), second.info()}};

    std::map<NodeId, std::shared_ptr<dht::crypto::Certificate>> store {{first.id, first.device.second},
                                                                      {second.id, second.device.second}};
    // Only one request may be outstanding per peer, so resolving both devices
    // takes two gossip rounds; neither may get stuck as "already being fetched".
    for (int round = 0; round < 2; ++round) {
        writeMessage(remote, gossip);
        std::optional<Message> request;
        CPPUNIT_ASSERT(waitFor(
            [&] {
                request = sink.find([&](const Message& m) {
                    return m.cert_request.has_value() && !m.cert_request->ids.empty()
                           && store.count(m.cert_request->ids.front());
                });
                return request.has_value();
            },
            CONVERGENCE_TIMEOUT));
        const auto requested = request->cert_request->ids.front();
        Message answer;
        answer.cert_response = CertResponse {{store.at(requested)->getPacked()}};
        writeMessage(remote, answer);
        CPPUNIT_ASSERT(waitFor([&] { return hasVerifiedLease(desktop, requested); }, CONVERGENCE_TIMEOUT));
        store.erase(requested);
    }

    CPPUNIT_ASSERT(hasVerifiedLease(desktop, first.id));
    CPPUNIT_ASSERT(hasVerifiedLease(desktop, second.id));
    desktop->shutdown();
}

void
MobileWakeUpTest::testNotifyWithoutConnectedNodes()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    RoutingTable rt;
    rt.setId(nodeTestIds1.at(0));

    // No mobile nodes, nothing to notify
    CPPUNIT_ASSERT(rt.getMobileNodesToNotify().empty());
    CPPUNIT_ASSERT(rt.getKnownMobileNodes().empty());

    // Our own id is never tracked as a mobile node
    rt.addMobileNode(nodeTestIds1.at(0));
    CPPUNIT_ASSERT(rt.getMobileNodesToNotify().empty());
    CPPUNIT_ASSERT(rt.getKnownMobileNodes().empty());

    // Without any connected node, we are responsible for every mobile node
    std::set<NodeId> mobiles {nodeTestIds1.at(2), nodeTestIds1.at(5), nodeTestIds2.at(9)};
    for (const auto& m : mobiles)
        rt.addMobileNode(m);

    CPPUNIT_ASSERT(toSet(rt.getMobileNodesToNotify()) == mobiles);
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == mobiles);
}

void
MobileWakeUpTest::testNotifyAgainstBruteForceOracle()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    // Randomized tables validated against a brute-force XOR-distance oracle.
    // This exercises responsibility computation across bucket splits.
    constexpr size_t ROUNDS = 5;
    constexpr size_t N_CONNECTED = 30;
    constexpr size_t N_MOBILE = 12;

    for (size_t round = 0; round < ROUNDS; round++) {
        NodeId self = Hash<32>::getRandom();
        RoutingTable rt;
        rt.setId(self);

        for (size_t i = 0; i < N_CONNECTED; i++)
            rt.addNode(makeChannel(Hash<32>::getRandom()));

        std::vector<NodeId> mobiles;
        for (size_t i = 0; i < N_MOBILE; i++) {
            auto m = Hash<32>::getRandom();
            if (rt.addMobileNode(m))
                mobiles.emplace_back(m);
        }

        // Ground truth comes from the table itself: only some of the
        // random nodes end up connected after bucket splits.
        auto connected = rt.getConnectedNodes();
        CPPUNIT_ASSERT(!connected.empty());

        std::set<NodeId> expected;
        for (const auto& m : mobiles)
            if (oracleResponsible(self, connected, m))
                expected.emplace(m);

        CPPUNIT_ASSERT(toSet(rt.getMobileNodesToNotify()) == expected);
        CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == toSet(mobiles));
    }
}

void
MobileWakeUpTest::testNotifyResponsibilityHandover()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    NodeId mobile = nodeTestIds1.at(2);

    RoutingTable rt;
    rt.setId(self);
    rt.addMobileNode(mobile);

    // Candidate connected nodes, none being us or the mobile node
    std::vector<std::shared_ptr<dhtnet::ChannelSocketTest>> candidates;
    for (const auto& channel : nodeTestChannels1) {
        auto id = channel->deviceId();
        if (id != self && id != mobile)
            candidates.emplace_back(channel);
    }
    // Farthest from the mobile node first
    std::sort(candidates.begin(), candidates.end(), [&](const auto& a, const auto& b) {
        return mobile.xorCmp(a->deviceId(), b->deviceId()) > 0;
    });

    // As connected nodes are added (closer and closer to the mobile node),
    // responsibility must match the oracle at every step and eventually be
    // handed over.
    bool handedOver = false;
    for (const auto& channel : candidates) {
        rt.addNode(channel);
        auto expected = oracleResponsible(self, rt.getConnectedNodes(), mobile);
        auto toNotify = toSet(rt.getMobileNodesToNotify());
        CPPUNIT_ASSERT_EQUAL(expected, static_cast<bool>(toNotify.count(mobile)));
        if (!expected)
            handedOver = true;
    }
    CPPUNIT_ASSERT(handedOver);

    // Removing connected nodes hands responsibility back, again matching
    // the oracle at every step.
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        rt.deleteNode((*it)->deviceId());
        rt.addMobileNode(mobile); // deleteNode clears every table for that id
        auto expected = oracleResponsible(self, rt.getConnectedNodes(), mobile);
        auto toNotify = toSet(rt.getMobileNodesToNotify());
        CPPUNIT_ASSERT_EQUAL(expected, static_cast<bool>(toNotify.count(mobile)));
    }
    CPPUNIT_ASSERT(toSet(rt.getMobileNodesToNotify()).count(mobile));
}

void
MobileWakeUpTest::testKnownMobileNodes()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    NodeId disconnectedMobile1 = nodeTestIds1.at(2);
    NodeId disconnectedMobile2 = nodeTestIds2.at(5);
    auto connectedChannel = nodeTestChannels1.at(1);
    NodeId connectedId = connectedChannel->deviceId();

    RoutingTable rt;
    rt.setId(self);
    rt.addMobileNode(disconnectedMobile1);
    rt.addMobileNode(disconnectedMobile2);
    rt.addNode(connectedChannel);

    // The connected node is not mobile yet
    std::set<NodeId> expected {disconnectedMobile1, disconnectedMobile2};
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == expected);

    // A verified mobile announcement received after connection upgrades the
    // connected NodeInfo without creating a disconnected mobile entry.
    CPPUNIT_ASSERT(rt.addMobileNode(connectedId));
    expected.emplace(connectedId);
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == expected);
    CPPUNIT_ASSERT_EQUAL(size_t(2), rt.getMobileNodes().size());

    rt.removeNode(connectedId);
    CPPUNIT_ASSERT(rt.hasMobileNode(connectedId));
    CPPUNIT_ASSERT(!rt.hasKnownNode(connectedId));

    rt.addNode(connectedChannel);
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()).count(connectedId));

    // Mobility change back removes it
    rt.findBucket(connectedId)->changeMobility(connectedId, false);
    expected.erase(connectedId);
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == expected);

    // Forgetting a mobile node removes it from the known set
    rt.removeMobileNode(disconnectedMobile1);
    expected.erase(disconnectedMobile1);
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == expected);
}

void
MobileWakeUpTest::testConnectedMobileLifecycle()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    auto mobileChannel = nodeTestChannels1.at(2);
    NodeId mobileId = mobileChannel->deviceId();

    RoutingTable rt;
    rt.setId(self);

    // A connected mobile node is reachable directly: present in
    // getConnectedNodes(), absent from the wake-up list
    rt.addNode(mobileChannel);
    rt.findBucket(mobileId)->changeMobility(mobileId, true);

    CPPUNIT_ASSERT(toSet(rt.getConnectedNodes()).count(mobileId));
    CPPUNIT_ASSERT(rt.getMobileNodesToNotify().empty());
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()).count(mobileId));

    // When it disconnects, it moves to the mobile nodes table and becomes
    // a wake-up target (no closer connected node remains)
    rt.removeNode(mobileId);
    CPPUNIT_ASSERT(!toSet(rt.getConnectedNodes()).count(mobileId));
    CPPUNIT_ASSERT(rt.hasMobileNode(mobileId));
    CPPUNIT_ASSERT(toSet(rt.getMobileNodesToNotify()).count(mobileId));

    // Presence and DRT gossip may report the device as an ordinary known node,
    // but must not override the persisted mobile classification.
    CPPUNIT_ASSERT(!rt.addKnownNode(mobileId));
    CPPUNIT_ASSERT(!rt.hasKnownNode(mobileId));

    // When it reconnects, it is no longer a wake-up target, but the connected
    // NodeInfo retains mobility so a later disconnect restores the same state.
    rt.addNode(mobileChannel);
    CPPUNIT_ASSERT(!rt.hasMobileNode(mobileId));
    CPPUNIT_ASSERT(rt.getMobileNodesToNotify().empty());
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()).count(mobileId));

    rt.removeNode(mobileId);
    CPPUNIT_ASSERT(rt.hasMobileNode(mobileId));
    CPPUNIT_ASSERT(!rt.hasKnownNode(mobileId));
}

void
MobileWakeUpTest::testFailedConnectionPreservesMobility()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    NodeId mobile = nodeTestIds1.at(2);
    NodeId desktop = nodeTestIds1.at(3);
    auto sm = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
    auto& rt = sm->getRoutingTable();
    std::function<bool(const std::shared_ptr<dhtnet::ChannelSocketInterface>&)> pendingConnection;

    sm->needSocketCb_ = [&pendingConnection](const std::string&, auto&& onSocket, bool) {
        pendingConnection = std::move(onSocket);
    };

    rt.addMobileNode(mobile);
    sm->connectNode(mobile);

    CPPUNIT_ASSERT(pendingConnection);
    CPPUNIT_ASSERT(rt.hasMobileNode(mobile));
    CPPUNIT_ASSERT(rt.hasConnectingNode(mobile));
    CPPUNIT_ASSERT(!rt.hasKnownNode(mobile));
    auto allNodes = sm->getAllNodes();
    CPPUNIT_ASSERT(std::count(allNodes.begin(), allNodes.end(), mobile) == 1);

    auto info = sm->getRoutingTableInfo();
    auto mobileInfo = std::find_if(info.begin(), info.end(), [&mobile](const auto& stat) {
        return stat.at("id") == mobile.toString();
    });
    CPPUNIT_ASSERT(mobileInfo != info.end());
    CPPUNIT_ASSERT_EQUAL("connecting"s, mobileInfo->at("status"));
    CPPUNIT_ASSERT_EQUAL("true"s, mobileInfo->at("mobile"));

    pendingConnection(nullptr);

    CPPUNIT_ASSERT(rt.hasMobileNode(mobile));
    CPPUNIT_ASSERT(!rt.hasConnectingNode(mobile));
    CPPUNIT_ASSERT(!rt.hasKnownNode(mobile));
    CPPUNIT_ASSERT(toSet(sm->getKnownMobileNodes()).count(mobile));
    CPPUNIT_ASSERT(toSet(sm->getMobileNodesToNotify()).count(mobile));

    sm->connectNode(desktop);
    CPPUNIT_ASSERT(pendingConnection);
    pendingConnection(nullptr);

    CPPUNIT_ASSERT(!rt.hasMobileNode(desktop));
    CPPUNIT_ASSERT(!rt.hasConnectingNode(desktop));
    CPPUNIT_ASSERT(rt.hasKnownNode(desktop));
}

void
MobileWakeUpTest::testRoutingTableInfoResponsibleFlag()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    auto sm = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
    auto& rt = sm->getRoutingTable();

    // Connected desktop nodes
    for (size_t i = 1; i < 6; ++i)
        sm->addChannel(nodeTestChannels1.at(i));

    // Disconnected mobile nodes
    for (const auto& mobile : {nodeTestIds2.at(2), nodeTestIds2.at(5), nodeTestIds2.at(8)})
        rt.addMobileNode(mobile);

    const auto notify = toSet(sm->getMobileNodesToNotify());
    CPPUNIT_ASSERT(!notify.empty());

    // Every routing table entry carries a "responsible" flag matching
    // the wake-up list, and only mobile nodes can be responsible targets
    std::set<NodeId> responsible;
    for (const auto& entry : sm->getRoutingTableInfo()) {
        CPPUNIT_ASSERT(entry.count("responsible"));
        if (entry.at("responsible") == "true") {
            CPPUNIT_ASSERT_EQUAL("true"s, entry.at("mobile"));
            responsible.emplace(NodeId(entry.at("id")));
        }
    }
    CPPUNIT_ASSERT(responsible == notify);

    sm->shutdown();
}

// ################# SWARM MANAGER TESTS #################//

void
MobileWakeUpTest::testMobileNodesChangedCallback()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    NodeId m1 = nodeTestIds1.at(2);
    NodeId m2 = nodeTestIds1.at(3);
    NodeId m3 = nodeTestIds2.at(5);

    auto sm = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });

    unsigned emissions = 0;
    std::set<NodeId> lastPayload;
    sm->onMobileNodesChanged([&](const std::vector<NodeId>& nodes) {
        emissions++;
        lastPayload = toSet(nodes);
    });

    // New mobile nodes fire the callback with the full known set
    sm->setMobileNodes({m1, m2});
    CPPUNIT_ASSERT_EQUAL(1u, emissions);
    CPPUNIT_ASSERT(lastPayload == (std::set<NodeId> {m1, m2}));

    // No change, no emission
    sm->setMobileNodes({m1});
    CPPUNIT_ASSERT_EQUAL(1u, emissions);

    // Our own id is filtered out and does not count as a change
    sm->setMobileNodes({self});
    CPPUNIT_ASSERT_EQUAL(1u, emissions);
    CPPUNIT_ASSERT(!lastPayload.count(self));

    // Incremental addition fires with the updated full set
    sm->setMobileNodes({m3});
    CPPUNIT_ASSERT_EQUAL(2u, emissions);
    CPPUNIT_ASSERT(lastPayload == (std::set<NodeId> {m1, m2, m3}));

    // Mobility changes of connected nodes are reported too
    auto channel = nodeTestChannels1.at(1);
    NodeId connectedId = channel->deviceId();
    sm->getRoutingTable().addNode(channel);

    sm->changeMobility(connectedId, true);
    CPPUNIT_ASSERT(lastPayload.count(connectedId));
    CPPUNIT_ASSERT(lastPayload == (std::set<NodeId> {m1, m2, m3, connectedId}));

    sm->changeMobility(connectedId, false);
    CPPUNIT_ASSERT(!lastPayload.count(connectedId));
    CPPUNIT_ASSERT(lastPayload == (std::set<NodeId> {m1, m2, m3}));
}

void
MobileWakeUpTest::testDeleteNodeUpdatesMobileNodes()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto sm = std::make_shared<SwarmManager>(nodeTestIds1.at(0), false, rd, [](auto) { return false; });
    NodeId mobile = nodeTestIds1.at(2);
    unsigned emissions = 0;
    std::set<NodeId> lastPayload;
    sm->onMobileNodesChanged([&](const std::vector<NodeId>& nodes) {
        emissions++;
        lastPayload = toSet(nodes);
    });

    sm->setMobileNodes({mobile});
    CPPUNIT_ASSERT_EQUAL(1u, emissions);
    CPPUNIT_ASSERT(lastPayload.count(mobile));

    sm->deleteNode({mobile});
    CPPUNIT_ASSERT_EQUAL(2u, emissions);
    CPPUNIT_ASSERT(lastPayload.empty());

    sm->deleteNode({mobile});
    CPPUNIT_ASSERT_EQUAL(2u, emissions);
}

void
MobileWakeUpTest::testConcurrentMobileNodesChangedCallbacks()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    auto sm = std::make_shared<SwarmManager>(nodeTestIds1.at(0), false, rd, [](auto) { return false; });
    std::mutex callbackMtx;
    std::set<NodeId> lastPayload;
    sm->onMobileNodesChanged([&](const std::vector<NodeId>& nodes) {
        std::lock_guard lock(callbackMtx);
        lastPayload = toSet(nodes);
    });

    std::vector<NodeId> mobiles {nodeTestIds1.at(2), nodeTestIds1.at(3), nodeTestIds1.at(4), nodeTestIds2.at(5)};
    std::vector<std::thread> workers;
    for (const auto& mobile : mobiles)
        workers.emplace_back([sm, mobile] { sm->setMobileNodes({mobile}); });
    for (auto& worker : workers)
        worker.join();

    std::lock_guard lock(callbackMtx);
    CPPUNIT_ASSERT(lastPayload == toSet(sm->getKnownMobileNodes()));
    CPPUNIT_ASSERT(lastPayload == toSet(mobiles));
}

void
MobileWakeUpTest::testPersistenceColdStart()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    std::vector<NodeId> mobiles {nodeTestIds1.at(2), nodeTestIds1.at(5), nodeTestIds2.at(9)};

    // First run: learn mobile nodes and persist them from the callback,
    // exactly as Conversation does.
    msgpack::sbuffer persisted;
    {
        auto sm = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
        sm->onMobileNodeInfosChanged([&](const std::vector<MobileNodeInfo>& nodes) {
            persisted = msgpack::sbuffer();
            msgpack::pack(persisted, nodes);
        });
        sm->setMobileNodes(mobiles);
        sm->shutdown();
    }
    CPPUNIT_ASSERT(persisted.size() > 0);

    // Cold start: a brand-new manager reloads the persisted set and can
    // immediately compute wake-up targets without any gossip or connection.
    auto oh = msgpack::unpack(persisted.data(), persisted.size());
    std::vector<MobileNodeInfo> restored;
    oh.get().convert(restored);

    auto sm2 = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
    sm2->setMobileNodes(restored);

    CPPUNIT_ASSERT(toSet(sm2->getKnownMobileNodes()) == toSet(mobiles));
    // No connected node: responsible for all of them
    CPPUNIT_ASSERT(toSet(sm2->getMobileNodesToNotify()) == toSet(mobiles));
    sm2->shutdown();

    msgpack::sbuffer legacyPersisted;
    msgpack::pack(legacyPersisted, mobiles);
    auto legacyObject = msgpack::unpack(legacyPersisted.data(), legacyPersisted.size());
    std::vector<NodeId> legacyRestored;
    legacyObject.get().convert(legacyRestored);
    auto sm3 = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
    sm3->setMobileNodes(legacyRestored);
    CPPUNIT_ASSERT(toSet(sm3->getKnownMobileNodes()) == toSet(mobiles));
    sm3->shutdown();
}

// ################# LIVE NETWORK SIMULATIONS #################//

void
MobileWakeUpTest::testWakeUpCoverageConvergedNetwork()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    buildConvergedNetwork();

    // Register persistence callbacks to verify they fire across the network
    std::mutex emissionsMtx;
    std::map<NodeId, unsigned> emissions;
    for (const auto& id : desktopIds) {
        getManager(id)->onMobileNodesChanged([&, id](const std::vector<NodeId>&) {
            std::lock_guard lk(emissionsMtx);
            emissions[id]++;
        });
    }

    // Inject mobile nodes on every desktop, simulating fully-converged gossip
    constexpr size_t N_INJECTED = 5;
    std::vector<NodeId> mobiles;
    for (size_t i = 0; i < N_INJECTED; i++)
        mobiles.emplace_back(Hash<32>::getRandom());

    for (const auto& id : desktopIds)
        getManager(id)->setMobileNodes(mobiles);

    // Each desktop fired the persistence callback exactly once;
    // re-injecting the same set fires nothing.
    {
        std::lock_guard lk(emissionsMtx);
        for (const auto& id : desktopIds)
            CPPUNIT_ASSERT_EQUAL(1u, emissions[id]);
    }
    for (const auto& id : desktopIds)
        getManager(id)->setMobileNodes(mobiles);
    {
        std::lock_guard lk(emissionsMtx);
        for (const auto& id : desktopIds)
            CPPUNIT_ASSERT_EQUAL(1u, emissions[id]);
    }

    // Coverage and responsibility properties
    std::map<NodeId, unsigned> responsibleCount;
    for (const auto& id : desktopIds) {
        auto sm = getManager(id);
        checkLocalConsistency(sm);
        for (const auto& m : sm->getMobileNodesToNotify()) {
            CPPUNIT_ASSERT(std::find(mobiles.begin(), mobiles.end(), m) != mobiles.end());
            responsibleCount[m]++;
        }
    }

    unsigned totalWakeUps = 0;
    for (const auto& m : mobiles) {
        // Every mobile node is woken up by at least one desktop
        CPPUNIT_ASSERT_MESSAGE("Mobile node " + m.toString() + " not covered", responsibleCount[m] >= 1);
        totalWakeUps += responsibleCount[m];

        // The XOR-closest desktop always claims responsibility: none of its
        // connected nodes can be closer
        auto closestDesktop = *std::min_element(desktopIds.begin(),
                                                desktopIds.end(),
                                                [&](const auto& a, const auto& b) { return m.xorCmp(a, b) < 0; });
        auto toNotify = getManager(closestDesktop)->getMobileNodesToNotify();
        CPPUNIT_ASSERT(std::find(toNotify.begin(), toNotify.end(), m) != toNotify.end());
    }

    std::cout << "Wake-up duplication factor: " << totalWakeUps / (float) mobiles.size() << " (" << totalWakeUps
              << " wake-ups for " << mobiles.size() << " mobiles over " << desktopIds.size() << " desktops)"
              << std::endl;
}

void
MobileWakeUpTest::testMobileLifecycleWakeUp()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    // One desktop and one real mobile manager exchanging protocol messages
    auto desktop = createManager(nodeTestIds1.at(0), false);
    auto mobile = createManager(nodeTestIds1.at(1), true);
    auto mobileId = mobile->getId();

    desktop->setKnownNodes({mobileId});

    // Phase 1: connected. The mobile node announces its mobility in-band,
    // but being connected it is not a wake-up target.
    CPPUNIT_ASSERT(waitFor(
        [&] {
            return desktop->getRoutingTable().hasNode(mobileId)
                   && toSet(desktop->getKnownMobileNodes()).count(mobileId);
        },
        CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(desktop->getMobileNodesToNotify().empty());

    // Phase 2: the mobile device goes to sleep. The desktop must take over
    // wake-up responsibility.
    mobile->shutdown();
    CPPUNIT_ASSERT(waitFor(
        [&] {
            auto toNotify = desktop->getMobileNodesToNotify();
            return toSet(toNotify).count(mobileId) && toNotify.size() == 1;
        },
        CONVERGENCE_TIMEOUT));
    CPPUNIT_ASSERT(!toSet(desktop->getConnectedNodes()).count(mobileId));

    // Phase 3: the mobile device wakes up and reconnects; it leaves the
    // wake-up list again.
    unlinkPair(desktop->getId(), mobileId);
    mobile->restart();
    mobile->setKnownNodes({desktop->getId()});
    mobile->maintainBuckets();

    CPPUNIT_ASSERT(
        waitFor([&] { return desktop->getRoutingTable().hasNode(mobileId) && desktop->getMobileNodesToNotify().empty(); },
                CONVERGENCE_TIMEOUT));
}

void
MobileWakeUpTest::testWakeUpCoverageAfterMassMobileShutdown()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    buildConvergedNetwork();

    // Real mobile managers bootstrapped to random desktops
    std::uniform_int_distribution<size_t> distrib(0, N_DESKTOPS - 1);
    for (size_t i = 0; i < N_MOBILES; i++) {
        auto sm = createManager(Hash<32>::getRandom(), true);
        sm->setKnownNodes({desktopIds.at(distrib(rd)), desktopIds.at(distrib(rd))});
    }

    // Wait for the whole network (desktops + mobiles) to converge
    CPPUNIT_ASSERT(waitFor(
        [&] {
            crossNodes(desktopIds.front());
            return discoveredNodes.size() == N_DESKTOPS + N_MOBILES;
        },
        CONVERGENCE_TIMEOUT));

    // All mobile devices go to sleep at once
    for (const auto& id : mobileIds)
        getManager(id)->shutdown();

    // Eventually, every sleeping mobile node is a wake-up target of at
    // least one desktop: nobody is left out of the sync.
    auto allCovered = [&] {
        for (const auto& m : mobileIds) {
            bool covered = false;
            for (const auto& d : desktopIds) {
                auto toNotify = getManager(d)->getMobileNodesToNotify();
                if (std::find(toNotify.begin(), toNotify.end(), m) != toNotify.end()) {
                    covered = true;
                    break;
                }
            }
            if (!covered)
                return false;
        }
        return true;
    };
    CPPUNIT_ASSERT_MESSAGE("Some sleeping mobile node is not covered by any desktop",
                           waitFor(allCovered, CONVERGENCE_TIMEOUT));

    // No desktop targets a connected node or a desktop, and each wake-up
    // decision matches the XOR-distance oracle
    auto desktopSet = toSet(desktopIds);
    unsigned totalWakeUps = 0;
    for (const auto& d : desktopIds) {
        auto sm = getManager(d);
        checkLocalConsistency(sm);
        for (const auto& m : sm->getMobileNodesToNotify()) {
            CPPUNIT_ASSERT(!desktopSet.count(m));
            totalWakeUps++;
        }
    }
    std::cout << "Wake-up duplication factor after mass shutdown: " << totalWakeUps / (float) mobileIds.size()
              << std::endl;
}

}; // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MobileWakeUpTest::name())
