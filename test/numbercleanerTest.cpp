/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <sstream>
#include <dlfcn.h>

#include "numbercleanerTest.h"

#define NUMBER_TEST_1   "514 333 4444"
#define NUMBER_TEST_2   "514-333-4444"
#define NUMBER_TEST_3   "(514) 333 4444"
#define NUMBER_TEST_4   "(514)-333-4444"
#define NUMBER_TEST_5   "(514) 333-4444"
#define NUMBER_TEST_6   "514 333  4444"
#define NUMBER_TEST_7   "ext 136"
#define NUMBER_TEST_8   "514 333  4444 ext. 136"
#define NUMBER_TEST_9   "514 333  4444 ext 136"

#define VALID_NUMBER                "5143334444"
#define VALID_PREPENDED_NUMBER      "95143334444"

using std::cout;
using std::endl;


void NumberCleanerTest::setUp(){
    // Instanciate the cleaner singleton
    cleaner = new NumberCleaner ();
}

void NumberCleanerTest::test_format_1 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_1) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_2 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_2) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_3 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_3) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_4 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_4) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_5 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_5) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_6 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_6) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_7 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_7) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_8 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_8) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_9 (void) {

    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_9) == VALID_NUMBER);
}

void NumberCleanerTest::test_format_10 (void) {

    cleaner->set_phone_number_prefix ("9");
    CPPUNIT_ASSERT (cleaner->get_phone_number_prefix () == "9");
    CPPUNIT_ASSERT (cleaner->clean (NUMBER_TEST_1) == VALID_PREPENDED_NUMBER);
}

void NumberCleanerTest::tearDown(){
    // Delete the cleaner object
    delete cleaner; cleaner=0;
}
