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

#include "manager.h"
#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "../../test_runner.h"
#include "jami.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <memory>

namespace jami {
namespace test {

class SdpLifetimeTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "SdpLifetime"; }

private:
    void testSdpReleasedAfterSipLinkShutdown();

    CPPUNIT_TEST_SUITE(SdpLifetimeTest);
    CPPUNIT_TEST(testSdpReleasedAfterSipLinkShutdown);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SdpLifetimeTest, SdpLifetimeTest::name());

// A call (and thus its Sdp) may be destroyed from a task still queued on the
// main thread after Manager::finish() shut down SIPVoIPLink. Releasing the
// Sdp memory pool must not touch the destroyed caching pool.
void
SdpLifetimeTest::testSdpReleasedAfterSipLinkShutdown()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    auto sdp = std::make_unique<Sdp>("sdp-lifetime-test");
    std::weak_ptr<pj_caching_pool> cachingPool = Manager::instance().sipVoIPLink().getSharedCachingPool();
    CPPUNIT_ASSERT(not cachingPool.expired());

    // Keep pjlib initialized, like the still-running main thread would.
    CPPUNIT_ASSERT(pj_init() == PJ_SUCCESS);
    libjami::fini();

    // The caching pool must still be alive as long as the Sdp owns a pool.
    CPPUNIT_ASSERT(not cachingPool.expired());
    sdp.reset();
    CPPUNIT_ASSERT(cachingPool.expired());
    pj_shutdown();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SdpLifetimeTest::name())
