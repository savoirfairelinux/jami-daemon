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

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "archiver.h"
#include "base64.h"
#include "jami.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <git2.h>
#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class LinkDeviceTest : public CppUnit::TestFixture
{
public:
    LinkDeviceTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~AccountArchiveTest() { libjami::fini(); }
    static std::string name() { return "LinkDevice"; }
    void setUp();
    void tearDown();

    std::string acc, rec;
    auto tmpAcc;

private:
    void testEstablishPeerConnection(); // test channel (TODO move into testPasswordSend??)
    void concept(); // test channel (TODO move into testPasswordSend??)
    void testPasswordSend(); // test sending correct password and making sure it is readable
    void testWrongPassword(); // because should not be sending archive in case of failure (TODO move into testPasswordSend)
    void testMultipleConnections(); // because should not be trying to send the password data to the wrong connection (test leaking password, archive)
    void testArchiveSend(); // test no password here in order to have two tests in one (test archive correctness/can be used on a new device properly)
    void testAccountsUpdated(); // test that account list is updated after loading function (see ut_conversation)

    CPPUNIT_TEST_SUITE(LinkDeviceTest);
    CPPUNIT_TEST(testEstablishPeerConnection);
    CPPUNIT_TEST(concept);
    CPPUNIT_TEST(testPasswordSend);
    CPPUNIT_TEST(testWrongPassword);
    CPPUNIT_TEST(testMultipleConnections);
    CPPUNIT_TEST(testArchiveSend);
    CPPUNIT_TEST(testAccountsUpdated);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountArchiveTest, AccountArchiveTest::name());

void
AccountArchiveTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-password.yml");
    auto acc = actors["alice"]; // this will be the old device account
    auto tmpAcc = dht::crypto::generateEcIdentity("Jami Temporary CA"); // this will be the temporary account created on the new device to facilitate the xfer of the archive
}

void
AccountArchiveTest::tearDown()
{
    wait_for_removal_of({oldDev});
}

void
LinkDeviceTest::testEstablishPeerConnection() {
    // auto acc = Manager::instance().getAccount<JamiAccount>(oldDev);
    // ArchiveAccountManager::startLoadFromPeer...
}

// ok so this would be very easy if I were able to add an export account archive feature so I think I will do that... it will basically form the TLS connection and then decide which archive to send from all the available archives... this way I can doudle-dip for moving conversations to a new account by exporting a different archive while also being able to write this test very simply
// after looking at the other unit tests I realized I can instead load bob and send a message from alice to bob and also send a message from the imported/loaded archive and then also check that the message count is equal to 2
void
concept() {
    // const std::string accountScheme = "jami-auth://" + tmpAcc.getId();
    const auto tmpId = tmpAcc.getId();
    auto oldAcc = Manager::instance().getAccount<JamiAccount>(acc);
    auto receiver = Manager::instance.getACcount<JamiAccount>(rec);

    auto testAcc_01 =
    ArchiveAccountManager::init
void
ArchiveAccountManager::initAuthentication(const std::string& accountId,
                                          PrivateKey key,
                                          std::string deviceName,
                                          std::unique_ptr<AccountCredentials> credentials,
                                          AuthSuccessCallback onSuccess,
                                          AuthFailureCallback onFailure,
                                          const OnChangeCallback& onChange)


    // pseudocode

    // wait this won't work because the account will already be listed... need to come up with a way to see when the account was added... maybe make two account lists somehow to simulate two different devices with independent account lists
    libjami::startImportFrom(for: oldAcc.getId(), connectionType: "peer", accountToSearchFor: tmpId);

    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() {
        return tmpId in accountIdList;
        // libjami::getAccountList()
    }))
}

void
testPasswordSend() {

}

void
testWrongPassword()  {

}

void
testMultipleConnections() {

}

void
testArchiveSend() {

}

///
///
///
///

void
AccountArchiveTest::testExportImportNoPassword()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(oldDev);

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
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(oldDev);

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
    CPPUNIT_ASSERT(!libjami::changeAccountPassword(oldDev, "wrong", "new"));
    // Test correct password, should succeed
    CPPUNIT_ASSERT(libjami::changeAccountPassword(oldDev, "", "new"));
    // Now it should fail
    CPPUNIT_ASSERT(!libjami::changeAccountPassword(oldDev, "", "new"));
    // Remove password again (should succeed)
    CPPUNIT_ASSERT(libjami::changeAccountPassword(oldDev, "new", ""));
}

/// !
///
///
///

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::AccountArchiveTest::name())
