/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include <actions.h>
#include <calllist.h>
#include <config.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <statusicon.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <gtk/gtk.h>
#include <stdlib.h>

#include "shortcuts.h"

/**
 * Stop logging engine
 */
static void
shutdown_logging ()
{
  if (log4c_fini ())
    {
      ERROR("log4c_fini() failed");
    }
}

/**
 * Start loggin engine
 */
static void
startup_logging ()
{
  log4c_init ();
  if (log4c_load (DATA_DIR "/log4crc") == -1)
    g_warning ("Cannot load log4j configuration file : %s", DATA_DIR "/log4crc");

  log4c_sfl_gtk_category = log4c_category_get ("org.sflphone.gtk");
}

int
main (int argc, char *argv[])
{
  // Handle logging
  int i;

  // Startup logging
  startup_logging ();

  // Check arguments if debug mode is activated
  for (i = 0; i < argc; i++)
    if (g_strcmp0 (argv[i], "--debug") == 0)
      log4c_category_set_priority (log4c_sfl_gtk_category, LOG4C_PRIORITY_DEBUG);

  // Start GTK application

  gtk_init (&argc, &argv);

  g_print ("%s %s\n", PACKAGE, VERSION);
  g_print ("\nCopyright (c) 2005 2006 2007 2008 2009 2010 Savoir-faire Linux Inc.\n\n");
  g_print ("This is free software.  You may redistribute copies of it under the terms of\n" \
           "the GNU General Public License Version 3 <http://www.gnu.org/licenses/gpl.html>.\n" \
           "There is NO WARRANTY, to the extent permitted by law.\n\n" \
           "Additional permission under GNU GPL version 3 section 7:\n\n" \
           "If you modify this program, or any covered work, by linking or\n" \
           "combining it with the OpenSSL project's OpenSSL library (or a\n" \
           "modified version of that library), containing parts covered by the\n" \
           "terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.\n" \
           "grants you additional permission to convey the resulting work.\n" \
           "Corresponding Source for a non-source form of such a combination\n" \
           "shall include the source code for the parts of OpenSSL used as well\n" \
           "as that of the covered work.\n\n");

  DEBUG("Logging Started");

  srand (time (NULL));

  // Internationalization
  bindtextdomain ("sflphone-client-gnome", LOCALEDIR);
  textdomain ("sflphone-client-gnome");

  // Initialises the GNOME libraries
  gnome_program_init ("sflphone", VERSION, LIBGNOMEUI_MODULE, argc, argv,
      GNOME_PROGRAM_STANDARD_PROPERTIES,
						NULL) ;

  if (sflphone_init ())
    {

      if (eel_gconf_get_integer (SHOW_STATUSICON))
		  show_status_icon ();

      create_main_window ();

      if (eel_gconf_get_integer (SHOW_STATUSICON) && eel_gconf_get_integer (START_HIDDEN))
        {
          gtk_widget_hide (GTK_WIDGET( get_main_window() ));
          set_minimized (TRUE);
        }


      status_bar_display_account ();

      // Load the history
      sflphone_fill_history ();

      // Get the active calls and conferences at startup
      sflphone_fill_call_list ();
      sflphone_fill_conference_list ();

      // Update the GUI
      update_actions ();

      shortcuts_initialize_bindings();

      /* start the main loop */
      gtk_main ();
    }

  // Cleanly stop logging
  shutdown_logging ();

  shortcuts_destroy_bindings();

  return 0;
}

/** @mainpage SFLphone GTK+ Client Documentation
 * SFLphone GTK+ Client was started as a debuging tool for the new dbus API but
 * ended being a full featured client.
 * @section intro_sec Architecture
 * SFLphone respects the MVC principle.  Since the internal workings and the UI
 * are too different programs, dbus is used to exchange data between them.  Dbus
 * is thereby inforcing MVC by only allowing access to high level functions and data.
 *
 * Therefore, when a button is clicked, a direct dbus API call should happen
 * (defined in dbus.h).  The UI should only be updated when signals are received
 * from dbus.  The call back to those signals are defined in dbus.c, but they call
 * functions in actions.h.  This makes things cleaner as one signal could have many
 * actions.
 *
 * Accounts are stored in form of a account_t in an account list with access functions
 * defined in accountlist.h.
 *
 * Calls are stored in form of a call_t in a call list with access functions defined
 * in calllist.h.
 *
 */

// This doc is for generated files that get overridden by tools.
/** @file marshaller.h
 * @brief This file contains marshallers functions for dbus signals.
 * This file is generated by glib-genmarshall.
 * Every dbus signal has to have a marshaller.  To generate a new marshaller function,
 * add its signature to the marshaller.list.  Then run :
 * <pre>glib-genmarshal --body --g-fatal-warnings marshaller.list > marshaller.c
 * glib-genmarshal --header --g-fatal-warnings marshaller.list > marshaller.h</pre>
 * to get the generated marshallers.
 * Just before connecting to the dbus signal, register the marshaller with:
 * dbus_g_object_register_marshaller().
 */

/** @file callmanager-glue.h, configurationmanager-glue.h, contactmanager-glue.h
 * @brief CallManager, ConfigurationManager and ContactManager dbus APIs.
 * These files are generated by dbus-binding-tool using the server's files named *-introspec.xml:
 * <pre>dbus-binding-tool --mode=glib-client "../../src/dbus/callmanager-introspec.xml" > callmanager-glue.h</pre>
 * <pre>dbus-binding-tool --mode=glib-client "../../src/dbus/configurationmanager-introspec.xml" > configurationmanager-glue.h</pre>
 * <pre>dbus-binding-tool --mode=glib-client "../../src/dbus/contactmanager-introspec.xml" > contactmanager-glue.h</pre>
 * These files dbus call wrapper functions to simplify access to dbus API.
 */
