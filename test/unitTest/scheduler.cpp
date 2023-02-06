/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfaielinux.com>
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

#include "test_runner.h"

#include "scheduled_executor.h"
#include <opendht/rng.h>

namespace jami { namespace test {

class SchedulerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "scheduler"; }

private:
    void schedulerTest();

    CPPUNIT_TEST_SUITE(SchedulerTest);
    CPPUNIT_TEST(schedulerTest);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SchedulerTest, SchedulerTest::name());

void
SchedulerTest::schedulerTest()
{
    jami::ScheduledExecutor executor("test");

    constexpr unsigned N = 1024;
    std::mutex mtx;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lk(mtx);

    std::atomic_uint64_t taskRun {0};
    std::atomic_uint64_t result {0};

    auto task = [&]{
        auto rng = dht::crypto::getSeededRandomEngine();
        uint64_t sum {0};
        for (uint64_t i=0; i<64 * N; i++)
            sum += rng();
        result += sum;
        std::lock_guard<std::mutex> l(mtx);
        if (++taskRun == N)
            cv.notify_all();
    };
    CPPUNIT_ASSERT(taskRun == 0);

    for (unsigned i=0; i<N; i++)
        executor.run(task);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&]{
        return taskRun == N;
    }));

    for (unsigned i=0; i<N; i++)
        executor.scheduleIn(task, std::chrono::microseconds(1));

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&]{
        return taskRun == 2 * N;
    }));

    for (unsigned i=0; i<N; i++)
        executor.scheduleIn(task, std::chrono::microseconds(1));
    executor.stop();
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::SchedulerTest::name());
