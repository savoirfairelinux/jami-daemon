/*
 *  Copyright (C) 2024 Savoir-faire Linux Inc.
 *  Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <algorithm>
#include <msgpack.hpp>
#include <opendht/thread_pool.h>
#include <opendht/utils.h>

#include <iostream>
#include <fstream>
#include <string>

#include "../../test_runner.h"
#include "jami.h"
#include "../common.h"
#include "jamidht/swarm/swarm_manager.h"
#include <dhtnet/multiplexed_socket.h>
#include "nodes.h"

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

constexpr size_t nNodes = 10;

constexpr size_t BOOTSTRAP_SIZE = 2;
auto time = 30s;

int TOTAL_HOPS = 0;
int moyenne = 0;
int max = 0;
int min = 10000;

struct Message
{
    int identifier_; // message identifier
    int hops_ = 0;   // number of hops
    MSGPACK_DEFINE_MAP(identifier_, hops_);
};

struct Counter
{
    Counter(unsigned t)
        : target(t)
    {}
    const unsigned target;
    unsigned added {0};
    std::mutex mutex;
    std::condition_variable cv;

    void count()
    {
        std::lock_guard lock(mutex);
        ++added;
        if (added == target)
            cv.notify_one();
    }
    bool wait(std::chrono::steady_clock::duration timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [&] { return added == target; });
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait(lock, [&] { return added == target; });
    }
};

class SwarmMessageSpread : public CppUnit::TestFixture
{
public:
    ~SwarmMessageSpread() { libjami::fini(); }
    static std::string name() { return "SwarmMessageSpread"; }

    void setUp();
    void tearDown();

private:
    std::mt19937_64 rd {dht::crypto::getSeededRandomEngine<std::mt19937_64>()};
    std::mutex channelSocketsMtx_;
    std::condition_variable channelSocketsCv_;

    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagers;
    std::map<NodeId,
             std::map<NodeId,
                      std::pair<std::shared_ptr<dhtnet::ChannelSocketTest>,
                                std::shared_ptr<dhtnet::ChannelSocketTest>>>>
        channelSockets_;
    std::vector<NodeId> randomNodeIds;
    std::vector<std::shared_ptr<jami::SwarmManager>> swarmManagersShuffled;
    std::set<NodeId> discoveredNodes;
    std::map<NodeId, int> numberTimesReceived;
    std::map<NodeId, int> requestsReceived;
    std::map<NodeId, int> answersSent;

    int iterations = 0;

    void generateSwarmManagers();
    void needSocketCallBack(const std::shared_ptr<SwarmManager>& sm);
    void sendMessage(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, Message msg);
    void receiveMessage(const NodeId nodeId, const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket);
    void relayMessageToRoutingTable(const NodeId nodeId, const NodeId sourceId, const Message msg);
    void updateHops(int hops);
    void crossNodes(NodeId nodeId);
    void displayBucketDistribution(const NodeId& id);
    void distribution();
    std::shared_ptr<jami::SwarmManager> getManager(const NodeId& id);

    void testWriteMessage();

    CPPUNIT_TEST_SUITE(SwarmMessageSpread);
    CPPUNIT_TEST(testWriteMessage);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SwarmMessageSpread, SwarmMessageSpread::name());

void
SwarmMessageSpread::setUp()
{
    libjami::init(
        libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized) {
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    generateSwarmManagers();
}

void
SwarmMessageSpread::tearDown()
{}

void
SwarmMessageSpread::generateSwarmManagers()
{
    for (size_t i = 0; i < nNodes; i++) {
        const NodeId node = Hash<32>::getRandom();
        auto sm = std::make_shared<SwarmManager>(node, rd, std::move([](auto) {return false;}));
        swarmManagers[node] = sm;
        randomNodeIds.emplace_back(node);
        swarmManagersShuffled.emplace_back(sm);
    }
}

std::shared_ptr<jami::SwarmManager>
SwarmMessageSpread::getManager(const NodeId& id)
{
    auto it = swarmManagers.find(id);
    return it == swarmManagers.end() ? nullptr : it->second;
}

void
SwarmMessageSpread::crossNodes(NodeId nodeId)
{
    std::list<NodeId> pendingNodes {nodeId};
    discoveredNodes.clear();

    for (const auto& curNode : pendingNodes) {
        if (discoveredNodes.emplace(curNode).second) {
            if (auto sm = getManager(curNode))
                for (const auto& node : sm->getRoutingTable().getNodes()) {
                    pendingNodes.emplace_back(node);
                }
        }
    }
}

void
SwarmMessageSpread::sendMessage(const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket, Message msg)
{
    auto buffer = std::make_shared<msgpack::sbuffer>(32);
    msgpack::packer<msgpack::sbuffer> pk(buffer.get());
    pk.pack(msg);

    dht::ThreadPool::io().run([socket, buffer = std::move(buffer)] {
        std::error_code ec;
        // std::this_thread::sleep_for(std::chrono::milliseconds(50));

        socket->write(reinterpret_cast<const unsigned char*>(buffer->data()), buffer->size(), ec);
    });
}

void
SwarmMessageSpread::receiveMessage(const NodeId nodeId,
                                   const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket)
{
    struct DecodingContext
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               32};
    };

    socket->setOnRecv([this,
                       wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket),
                       ctx = std::make_shared<DecodingContext>(),
                       nodeId](const uint8_t* buf, size_t len) {
        auto socket = wsocket.lock();
        if (!socket)
            return 0lu;

        ctx->pac.reserve_buffer(len);
        std::copy_n(buf, len, ctx->pac.buffer());
        ctx->pac.buffer_consumed(len);

        msgpack::object_handle oh;
        while (ctx->pac.next(oh)) {
            try {
                Message msg;
                oh.get().convert(msg);

                if (msg.identifier_ == 1) {
                    std::lock_guard lk(channelSocketsMtx_);
                    auto var = numberTimesReceived.find(nodeId);
                    iterations = iterations + 1;

                    if (var != numberTimesReceived.end()) {
                        var->second += 1;
                    } else {
                        Message msgToSend;
                        msgToSend.identifier_ = 1;
                        msgToSend.hops_ = msg.hops_ + 1;
                        numberTimesReceived[nodeId] = 1;
                        updateHops(msgToSend.hops_);
                        relayMessageToRoutingTable(nodeId, socket->deviceId(), msgToSend);
                    }
                    channelSocketsCv_.notify_all();
                }

            } catch (const std::exception& e) {
                JAMI_WARNING("Error DRT recv: {}", e.what());
                return 0lu;
            }
        }

        return 0lu;
    });
};

void
SwarmMessageSpread::updateHops(int hops)
{
    if (hops > TOTAL_HOPS) {
        TOTAL_HOPS = hops;
    }
}

void
SwarmMessageSpread::needSocketCallBack(const std::shared_ptr<SwarmManager>& sm)
{
    sm->needSocketCb_ = [this, wsm = std::weak_ptr<SwarmManager>(sm)](const std::string& nodeId,
                                                                      auto&& onSocket) {
        dht::ThreadPool::io().run(
            [this, wsm = std::move(wsm), nodeId, onSocket = std::move(onSocket)] {
                auto sm = wsm.lock();
                if (!sm)
                    return;

                NodeId node = dhtnet::DeviceId(nodeId);
                if (auto smRemote = getManager(node)) {
                    auto myId = sm->getId();
                    std::unique_lock<std::mutex> lk(channelSocketsMtx_);
                    auto& cstRemote = channelSockets_[node][myId];
                    auto& cstMe = channelSockets_[myId][node];
                    if (cstMe.second && cstMe.first)
                        return;
                    if (!cstMe.second) {
                        cstMe.second = std::make_shared<dhtnet::ChannelSocketTest>(Manager::instance().ioContext(), node, "test1", 1);
                        cstRemote.second = std::make_shared<dhtnet::ChannelSocketTest>(Manager::instance().ioContext(), myId, "test1", 1);
                    }
                    if (!cstMe.first) {
                        cstRemote.first = std::make_shared<dhtnet::ChannelSocketTest>(Manager::instance().ioContext(), myId, "swarm1", 0);
                        cstMe.first = std::make_shared<dhtnet::ChannelSocketTest>(Manager::instance().ioContext(), node, "swarm1", 0);
                    }
                    lk.unlock();
                    dhtnet::ChannelSocketTest::link(cstMe.second, cstRemote.second);
                    receiveMessage(myId, cstMe.second);
                    receiveMessage(node, cstRemote.second);
                    // std::this_thread::sleep_for(std::chrono::seconds(5));
                    dhtnet::ChannelSocketTest::link(cstMe.first, cstRemote.first);
                    smRemote->addChannel(cstRemote.first);
                    onSocket(cstMe.first);
                }
            });
    };
}

void
SwarmMessageSpread::relayMessageToRoutingTable(const NodeId nodeId,
                                               const NodeId sourceId,
                                               const Message msg)
{
    auto swarmManager = getManager(nodeId);
    const auto& routingtable = swarmManager->getRoutingTable().getNodes();
    for (auto& node : routingtable) {
        if (node != sourceId) {
            auto channelToSend = channelSockets_[nodeId][node].second;
            sendMessage(channelToSend, msg);
        }
    }
}

void
SwarmMessageSpread::distribution()
{
    std::string const fileName("distrib_nodes_" + std::to_string(nNodes) + ".txt");
    std::ofstream myStream(fileName.c_str());

    std::vector<unsigned> dist(10);
    int mean = 0;
    for (const auto& sm : swarmManagers) {
        auto val = sm.second->getRoutingTable().getRoutingTableNodeCount();
        if (dist.size() <= val)
            dist.resize(val + 1);

        dist[val]++;
    }

    for (size_t i = 0; i < dist.size(); i++) {
        // std::cout << "Swarm Managers with " << i << " nodes: " << dist[i] << std::endl;
        if (myStream) {
            myStream << i << "," << dist[i] << std::endl;
        }
        mean += i * dist[i];
    }
    std::cout << "Le noeud avec le plus de noeuds dans sa routing table: " << dist.size()
              << std::endl;
    std::cout << "Moyenne de nombre de noeuds par Swarm: " << mean / (float) swarmManagers.size()
              << std::endl;
}

void
SwarmMessageSpread::displayBucketDistribution(const NodeId& id)
{
    std::string const fileName("distrib_rt_" + std::to_string(nNodes) + "_" + id.toString()
                               + ".txt");
    std::ofstream myStream(fileName.c_str());

    const auto& routingtable = swarmManagers[id]->getRoutingTable().getBuckets();

    std::cout << "Bucket distribution for node " << id << std::endl;

    for (auto it = routingtable.begin(); it != routingtable.end(); ++it) {
        auto lowerLimit = it->getLowerLimit().toString();

        std::string hex_prefix = lowerLimit.substr(0, 4); // extraire les deux premiers caractères
        std::cout << "Bucket " << hex_prefix << " has " << it->getNodesSize() << " nodes"
                  << std::endl;

        if (myStream) {
            myStream << hex_prefix << "," << it->getNodesSize() << std::endl;
        }
    }
}

void
SwarmMessageSpread::testWriteMessage()
{
    std::cout << "\ntestWriteMessage()" << std::endl;
    for (const auto& sm : swarmManagersShuffled) {
        needSocketCallBack(sm);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
        dht::ThreadPool::computation().run([&] {
            std::vector<NodeId> randIds(BOOTSTRAP_SIZE);
            std::uniform_int_distribution<size_t> distribution(0, randomNodeIds.size() - 1);
            std::generate(randIds.begin(), randIds.end(), [&] {
                auto dev = randomNodeIds[distribution(rd)];
                return dev;
            });
            sm.second->setKnownNodes(randIds);
            counter.count();
        });
    }
    counter.wait();

    std::this_thread::sleep_for(time);

    auto& firstNode = *channelSockets_.begin();

    crossNodes(swarmManagers.begin()->first);
    CPPUNIT_ASSERT_EQUAL(swarmManagers.size(), discoveredNodes.size());

    std::cout << "Sending First Message to " << firstNode.second.size() << std::endl;
    auto start = std::chrono::steady_clock::now();

    numberTimesReceived[firstNode.first] = 1;

    for (const auto& channel : firstNode.second) {
        if (channel.second.second) {
            sendMessage(channel.second.second, {1, 0});
        }
    }

    std::unique_lock<std::mutex> lk(channelSocketsMtx_);
    bool ok = channelSocketsCv_.wait_for(lk, 1200s, [&] {
        std::cout << "\r"
                  << "Size of Received " << numberTimesReceived.size();
        return numberTimesReceived.size() == swarmManagers.size();
    });
    auto now = std::chrono::steady_clock::now();

    std::cout << "#########################################################################"
              << std::endl;
    std::cout << "Time for everyone to receive the message " << dht::print_duration(now - start)
              << std::endl;
    std::cout << " IS OK " << ok << std::endl;

    // ##############################################################################

    for (const auto& count : numberTimesReceived) {
        moyenne = moyenne + count.second;

        if (count.second > max) {
            max = count.second;
        }

        if (count.second < min) {
            min = count.second;
        }
    }

    auto it = channelSockets_.begin();

    displayBucketDistribution((*it).first);
    std::advance(it, swarmManagers.size() / 2);
    displayBucketDistribution((*it).first);
    std::advance(it, swarmManagers.size() / 2 - 1);
    displayBucketDistribution((*it).first);

    std::cout << "MOYENNE DE RECEPTION PAR NOEUD [ " << moyenne / (float) numberTimesReceived.size()
              << " ] " << std::endl;
    std::cout << "MAX DE RECEPTION PAR NOEUD     [ " << max << " ] " << std::endl;
    std::cout << "MIN DE RECEPTION PAR NOEUD     [ " << min << " ] " << std::endl;

    std::cout << "NOMBRE DE SAUTS DIRECTS        [ " << TOTAL_HOPS << " ] " << std::endl;
    std::cout << "NOMBRE D'ITERATIONS            [ " << iterations << " ] " << std::endl;

    distribution();
    std::cout << "#########################################################################"
              << std::endl;

    std::cout << "Number of times received " << numberTimesReceived.size() << std::endl;
    std::cout << "Number of swarm managers " << swarmManagers.size() << std::endl;

    CPPUNIT_ASSERT(true);
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::SwarmMessageSpread::name())
