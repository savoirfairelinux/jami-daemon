/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Patrick Keroulas  <patrick.keroulas@savoirfairelinux.com>
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


#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "uibuilder.h"
#include "uimanager.h" //FIXME: this is tmp for callbacks
#include "str_utils.h"
#include "actions.h"
#include "preferencesdialog.h"
#include "mainwindow.h"
#include "assistant.h"
#include "statusicon.h"
#include "config/audioconf.h"
#include "statusicon.h"
#include "accountlist.h"
#include "sliders.h"
#include "account_schema.h"

#include "contacts/addrbookfactory.h"
#include "contacts/calltab.h"
#include "config/addressbook-config.h"
#include "config/accountlistconfigdialog.h"

#include "messaging/message_tab.h"
#include "icons/icon_theme.h"
#include "dbus/dbus.h"

#ifdef SFL_PRESENCE
#include "presencewindow.h"
#include "presence.h"
#endif


#define GW(builder, name) GTK_WIDGET(gtk_builder_get_object(builder, name))

static GtkToolItem * pickUpWidget_;
static GtkToolItem * newCallWidget_;
static GtkToolItem * hangUpWidget_;
#define G_ACTION_CALL_TEST   "call-test"
#define G_ACTION_CALL_NEW    "call-new"
#define G_ACTION_CALL_PICKUP  "call-pickup"
#define G_ACTION_CALL_HANGUP  "call-hangup"
#define G_ACTION_CALL_ONHOLD   "call-onhold"
#define G_ACTION_CALL_OFFHOLD   "call-offhold"
#define G_ACTION_CALL_IM   "call-instant-messaging"
#define G_ACTION_CALL_SCREEN_SHARING   "call-screen-sharing"
#define G_ACTION_CALL_ACCOUNT   "call-account-assistant"
#define G_ACTION_CALL_VOICEMAIL   "call-voicemail"
#define G_ACTION_CALL_CLOSE   "call-close"
#define G_ACTION_CALL_QUIT   "call-quit"



static gboolean
is_non_empty(const char *str)
{
    return str && strlen(str) > 0;
}

/* --------------------- CALLBACK FUNCTIONS ------------------------*/

static void
call_dial_cb (G_GNUC_UNUSED GSimpleAction *simple,
         G_GNUC_UNUSED GVariant *parameter,
         gpointer client)
{
    g_debug("New call button pressed");
    sflphone_new_call(client);
}

static void
call_pickup_cb (G_GNUC_UNUSED GSimpleAction *simple,
        G_GNUC_UNUSED GVariant *parameter,
        gpointer client)
{
    if (calllist_get_size(current_calls_tab) > 0) {
        sflphone_pick_up(client);
    } else if (calllist_get_size(active_calltree_tab) > 0) {
        callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);

        if (selectedCall) {
            callable_obj_t *new_call = create_new_call(CALL, CALL_STATE_DIALING, "", "", "",
                                       selectedCall->_peer_number);
            calllist_add_call(current_calls_tab, new_call);
            calltree_add_call(current_calls_tab, new_call, NULL);
            sflphone_place_call(new_call, client);
            calltree_display(current_calls_tab, client);
        } else {
            sflphone_new_call(client);
            calltree_display(current_calls_tab, client);
        }
    } else {
        sflphone_new_call(client);
        calltree_display(current_calls_tab, client);
    }
}

static void
call_hangup_cb(G_GNUC_UNUSED GSimpleAction *simple,
         G_GNUC_UNUSED GVariant *parameter,
         gpointer client)
{
    g_debug("Hang up button pressed(call)");
    /*
     * [#3020]	Restore the record toggle button
     *			We set it to FALSE, as when we hang up a call, the recording is stopped.
     */

    sflphone_hang_up(client);
}

/* ------------------------------ TOOLBAR -------------------------------- */


static GtkToolItem *
toolbar_item_create (const gchar *action_name, const gchar *icon_name, const gchar *label, const gchar *tooltip_text)
{
  GtkWidget *widget = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
  GtkToolItem *item = gtk_tool_button_new (widget, label);

  gtk_widget_show (widget);
  gtk_tool_item_set_tooltip_text (item, tooltip_text);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name);
  gtk_widget_show (GTK_WIDGET (item));

  return item;
}

/* ------------------------------ ACTIONS -------------------------------- */


static void
update_toolbar_for_call(callable_obj_t *selectedCall, gboolean instant_messaging_enabled, SFLPhoneClient *client)
{
    int pos = 0;

    if (selectedCall == NULL) {
        g_warning("Selected call is NULL while updating toolbar");
        return;
    }

    g_debug("Update toolbar for call %s", selectedCall->_callID);

    // update icon in systray
    // TODO: restore this
//     show_status_hangup_icon(client);
/*
    gtk_action_set_sensitive(copyAction_, TRUE);
    GtkWidget *toolbar = client->toolbar;
*/

    switch (selectedCall->_state) {
        case CALL_STATE_INCOMING:
        {
                g_debug("Call State Incoming");
                /* Replace the dial button with the hangup button */
                gtk_widget_hide(uibuilder_get_widget(client,"toolbutton_dial"));
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_pickup"));
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_hangup"));
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_dial"), FALSE);
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_pickup"), TRUE);
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_hangup"), TRUE);
                break;
        }
        case CALL_STATE_HOLD:
        {
                g_debug("Call State Hold");
                /*
                gtk_widget_set_sensitive(holdMenu_, TRUE);
                gtk_widget_set_sensitive(offHoldToolbar_, TRUE);
                gtk_widget_set_sensitive(newCallWidget_, TRUE);

                // Replace the hold button with the off-hold button
                pos = 1;
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                add_to_toolbar(toolbar, offHoldToolbar_, pos++);

                if (instant_messaging_enabled) {
                    gtk_action_set_sensitive(imAction_, TRUE);
                    add_to_toolbar(toolbar, imToolbar_, pos++);
                }
                */
                break;
        }
        case CALL_STATE_RINGING:
        {
                g_debug("Call State Ringing");
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_pickup"));
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_hangup"));
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_pickup"), TRUE);
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_hangup"), TRUE);
                break;
        }
        case CALL_STATE_DIALING:
        {
                g_debug("Call State Dialing");
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_pickup"));
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_hangup"));
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_pickup"), TRUE);
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_hangup"), TRUE);

                // FIXME: ??
                /*
                if (calltab_has_name(active_calltree_tab, CURRENT_CALLS))
                    gtk_action_set_sensitive(hangUpAction_, TRUE);

                remove_from_toolbar(toolbar, newCallWidget_);
                pos = 0;
                add_to_toolbar(toolbar, pickUpWidget_, pos++);
                */

                if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
//                     add_to_toolbar(toolbar, hangUpWidget_, pos++);
                    main_window_hide_playback_scale();
                } else if (calltab_has_name(active_calltree_tab, HISTORY)) {
                    main_window_show_playback_scale();
                    if (is_non_empty(selectedCall->_recordfile))
                        main_window_set_playback_scale_sensitive();
                    else
                        main_window_set_playback_scale_unsensitive();
                } else {
                    main_window_hide_playback_scale();
                }
                break;
        }
        case CALL_STATE_CURRENT:
        {
                g_debug("Call State Current");
                gtk_widget_show(uibuilder_get_widget(client,"toolbutton_pickup"));
                gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_pickup"), TRUE);
                /*
#ifdef SFL_VIDEO
            const gboolean video_enabled = is_video_call(selectedCall);
#endif

                g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                g_signal_handler_block(recordWidget_, recordButtonConnId_);
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                gtk_action_set_sensitive(recordAction_, TRUE);
                gtk_widget_set_sensitive(holdMenu_, TRUE);
                gtk_widget_set_sensitive(holdToolbar_, TRUE);
                gtk_widget_set_sensitive(transferToolbar_, TRUE);
                if (instant_messaging_enabled)
                    gtk_action_set_sensitive(imAction_, TRUE);
#ifdef SFL_VIDEO
                if (video_enabled)
                    gtk_action_set_sensitive(screenshareAction_, TRUE);
#endif
                pos = 1;
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                add_to_toolbar(toolbar, holdToolbar_, pos++);
                add_to_toolbar(toolbar, transferToolbar_, pos++);
                add_to_toolbar(toolbar, recordWidget_, pos++);
                if (instant_messaging_enabled)
                    add_to_toolbar(toolbar, imToolbar_, pos++);
#ifdef SFL_VIDEO
                if (video_enabled)
                    add_to_toolbar(toolbar, screenshareToolbar_, pos++);
#endif

                gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), FALSE);
                gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(recordWidget_), dbus_get_is_recording(selectedCall));

                g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                g_signal_handler_unblock(recordWidget_, recordButtonConnId_);
                */
                break;
        }

        case CALL_STATE_BUSY:
        case CALL_STATE_FAILURE:
        {
                pos = 1;
                g_debug("Call State Busy/Failure");
                /*
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                */
                break;
        }
        case CALL_STATE_TRANSFER:
        {
                /*
                pos = 1;
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                gtk_widget_set_sensitive(holdMenu_, TRUE);
                gtk_widget_set_sensitive(holdToolbar_, TRUE);
                gtk_widget_set_sensitive(transferToolbar_, TRUE);

                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                add_to_toolbar(toolbar, transferToolbar_, pos++);
                g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), TRUE);
                g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                */
                break;
        }
        default:
            g_warning("Unknown state in action uibuilderupdate!");
            break;
    }
}


void
uibuilder_action_update(SFLPhoneClient *client)
{
    g_warning("===================================");

    // hide all
//     gtk_widget_hide(uibuilder_get_widget(client,"toolbutton_dial"));
    gtk_widget_hide(uibuilder_get_widget(client,"toolbutton_pickup"));
    gtk_widget_hide(uibuilder_get_widget(client,"toolbutton_hangup"));

//     gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_dial"), FALSE);
    gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_pickup"), FALSE);
    gtk_widget_set_sensitive(uibuilder_get_widget(client,"menu_hangup"), FALSE);

    callable_obj_t * selectedCall = NULL;
    conference_obj_t * selectedConf = NULL;

    const gboolean instant_messaging_enabled = g_settings_get_boolean(client->settings, "instant-messaging-enabled");

    if (!calllist_empty(active_calltree_tab)) {
        selectedCall = calltab_get_selected_call(active_calltree_tab);
        selectedConf = calltab_get_selected_conf(active_calltree_tab);
    }

    if (selectedCall) {
        update_toolbar_for_call(selectedCall, instant_messaging_enabled, client);
    }
    /*else if (selectedConf) {
        update_toolbar_for_conference(selectedConf, instant_messaging_enabled, client);
    } else {
        // update icon in systray
        hide_status_hangup_icon();

        if (account_list_get_size() > 0 && current_account_has_mailbox()) {
            add_to_toolbar(toolbar, voicemailToolbar_, -1);
            update_voicemail_status();
        }
    }
    */
}

/*-------------------------------- BUILDER --------------------------------*/

GtkWidget*
uibuilder_get_widget(SFLPhoneClient *client, const gchar* widget_name)
{
     return GTK_WIDGET(gtk_builder_get_object (client->builder, widget_name));
}

GtkBuilder *
uibuilder_new(const gchar* filename)
{
    /* Create an accel group for window's shortcuts */
    gchar *path = g_build_filename(SFLPHONE_UIDIR_UNINSTALLED, filename, NULL);
    guint builder_id;
    GError *error = NULL;
    GtkBuilder *builder = gtk_builder_new();

    if (g_file_test(path, G_FILE_TEST_EXISTS))
        builder_id = gtk_builder_add_from_file(builder, path, &error);
    else {
        g_free(path);
        path = g_build_filename(SFLPHONE_UIDIR, filename, NULL);

        if (!g_file_test(path, G_FILE_TEST_EXISTS))
            goto fail;

        builder_id = gtk_builder_add_from_file(builder, path, &error);
    }

    if ((error) || !builder_id)
        goto fail;

    g_free(path);

    //TODO move this
    active_calltree_tab = current_calls_tab;

    g_debug("UI builder created.");
    return builder;

fail:
    if (error)
        g_error_free(error);

    g_free(path);

    return NULL;
}


void
uibuilder_build(SFLPhoneClient *client)
{
    client->win = uibuilder_get_widget(client, "window");
    gtk_application_add_window(GTK_APPLICATION(client), GTK_WINDOW(client->win));

    /* Toolbar */
    // FIXME: doesn't work when sflphone stock-id is specified in the glade file
    client->toolbar = uibuilder_get_widget(client, "toolbar");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(uibuilder_get_widget(client, "toolbutton_dial")), GTK_STOCK_DIAL);
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(uibuilder_get_widget(client, "toolbutton_pickup")), GTK_STOCK_PICKUP);
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(uibuilder_get_widget(client, "toolbutton_hangup")), GTK_STOCK_HANGUP);

    /* Signals */
    // FIXME need "-Wl,--export-dynamic" flag or "-rdynamic" to avoid all the connect
//     gtk_builder_connect_signals(client->builder, client->win);

    g_signal_connect(G_OBJECT(uibuilder_get_widget(client, "menu_dial")), "activate", G_CALLBACK(call_dial_cb), client);
    g_signal_connect(G_OBJECT(uibuilder_get_widget(client, "menu_pickup")), "activate", G_CALLBACK(call_pickup_cb), client);
    g_signal_connect(G_OBJECT(uibuilder_get_widget(client, "menu_hangup")), "activate", G_CALLBACK(call_hangup_cb), client);
    g_signal_connect(G_OBJECT(uibuilder_get_widget(client, "toolbutton_dial")), "clicked", G_CALLBACK(call_dial_cb), client);
    g_signal_connect(G_OBJECT(uibuilder_get_widget(client, "toolbutton_pickup")), "clicked", G_CALLBACK(call_pickup_cb), client);
    g_signal_connect(G_OBJECT(uibuilder_get_widget(client, "toolbutton_hangup")), "clicked", G_CALLBACK(call_hangup_cb), client);
}
