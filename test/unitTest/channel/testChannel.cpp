/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <future>
#include <algorithm>

#include "test_runner.h"

#include "channel.h"

namespace jami { namespace test {

// During a receive operation we should not wait more than this timeout,
// otherwise it would indicate a thread starvation.
static const std::chrono::seconds timeout {1};

class ChannelTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "channel"; }

private:
    void emptyStateTest();
    void sendTest();
    void receiveTest();
    void simpleUnlimitedTest();
    void flushTest();
    void dinningPhilosophersTest();

    CPPUNIT_TEST_SUITE(ChannelTest);
    CPPUNIT_TEST(emptyStateTest);
    CPPUNIT_TEST(sendTest);
    CPPUNIT_TEST(receiveTest);
    CPPUNIT_TEST(simpleUnlimitedTest);
    CPPUNIT_TEST(flushTest);
    CPPUNIT_TEST(dinningPhilosophersTest);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ChannelTest, ChannelTest::name());

//==============================================================================

void
ChannelTest::emptyStateTest()
{
    Channel<int, 1> limited_channel;
    Channel<int> unlimited_channel;

    CPPUNIT_ASSERT(limited_channel.empty());
    CPPUNIT_ASSERT(limited_channel.size() == 0);
    CPPUNIT_ASSERT(unlimited_channel.empty());
    CPPUNIT_ASSERT(unlimited_channel.size() == 0);

    CPPUNIT_ASSERT_THROW(limited_channel.receive(timeout), jami::ChannelEmpty);
    CPPUNIT_ASSERT_THROW(unlimited_channel.receive(timeout), jami::ChannelEmpty);
}

void
ChannelTest::sendTest()
{
    Channel<int, 2> limited_channel;
    Channel<int> unlimited_channel;

    // First insert
    limited_channel.send(42);
    CPPUNIT_ASSERT(!limited_channel.empty());
    CPPUNIT_ASSERT_EQUAL(1ul, limited_channel.size());

    unlimited_channel.send(999);
    CPPUNIT_ASSERT(!unlimited_channel.empty());
    CPPUNIT_ASSERT_EQUAL(1ul, unlimited_channel.size());

    // Second insert
    limited_channel.send(123456789);
    CPPUNIT_ASSERT(!limited_channel.empty());
    CPPUNIT_ASSERT_EQUAL(2ul, limited_channel.size());

    unlimited_channel.send(-1);
    CPPUNIT_ASSERT(!unlimited_channel.empty());
    CPPUNIT_ASSERT_EQUAL(2ul, unlimited_channel.size());
}

void
ChannelTest::receiveTest()
{
    Channel<int> channel;
    int first = 1, second = 2, third = 3;

    channel.send(first);
    channel.send(second);
    CPPUNIT_ASSERT(!channel.empty() && 2ul == channel.size());

    int v = 0;
    CPPUNIT_ASSERT_NO_THROW(channel.receive(v, timeout));
    CPPUNIT_ASSERT_EQUAL(v, first);
    CPPUNIT_ASSERT_EQUAL(1ul, channel.size());
    CPPUNIT_ASSERT(!channel.empty());
    CPPUNIT_ASSERT_NO_THROW(channel.receive(v, timeout));
    CPPUNIT_ASSERT_EQUAL(v, second);
    CPPUNIT_ASSERT_EQUAL(0ul, channel.size());
    CPPUNIT_ASSERT(channel.empty());

    channel.send(third);
    CPPUNIT_ASSERT(!channel.empty() && 1ul == channel.size());
    v = channel.receive();
    CPPUNIT_ASSERT_EQUAL(v, third);
    CPPUNIT_ASSERT_EQUAL(0ul, channel.size());
    CPPUNIT_ASSERT(channel.empty());
}

void
ChannelTest::simpleUnlimitedTest()
{
    Channel<int> c;
    std::array<int, 3> values = {1, 2, 3};
    c.send(&values[0], values.size()); // this API exists only for unlimited-version
}

void
ChannelTest::flushTest()
{
    Channel<int> c;
    c.send(1); c.send(1); c.send(1);
    CPPUNIT_ASSERT(!c.empty() && 3ul == c.size());

    auto queue = c.flush();
    bool correct = true;
    while (!queue.empty()) {
        correct &= queue.front() == 1;
        queue.pop();
    }
    CPPUNIT_ASSERT_MESSAGE("invalid items returned by Channel::flush()", correct);
    CPPUNIT_ASSERT(c.empty());
    CPPUNIT_ASSERT_EQUAL(0ul, c.size());
}

//==============================================================================

// Classical concurrential problem: dinning philosophers
// https://en.wikipedia.org/wiki/Dining_philosophers_problem

template <int N>
struct DinningTable
{
    // Per-fork pick-up order channel (entry = philosopher #)
    Channel<int> pickUpChannels[N];

    // Per-fork put-down order channel (entry = philosopher #)
    Channel<int> putDownChannels[N];
};

template <int N>
void
philosopher_job(DinningTable<N>& table, int i)
{
    // Think -> pick-up Left+Right forks -> eat -> put-down forks

    if (i) {
        table.pickUpChannels[i].send_emplace(i);
        table.pickUpChannels[(i + 1) % N].send_emplace(i);
    } else {
        // For the fist one, we swap fork picking order
        table.pickUpChannels[(i + 1) % N].send_emplace(i);
        table.pickUpChannels[i].send_emplace(i);
    }

    table.putDownChannels[i].send_emplace(i);
    table.putDownChannels[(i + 1) % N].send_emplace(i);
}

template <int N>
void
fork_job(DinningTable<N>& table, int i)
{
    // A fork wait to be pick-up first, then to be put-down
    table.pickUpChannels[i].receive(timeout);
    table.putDownChannels[i].receive(timeout);

    // In this configuration a fork could be used twice at least
    table.pickUpChannels[i].receive(timeout);
    table.putDownChannels[i].receive(timeout);
}

void
ChannelTest::dinningPhilosophersTest()
{
    constexpr int N = 5; // For the classical description of the problem
    DinningTable<N> table;

    // Dress-up the table first :-)
    std::future<void> forks[N];
    for (int i = 0; i < N; i++)
        forks[i] = std::async(std::launch::async,
                              [&table, i]{ fork_job<N>(table, i); });

    // Then invite philosophers...
    std::future<void> philosophers[N];
    for (int i = 0; i < N; i++)
        philosophers[i] = std::async(std::launch::async,
                                     [&table, i]{ philosopher_job<N>(table, i); });
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::ChannelTest::name());
