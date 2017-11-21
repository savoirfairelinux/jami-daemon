/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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

#include "test_runner.h"

#include "channel.h"

namespace ring { namespace test {

class ChannelTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "channel"; }

private:
    void dinningPhilosophersTest();

    CPPUNIT_TEST_SUITE(ChannelTest);
    CPPUNIT_TEST(dinningPhilosophersTest);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ChannelTest, ChannelTest::name());

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
    // for the test validation, a philosopher should not wait more this time
    static const std::chrono::seconds timeout {1};

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

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::ChannelTest::name());
