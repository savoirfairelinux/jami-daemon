/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include <glib/gi18n.h>
#include "str_utils.h"
#include "hooks-config.h"
#include "dbus.h"

URLHook_Config *_urlhook_config;

GtkWidget *field, *command, *prefix, *url;

void hooks_load_parameters(URLHook_Config** settings)
{

    GHashTable *_params = NULL;
    URLHook_Config *_settings;

    // Allocate a struct
    _settings = g_new0(URLHook_Config, 1);

    // Fetch the settings from D-Bus
    _params = (GHashTable*) dbus_get_hook_settings();

    if (_params == NULL) {
        _settings->sip_field = DEFAULT_SIP_URL_FIELD;
        _settings->command = DEFAULT_URL_COMMAND;
        _settings->sip_enabled = "false";
        _settings->iax2_enabled = "false";
        _settings->phone_number_enabled = "false";
        _settings->phone_number_prefix = "";
    } else {
        _settings->sip_field = (gchar*)(g_hash_table_lookup(_params, URLHOOK_SIP_FIELD));
        _settings->command = (gchar*)(g_hash_table_lookup(_params, URLHOOK_COMMAND));
        _settings->sip_enabled = (gchar*)(g_hash_table_lookup(_params, URLHOOK_SIP_ENABLED));
        _settings->iax2_enabled = (gchar*)(g_hash_table_lookup(_params, URLHOOK_IAX2_ENABLED));
        _settings->phone_number_enabled = (gchar*)(g_hash_table_lookup(_params, PHONE_NUMBER_HOOK_ENABLED));
        _settings->phone_number_prefix = (gchar*)(g_hash_table_lookup(_params, PHONE_NUMBER_HOOK_ADD_PREFIX));
    }

    *settings = _settings;
}


void hooks_save_parameters(SFLPhoneClient *client)
{
    GHashTable *params = g_hash_table_new(NULL, g_str_equal);
    g_hash_table_replace(params, (gpointer) URLHOOK_SIP_FIELD,
                         g_strdup((gchar *) gtk_entry_get_text(GTK_ENTRY(field))));
    g_hash_table_replace(params, (gpointer) URLHOOK_COMMAND,
                         g_strdup((gchar *) gtk_entry_get_text(GTK_ENTRY(command))));
    g_hash_table_replace(params, (gpointer) URLHOOK_SIP_ENABLED,
                         (gpointer) g_strdup(_urlhook_config->sip_enabled));
    g_hash_table_replace(params, (gpointer) URLHOOK_IAX2_ENABLED,
                         (gpointer) g_strdup(_urlhook_config->iax2_enabled));
    g_hash_table_replace(params, (gpointer) PHONE_NUMBER_HOOK_ENABLED,
                         (gpointer) g_strdup(_urlhook_config->phone_number_enabled));
    g_hash_table_replace(params, (gpointer) PHONE_NUMBER_HOOK_ADD_PREFIX,
                         g_strdup((gchar *) gtk_entry_get_text(GTK_ENTRY(prefix))));

    dbus_set_hook_settings(params);

    // Decrement the reference count
    g_hash_table_unref(params);

    g_settings_set_string(client->settings, "messaging-url-command", gtk_entry_get_text(GTK_ENTRY(url)));
}

static void sip_enabled_cb(GtkWidget *widget)
{

    guint check;

    check = (guint) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (check)
        _urlhook_config->sip_enabled="true";
    else
        _urlhook_config->sip_enabled="false";
}

static void iax2_enabled_cb(GtkWidget *widget)
{

    guint check;

    check = (guint) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (check)
        _urlhook_config->iax2_enabled="true";
    else
        _urlhook_config->iax2_enabled="false";
}

static void phone_number_enabled_cb(GtkWidget *widget)
{

    guint check;

    check = (guint) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (check) {
        _urlhook_config->phone_number_enabled="true";
        gtk_widget_set_sensitive(GTK_WIDGET(prefix), TRUE);
    } else {
        _urlhook_config->phone_number_enabled="false";
        gtk_widget_set_sensitive(GTK_WIDGET(prefix), FALSE);
    }
}


GtkWidget*
create_hooks_settings(SFLPhoneClient *client)
{
    GtkWidget *ret, *frame, *label, *widg;

    // Load the user value
    hooks_load_parameters(&_urlhook_config);

    ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    GtkWidget *grid;
    gnome_main_section_new_with_grid(_("URL Argument"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    gchar *message = "<small>Custom commands on incoming calls with URL. %s will be replaced with the passed URL.</small>";
    GtkWidget *info_bar = gnome_info_bar(message, GTK_MESSAGE_INFO);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), info_bar, 0, 0, 2, 1);

    widg = gtk_check_button_new_with_mnemonic(_("Trigger on specific _SIP header"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widg), utf8_case_equal(_urlhook_config->sip_enabled, "true"));
    g_signal_connect(G_OBJECT(widg) , "clicked" , G_CALLBACK(sip_enabled_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid), widg, 0, 2, 1, 1);

    field = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(field), _urlhook_config->sip_field);
    gtk_grid_attach(GTK_GRID(grid), field, 1, 2, 1, 1);

    widg = gtk_check_button_new_with_mnemonic(_("Trigger on _IAX2 URL"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widg), utf8_case_equal(_urlhook_config->iax2_enabled, "true"));
    g_signal_connect(G_OBJECT(widg) , "clicked" , G_CALLBACK(iax2_enabled_cb), NULL);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), widg, 0, 3, 2, 1);

    label = gtk_label_new_with_mnemonic(_("Command to _run"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.05, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);
    command = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), command);
    gtk_entry_set_text(GTK_ENTRY(command), _urlhook_config->command);
    gtk_grid_attach(GTK_GRID(grid), command, 1, 4, 1, 1);

    gnome_main_section_new_with_grid(_("Phone number rewriting"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    widg = gtk_check_button_new_with_mnemonic(_("_Prefix dialed numbers with"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widg), utf8_case_equal(_urlhook_config->phone_number_enabled, "true"));
    g_signal_connect(G_OBJECT(widg) , "clicked" , G_CALLBACK(phone_number_enabled_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid), widg, 0, 0, 1, 1);

    prefix = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefix);
    gtk_entry_set_text(GTK_ENTRY(prefix), _urlhook_config->phone_number_prefix);
    gtk_widget_set_sensitive(GTK_WIDGET(prefix), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widg)));
    gtk_grid_attach(GTK_GRID(grid), prefix, 1, 0, 1, 1);

    gnome_main_section_new_with_grid(_("Messaging"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    label = gtk_label_new_with_mnemonic(_("Open URL in"));
    url   = gtk_entry_new();

    gchar *url_command = g_settings_get_string(client->settings, "messaging-url-command");
    if (url_command && *url_command) {
        gtk_entry_set_text(GTK_ENTRY(url), url_command);
        g_free(url_command);
    } else
        gtk_entry_set_text(GTK_ENTRY(url), "xdg-open");
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), url);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), url, 1, 4, 1, 1);

    gtk_widget_show_all(ret);

    return ret;
}
