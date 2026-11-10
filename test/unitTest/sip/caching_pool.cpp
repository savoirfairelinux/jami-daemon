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

#include "connectivity/sip_utils.h"
#include "test_runner.h"

namespace jami {
namespace test {

class CachingPoolTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "caching_pool"; }

    void setUp() override { CPPUNIT_ASSERT_EQUAL(PJ_SUCCESS, pj_init()); }
    void tearDown() override { pj_shutdown(); }

private:
    void releaseBeforeDestroy();
    void releaseAfterDestroy();
    void createAfterDestroy();

    CPPUNIT_TEST_SUITE(CachingPoolTest);
    CPPUNIT_TEST(releaseBeforeDestroy);
    CPPUNIT_TEST(releaseAfterDestroy);
    CPPUNIT_TEST(createAfterDestroy);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CachingPoolTest, CachingPoolTest::name());

void
CachingPoolTest::releaseBeforeDestroy()
{
    sip_utils::CachingPool cp;
    auto pool = cp.createPool("test", 1024, 1024);
    CPPUNIT_ASSERT(pool);
    CPPUNIT_ASSERT(pj_pool_alloc(pool.get(), 128));
    pool.reset();
    CPPUNIT_ASSERT_EQUAL(static_cast<pj_size_t>(0), cp.get()->used_count);
    cp.destroy();
    cp.destroy();
}

// Regression test: a call (and its Sdp pool) may be destroyed on the main
// thread after SIPVoIPLink::shutdown() destroyed the caching pool, which
// already freed that pool. Releasing it again must not touch freed memory.
void
CachingPoolTest::releaseAfterDestroy()
{
    sip_utils::CachingPool::GuardedPoolPtr pool;
    {
        sip_utils::CachingPool cp;
        pool = cp.createPool("test", 1024, 1024);
        CPPUNIT_ASSERT(pool);
        CPPUNIT_ASSERT(pj_pool_alloc(pool.get(), 128));
        cp.destroy();
    }
    pool.reset();
}

void
CachingPoolTest::createAfterDestroy()
{
    sip_utils::CachingPool cp;
    cp.destroy();
    auto pool = cp.createPool("test", 1024, 1024);
    CPPUNIT_ASSERT(not pool);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CachingPoolTest::name());
