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
#include <set>

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

constexpr size_t N_DESKTOPS = 9;
constexpr size_t N_MOBILES = 3;
constexpr std::chrono::seconds CONVERGENCE_TIMEOUT {60};

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

    // Brute-force oracle: are we (self) responsible for waking up mobile,
    // i.e. closer to it than every connected node?
    static bool oracleResponsible(const NodeId& self, const std::vector<NodeId>& connected, const NodeId& mobile)
    {
        for (const auto& c : connected) {
            if (c == mobile)
                continue;
            if (mobile.xorCmp(self, c) >= 0)
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
    void testNotifyAgainstBruteForceOracle();
    void testNotifyResponsibilityHandover();
    void testKnownMobileNodes();
    void testConnectedMobileLifecycle();
    void testMobileNodesChangedCallback();
    void testPersistenceColdStart();
    void testWakeUpCoverageConvergedNetwork();
    void testMobileLifecycleWakeUp();
    void testWakeUpCoverageAfterMassMobileShutdown();

    CPPUNIT_TEST_SUITE(MobileWakeUpTest);
    CPPUNIT_TEST(testNotifyWithoutConnectedNodes);
    CPPUNIT_TEST(testNotifyAgainstBruteForceOracle);
    CPPUNIT_TEST(testNotifyResponsibilityHandover);
    CPPUNIT_TEST(testKnownMobileNodes);
    CPPUNIT_TEST(testConnectedMobileLifecycle);
    CPPUNIT_TEST(testMobileNodesChangedCallback);
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

std::shared_ptr<jami::SwarmManager>
MobileWakeUpTest::createManager(const NodeId& id, bool mobile)
{
    auto sm = std::make_shared<SwarmManager>(id, false, rd, [](auto) { return false; });
    sm->setMobility(mobile);
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
        // And we must be XOR-closer to them than every connected node
        CPPUNIT_ASSERT(oracleResponsible(sm->getId(), connected, m));
    }
    // Inverse: every disconnected known mobile we are responsible for
    // must be in the wake-up list
    auto toNotifySet = toSet(toNotify);
    for (const auto& m : knownMobiles) {
        if (connectedSet.count(m))
            continue;
        if (oracleResponsible(sm->getId(), connected, m))
            CPPUNIT_ASSERT(toNotifySet.count(m));
    }
}

// ################# DETERMINISTIC ROUTING TABLE TESTS #################//

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

    // A connected node flagged mobile joins the known mobile set,
    // but only disconnected mobiles are in getMobileNodes()
    rt.findBucket(connectedId)->changeMobility(connectedId, true);
    expected.emplace(connectedId);
    CPPUNIT_ASSERT(toSet(rt.getKnownMobileNodes()) == expected);
    CPPUNIT_ASSERT_EQUAL(size_t(2), rt.getMobileNodes().size());

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

    // When it reconnects, it is no longer a wake-up target
    rt.addNode(mobileChannel);
    CPPUNIT_ASSERT(!rt.hasMobileNode(mobileId));
    CPPUNIT_ASSERT(rt.getMobileNodesToNotify().empty());
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
MobileWakeUpTest::testPersistenceColdStart()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;

    NodeId self = nodeTestIds1.at(0);
    std::vector<NodeId> mobiles {nodeTestIds1.at(2), nodeTestIds1.at(5), nodeTestIds2.at(9)};

    // First run: learn mobile nodes and persist them from the callback,
    // exactly as Conversation does (msgpack vector of hex strings).
    msgpack::sbuffer persisted;
    {
        auto sm = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
        sm->onMobileNodesChanged([&](const std::vector<NodeId>& nodes) {
            std::vector<std::string> strs;
            strs.reserve(nodes.size());
            for (const auto& n : nodes)
                strs.emplace_back(n.toString());
            persisted = msgpack::sbuffer();
            msgpack::pack(persisted, strs);
        });
        sm->setMobileNodes(mobiles);
        sm->shutdown();
    }
    CPPUNIT_ASSERT(persisted.size() > 0);

    // Cold start: a brand-new manager reloads the persisted set and can
    // immediately compute wake-up targets without any gossip or connection.
    auto oh = msgpack::unpack(persisted.data(), persisted.size());
    std::vector<std::string> restoredStrs;
    oh.get().convert(restoredStrs);

    std::vector<NodeId> restored;
    restored.reserve(restoredStrs.size());
    for (const auto& s : restoredStrs)
        restored.emplace_back(NodeId(s));

    auto sm2 = std::make_shared<SwarmManager>(self, false, rd, [](auto) { return false; });
    sm2->setMobileNodes(restored);

    CPPUNIT_ASSERT(toSet(sm2->getKnownMobileNodes()) == toSet(mobiles));
    // No connected node: responsible for all of them
    CPPUNIT_ASSERT(toSet(sm2->getMobileNodesToNotify()) == toSet(mobiles));
    sm2->shutdown();
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
