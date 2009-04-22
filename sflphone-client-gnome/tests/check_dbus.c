/*
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
 */

#include <check.h>
#include <stdlib.h>

#include "../src/dbus/dbus.h"

START_TEST (test_dbus_connect)
{
    fail_unless (dbus_connect () == TRUE, "dbus_connect () returns FALSE");
}
END_TEST

Suite *
dbus_suite (void)
{
  Suite *s = suite_create ("D-Bus");

  TCase *tc_cases = tcase_create ("D-BUS connection");
  tcase_add_test (tc_cases, test_dbus_connect);
  suite_add_tcase (s, tc_cases);

  return s;
}

int
main (void)
{
  int number_failed;
  Suite *s = dbus_suite ();
  SRunner *sr = srunner_create (s);
  srunner_run_all (sr, CK_NORMAL);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
