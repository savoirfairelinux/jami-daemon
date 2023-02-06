/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "connectivity/connectionmanager.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "archiver.h"
#include "base64.h"
#include "jami.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"

#include <git2.h>
#include <filesystem>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class AccountArchiveTest : public CppUnit::TestFixture
{
public:
    AccountArchiveTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~AccountArchiveTest() { libjami::fini(); }
    static std::string name() { return "AccountArchive"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;

private:
    void testExportImportNoPassword();
    void testExportImportNoPasswordDoubleGunzip();
    void testExportImportPassword();
    void testExportImportPasswordDoubleGunzip();
    void testExportDht();
    void testExportDhtWrongPassword();
    void testChangePassword();

    CPPUNIT_TEST_SUITE(AccountArchiveTest);
    CPPUNIT_TEST(testExportImportNoPassword);
    CPPUNIT_TEST(testExportImportNoPasswordDoubleGunzip);
    CPPUNIT_TEST(testExportImportPassword);
    CPPUNIT_TEST(testExportImportPasswordDoubleGunzip);
    CPPUNIT_TEST(testExportDht);
    CPPUNIT_TEST(testExportDhtWrongPassword);
    CPPUNIT_TEST(testChangePassword);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountArchiveTest, AccountArchiveTest::name());

void
AccountArchiveTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-password.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
AccountArchiveTest::tearDown()
{
    if (!bob2Id.empty())
        wait_for_removal_of({aliceId, bob2Id, bobId});
    else
        wait_for_removal_of({aliceId, bobId});
}

void
AccountArchiveTest::testExportImportNoPassword()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    CPPUNIT_ASSERT(aliceAccount->exportArchive("test.gz"));

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto alice2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(alice2Account->getUsername() == aliceAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportImportNoPasswordDoubleGunzip()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    CPPUNIT_ASSERT(aliceAccount->exportArchive("test.gz"));
    auto dat = fileutils::loadFile("test.gz");
    archiver::compressGzip(dat, "test.gz");

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto alice2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(alice2Account->getUsername() == aliceAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportImportPassword()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    CPPUNIT_ASSERT(bobAccount->exportArchive("test.gz", "test"));

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";
    details[ConfProperties::ARCHIVE_PASSWORD] = "test";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportImportPasswordDoubleGunzip()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    CPPUNIT_ASSERT(bobAccount->exportArchive("test.gz", "test"));
    auto dat = fileutils::loadFile("test.gz");
    archiver::compressGzip(dat, "test.gz");

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";
    details[ConfProperties::ARCHIVE_PASSWORD] = "test";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportDht()
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string pin;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ExportOnRingEnded>(
        [&](const std::string& accountId, int status, const std::string& p) {
            if (accountId == bobId && status == 0)
                pin = p;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    CPPUNIT_ASSERT(libjami::exportOnRing(bobId, "test"));
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !pin.empty(); }));

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PIN] = pin;
    details[ConfProperties::ARCHIVE_PASSWORD] = "test";

    bob2Id = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(bob2Id);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);
    CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
}

void
AccountArchiveTest::testExportDhtWrongPassword()
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    int status;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ExportOnRingEnded>(
        [&](const std::string& accountId, int s, const std::string&) {
            if (accountId == bobId)
                status = s;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    CPPUNIT_ASSERT(libjami::exportOnRing(bobId, "wrong"));
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return status == 1; }));
}

void
AccountArchiveTest::testChangePassword()
{
    // Test wrong password, should fail
    CPPUNIT_ASSERT(!libjami::changeAccountPassword(aliceId, "wrong", "new"));
    // Test correct password, should succeed
    CPPUNIT_ASSERT(libjami::changeAccountPassword(aliceId, "", "new"));
    // Now it should fail
    CPPUNIT_ASSERT(!libjami::changeAccountPassword(aliceId, "", "new"));
    // Remove password again (should succeed)
    CPPUNIT_ASSERT(libjami::changeAccountPassword(aliceId, "new", ""));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::AccountArchiveTest::name())
