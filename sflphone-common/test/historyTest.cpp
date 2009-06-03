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

#include "historyTest.h"

using std::cout;
using std::endl;

#define HOMEDIR (getenv ("HOME"))
#define HISTORY_SAMPLE  "history-sample"
#define HISTORY_SAMPLE_SIZE     3

void HistoryTest::setUp(){
    // Instanciate the cleaner singleton
    history = new HistoryManager ();
}

void HistoryTest::test_create_history_path () {

    int result;
    std::string path;
    
    path = HOMEDIR;
    path += "/.sflphone/history";
    result = history->create_history_path ();
    CPPUNIT_ASSERT (result == 0);
    CPPUNIT_ASSERT (!history->is_loaded ());
    CPPUNIT_ASSERT (history->_history_path == path);
}

void HistoryTest::test_load_history_from_file ()
{
    bool res;

    history->create_history_path ();
    res = history->load_history_from_file ();

    CPPUNIT_ASSERT (history->is_loaded ());
    CPPUNIT_ASSERT (res == true);
}

void HistoryTest::test_load_history_items_map ()
{
    std::string path;
    int nb_items;
    
    history->set_history_path (HISTORY_SAMPLE);
    history->load_history_from_file ();
    nb_items = history->load_history_items_map ();
    std::cout << nb_items << std::endl; 
    CPPUNIT_ASSERT (nb_items == HISTORY_SAMPLE_SIZE);
    CPPUNIT_ASSERT (history->get_history_size () == HISTORY_SAMPLE_SIZE);
}

void HistoryTest::tearDown(){
    // Delete the history object
    delete history; history=0;
}
