/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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
#include "jamidht/connectionmanager.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "archiver.h"
#include "base64.h"
#include "dring.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"

#include <git2.h>
#include <filesystem>

using namespace std::string_literals;
using namespace DRing::Account;

namespace jami {
namespace test {

class AccountArchiveTest : public CppUnit::TestFixture
{
public:
    AccountArchiveTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~AccountArchiveTest() { DRing::fini(); }
    static std::string name() { return "AccountArchive"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testExportImportNoPassword();
    void testExportImportNoPasswordDoubleGunzip();
    void testExportImportPassword();
    void testExportImportPasswordDoubleGunzip();

    CPPUNIT_TEST_SUITE(AccountArchiveTest);
    CPPUNIT_TEST(testExportImportNoPassword);
    CPPUNIT_TEST(testExportImportNoPasswordDoubleGunzip);
    CPPUNIT_TEST(testExportImportPassword);
    CPPUNIT_TEST(testExportImportPasswordDoubleGunzip);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountArchiveTest,
                                      AccountArchiveTest::name());

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
    wait_for_removal_of({aliceId, bobId});
}

void
AccountArchiveTest::testExportImportNoPassword()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    CPPUNIT_ASSERT(aliceAccount->exportArchive("test.gz"));

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
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

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
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

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
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

    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";
    details[ConfProperties::ARCHIVE_PASSWORD] = "test";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::AccountArchiveTest::name())
