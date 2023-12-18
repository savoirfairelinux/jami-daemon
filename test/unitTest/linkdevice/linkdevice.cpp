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
// #include "jami/jami_account.h"
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
#include <dhtnet/multiplexed_socket.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <git2.h>
#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>




#include "account_factory.h"
#include "account_schema.h"
#include "common.h"



#include "account_const.h"
#include <condition_variable>
#include <filesystem>
#include <string>



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
    ~LinkDeviceTest() { libjami::fini(); }
    static std::string name() { return "LinkDevice"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bobId2;
    std::string carlaId;

private:
    // void testEstablishPeerConnection(); // test channel (TODO move into testPasswordSend??)
    // void concept(); // test channel (TODO move into testPasswordSend??)
    // void testPasswordSend(); // test sending correct password and making sure it is readable
    // void testWrongPassword(); // because should not be sending archive in case of failure (TODO move into testPasswordSend)
    // void testMultipleConnections(); // because should not be trying to send the password data to the wrong connection (test leaking password, archive)
    // void testArchiveSend(); // test no password here in order to have two tests in one (test archive correctness/can be used on a new device properly)
    // void testAccountsUpdated(); // test that account list is updated after loading function (see ut_conversation)
    void testExportToPeer();

    void testProtocol();
    void testProtocolNoPassword();
    void testProtocolWithCorrectPassword();
    void testProtocolWithWrongPassword();

    // void testArchiveSend();

    CPPUNIT_TEST_SUITE(LinkDeviceTest);
    CPPUNIT_TEST(testExportToPeer);
    // CPPUNIT_TEST(testProtocol);
    // CPPUNIT_TEST(testEstablishPeerConnection);
    // CPPUNIT_TEST(concept);
    // CPPUNIT_TEST(testPasswordSend);
    // CPPUNIT_TEST(testWrongPassword);
    // CPPUNIT_TEST(testMultipleConnections);
    // CPPUNIT_TEST(testArchiveSend);
    // CPPUNIT_TEST(testAccountsUpdated);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(LinkDeviceTest, LinkDeviceTest::name());

void
LinkDeviceTest::setUp()
{
    // auto actors = load_actors_and_wait_for_announcement("actors/alice-password.yml");
    // accName = actors["alice"];
   // auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla.yml");
   // aliceId = actors["alice"];
   // bobId = actors["bob"];
   // carlaId = actors["carla"];
}

void
LinkDeviceTest::tearDown()
{
    // auto accArchive = std::filesystem::current_path().string() + "/alice.gz";
    // std::remove(aliceArchive.c_str());
    // wait_for_removal_of({oldDev});
    // auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    // std::remove(bobArchive.c_str());

    //wait_for_removal_of({aliceId, bobId, carlaId});
    // if (bob2Id.empty()) {
    //     wait_for_removal_of({aliceId, bobId, carlaId});
    // } else {
    //     wait_for_removal_of({aliceId, bobId, carlaId, bob2Id});
    // }
}

// tests the qr code generation that relies on Dht to generate an auth uri
void
LinkDeviceTest::testExportToPeer()
{
    JAMI_DBG("[ut_linkdevice] starting testExportToPeer");


    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    std::string qrInfo;


    std::mutex mtxAddDev;
    std::unique_lock<std::mutex> lkAddDev {mtxAddDev};
    std::condition_variable cvAddDev;

    std::string latestDetailInfo;
    int authSignalCount = 0;



    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
        [&](const std::string& accountId, int status, const std::string& scheme) {
            std::lock_guard<std::mutex> lock(mtx);
            qrInfo = scheme;
            JAMI_DBG("[ut_linkdevice] cb -> DeviceAuthStateChanged %s", qrInfo.c_str());
            // CPPUNIT_ASSERT(qrInfo.find("jami-auth://") != std::string::npos);
            CPPUNIT_ASSERT(scheme.substr(0,9) == "jami-auth");
            // CPPUNIT_ASSERT(libjami::exportToPeer(aliceId, qrInfo) == 0);
            // JAMI_DBG("[ut_linkdevice] testing libjami::exportToPeer(%s, %s)", bobId.c_str(), qrInfo.c_str());
            // CPPUNIT_ASSERT(libjami::exportToPeer(bobId, qrInfo) == 0);
            // CPPUNIT_ASSERT(libjami::exportToPeer(aliceId, qrInfo) == 0);
            cv.notify_one();
        }
    ));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::AddDeviceStateChanged>(
        [&](const std::string& accountId, int status, const std::string& detail) {
            std::lock_guard<std::mutex> lock(mtxAddDev);
            latestDetailInfo = detail;
            JAMI_DBG("[ut_linkdevice] cb -> AddDeviceStateChanged w/ detail of %s", latestDetailInfo.c_str());
            ++authSignalCount;
            JAMI_DBG("[ut_linkdevice] authSignalCount = %d", authSignalCount);
            cvAddDev.notify_one();
        }
    ));
    std::string oldDeviceId;
    auto oldDeviceStarted = false;
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
                std::lock_guard<std::mutex> lock(mtx);
                if (accountId == oldDeviceId) {
                    auto daemonStatus = details.at(
                        libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
                    if (daemonStatus == "true")
                        oldDeviceStarted = true;
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);
    JAMI_DBG("[ut_linkdevice] registered handlers");

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    // details[ConfProperties::ARCHIVE_URL] = "jami-auth"; // TODO rewrite jamiaccount.cpp logic to check archive
    details[ConfProperties::ARCHIVE_PATH] = "jami-auth";
    auto bobId = jami::Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&] { return not qrInfo.empty(); }));

    std::map<std::string, std::string> detailsOldDevice = libjami::getAccountTemplate("RING");
    oldDeviceId = jami::Manager::instance().addAccount(detailsOldDevice);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return oldDeviceStarted; }));



    JAMI_DBG("[ut_linkdevice] testing libjami::exportToPeer(%s, %s)", oldDeviceId.c_str(), qrInfo.c_str());
    CPPUNIT_ASSERT(libjami::exportToPeer(oldDeviceId, qrInfo) == 0);
    CPPUNIT_ASSERT(cv.wait_for(lkAddDev, 450s, [&] { return authSignalCount > 4; }));

    // CPPUNIT_ASSERT(libjami::exportToPeer(oldDeviceId, qrInfo) == 0);
    // CPPUNIT_ASSERT(libjami::exportToPeer(oldDeviceId, qrInfo) == 0);
    // CPPUNIT_ASSERT(libjami::exportToPeer(bobId, "jami-auth://" + qr_in+ "?code=123456") == 0);
    // CPPUNIT_ASSERT(qrInfo.find("jami-auth://") != std::string::npos);
    JAMI_DBG("[ut_linkdevice] passed testExportToPeer");


    // auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    //
    // wait_for_announcement_of(bob2Id);
    // auto bob2Account = Manager::instance().getAccount<JamiAccount>(bob2Id);
    // CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
}

// TODO write the protocol tests
void
LinkDeviceTest::testProtocol()
{
    // auto channel = std::make_shared<dhtnet::ChannelSocket>();
    testProtocolNoPassword();
    testProtocolWithCorrectPassword();
    testProtocolWithWrongPassword();
}

void LinkDeviceTest::testProtocolNoPassword()
{
}

void LinkDeviceTest::testProtocolWithCorrectPassword()
{
    // TODO
}

void LinkDeviceTest::testProtocolWithWrongPassword()
{
    // TODO
}


// // TODO write the archive download tests
// void LinkDeviceTest::testArchiveSend()
// {
//     // TODO
// }

//
// void
// LinkDeviceTest::testTransferArchive() {
//     // flipped testing
//     // - usually newDev generates QR code but for now will have oldDev generate the QR code
//     auto accTemplate = Manager::instance().getAccount<JamiAccount>(accName);
//
//     std::map<std::string, std::string> detailsOldAcc = libjami::getAccountTemplate("RING");
//     details[ConfProperties::ARCHIVE_PASSWORD] = "password";
//     details[ConfProperties::ARCHIVE_PATH] = std::filesystem::current_path().string() + "/alice.gz";
//     oldDev = jami::Manager::instance().addAccount(detailsOldAcc);
//     wait_for_announcement_of(oldDev);
//
//     std::map<std::string, std::string> detailsTmpAcc = libjami::getAccountTemplate("RING");
//     details[ConfProperties::ARCHIVE_URL] = "jami-auth://" + oldDev->accountId();
//     newDev = jami::Manager::instance().addAccount(detailsTmpAcc);
//     wait_for_announcement_of(newDev); // wait for p2p protocol to finish and download the oldDev archive
//
//     // TODO load the archive that was downloaded
//     std::map<std::string, std::string> detailsImpAcc = libjami::getAccountTemplate("RING");
//     details[ConfProperties::ARCHIVE_PATH] = "how to find this";
//     wait_for_announcement_of()
//     auto importedAccount =
//
//     // TODO assert that newly added account accountId is equal to oldDev->accountId
//     CPPUNIT_ASSERT(oldDev->accountId() == importedAccount.accountId())
//
// }
//
// // ok so this would be very easy if I were able to add an export account archive feature so I think I will do that... it will basically form the TLS connection and then decide which archive to send from all the available archives... this way I can doudle-dip for moving conversations to a new account by exporting a different archive while also being able to write this test very simply
// // after looking at the other unit tests I realized I can instead load bob and send a message from alice to bob and also send a message from the imported/loaded archive and then also check that the message count is equal to 2
// void
// concept() {
//     // const std::string accountScheme = "jami-auth://" + tmpAcc.getId();
//     const auto tmpId = tmpAcc.getId();
//     auto oldAcc = Manager::instance().getAccount<JamiAccount>(acc);
//     auto receiver = Manager::instance.getACcount<JamiAccount>(rec);
//
//     auto testAcc_01 =
//     ArchiveAccountManager::init
// void
// ArchiveAccountManager::initAuthentication(const std::string& accountId,
//                                           PrivateKey key,
//                                           std::string deviceName,
//                                           std::unique_ptr<AccountCredentials> credentials,
//                                           AuthSuccessCallback onSuccess,
//                                           AuthFailureCallback onFailure,
//                                           const OnChangeCallback& onChange)
//
//
//     // pseudocode
//
//     // wait this won't work because the account will already be listed... need to come up with a way to see when the account was added... maybe make two account lists somehow to simulate two different devices with independent account lists
//     libjami::startImportFrom(for: oldAcc.getId(), connectionType: "peer", accountToSearchFor: tmpId);
//
//     CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&]() {
//         return tmpId in accountIdList;
//         // libjami::getAccountList()
//     }))
// }
//
//
// void
// testPasswordSend() {
//
// }
//
// void
// testWrongPassword()  {
//
// }
//
// void
// testArchiveSend() {
//
// }
//
// // fuzzing
// void
// testMultipleConnections() {
//
// }
//
// // fuzzing
// void
// LinkDeviceTest::testExportImportNoPassword()
// {
//     auto aliceAccount = Manager::instance().getAccount<JamiAccount>(oldDev);
//
//     CPPUNIT_ASSERT(aliceAccount->exportArchive("test.gz"));
//
//     std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
//     details[ConfProperties::ARCHIVE_PATH] = "test.gz";
//
//     auto accountId = jami::Manager::instance().addAccount(details);
//     wait_for_announcement_of(accountId);
//     auto alice2Account = Manager::instance().getAccount<JamiAccount>(accountId);
//     CPPUNIT_ASSERT(alice2Account->getUsername() == aliceAccount->getUsername());
//     std::remove("test.gz");
//     wait_for_removal_of(accountId);
// }
//
// // security
// void
// LinkDeviceTest::testExportImportWithPasswordString()
// {
//     auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
//
//     CPPUNIT_ASSERT(bobAccount->exportArchive("test.gz", "test"));
//     auto dat = fileutils::loadFile("test.gz");
//     archiver::compressGzip(dat, "test.gz");
//
//     std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
//     details[ConfProperties::ARCHIVE_PATH] = "test.gz";
//     details[ConfProperties::ARCHIVE_PASSWORD] = "test";
//
//     auto accountId = jami::Manager::instance().addAccount(details);
//     wait_for_announcement_of(accountId);
//     auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
//     CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
//     std::remove("test.gz");
//     wait_for_removal_of(accountId);
// }
//
// // security
// void
// LinkDeviceTest::testExportImportWithPasswordKey()
// {
//   // TODO with biometrics
//   // want to make sure that the key way works as well as the password way
//   // TODO combine this and PasswordString into testExportImportWithPassword
// }
//
// // security
// void
// LinkDeviceTest;:testExportImportWithPassword()
// {
//   testExportImportWithPasswordString();
//   testExportImportWithPasswordKey();
// }
//
// // penetration
// void
// LinkDeviceTest::testExportImportWithWrongAttempts()
// {
//   // TODO this test should make sure that actors cannot try to guess the password a large number of times
//   // I am thinking of a couple of scenarios:
//   // 1. as an attacker I can create sockets and destroy them to achieve more than three attemtps and then can brute for the encrypted achive given enough time (especially on fast devices)
// }
//
// void
// LinkDeviceTest::testExportImportPassword()
// {
//     auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
//
//     CPPUNIT_ASSERT(bobAccount->exportArchive("test.gz", "test"));
//
//     std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
//     details[ConfProperties::ARCHIVE_PATH] = "test.gz";
//     details[ConfProperties::ARCHIVE_PASSWORD] = "test";
//
//     auto accountId = jami::Manager::instance().addAccount(details);
//     wait_for_announcement_of(accountId);
//     auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
//     CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
//     std::remove("test.gz");
//     wait_for_removal_of(accountId);
// }

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::LinkDeviceTest::name())
