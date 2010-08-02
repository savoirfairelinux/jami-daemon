/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <stdio.h>
#include <sstream>

#include "historytest.h"
#include "manager.h"
#include "constants.h"
#include "validator.h"

using std::cout;
using std::endl;

void HistoryTest::setUp()
{
    // Instanciate the cleaner singleton
    history = new HistoryManager();
}

void HistoryTest::test_create_history_path()
{
    _debug ("-------------------- HistoryTest::test_create_history_path --------------------\n");

    int result;
    char *cpath;
    std::string path;

    cpath = getenv ("XDG_DATA_HOME");
    (cpath != NULL) ? path = std::string (cpath) : path = std::string (HOMEDIR)
                             + "/.local/share/sflphone/history";

    result = history->create_history_path();
    CPPUNIT_ASSERT (result == 0);
    CPPUNIT_ASSERT (!history->is_loaded ());
    CPPUNIT_ASSERT (history->_history_path == path);
}

void HistoryTest::test_load_history_from_file()
{
    _debug ("-------------------- HistoryTest::test_load_history_from_file --------------------\n");

    bool res;
    Conf::ConfigTree history_list;

    history->create_history_path();
    res = history->load_history_from_file (&history_list);

    CPPUNIT_ASSERT (history->is_loaded ());
    CPPUNIT_ASSERT (res == true);
}

void HistoryTest::test_load_history_items_map()
{
    _debug ("-------------------- HistoryTest::test_load_history_items_map --------------------\n");

    std::string path;
    int nb_items;
    Conf::ConfigTree history_list;

    history->set_history_path (HISTORY_SAMPLE);
    history->load_history_from_file (&history_list);
    nb_items = history->load_history_items_map (&history_list,
               HUGE_HISTORY_LIMIT);
    CPPUNIT_ASSERT (nb_items == HISTORY_SAMPLE_SIZE);
    CPPUNIT_ASSERT (history->get_history_size () == HISTORY_SAMPLE_SIZE);
}

void HistoryTest::test_save_history_items_map()
{
    _debug ("-------------------- HistoryTest::test_save_history_items_map --------------------\n");

    std::string path;
    int nb_items_loaded, nb_items_saved;
    Conf::ConfigTree history_list, history_list2;

    history->set_history_path (HISTORY_SAMPLE);
    history->load_history_from_file (&history_list);
    nb_items_loaded = history->load_history_items_map (&history_list,
                      HUGE_HISTORY_LIMIT);
    nb_items_saved = history->save_history_items_map (&history_list2);
    CPPUNIT_ASSERT (nb_items_loaded == nb_items_saved);
}

void HistoryTest::test_save_history_to_file()
{
    _debug ("-------------------- HistoryTest::test_save_history_to_file --------------------\n");

    std::string path;
    Conf::ConfigTree history_list, history_list2;
    std::map<std::string, std::string> res;
    std::map<std::string, std::string>::iterator iter;

    history->set_history_path (HISTORY_SAMPLE);
    history->load_history_from_file (&history_list);
    history->load_history_items_map (&history_list, HUGE_HISTORY_LIMIT);
    history->save_history_items_map (&history_list2);
    CPPUNIT_ASSERT (history->save_history_to_file (&history_list2));
}

void HistoryTest::test_get_history_serialized()
{
    _debug ("-------------------- HistoryTest::test_get_history_serialized --------------------\n");

    std::map<std::string, std::string> res;
    std::map<std::string, std::string>::iterator iter;
    std::string tmp;

    CPPUNIT_ASSERT (history->load_history (HUGE_HISTORY_LIMIT, HISTORY_SAMPLE) == HISTORY_SAMPLE_SIZE);
    res = history->get_history_serialized();
    CPPUNIT_ASSERT (res.size() ==HISTORY_SAMPLE_SIZE);

    // Warning - If you change the history-sample file, you must change the following lines also so that the tests could work
    // The reference here is the file history-sample in this test directory
    // The serialized form is: calltype%to%from%callid

    // Check the first
    tmp = "0|514-276-5468|Savoir-faire Linux|144562458|empty";
    CPPUNIT_ASSERT (Validator::isEqual (tmp, res ["144562436"]));

    tmp = "2|136|Emmanuel Milou|747638765|Account:1239059899";
    CPPUNIT_ASSERT (Validator::isEqual (tmp, res ["747638685"]));

    // the account ID does not correspond to a loaded account
    tmp = "1|5143848557|empty|775354987|empty";
    CPPUNIT_ASSERT (Validator::isEqual (tmp, res ["775354456"]));
}

void HistoryTest::test_set_serialized_history()
{
    _debug ("-------------------- HistoryTest::test_set_serialized_history --------------------\n");

    // We build a map to have an efficient test
    std::map<std::string, std::string> map_test;
    std::string tmp;
    Conf::ConfigTree history_list;

    map_test["144562436"] = "0|514-276-5468|Savoir-faire Linux|144562458|empty";
    map_test["747638685"] = "2|136|Emmanuel Milou|747638765|Account:1239059899";
    map_test["775354456"] = "1|5143848557|empty|775354987|Account:43789459478";

    CPPUNIT_ASSERT (history->load_history (HUGE_HISTORY_LIMIT, HISTORY_SAMPLE) == HISTORY_SAMPLE_SIZE);
    // We use a large history limit to be able to interpret results
    CPPUNIT_ASSERT (history->set_serialized_history (map_test, HUGE_HISTORY_LIMIT) == 3);
    CPPUNIT_ASSERT (history->get_history_size () == 3);

    map_test.clear();
    map_test = history->get_history_serialized();
    CPPUNIT_ASSERT (map_test.size() ==3);

    // Check the first
    tmp = "0|514-276-5468|Savoir-faire Linux|144562458|empty";
    CPPUNIT_ASSERT (Validator::isEqual (tmp, map_test ["144562436"]));

    tmp = "2|136|Emmanuel Milou|747638765|Account:1239059899";
    CPPUNIT_ASSERT (Validator::isEqual (tmp, map_test ["747638685"]));

    // the account ID does not correspond to a loaded account
    tmp = "1|5143848557|empty|775354987|empty";
    CPPUNIT_ASSERT (Validator::isEqual (tmp, map_test ["775354456"]));

    history->save_history_items_map (&history_list);
    CPPUNIT_ASSERT (history->save_history_to_file (&history_list));
}

void HistoryTest::test_set_serialized_history_with_limit()
{
    _debug ("-------------------- HistoryTest::test_set_serialized_history_with_limit --------------------\n");

    // We build a map to have an efficient test
    std::map<std::string, std::string> map_test;
    std::string tmp;
    Conf::ConfigTree history_list;
    time_t current, day = 86400; // One day in unix timestamp
    std::stringstream current_1, current_2, current_3;

    (void) time (&current);
    current_1 << (current - 2 * day) << std::endl;
    current_2 << (current - 5 * day) << std::endl;
    current_3 << (current - 11 * day) << std::endl;

    map_test[current_1.str() ]
    = "0|514-276-5468|Savoir-faire Linux|144562458|empty";
    map_test[current_2.str() ]
    = "2|136|Emmanuel Milou|747638765|Account:1239059899";
    map_test[current_3.str() ]
    = "1|5143848557|empty|775354987|Account:43789459478";

    CPPUNIT_ASSERT (history->load_history (HUGE_HISTORY_LIMIT, HISTORY_SAMPLE) == HISTORY_SAMPLE_SIZE);
    // We use different value of history limit
    // 10 days - the last entry should not be saved
    CPPUNIT_ASSERT (history->set_serialized_history (map_test, 10) == 2);
    CPPUNIT_ASSERT (history->get_history_size () == 2);

    //  4 days - the two last entries should not be saved
    CPPUNIT_ASSERT (history->set_serialized_history (map_test, 4) == 1);
    CPPUNIT_ASSERT (history->get_history_size () == 1);

    //  1 day - no entry should not be saved
    CPPUNIT_ASSERT (history->set_serialized_history (map_test, 1) == 0);
    CPPUNIT_ASSERT (history->get_history_size () == 0);
}

void HistoryTest::tearDown()
{
    // Delete the history object
    delete history;
    history = 0;
}
