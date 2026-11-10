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

#include <cstdlib>

#include "test_runner.h"
#include "jami.h"

namespace jami {
namespace test {

class InitTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "init"; }

private:
    void testSslKeyLogFileClearedOnWindows();

    CPPUNIT_TEST_SUITE(InitTest);
    CPPUNIT_TEST(testSslKeyLogFileClearedOnWindows);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(InitTest, InitTest::name());

void
InitTest::testSslKeyLogFileClearedOnWindows()
{
#ifdef _WIN32
    CPPUNIT_ASSERT_EQUAL(0, _putenv_s("SSLKEYLOGFILE", "jami-keylog.txt"));
    CPPUNIT_ASSERT(std::getenv("SSLKEYLOGFILE") != nullptr);
#endif

    CPPUNIT_ASSERT(libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG)));

#ifdef _WIN32
    const char* keylog = std::getenv("SSLKEYLOGFILE");
    CPPUNIT_ASSERT(keylog == nullptr || *keylog == '\0');
#endif
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::InitTest::name());
