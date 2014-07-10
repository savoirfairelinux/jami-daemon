/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Copyright (C) 2001-2007 Bastien Nocera <hadess@hadess.net>
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include "sflphone_client.h"
#include "sflphone_options.h"
#include "actions.h"
#include "statusicon.h"
#include "shortcuts.h"
#include "mainwindow.h"
#ifdef SFL_VIDEO
#include "video/video_widget.h"
#endif

G_DEFINE_TYPE(SFLPhoneClient, sflphone_client, GTK_TYPE_APPLICATION);

static int
sflphone_client_command_line_handler(G_GNUC_UNUSED GApplication *application,
                                     GApplicationCommandLine *cmdline,
                                     SFLPhoneClient *client)
{
    gint argc;
    gchar **argv = g_application_command_line_get_arguments(cmdline, &argc);
    GOptionContext *context = sflphone_options_get_context();
    g_option_context_set_help_enabled(context, TRUE);
    GError *error = NULL;
    if (g_option_context_parse(context, &argc, &argv, &error) == FALSE) {
        g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
                error->message, argv[0]);
        g_error_free(error);
        g_option_context_free(context);
        return 1;
    }

    g_option_context_free(context);

    /* Override theme since we don't have appropriate icons for a dark them (yet) */
	GtkSettings *gtk_settings = gtk_settings_get_default();
	g_object_set(G_OBJECT(gtk_settings), "gtk-application-prefer-dark-theme",
                 FALSE, NULL);

    if (!sflphone_init(&error, client)) {
        g_warning("%s", error->message);
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                _("Unable to initialize.\nMake sure the daemon is running.\nError: %s"),
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

    g_strfreev(argv);
    return 0;
}

SFLPhoneClient *
sflphone_client_new()
{
    SFLPhoneClient *client = g_object_new(sflphone_client_get_type(),
            "application-id", "org.sfl.SFLphone",
            "flags", G_APPLICATION_HANDLES_COMMAND_LINE, NULL);
    return client;
}

static void
sflphone_client_init(SFLPhoneClient *self)
{
    self->settings = g_settings_new(SFLPHONE_GSETTINGS_SCHEMA);

#ifdef SFL_VIDEO
    self->video = video_widget_new();
#endif

    self->win = 0;
    g_signal_connect(G_OBJECT(self), "command-line", G_CALLBACK(sflphone_client_command_line_handler), self);
}

static void
sflphone_client_dispose(GObject *object)
{
    SFLPhoneClient *self = SFLPHONE_CLIENT(object);
    /*
     * Unref all members to which self owns a reference.
     */

    /* dispose might be called multiple times, so we must guard against
     * calling g_object_unref() on an invalid GObject.
     */
      if (self->settings) {
          g_object_unref(self->settings);
          self->settings = NULL;
      }

      /* Chain up to the parent class */
      G_OBJECT_CLASS(sflphone_client_parent_class)->dispose(object);
}

static void
sflphone_client_finalize(GObject *object)
{
    /* Chain up to the parent class */
    G_OBJECT_CLASS(sflphone_client_parent_class)->finalize(object);
}

static void
sflphone_client_class_init(SFLPhoneClientClass *klass)
{
    GObjectClass *object_class = (GObjectClass *) klass;

    object_class->dispose = sflphone_client_dispose;
    object_class->finalize = sflphone_client_finalize;
    /* TODO: add properties, signals, and signal handlers */
}
