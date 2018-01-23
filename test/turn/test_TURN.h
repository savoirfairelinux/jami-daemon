/*
 *  Copyright (C) 2017-2018 Savoir-faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 *
 */
#pragma once

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>


/*
 * @file test_TURN.h
 * @brief Regroups unitary tests related to the TURN transport class
 */

class test_TURN : public CppUnit::TestFixture
{
private:
    void testSimpleConnection(void);

    /**
     * Use cppunit library macros to add unit test to the factory
     */
    CPPUNIT_TEST_SUITE(test_TURN);
    CPPUNIT_TEST(testSimpleConnection);
    CPPUNIT_TEST_SUITE_END();
};
