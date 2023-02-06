/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "account_factory.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "manager.h"
#include "account_const.h"
#include "account_schema.h"
#include "common.h"
#include "jamidht/jamiaccount.h"

using namespace std::literals::chrono_literals;

namespace jami { namespace test {

class Account_factoryTest : public CppUnit::TestFixture {
public:
    Account_factoryTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~Account_factoryTest() { libjami::fini(); }
    static std::string name() { return "Account_factory"; }
    void setUp();
    void tearDown();

private:
    void testAddRemoveSIPAccount();
    void testAddRemoveRINGAccount();
    void testClear();

    CPPUNIT_TEST_SUITE(Account_factoryTest);
    CPPUNIT_TEST(testAddRemoveSIPAccount);
    CPPUNIT_TEST(testAddRemoveRINGAccount);
    CPPUNIT_TEST(testClear);
    CPPUNIT_TEST_SUITE_END();

    const std::string SIP_ID = "SIP_ID";
    const std::string JAMI_ID = "JAMI_ID";
    bool sipReady, ringReady, accountsRemoved, knownDevicesChanged;
    size_t initialAccounts;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(Account_factoryTest, Account_factoryTest::name());

void
Account_factoryTest::setUp()
{
    sipReady = false;
    ringReady = false;
    accountsRemoved = false;
    initialAccounts = Manager::instance().accountCount();

    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountID,
                const std::map<std::string, std::string>& details) {
                if (accountID != JAMI_ID && accountID != SIP_ID) {
                    return;
                }
                try {
                    ringReady |= accountID == JAMI_ID
                                && details.at(jami::Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS) == "REGISTERED"
                                && details.at(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED) == "true";
                    sipReady |= accountID == SIP_ID
                                && details.at(jami::Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS) == "READY";
                } catch (const std::out_of_range&) {}
            }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountsChanged>([&]() {
            if (jami::Manager::instance().getAccountList().size() <= initialAccounts) {
                accountsRemoved = true;
            }
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::KnownDevicesChanged>([&](auto, auto) {
            knownDevicesChanged = true;
        }));
    libjami::registerSignalHandlers(confHandlers);
}

void
Account_factoryTest::tearDown()
{
    libjami::unregisterSignalHandlers();
}

void
Account_factoryTest::testAddRemoveSIPAccount()
{
    AccountFactory* accountFactory = &Manager::instance().accountFactory;

    auto accDetails = libjami::getAccountTemplate("SIP");
    auto newAccount = Manager::instance().addAccount(accDetails, SIP_ID);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
        return sipReady;
    }));

    CPPUNIT_ASSERT(accountFactory->hasAccount(SIP_ID));
    CPPUNIT_ASSERT(!accountFactory->hasAccount(JAMI_ID));
    CPPUNIT_ASSERT(!accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount() == 1 + initialAccounts);

    auto details = Manager::instance().getVolatileAccountDetails(SIP_ID);
    CPPUNIT_ASSERT(details.find(libjami::Account::ConfProperties::DEVICE_ID) == details.end());

    Manager::instance().removeAccount(SIP_ID, true);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return accountsRemoved; }));
}

void
Account_factoryTest::testAddRemoveRINGAccount()
{
    AccountFactory* accountFactory = &Manager::instance().accountFactory;

    auto accDetails = libjami::getAccountTemplate("RING");
    auto newAccount = Manager::instance().addAccount(accDetails, JAMI_ID);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] {
        return ringReady;
    }));

    CPPUNIT_ASSERT(accountFactory->hasAccount(JAMI_ID));
    CPPUNIT_ASSERT(!accountFactory->hasAccount(SIP_ID));
    CPPUNIT_ASSERT(!accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount() == 1 + initialAccounts);

    auto details = Manager::instance().getVolatileAccountDetails(JAMI_ID);
    CPPUNIT_ASSERT(details.find(libjami::Account::ConfProperties::DEVICE_ID) != details.end());

    std::map<std::string, std::string> newDetails;
    newDetails[libjami::Account::ConfProperties::DEVICE_NAME] = "foo";
    knownDevicesChanged = false;
    libjami::setAccountDetails(JAMI_ID, newDetails);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return knownDevicesChanged; }));
    details = Manager::instance().getAccountDetails(JAMI_ID);
    CPPUNIT_ASSERT(details[libjami::Account::ConfProperties::DEVICE_NAME] == "foo");


    Manager::instance().removeAccount(JAMI_ID, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return accountsRemoved; }));
}

void
Account_factoryTest::testClear()
{
    AccountFactory accountFactory;
    // verify if there is no account at the beginning
    CPPUNIT_ASSERT(accountFactory.empty());
    CPPUNIT_ASSERT(accountFactory.accountCount() == 0);

    const int nbrAccount = 5;

    for(int i = 0; i < nbrAccount ; ++i) {
        accountFactory.createAccount(libjami::Account::ProtocolNames::RING, JAMI_ID+std::to_string(i));
    }

    CPPUNIT_ASSERT(accountFactory.accountCount()==nbrAccount);
    CPPUNIT_ASSERT(!accountFactory.empty());

    accountFactory.clear();

    CPPUNIT_ASSERT(accountFactory.empty());
    CPPUNIT_ASSERT(accountFactory.accountCount()==0);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::Account_factoryTest::name())
