/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"
#include "dring.h"
#include "data_transfer.h"
#include "manager.h"

#include <chrono>
#include <fstream>
#include <map>
#include <thread>

namespace ring { namespace test {

class FileTransferTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "filetransfer"; }
    void setUp();
    void tearDown();

    // TEST SENDFILE (outgoing)
    // send without account
    // send with account but no file
    // send with account and file. Doit etre dans list en created

    // incoming?

private:
    CPPUNIT_TEST_SUITE(FileTransferTest);
    CPPUNIT_TEST(testSendFileInvalidAccount);
    CPPUNIT_TEST(testSendFileInvalidFile);
    CPPUNIT_TEST(testSendFileIsCreated);
    CPPUNIT_TEST_SUITE_END();

    void testSendFileInvalidAccount();
    void testSendFileInvalidFile();
    void testSendFileIsCreated();

    const std::string RING_type="RING";
    const std::string RING_ID1="GLaDOs";
    const std::string RING_ID2="Tars";

};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileTransferTest, FileTransferTest::name());

void
FileTransferTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    std::map<std::string, std::string> details;
    details.emplace(std::make_pair("Account.alias", RING_ID1));
    details.emplace(std::make_pair("Account.type", "RING"));
    details.emplace(std::make_pair("Account.archivePassword", ""));
    Manager::instance().addAccount(details);
    details.emplace(std::make_pair("Account.alias", RING_ID2));
    Manager::instance().addAccount(details);

    // Wait for REGISTERED status (with a 10 secs timeout)
    for (const auto& account : Manager::instance().getAccountList()) {
        bool registered = false;
        auto i = 0;
        while (!registered && i < 20) {
            auto volatileDetails = Manager::instance().getAccount(account)->getVolatileAccountDetails();
            registered = volatileDetails.at("Account.registrationStatus") == "REGISTERED";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            ++i;
        }
    }
}

void
FileTransferTest::testSendFileInvalidAccount()
{
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(Manager::instance().dataTransfers->list().size()));
    DRing::DataTransferInfo dring_info;
    dring_info.accountId = "Heisenberg";
    uint64_t id;
    auto res = DRing::sendFile(dring_info, id);
    CPPUNIT_ASSERT_EQUAL(res, DRing::DataTransferError::invalid_argument);
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(Manager::instance().dataTransfers->list().size()));
}

void
FileTransferTest::testSendFileInvalidFile()
{
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(Manager::instance().dataTransfers->list().size()));
    DRing::DataTransferInfo dring_info;
    dring_info.accountId = Manager::instance().getAccountList()[0];
    dring_info.path = "notAFile";
    uint64_t id;
    auto res = DRing::sendFile(dring_info, id);
    CPPUNIT_ASSERT_EQUAL(res, DRing::DataTransferError::invalid_argument);
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(Manager::instance().dataTransfers->list().size()));
}

void
FileTransferTest::testSendFileIsCreated()
{
    std::ofstream tempFile;
    tempFile.open("thisIsAFile");
    tempFile << "And this is the content";
    tempFile.close();

    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(Manager::instance().dataTransfers->list().size()));
    DRing::DataTransferInfo dring_info;
    dring_info.accountId = Manager::instance().getAccountList()[0];
    dring_info.path = "thisIsAFile";
    auto account2 = Manager::instance().getAccountList()[1];

    dring_info.peer = Manager::instance().getAccountDetails(account2).at("Account.username");
    uint64_t id;
    auto res = DRing::sendFile(dring_info, id);
    CPPUNIT_ASSERT_EQUAL(res, DRing::DataTransferError::success);
    CPPUNIT_ASSERT_EQUAL(1, static_cast<int>(Manager::instance().dataTransfers->list().size()));

    std::remove("thisIsAFile");
}

void
FileTransferTest::tearDown()
{
    Manager::instance().removeAccounts();
    // Stop daemon
    DRing::fini();
}


}} // namespace ring::test

RING_TEST_RUNNER("filetransfer")
