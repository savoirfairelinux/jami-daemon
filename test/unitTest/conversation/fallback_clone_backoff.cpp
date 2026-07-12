/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"

#include "jamidht/fallback_clone_backoff.h"

namespace jami {
namespace test {

using namespace std::chrono_literals;

class FallbackCloneBackoffTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "fallback_clone_backoff"; }

private:
    void testSchedulesOncePerRound();
    void testCapsDelay();
    void testCancelResetsDelay();

    CPPUNIT_TEST_SUITE(FallbackCloneBackoffTest);
    CPPUNIT_TEST(testSchedulesOncePerRound);
    CPPUNIT_TEST(testCapsDelay);
    CPPUNIT_TEST(testCancelResetsDelay);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FallbackCloneBackoffTest, FallbackCloneBackoffTest::name());

void
FallbackCloneBackoffTest::testSchedulesOncePerRound()
{
    FallbackCloneBackoff backoff;

    auto delay = backoff.schedule();
    CPPUNIT_ASSERT(delay);
    CPPUNIT_ASSERT(*delay == 5s);
    CPPUNIT_ASSERT(!backoff.schedule());
    CPPUNIT_ASSERT(!backoff.schedule());

    backoff.timerFired();
    delay = backoff.schedule();
    CPPUNIT_ASSERT(delay);
    CPPUNIT_ASSERT(*delay == 10s);
}

void
FallbackCloneBackoffTest::testCapsDelay()
{
    FallbackCloneBackoff backoff;

    for (auto expected :
         {5s, 10s, 20s, 40s, 80s, 160s, 320s, 640s, 1280s, 2560s, 5120s, 10240s, 20480s, 40960s, 43200s}) {
        auto delay = backoff.schedule();
        CPPUNIT_ASSERT(delay);
        CPPUNIT_ASSERT(*delay == expected);
        backoff.timerFired();
    }
    auto delay = backoff.schedule();
    CPPUNIT_ASSERT(delay);
    CPPUNIT_ASSERT(*delay == 43200s);
}

void
FallbackCloneBackoffTest::testCancelResetsDelay()
{
    FallbackCloneBackoff backoff;

    CPPUNIT_ASSERT(backoff.schedule());
    backoff.timerFired();
    CPPUNIT_ASSERT(backoff.schedule());
    backoff.cancel();

    auto delay = backoff.schedule();
    CPPUNIT_ASSERT(delay);
    CPPUNIT_ASSERT(*delay == 5s);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::FallbackCloneBackoffTest::name());
