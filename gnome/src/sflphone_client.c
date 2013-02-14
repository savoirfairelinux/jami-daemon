/*
 *  Copyright (C) 2012 Savoir-Faire Linux Inc.
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

#include "sflphone_client.h"

G_DEFINE_TYPE(SFLPhoneClient, sflphone_client, GTK_TYPE_APPLICATION);

static int
sflphone_client_command_line_handler(G_GNUC_UNUSED GApplication *application,
                                     GApplicationCommandLine *cmdline)
{
    // FIXME: replace with GLib logging
    gint argc;
    gchar **argv = g_application_command_line_get_arguments(cmdline, &argc);
    for (gint i = 0; i < argc; i++)
        if (g_strcmp0(argv[i], "--debug") == 0) {}
            ;//set_log_level(LOG_DEBUG);

    g_strfreev (argv);
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
    self->win = 0;
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
    GApplicationClass *application_class = G_APPLICATION_CLASS(klass);
    GObjectClass *object_class = (GObjectClass *) klass;

    object_class->dispose = sflphone_client_dispose;
    object_class->finalize = sflphone_client_finalize;
    application_class->command_line = sflphone_client_command_line_handler;
    /* TODO: add properties, signals, and signal handlers */
}
