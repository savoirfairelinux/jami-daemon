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

#include "account_factory.h"
#include "../../test_runner.h"
#include "dring.h"
#include "account_const.h"


namespace ring { namespace test {

class Account_factoryTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "account_factory"; }
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

    const std::string SIP_ID="SIP_ID";
    const std::string RING_ID="RING_ID";
    std::unique_ptr<AccountFactory> accountFactory;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(Account_factoryTest, Account_factoryTest::name());

void
Account_factoryTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    accountFactory.reset(new AccountFactory);
}

void
Account_factoryTest::tearDown()
{
    // Stop daemon
    DRing::fini();
}


void
Account_factoryTest::testAddRemoveSIPAccount()
{
    // verify if there is no account at the beginning
    CPPUNIT_ASSERT(accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==0);

    accountFactory->createAccount(DRing::Account::ProtocolNames::SIP, SIP_ID);

    CPPUNIT_ASSERT(accountFactory->hasAccount(SIP_ID));
    CPPUNIT_ASSERT(!accountFactory->hasAccount(RING_ID));
    CPPUNIT_ASSERT(!accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==1);

    accountFactory->removeAccount(SIP_ID);

    CPPUNIT_ASSERT(accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==0);
}

void
Account_factoryTest::testAddRemoveRINGAccount()
{
    // verify if there is no account at the beginning
    CPPUNIT_ASSERT(accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==0);

    accountFactory->createAccount(DRing::Account::ProtocolNames::RING, RING_ID);

    CPPUNIT_ASSERT(accountFactory->hasAccount(RING_ID));
    CPPUNIT_ASSERT(!accountFactory->hasAccount(SIP_ID));
    CPPUNIT_ASSERT(!accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==1);

    accountFactory->removeAccount(RING_ID);

    CPPUNIT_ASSERT(accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==0);
}

void
Account_factoryTest::testClear()
{
    // verify if there is no account at the beginning
    CPPUNIT_ASSERT(accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==0);

    const int nbrAccount = 5;

    for(int i = 0; i < nbrAccount ; ++i) {
        accountFactory->createAccount(DRing::Account::ProtocolNames::RING, RING_ID+std::to_string(i));
    }

    CPPUNIT_ASSERT(accountFactory->accountCount()==nbrAccount);
    CPPUNIT_ASSERT(!accountFactory->empty());

    accountFactory->clear();

    CPPUNIT_ASSERT(accountFactory->empty());
    CPPUNIT_ASSERT(accountFactory->accountCount()==0);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::Account_factoryTest::name())
