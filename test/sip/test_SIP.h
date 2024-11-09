/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

// Application import
#include "manager.h"

#include "sip/sipvoiplink.h"
#include "connectivity/sip_utils.h"

#include <atomic>

#include <thread>
class RAIIThread
{
public:
    RAIIThread() = default;

    RAIIThread(std::thread&& t)
        : thread_ {std::move(t)} {};

    ~RAIIThread() { join(); }

    void join()
    {
        if (thread_.joinable())
            thread_.join();
    }

    RAIIThread(RAIIThread&&) = default;
    RAIIThread& operator=(RAIIThread&&) = default;

private:
    std::thread thread_;
};

/*
 * @file Test-sip.h
 * @brief       Regroups unitary tests related to the SIP module
 */

class test_SIP : public CppUnit::TestFixture
{
public:
    /*
     * Code factoring - Common resources can be initialized here.
     * This method is called by unitcpp before each test
     */
    void setUp();

    /*
     * Code factoring - Common resources can be released here.
     * This method is called by unitcpp after each test
     */
    void tearDown();

private:
    void testSIPURI(void);

    /**
     * Use cppunit library macros to add unit test to the factory
     */
    CPPUNIT_TEST_SUITE(test_SIP);
    CPPUNIT_TEST(testSIPURI);
    CPPUNIT_TEST_SUITE_END();

    RAIIThread eventLoop_;
    std::atomic_bool running_;
};
