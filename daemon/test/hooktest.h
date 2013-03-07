/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef HOOKTEST_H_
#define HOOKTEST_H_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class HookTest : public CppUnit::TestFixture {

        CPPUNIT_TEST_SUITE(HookTest);
        CPPUNIT_TEST(RunHookWithNoArgs);
        CPPUNIT_TEST(RunHookWithArgs);
        CPPUNIT_TEST_SUITE_END();

    public:
        void RunHookWithNoArgs();
        void RunHookWithArgs();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HookTest, "HookTest");
CPPUNIT_TEST_SUITE_REGISTRATION(HookTest);

#endif /* HOOKTEST_H_ */
