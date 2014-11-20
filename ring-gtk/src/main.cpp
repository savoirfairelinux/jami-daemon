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

#include "lib/callmodel.h"
#include "sflphone_client.h"

static void
call_clicked_cb(G_GNUC_UNUSED GtkWidget *call_button, GtkWidget *call_entry)
{
    // get entry text and place call
    Call* newCall = CallModel::instance()->dialingCall();
    newCall->setDialNumber(gtk_entry_get_text(GTK_ENTRY(call_entry)));
    newCall->performAction(Call::Action::ACCEPT);
}

int
main(int argc, char *argv[])
{
    // Start GTK application
    gtk_init(&argc, &argv);
    srand(time(NULL));

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

    // create test window
    client->win = gtk_application_window_new(GTK_APPLICATION(client));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *number_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), number_entry, FALSE, FALSE, 0);
    GtkWidget *call_button = gtk_button_new_with_label("call");
    g_signal_connect(G_OBJECT(call_button), "clicked", G_CALLBACK(call_clicked_cb), number_entry);
    gtk_box_pack_start(GTK_BOX(vbox), call_button, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(client->win), vbox);
    gtk_widget_show_all(client->win);
    gtk_application_add_window(GTK_APPLICATION(client), GTK_WINDOW(client->win));
    gtk_window_present(GTK_WINDOW(client->win));

    QCoreApplication *app = new QCoreApplication(argc, argv);

    gint status = 0;

    try
    {
        //dbus configuration
        CallModel::instance();
    }
    catch(const char * msg)
    {
        printf("caught error: %s\n", msg);
    }

    // start app and main loop
    status = app->exec();
    // gint status = g_application_run(G_APPLICATION(client), argc, argv);

    g_object_unref(client);

    delete app;
    return status;
}
