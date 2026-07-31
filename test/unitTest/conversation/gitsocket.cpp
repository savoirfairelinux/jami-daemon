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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <dhtnet/multiplexed_socket.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "jamidht/conversation.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

namespace {

/**
 * A channel with no endpoint. shutdown() still marks it closed and notifies the
 * shutdown callback; only the EOF write is dropped, which these tests do not
 * observe.
 */
std::shared_ptr<dhtnet::ChannelSocket>
makeChannel(std::atomic_bool& closed)
{
    auto socket = std::make_shared<dhtnet::ChannelSocket>(std::weak_ptr<dhtnet::MultiplexedSocket>(),
                                                          "git://test/conversation",
                                                          1);
    socket->onShutdown([&closed](const std::error_code&) { closed = true; });
    return socket;
}

const DeviceId DEVICE_A {std::string(64, 'a')};
const DeviceId DEVICE_B {std::string(64, 'b')};

} // namespace

/**
 * Dropping the references the daemon holds to a git channel does not close it:
 * the channel stays registered in its multiplexed socket, and the peer keeps a
 * GitServer running for it, until one side sends EOF. These tests pin that the
 * container Conversation stores git channels in closes them when it lets go.
 */
class GitSocketTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "gitsocket"; }

private:
    void testErasingASlotClosesTheChannel();
    void testOverwritingASlotClosesTheOldChannel();
    void testReassigningTheSameChannelKeepsItOpen();
    void testClearingTheListClosesEveryChannel();
    void testMovingOutOfASlotDoesNotCloseTheChannel();

    CPPUNIT_TEST_SUITE(GitSocketTest);
    CPPUNIT_TEST(testErasingASlotClosesTheChannel);
    CPPUNIT_TEST(testOverwritingASlotClosesTheOldChannel);
    CPPUNIT_TEST(testReassigningTheSameChannelKeepsItOpen);
    CPPUNIT_TEST(testClearingTheListClosesEveryChannel);
    CPPUNIT_TEST(testMovingOutOfASlotDoesNotCloseTheChannel);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(GitSocketTest, GitSocketTest::name());

void
GitSocketTest::testErasingASlotClosesTheChannel()
{
    std::atomic_bool closed {false};
    GitSocketList sockets;
    sockets[DEVICE_A] = makeChannel(closed);

    sockets.erase(DEVICE_A);

    CPPUNIT_ASSERT(closed);
}

void
GitSocketTest::testOverwritingASlotClosesTheOldChannel()
{
    std::atomic_bool firstClosed {false};
    std::atomic_bool secondClosed {false};
    GitSocketList sockets;
    sockets[DEVICE_A] = makeChannel(firstClosed);

    sockets[DEVICE_A] = makeChannel(secondClosed);

    CPPUNIT_ASSERT(firstClosed);
    CPPUNIT_ASSERT(!secondClosed);
}

void
GitSocketTest::testReassigningTheSameChannelKeepsItOpen()
{
    std::atomic_bool closed {false};
    GitSocketList sockets;
    auto channel = makeChannel(closed);
    sockets[DEVICE_A] = channel;

    // Registering the channel a slot already owns must leave it usable.
    sockets[DEVICE_A] = channel;

    CPPUNIT_ASSERT(!closed);
    CPPUNIT_ASSERT(sockets[DEVICE_A].get() == channel);
}

void
GitSocketTest::testClearingTheListClosesEveryChannel()
{
    std::atomic_bool closedA {false};
    std::atomic_bool closedB {false};
    GitSocketList sockets;
    sockets[DEVICE_A] = makeChannel(closedA);
    sockets[DEVICE_B] = makeChannel(closedB);

    sockets.clear();

    CPPUNIT_ASSERT(closedA);
    CPPUNIT_ASSERT(closedB);
}

void
GitSocketTest::testMovingOutOfASlotDoesNotCloseTheChannel()
{
    std::atomic_bool closed {false};
    GitSocketList sockets;
    sockets[DEVICE_A] = makeChannel(closed);

    // Handing the channel over to another owner must not close it.
    auto handedOver = std::move(sockets[DEVICE_A]);
    sockets.erase(DEVICE_A);

    CPPUNIT_ASSERT(!closed);
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::GitSocketTest::name())
