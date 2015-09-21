/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

/*
 * @file instantmessagingtest.h
 * @brief Unit tests related to the instant messaging module
 */

#ifndef INSTANTMANAGER_TEST_
#define INSTANTMANAGER_TEST_

namespace ring { namespace InstantMessaging { namespace test {

class InstantMessagingTest : public CppUnit::TestCase {
        CPPUNIT_TEST_SUITE(InstantMessagingTest);
        CPPUNIT_TEST(testSaveSingleMessage);
        CPPUNIT_TEST(testSaveMultipleMessage);
        CPPUNIT_TEST(testGenerateXmlUriList);
        CPPUNIT_TEST(testXmlUriListParsing);
        CPPUNIT_TEST(testGetTextArea);
        CPPUNIT_TEST(testGetUriListArea);
        CPPUNIT_TEST(testIllFormatedMessage);
        CPPUNIT_TEST_SUITE_END();

    public:
        InstantMessagingTest() : CppUnit::TestCase("Instant messaging module Tests") {}

        void testSaveSingleMessage();
        void testSaveMultipleMessage();
        void testGenerateXmlUriList();
        void testXmlUriListParsing();
        void testGetTextArea();
        void testGetUriListArea();
        void testIllFormatedMessage();
};

/* Register our test module */
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(InstantMessagingTest, "InstantMessagingTest");
CPPUNIT_TEST_SUITE_REGISTRATION(InstantMessagingTest);

}}} // namespace ring::InstantMessaging::test

#endif
