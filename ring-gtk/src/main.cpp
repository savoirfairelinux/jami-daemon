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
#include "sflphone_client.h"

static Call *curr_call;

static void
call_clicked_cb(G_GNUC_UNUSED GtkWidget *call_button, GtkWidget *call_entry)
{
    // get entry text and place call
    // Call* newCall
    Call::State state;
    if (curr_call) {
        state = curr_call->state();
    } else {
        curr_call = CallModel::instance()->dialingCall();
        curr_call->setDialNumber(gtk_entry_get_text(GTK_ENTRY(call_entry)));
        state = curr_call->state();
    }

    curr_call->performAction(Call::Action::ACCEPT);
}

static void
update_call_state(GtkWidget *label_callstate, GtkWidget *button_call, GtkWidget *button_hangup)
{
    Call::State state = curr_call->state();
    switch (state) {
        case Call::State::INCOMING:
            gtk_label_set_text(GTK_LABEL(label_callstate), "incoming");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, TRUE);
            gtk_button_set_label(GTK_BUTTON(button_call), "answer");
            break;
        case Call::State::RINGING:
            gtk_label_set_text(GTK_LABEL(label_callstate), "ringing");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::INITIALIZATION:
            gtk_label_set_text(GTK_LABEL(label_callstate), "init");
            gtk_widget_set_sensitive(button_hangup, FALSE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::CURRENT:
            gtk_label_set_text(GTK_LABEL(label_callstate), "current");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::DIALING:
            gtk_label_set_text(GTK_LABEL(label_callstate), "dialing");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, TRUE);
            break;
        case Call::State::HOLD:
            gtk_label_set_text(GTK_LABEL(label_callstate), "hold");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::FAILURE:
            gtk_label_set_text(GTK_LABEL(label_callstate), "failure");
            gtk_widget_set_sensitive(button_hangup, FALSE);
            gtk_widget_set_sensitive(button_call, TRUE);
            gtk_button_set_label(GTK_BUTTON(button_call), "call");
            break;
        case Call::State::BUSY:
            gtk_label_set_text(GTK_LABEL(label_callstate), "busy");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::TRANSFERRED:
            gtk_label_set_text(GTK_LABEL(label_callstate), "transfered");
            gtk_widget_set_sensitive(button_hangup, FALSE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::TRANSF_HOLD:
            gtk_label_set_text(GTK_LABEL(label_callstate), "transfer hold");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::OVER:
            gtk_label_set_text(GTK_LABEL(label_callstate), "over");
            gtk_widget_set_sensitive(button_hangup, FALSE);
            gtk_widget_set_sensitive(button_call, TRUE);
            gtk_button_set_label(GTK_BUTTON(button_call), "call");
            break;
        case Call::State::ERROR:
            gtk_label_set_text(GTK_LABEL(label_callstate), "error");
            gtk_widget_set_sensitive(button_hangup, FALSE);
            gtk_widget_set_sensitive(button_call, TRUE);
            break;
        case Call::State::CONFERENCE:
            gtk_label_set_text(GTK_LABEL(label_callstate), "conference");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::CONFERENCE_HOLD:
            gtk_label_set_text(GTK_LABEL(label_callstate), "conference hold");
            gtk_widget_set_sensitive(button_hangup, TRUE);
            gtk_widget_set_sensitive(button_call, FALSE);
            break;
        case Call::State::__COUNT:
            break;
        default:
            gtk_widget_set_sensitive(button_hangup, FALSE);
            gtk_widget_set_sensitive(button_call, TRUE);
            g_debug("unknown call state");
    }
    g_debug("state changed");
}

static void
hangup_cb(G_GNUC_UNUSED GtkWidget *button_hangup, G_GNUC_UNUSED gpointer *user_data)
{
    curr_call->performAction(Call::Action::REFUSE);
}

/* Loads the menu ui, aborts the program on failure */
static GtkBuilder *uibuilder_new(const gchar *file_name)
{
    GtkBuilder* uibuilder = gtk_builder_new();
    GError *error = NULL;
    /* try local dir first */
    gchar *ui_path = g_build_filename("../../src/", file_name, NULL);

    if (!g_file_test(ui_path, G_FILE_TEST_EXISTS)) {
        g_warning("Could not find \"%s\"", ui_path);
        g_free(ui_path);
        ui_path = NULL;
        g_object_unref(uibuilder);
        uibuilder = NULL;
    }

    /* If there is an error in parsing the UI file, the program must be aborted, as per the documentation:
     * It’s not really reasonable to attempt to handle failures of this call.
     * You should not use this function with untrusted files (ie: files that are not part of your application).
     * Broken GtkBuilder files can easily crash your program,
     * and it’s possible that memory was leaked leading up to the reported failure. */
    if (ui_path && !gtk_builder_add_from_file(uibuilder, ui_path, &error)) {
        g_assert(error);
        g_warning("Error adding \"%s\" file to gtk builder: %s", ui_path, error->message);
        g_object_unref(uibuilder);
        uibuilder = NULL;
    } else {
        /* loaded file successfully */
        g_free(ui_path);
    }
    return uibuilder;
}


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

    GtkBuilder *uibuilder = uibuilder_new("ring_main.ui");
    if (!uibuilder) {
        g_object_unref(client);
        return 1;
    }

    client->win = GTK_WIDGET(gtk_builder_get_object(uibuilder, "applicationwindow_main"));
    GtkWidget *entry_call_uri = GTK_WIDGET(gtk_builder_get_object(uibuilder, "entry_call_uri"));
    GtkWidget *button_call = GTK_WIDGET(gtk_builder_get_object(uibuilder, "button_call"));
    GtkWidget *button_hangup = GTK_WIDGET(gtk_builder_get_object(uibuilder, "button_hangup"));
    g_signal_connect(G_OBJECT(button_hangup), "clicked", G_CALLBACK(hangup_cb), NULL);
    GtkWidget *label_callstate = GTK_WIDGET(gtk_builder_get_object(uibuilder, "label_callstate"));
    g_signal_connect(G_OBJECT(button_call), "clicked", G_CALLBACK(call_clicked_cb), entry_call_uri);

    gtk_window_present(GTK_WINDOW(client->win));

    QCoreApplication *app = NULL;

    gint status = 0;

    curr_call = NULL;

    try
    {
        app = new QCoreApplication(argc, argv);
        //dbus configuration
        CallModel::instance();

        // connect signals
        QObject::connect(
            CallModel::instance(),
            &CallModel::callStateChanged,
            [=](void) { update_call_state(label_callstate, button_call, button_hangup);}
        );

        // account list
        int num_accounts = AccountListModel::instance()->rowCount();
        g_debug("number accounts: %d", num_accounts);
        qDebug() << "account alias: " << AccountListModel::instance()->data(AccountListModel::instance()->index(0, Account::Role::Alias).toString();

        // start app and main loop
        status = app->exec();
    }
    catch(const char * msg)
    {
        g_debug("caught error: %s\n", msg);
        g_object_unref(client);
        return 1;
    }

    // update_call_state();

    // QObject::connect(CallModel::instance(), SIGNAL(callStateChanged(Call*,Call::State)), NULL, SLOT(update_call_state()));

    // start app and main loop
    // status = app->exec();
    // gint status = g_application_run(G_APPLICATION(client), argc, argv);

    g_object_unref(client);

    delete app;
    return status;
}
