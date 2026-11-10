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
#include <string>

#include "jami.h"
#include "../../test_runner.h"

namespace jami {
namespace test {

class GnutlsKeylogTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "gnutls_keylog"; }

private:
    void sslKeyLogFileHandledOnInit();

    CPPUNIT_TEST_SUITE(GnutlsKeylogTest);
    CPPUNIT_TEST(sslKeyLogFileHandledOnInit);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(GnutlsKeylogTest, GnutlsKeylogTest::name());

void
GnutlsKeylogTest::sslKeyLogFileHandledOnInit()
{
#ifdef _WIN32
    _putenv_s("SSLKEYLOGFILE", "jami-keylog.txt");
#else
    setenv("SSLKEYLOGFILE", "jami-keylog.txt", 1);
#endif

    CPPUNIT_ASSERT(libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG)));

    const char* keylog = getenv("SSLKEYLOGFILE");
#ifdef _WIN32
    // GnuTLS must never see SSLKEYLOGFILE on Windows (jami-client-qt#2517)
    CPPUNIT_ASSERT(keylog == nullptr || *keylog == '\0');
#else
    CPPUNIT_ASSERT(keylog != nullptr);
    CPPUNIT_ASSERT_EQUAL(std::string("jami-keylog.txt"), std::string(keylog));
    unsetenv("SSLKEYLOGFILE");
#endif

    libjami::fini();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::GnutlsKeylogTest::name());
