/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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

#include <condition_variable>

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "account_const.h"

using namespace DRing::Account;

namespace jami { namespace test {

class ConnectionManagerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "ConnectionManager"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testConnectDevice();

    CPPUNIT_TEST_SUITE(ConnectionManagerTest);
    CPPUNIT_TEST(testConnectDevice);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConnectionManagerTest, ConnectionManagerTest::name());

void
ConnectionManagerTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));


    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    bool ready = false;
    bool idx = 0;
    while(!ready && idx < 100) {
        auto details = aliceAccount->getVolatileAccountDetails();
        auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready = (daemonStatus == "REGISTERED");
        details = bobAccount->getVolatileAccountDetails();
        daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
        ready &= (daemonStatus == "REGISTERED");
        if (!ready) {
            idx += 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void
ConnectionManagerTest::tearDown()
{
    Manager::instance().removeAccount(aliceId);
    Manager::instance().removeAccount(bobId);
    // Stop daemon
    DRing::fini();
}

void
ConnectionManagerTest::testConnectDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobDeviceId = bobAccount->getAccountDetails()[ConfProperties::RING_DEVICE_ID];

    std::mutex mtx;
    std::unique_lock<std::mutex> lk{ mtx };
    std::condition_variable cv;
    bool successfullyConnected = false;

    aliceAccount->connectionManager().connectDevice(bobDeviceId, "git://*", [&cv, &successfullyConnected](std::shared_ptr<ChannelSocket> socket) {
        if (socket) {
            successfullyConnected = true;
            cv.notify_one();
        }
    });
    cv.wait_for(lk, std::chrono::seconds(10));
    CPPUNIT_ASSERT(successfullyConnected);
}

}} // namespace test

RING_TEST_RUNNER(jami::test::ConnectionManagerTest::name())
