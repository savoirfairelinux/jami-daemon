/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <check.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "../src/accountlist.h"
#include "../src/sflphone_const.h"

account_t* create_test_account (gchar *alias)
{
    account_t *test;
    gchar *id;
    
    srand(time(NULL));
    
    test = g_new0 (account_t, 1); 
    id = g_new0(gchar, 30);
    g_sprintf(id, "%s-%d", alias, rand());
    test->accountID = g_strdup (id);
    test->state = ACCOUNT_STATE_REGISTERED;
    test->properties = g_hash_table_new(NULL, g_str_equal);
    
    // Populate the properties
    g_hash_table_replace (test->properties, ACCOUNT_ENABLED, "1");
    g_hash_table_replace (test->properties, ACCOUNT_ALIAS, alias);
    g_hash_table_replace (test->properties, ACCOUNT_TYPE, "SIP");
    g_hash_table_replace (test->properties, ACCOUNT_HOSTNAME, "sflphone.org");
    g_hash_table_replace (test->properties, ACCOUNT_USERNAME, "1260");
    g_hash_table_replace (test->properties, ACCOUNT_PASSWORD, "NIPAgmLo");
    g_hash_table_replace (test->properties, ACCOUNT_MAILBOX, "");
    g_hash_table_replace (test->properties, ACCOUNT_SIP_STUN_SERVER, "");
    g_hash_table_replace (test->properties, ACCOUNT_SIP_STUN_ENABLED, "0");

    return test;
    
}

START_TEST (test_add_account)
{
    account_t *test = create_test_account ("test");

    account_list_init ();
    account_list_add (test);
    fail_unless (account_list_get_registered_accounts () == 1, "ERROR");
    account_list_remove (test->accountID);
    fail_unless (account_list_get_registered_accounts () == 0, "ERROR");
}
END_TEST

START_TEST (test_ordered_list)
{
    gchar *list;
    account_t *test = create_test_account ("test");

    list = g_new0(gchar, 30);
    g_sprintf(list, "%s/%s/", test->accountID, test->accountID);
    account_list_init ();
    account_list_add (test);
    account_list_add (test);
    fail_unless (g_strcasecmp (account_list_get_ordered_list (), list) == 0, "ERROR - BAD ACCOUNT LIST SERIALIZING");
}
END_TEST

START_TEST (test_get_by_id)
{
    account_t *test = create_test_account ("test");
    account_t *tmp;

    account_list_init ();
    account_list_add (test);
    tmp = account_list_get_by_id (test->accountID);
    fail_unless (g_strcasecmp (tmp->accountID, test->accountID) == 0, "ERROR - ACCOUNTLIST_GET_BY_ID");
}
END_TEST

START_TEST (test_sip_account)
{
    account_t *test = create_test_account ("test");

    account_list_init ();
    account_list_add (test);
    fail_unless (account_list_get_sip_account_number () == 1, "ERROR - BAD SIP ACCOUNT NUMBER");
}
END_TEST

START_TEST (test_get_account_position)
{
    guint pos, pos1;
    account_t *test = create_test_account ("test");
    account_t *test2 = create_test_account ("test2");
    
    account_list_init ();
    account_list_add (test);
    account_list_add (test2);

    pos = account_list_get_position (test);
    pos1 = account_list_get_position (test2);
    fail_if (pos == -1, "ERROR - bad account position");
    fail_unless (pos == 0, "ERROR - bad account position");

    fail_if (pos1 == -1, "ERROR - bad account position");
    fail_unless (pos1 == 1, "ERROR - bad account position");
    
    account_list_set_current (test);
    pos = account_list_get_position (test);
    pos1 = account_list_get_position (test2);
    fail_if (pos == -1, "ERROR - bad account position");
    fail_unless (pos == 0, "ERROR - bad account position");
    fail_unless (pos1 == 1, "ERROR - bad account position");
}
END_TEST

START_TEST (test_get_current_account)
{
    account_t *test = create_test_account ("test");
    account_t *test2 = create_test_account ("test2");
    account_t *current;

    account_list_init ();
    account_list_add (test);
    account_list_add (test2);
    current = account_list_get_current ();
    fail_unless (current != NULL, "ERROR - current account NULL");
    // The current account must be the first we add
    if (current)
    {
        fail_unless (g_strcasecmp (g_hash_table_lookup(current->properties, ACCOUNT_ALIAS) , 
                                   g_hash_table_lookup(test->properties, ACCOUNT_ALIAS)) == 0, 
                     "ERROR - BAD CURRENT ACCOUNT");
    }

    // Then we try to change the current account
    account_list_set_current (test2);
    current = account_list_get_current ();
    fail_unless (current != NULL, "ERROR - current account NULL");
    // The current account must be the first we add
    if (current)
    {
        fail_unless (g_strcasecmp (g_hash_table_lookup(current->properties, ACCOUNT_ALIAS) , 
                                    g_hash_table_lookup(test2->properties, ACCOUNT_ALIAS)) == 0, 
                    "ERROR - BAD CURRENT ACCOUNT");
    }
}
END_TEST

START_TEST (test_current_account_has_mailbox)
{
    account_t *test = create_test_account ("test");

    account_list_init ();
    account_list_add (test);
    fail_unless (account_list_current_account_has_mailbox () == FALSE, "current account has a default mailbox");

    g_hash_table_replace (test->properties, ACCOUNT_MAILBOX, "888");
    fail_unless (account_list_current_account_has_mailbox () == TRUE, "current account has not no voicemail number");
}
END_TEST


Suite *
global_suite (void)
{
  Suite *s = suite_create ("Global");

  TCase *tc_cases = tcase_create ("Accounts");
  tcase_add_test (tc_cases, test_add_account);
  tcase_add_test (tc_cases, test_ordered_list);
  tcase_add_test (tc_cases, test_sip_account);
  tcase_add_test (tc_cases, test_get_by_id);
  tcase_add_test (tc_cases, test_get_account_position);
  tcase_add_test (tc_cases, test_get_current_account);
  tcase_add_test (tc_cases, test_current_account_has_mailbox);
  suite_add_tcase (s, tc_cases);

  return s;
}

int
main (void)
{
  int number_failed;
  Suite *s = global_suite ();
  SRunner *sr = srunner_create (s);
  srunner_run_all (sr, CK_NORMAL);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
