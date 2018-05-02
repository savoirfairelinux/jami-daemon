/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
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

#include "../../test_runner.h"
#include "dring.h"
#include "configurationmanager_interface.h"

#include <iostream>

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
    CPPUNIT_TEST_SUITE_END();

};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileTransferTest, FileTransferTest::name());

void
FileTransferTest::setUp()
{
    // Init daemon
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));

    auto account_list = DRing::getAccountList();
    for (const auto& account: account_list) {
        std::cout << account << std::endl;
    }
}

void
FileTransferTest::tearDown()
{
    // Stop daemon
    DRing::fini();
}


}} // namespace ring::test

RING_TEST_RUNNER(ring::test::FileTransferTest::name())
