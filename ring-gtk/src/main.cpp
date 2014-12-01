/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

// #include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include <QtCore/QString>
#include <QCoreApplication>
#include <QVariant>

#include "lib/callmodel.h"
#include "lib/accountlistmodel.h"
#include "lib/historymodel.h"
#include "lib/legacyhistorybackend.h"
#include "sflphone_client.h"
#include "gtkqtreemodel.h"
#include "gtkaccessproxymodel.h"
#include "lib/dbus/instancemanager.h"

#include "ringapplicationwindow.h"

static void
quit_qapp(QCoreApplication *qapp)
{
    g_debug("exiting application");
    qapp->exit();
}

// /* Loads the menu ui, aborts the program on failure */
// static GtkBuilder *uibuilder_new(const gchar *file_name)
// {
//     GtkBuilder* uibuilder = gtk_builder_new();
//     GError *error = NULL;
//     /* try local dir first */
//     gchar *ui_path = g_build_filename("../../src/", file_name, NULL);

//     if (!g_file_test(ui_path, G_FILE_TEST_EXISTS)) {
//         g_warning("Could not find \"%s\"", ui_path);
//         g_free(ui_path);
//         ui_path = NULL;
//         g_object_unref(uibuilder);
//         uibuilder = NULL;
//     }

//     /* If there is an error in parsing the UI file, the program must be aborted, as per the documentation:
//      * It’s not really reasonable to attempt to handle failures of this call.
//      * You should not use this function with untrusted files (ie: files that are not part of your application).
//      * Broken GtkBuilder files can easily crash your program,
//      * and it’s possible that memory was leaked leading up to the reported failure. */
//     if (ui_path && !gtk_builder_add_from_file(uibuilder, ui_path, &error)) {
//         g_assert(error);
//         g_warning("Error adding \"%s\" file to gtk builder: %s", ui_path, error->message);
//         g_object_unref(uibuilder);
//         uibuilder = NULL;
//     } else {
//         /* loaded file successfully */
//         g_free(ui_path);
//     }
//     return uibuilder;
// }

int
main(int argc, char *argv[])
{
    // Start GTK application
    gtk_init(&argc, &argv);
    srand(time(NULL));

    // enable debug
    g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
    g_debug("debug enabled");

    // Internationalization
    // bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    // textdomain(PACKAGE_NAME);

    g_set_application_name("Ring");
    SFLPhoneClient *client = sflphone_client_new();
    GError *err = NULL;
    if (!g_application_register(G_APPLICATION(client), NULL, &err)) {
        g_warning("Could not register application: %s", err->message);
        g_error_free(err);
        g_object_unref(client);
        return 1;
    }

    QCoreApplication *app = NULL;
    gint status = 0;

    try
    {
        app = new QCoreApplication(argc, argv);

        //dbus configuration
        CallModel::instance();
        // history
        HistoryModel::instance()->addBackend(new LegacyHistoryBackend(app),LoadOptions::FORCE_ENABLED);

        client->win = ring_application_window_new(GTK_APPLICATION(client));
        g_signal_connect_swapped(client->win, "destroy", G_CALLBACK(quit_qapp), app);

        gtk_window_present(GTK_WINDOW(client->win));

        // start app and main loop
        status = app->exec();
    }
    catch(const char * msg)
    {
        g_debug("caught error: %s\n", msg);
        g_object_unref(client);
        return 1;
    }

    g_object_unref(client);

    delete app;
    return status;
}
