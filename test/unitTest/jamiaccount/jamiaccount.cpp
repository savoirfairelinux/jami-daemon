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

#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "../../test_runner.h"

#include <dhtnet/fileutils.h>

#include <filesystem>

namespace jami {
namespace test {

class JamiAccountTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "JamiAccount"; }

private:
    void testAccountDeviceFoundWithoutAccountInfo();

    CPPUNIT_TEST_SUITE(JamiAccountTest);
    CPPUNIT_TEST(testAccountDeviceFoundWithoutAccountInfo);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(JamiAccountTest, JamiAccountTest::name());

void
JamiAccountTest::testAccountDeviceFoundWithoutAccountInfo()
{
    struct SyncOnRegisterGuard
    {
        bool previous;
        ~SyncOnRegisterGuard() { Manager::instance().syncOnRegister = previous; }
    } guard {Manager::instance().syncOnRegister};
    Manager::instance().syncOnRegister = true;

    std::filesystem::path certPath {"plugins/data/Test.crt"};
    if (!std::filesystem::exists(certPath))
        certPath = "test/unitTest/plugins/data/Test.crt";
    auto cert = std::make_shared<dht::crypto::Certificate>(fileutils::loadFile(certPath));

    auto account = std::make_shared<JamiAccount>("JAMI_ACCOUNT_TEST_ID");
    CPPUNIT_ASSERT(!account->accountManager());
    account->onAccountDeviceFoundForTest(cert);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::JamiAccountTest::name())
