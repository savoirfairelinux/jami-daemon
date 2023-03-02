/*
 *  Copyright (C) 2023 Savoir-faire Linux Inc.
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

#include "../../test_runner.h"
#include "jami.h"
#include "../common.h"
#include "jamidht/swarm/swarm_manager.h"
#include "connectivity/multiplexed_socket.h"
#include "connectivity/peer_connection.h"
#include "nodes.h"

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace dht;
using NodeId = dht::PkId;

namespace jami {
namespace test {

constexpr size_t nNodes = 10;

constexpr size_t BOOTSTRAP_SIZE = 2;
std::chrono::seconds time = 30s;

int TOTAL_HOPS = 0;

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
        std::lock_guard<std::mutex> lock(mutex);
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

    std::map<NodeId, std::shared_ptr<jami::SwarmManager>> swarmManagers;
    std::map<NodeId, std::map<NodeId, std::shared_ptr<jami::ChannelSocketTest>>> channelSockets_;
    std::vector<NodeId> randomNodeIds;
    std::map<NodeId, int> numberTimesReceived;

    int iterations = 0;

    void generateSwarmManagers();
    void needSocketCallBack(const std::shared_ptr<SwarmManager>& sm);
    void sendMessage(const std::shared_ptr<ChannelSocketInterface>& socket, Message msg);
    void receiveMessage(const NodeId nodeId, const std::shared_ptr<ChannelSocketInterface>& socket);
    void relayMessageToRoutingTable(const NodeId nodeId, const NodeId sourceId, const Message msg);
    void updateHops(int hops);

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
        auto sm = std::make_shared<SwarmManager>(node);
        swarmManagers[node] = sm;
        randomNodeIds.emplace_back(node);
    }
}

std::shared_ptr<jami::SwarmManager>
SwarmMessageSpread::getManager(const NodeId& id)
{
    auto it = swarmManagers.find(id);
    return it == swarmManagers.end() ? nullptr : it->second;
}

void
SwarmMessageSpread::sendMessage(const std::shared_ptr<ChannelSocketInterface>& socket, Message msg)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;

    pk.pack(msg);

    // std::cout << "sending message to " << socket->deviceId() << std::endl;
    socket->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
}

void
SwarmMessageSpread::receiveMessage(const NodeId nodeId,
                                   const std::shared_ptr<ChannelSocketInterface>& socket)
{
    struct DecodingContext
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               512};
    };

    socket->setOnRecv([this,
                       wsocket = std::weak_ptr<ChannelSocketInterface>(socket),
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
                    auto var = numberTimesReceived.find(nodeId);
                    iterations = iterations + 1;

                    if (var != numberTimesReceived.end()) {
                        numberTimesReceived[nodeId] += 1;

                    } else {
                        Message msgToSend;
                        msgToSend.identifier_ = 1;
                        msgToSend.hops_ = msg.hops_ + 1;
                        numberTimesReceived.insert(std::pair<NodeId, int>(nodeId, 1));
                        updateHops(msgToSend.hops_);
                        relayMessageToRoutingTable(nodeId, socket->deviceId(), msgToSend);
                    }
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
        Manager::instance().ioContext()->post([this, wsm, nodeId, onSocket = std::move(onSocket)] {
            auto sm = wsm.lock();
            if (!sm)
                return;

            NodeId node = DeviceId(nodeId);
            std::lock_guard<std::mutex> lk(channelSocketsMtx_);
            if (auto smRemote = getManager(node)) {
                auto myId = sm->getId();
                auto& cstRemote = channelSockets_[node][myId];
                auto& cstMe = channelSockets_[myId][node];
                if (!cstRemote) {
                    cstRemote = std::make_shared<ChannelSocketTest>(myId, "test1", 0);
                }
                if (!cstMe) {
                    cstMe = std::make_shared<ChannelSocketTest>(node, "test1", 0);
                }
                ChannelSocketTest::link(cstMe, cstRemote);

                onSocket(cstMe);
                smRemote->addChannel(cstRemote);
                receiveMessage(myId, cstMe);
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
            auto channelToSend = channelSockets_[nodeId][node];
            std::this_thread::sleep_for(0.5s);
            sendMessage(channelToSend, msg);
        }
    }
}

void
SwarmMessageSpread::testWriteMessage()
{
    std::cout << "\ntestWriteMessage()" << std::endl;
    for (const auto& sm : swarmManagers) {
        needSocketCallBack(sm.second);
    }

    Counter counter(swarmManagers.size());
    for (const auto& sm : swarmManagers) {
        dht::ThreadPool::computation().run([&] {
            std::vector<NodeId> randIds(BOOTSTRAP_SIZE);
            std::uniform_int_distribution<size_t> distribution(0, randomNodeIds.size() - 1);
            std::generate(randIds.begin(), randIds.end(), [&] {
                return randomNodeIds[distribution(rd)];
            });
            sm.second->setKnownNodes(randIds);
            counter.count();
        });
    }

    counter.wait();

    std::cout << "Sleeping 20 s" << std::endl;
    std::this_thread::sleep_for(20s);

    std::cout << "Sending First Message " << std::endl;
    numberTimesReceived[channelSockets_.begin()->first] = 1;
    for (const auto& channel : channelSockets_.begin()->second) {
        if (channel.second) {
            sendMessage(channel.second, {1, 0});
        }
    }

    //  std::this_thread::sleep_for(time);

    auto start = std::chrono::steady_clock::now();
    bool isEqual {false};

    do {
        // std::this_thread::sleep_for(1s);

        isEqual = numberTimesReceived.size() == swarmManagers.size();

        // std::cout << "Size of Received " << numberTimesReceived.size() << std::endl;

        if (isEqual)
            break;
    } while (std::chrono::steady_clock::now() - start < time);

    std::cout << "Time for everyone to receive the message "
              << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()
                                                                  - start)
                     .count()
              << " s" << std::endl;
    CPPUNIT_ASSERT(isEqual);

    // std::cout << " NUMBER TIME RECEIVED " << std::endl;
    // for (const auto& count : numberTimesReceived) {
    //     std::cout << "Number of time " << count.first << " received " << count.second << std::endl;
    // }

    // ##############################################################################

    int moyenne = 0;
    int max = 0;
    int min = 10000;

    for (const auto& count : numberTimesReceived) {
        moyenne = moyenne + count.second;

        if (count.second > max) {
            max = count.second;
        }

        if (count.second < min) {
            min = count.second;
        }
    }

    std::cout << "MOYENNE DE RECEPTION PAR NOEUD [ " << moyenne / numberTimesReceived.size()
              << " ] " << std::endl;
    std::cout << "MAX DE RECEPTION PAR NOEUD     [ " << max << " ] " << std::endl;
    std::cout << "MIN DE RECEPTION PAR NOEUD     [ " << min << " ] " << std::endl;

    std::cout << "NOMBRE DE SAUTS DIRECTS        [ " << TOTAL_HOPS << " ] " << std::endl;
    std::cout << "NOMBRE D'ITERATIONS            [ " << iterations << " ] " << std::endl;
    // ##############################################################################

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be identical",
                                 numberTimesReceived.size(),
                                 swarmManagers.size());
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::SwarmMessageSpread::name())
