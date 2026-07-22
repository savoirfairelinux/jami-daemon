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

#include "jamidht/account_manager.h"
#include "../../test_runner.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace jami {
namespace test {

class TestAccountManager : public AccountManager
{
public:
    TestAccountManager()
        : AccountManager("testAccount", ".", "")
    {}

    void initAuthentication(std::string,
                            std::unique_ptr<AccountCredentials>,
                            AuthSuccessCallback,
                            AuthFailureCallback,
                            const OnChangeCallback&) override
    {}

    bool changePassword(const std::string&, const std::string&) override { return false; }

    void syncDevices() override {}
};

class AccountManagerTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "account_manager"; }

private:
    void testStartSyncWithoutIdentity();

    CPPUNIT_TEST_SUITE(AccountManagerTest);
    CPPUNIT_TEST(testStartSyncWithoutIdentity);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountManagerTest, AccountManagerTest::name());

void
AccountManagerTest::testStartSyncWithoutIdentity()
{
    TestAccountManager accountManager;
    bool deviceFound = false;
    bool deviceAnnounced = false;

    accountManager.startSync(
        [&](const std::shared_ptr<dht::crypto::Certificate>&) { deviceFound = true; },
        [&] { deviceAnnounced = true; });

    CPPUNIT_ASSERT(!deviceFound);
    CPPUNIT_ASSERT(!deviceAnnounced);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::AccountManagerTest::name())
