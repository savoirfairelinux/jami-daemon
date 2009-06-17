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

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>

#include <assert.h>

// Application import
#include "history/historymanager.h"

/*
 * @file historyTest.h  
 * @brief       Regroups unitary tests related to the phone number cleanup function.
 */

#ifndef _HISTORY_TEST_
#define _HISTORY_TEST_

class HistoryTest : public CppUnit::TestCase {

   /**
     * Use cppunit library macros to add unit test the factory
     */
    CPPUNIT_TEST_SUITE (HistoryTest);
        CPPUNIT_TEST (test_create_history_path);
        CPPUNIT_TEST (test_load_history_from_file);
        CPPUNIT_TEST (test_load_history_items_map);
        CPPUNIT_TEST (test_save_history_items_map);
        CPPUNIT_TEST (test_save_history_to_file);
        CPPUNIT_TEST (test_get_history_serialized);
        CPPUNIT_TEST (test_set_serialized_history);
        CPPUNIT_TEST (test_set_serialized_history_with_limit);
    CPPUNIT_TEST_SUITE_END ();

    public:
        HistoryTest() : CppUnit::TestCase("History Tests") {}
        
        /*
         * Code factoring - Common resources can be initialized here.
         * This method is called by unitcpp before each test
         */
        void setUp();

        void test_create_history_path ();

        void test_load_history_from_file ();

        void test_load_history_items_map ();

        void test_save_history_items_map ();

        void test_save_history_to_file ();
    
        void test_get_history_serialized ();

        void test_set_serialized_history ();

        void test_set_serialized_history_with_limit ();
            
        /*
         * Code factoring - Common resources can be released here.
         * This method is called by unitcpp after each test
         */
        inline void tearDown ();

    private:
        HistoryManager *history;
};

/* Register our test module */
CPPUNIT_TEST_SUITE_REGISTRATION( HistoryTest );

#endif
