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

constexpr size_t nNodes = 100;

constexpr size_t BOOTSTRAP_SIZE = 2;
std::chrono::seconds time = 40s;

struct Message
{
    int identifier; // message identifier
    MSGPACK_DEFINE_MAP(identifier);
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
    std::map<NodeId, int> messageReceivedTracker;
    std::vector<NodeId> randomNodeIds;
    std::map<NodeId, int> numberTimesReceived;

    void generateSwarmManagers();
    void needSocketCallBack(const std::shared_ptr<SwarmManager>& sm);
    void sendMessage(const std::shared_ptr<ChannelSocketInterface>& socket);
    void receiveMessage(const NodeId nodeId, const std::shared_ptr<ChannelSocketInterface>& socket);
    std::shared_ptr<jami::SwarmManager> getManager(const NodeId& id)
    {
        auto it = swarmManagers.find(id);
        return it == swarmManagers.end() ? nullptr : it->second;
    }

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
        messageReceivedTracker[node] = 0;
        numberTimesReceived[node] = 0;
    }
}

void
SwarmMessageSpread::sendMessage(const std::shared_ptr<ChannelSocketInterface>& socket)
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::error_code ec;

    Message msg {1};
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
                auto nid = DeviceId(nodeId);
                auto swarmManager = getManager(nid);

                auto it = messageReceivedTracker.find(nid);
                numberTimesReceived[nodeId] += 1;

                if (it->second == 0) {
                    const auto& routingtable = swarmManager->getRoutingTable().getNodes();
                    it->second = 1;
                    for (auto& node : routingtable) {
                        if (node != socket->deviceId()) {
                            auto channelToSend = channelSockets_[nid][node];
                            std::this_thread::sleep_for(0.01s);
                            sendMessage(channelToSend);
                        }
                    }
                    // std::cout << "###########################" << std::endl;
                }

                else {
                    // std::cout << "Message already received by "<< nodeId  << std::endl;
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

    std::this_thread::sleep_for(time);

    for (const auto& channel : channelSockets_.begin()->second) {
        messageReceivedTracker.begin()->second = 1;
        sendMessage(channel.second);
    }

    std::this_thread::sleep_for(time);

    std::cout << "TRACKER MESSAGE RECEIVED" << std::endl;
    size_t counter_ = 0;

    for (const auto& tracker : messageReceivedTracker) {
        // std::cout << "Message received by " << tracker.first << " " << tracker.second << std::endl;
        if (tracker.second == 1) {
            counter_ = counter_ + tracker.second;
        }
    }

    std::cout << " NUMBER TIME RECEIVED " << std::endl;
    for (const auto& count : numberTimesReceived) {
        std::cout << "Number of time " << count.first << " received " << count.second << std::endl;
    }

    std::cout << "counter " << counter_ << "swarm Manager size " << swarmManagers.size()
              << std::endl;

    //##############################################################################

    int moyenne = 0;
    int max = 0;
    int min = 100;

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
    //##############################################################################

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Supposed to be identical", counter_, swarmManagers.size());
}

}; // namespace test
} // namespace jami
RING_TEST_RUNNER(jami::test::SwarmMessageSpread::name())
