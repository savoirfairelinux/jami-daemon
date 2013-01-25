/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#include "actions.h"
#include "uimanager.h"
#include "calllist.h"
#include "config.h"
#include "logger.h"
#include "dbus/dbus.h"
#include "statusicon.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "mainwindow.h"
#include "sflphone_client.h"
#include "shortcuts.h"
#include "history.h"

static volatile sig_atomic_t interrupted;

static void
signal_handler(int code)
{
    // Unset signal handlers
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    printf("Caught signal %s, quitting...\n", strsignal(code));
    interrupted = 1;
}

static gboolean
check_interrupted(gpointer data)
{
    if (interrupted) {
        sflphone_quit(TRUE, data);
        return FALSE;
    }
    return TRUE;
}

void update_schema_dir(const gchar *argv_0)
{
    gchar *current_dir = g_path_get_dirname(argv_0);
    gchar *updated_data_dirs = g_strdup_printf("%s/../data", current_dir);
    g_free(current_dir);
    const gboolean overwrite = FALSE;
    g_setenv("GSETTINGS_SCHEMA_DIR", updated_data_dirs, overwrite);
    g_free(updated_data_dirs);
}

int
main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Must be called prior to any GLib calls (i.e. in update_schema_dir) */
    g_type_init();

    /* Tell glib to look for our schema in gnome/data in case SFLphone is not
     * installed. We have to do this early for it to work properly for older
     * versions of GLib. */
    update_schema_dir(argv[0]);

    // Start GTK application
    gtk_init(&argc, &argv);

    g_print("%s %s\n", PACKAGE, VERSION);
    g_print("\nCopyright (c) 2005 - 2012 Savoir-faire Linux Inc.\n\n");
    g_print("This is free software.  You may redistribute copies of it under the terms of\n" \
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

    srand(time(NULL));

    // Internationalization
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    g_set_application_name("SFLphone");
    SFLPhoneClient *client = sflphone_client_new();
    g_timeout_add(1000, check_interrupted, client);

    GError *error = NULL;
    if (!sflphone_init(&error, client)) {
        ERROR("%s", error->message);
        GtkWidget *dialog = gtk_message_dialog_new(
                                NULL,
                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                "Unable to initialize.\nMake sure the daemon is running.\nError: %s",
                                error->message);

        gtk_window_set_title(GTK_WINDOW(dialog), _("SFLphone Error"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        g_error_free(error);
        return 1;
    }

    create_main_window(client);
    gtk_application_add_window(GTK_APPLICATION(client), GTK_WINDOW(client->win));

    const gboolean show_status = g_settings_get_boolean(client->settings, "show-status-icon");
    if (show_status)
        show_status_icon(client);

    status_bar_display_account();

    sflphone_fill_history_lazy();
    sflphone_fill_conference_list(client);
    sflphone_fill_call_list();

    // Update the GUI
    update_actions(client);

    shortcuts_initialize_bindings(client);

    g_application_run(G_APPLICATION(client), argc, argv);

    codecs_unload();
    shortcuts_destroy_bindings();

    g_object_unref(client);

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
