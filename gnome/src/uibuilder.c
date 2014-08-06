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


static GtkToolItem * pickUpWidget_;
static GtkToolItem * newCallWidget_;
static GtkToolItem * hangUpWidget_;
/*
static GtkToolItem * holdMenu_;
static GtkToolItem * holdToolbar_;
static GtkToolItem * offHoldToolbar_;
static GtkToolItem * transferToolbar_;
static GtkToolItem * recordWidget_;
static GtkToolItem * voicemailToolbar_;
static GtkToolItem * imToolbar_;
static GtkToolItem * screenshareToolbar_;
*/
/*
enum
{
    TOOLBAR_ITEM_CALL_NEW,
    TOOLBAR_ITEM_PICKUP,
    TOOLBAR_ITEM_HANGUP,
    N_TOOLBAR_ITEM
};
GtkToolItem *toolbar_items[N_TOOLBAR_ITEM];
*/


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
/*
#define G_ACTION_CALL_TEST   "edit-copy"
#define G_ACTION_CALL_TEST   "edit-paste"
#define G_ACTION_CALL_TEST   "edit-clear-history"
#define G_ACTION_CALL_TEST   "edit-accounts"
#define G_ACTION_CALL_TEST   "edit-prefences"
#define G_ACTION_CALL_TEST   "view-help"
#define G_ACTION_CALL_TEST   "view-help-content"
#define G_ACTION_CALL_TEST   "view-about"
*/

static gboolean
is_non_empty(const char *str)
{
    return str && strlen(str) > 0;
}

/* ---------------------------------------------------------------- */
// CALLBACK FUNCTIONS

static void
call_new_call (G_GNUC_UNUSED GSimpleAction *simple,
        G_GNUC_UNUSED GVariant *parameter,
        SFLPhoneClient *client)
{
    g_debug("New call button pressed");
    sflphone_new_call(client);
}

static void
call_pick_up(G_GNUC_UNUSED GSimpleAction *simple,
        G_GNUC_UNUSED GVariant *parameter,
        SFLPhoneClient *client)
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
call_hang_up(G_GNUC_UNUSED GSimpleAction *simple,
        G_GNUC_UNUSED GVariant *parameter,
        SFLPhoneClient *client)
{
    g_debug("Hang up button pressed(call)");
    /*
     * [#3020]	Restore the record toggle button
     *			We set it to FALSE, as when we hang up a call, the recording is stopped.
     */

    sflphone_hang_up(client);
}

/* -------------------------------------------------------------- */
// TOOLBAR:

static void
remove_from_toolbar(GtkWidget *toolbar, GtkToolItem *item)
{
    g_object_ref(item);
    /* We must ensure that a item is a child of a container
     * before removing it. */
    if (gtk_widget_get_parent(GTK_WIDGET(item)) == toolbar)
        gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(item));
}

/* Inserts an item in a toolbar at a given position.
 * If the index exceeds the number of elements, the item is simply appended */
static void add_to_toolbar(GtkWidget *toolbar, GtkToolItem *item, gint pos)
{
    if (gtk_toolbar_get_n_items(GTK_TOOLBAR(toolbar)) < pos)
        pos = -1;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, pos);
}


static GtkToolItem *
toolbar_item_create (const gchar *action_name, const gchar *icon_name, const gchar *label, const gchar *tooltip_text)
{
  GtkWidget *widget = NULL;
  GtkToolItem *item = NULL;

  widget = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_show (widget);

  item = gtk_tool_button_new (widget, label);
  gtk_tool_item_set_tooltip_text (item, tooltip_text);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name);
  gtk_widget_show (GTK_WIDGET (item));

  return item;
}

/* -------------------------------------------------------------- */
// ACTION:

static const GActionEntry action_entries[] = {
    { G_ACTION_CALL_NEW, call_new_call, NULL, NULL, NULL},
    { G_ACTION_CALL_PICKUP, call_pick_up, NULL, NULL, NULL},
    { G_ACTION_CALL_HANGUP, call_hang_up, NULL, NULL, NULL},
};

void
action_enable(GActionMap *map, const gchar* action_name, gboolean value)
{
    GAction *action = g_action_map_lookup_action (map, action_name);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), value);
}

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
    show_status_hangup_icon(client);
/*
    gtk_action_set_sensitive(copyAction_, TRUE);
*/
    GtkWidget *toolbar = client->toolbar;

    switch (selectedCall->_state) {
        case CALL_STATE_INCOMING:
        {
                g_debug("Call State Incoming");
                // Make the button toolbar clickable
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_PICKUP, TRUE);
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                // Replace the dial button with the hangup button
                remove_from_toolbar(toolbar, newCallWidget_);
                pos = 0;
                add_to_toolbar(toolbar, pickUpWidget_, pos++);
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                break;
        }
        case CALL_STATE_HOLD:
        {
                g_debug("Call State Hold");
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
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
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_PICKUP, TRUE);
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                pos = 1;
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                break;
        }
        case CALL_STATE_DIALING:
        {
                g_debug("Call State Dialing");
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_PICKUP, TRUE);
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);

                // FIXME: ??
                /*
                if (calltab_has_name(active_calltree_tab, CURRENT_CALLS))
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                */

                remove_from_toolbar(toolbar, newCallWidget_);
                pos = 0;
                add_to_toolbar(toolbar, pickUpWidget_, pos++);

                if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
                    add_to_toolbar(toolbar, hangUpWidget_, pos++);
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
#ifdef SFL_VIDEO
            const gboolean video_enabled = is_video_call(selectedCall);
#endif

                g_debug("Call State Current");
                /*
                g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                g_signal_handler_block(recordWidget_, recordButtonConnId_);
                */
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                /*
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
                */
                pos = 1;
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                /*
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
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                break;
        }
        case CALL_STATE_TRANSFER:
        {
                pos = 1;
                action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, TRUE);
                /*
                gtk_widget_set_sensitive(holdMenu_, TRUE);
                gtk_widget_set_sensitive(holdToolbar_, TRUE);
                gtk_widget_set_sensitive(transferToolbar_, TRUE);
                */

                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                /*
                add_to_toolbar(toolbar, transferToolbar_, pos++);
                g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), TRUE);
                g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                */
                break;
        }
        default:
            g_warning("Unknown state in action update!");
            break;
    }
}


void
action_update(SFLPhoneClient *client)
{

    action_enable (G_ACTION_MAP (client), G_ACTION_CALL_NEW, TRUE);
    action_enable (G_ACTION_MAP (client), G_ACTION_CALL_HANGUP, FALSE);
    action_enable (G_ACTION_MAP (client), G_ACTION_CALL_PICKUP, FALSE);
/*
    gtk_action_set_sensitive(recordAction_, FALSE);
    gtk_action_set_sensitive(copyAction_, FALSE);
    gtk_action_set_sensitive(imAction_, FALSE);
#ifdef SFL_VIDEO
    gtk_action_set_sensitive(screenshareAction_, FALSE);
#endif

    gtk_widget_set_sensitive(holdMenu_, FALSE);
    gtk_widget_set_sensitive(holdToolbar_, FALSE);
    gtk_widget_set_sensitive(offHoldToolbar_, FALSE);
    gtk_widget_set_sensitive(recordWidget_, FALSE);
    gtk_widget_set_sensitive(historyButton_, FALSE);
*/
    // Increment the reference counter
/*
    g_object_ref(newCallWidget_);
    g_object_ref(hangUpWidget_);
    g_object_ref(recordWidget_);
    g_object_ref(holdToolbar_);
    g_object_ref(offHoldToolbar_);
    g_object_ref(historyButton_);
    g_object_ref(transferToolbar_);
    g_object_ref(voicemailToolbar_);
    g_object_ref(imToolbar_);
#ifdef SFL_VIDEO
    g_object_ref(screenshareToolbar_);
#endif

*/
    // Make sure the toolbar is reinitialized
    // Widget will be added according to the state
    // of the selected call
    GtkWidget *toolbar = client->toolbar;
    remove_from_toolbar(toolbar, newCallWidget_);
    remove_from_toolbar(toolbar, hangUpWidget_);
    remove_from_toolbar(toolbar, pickUpWidget_);
/*
    remove_from_toolbar(toolbar, recordWidget_);
    remove_from_toolbar(toolbar, transferToolbar_);
    remove_from_toolbar(toolbar, historyButton_);
    remove_from_toolbar(toolbar, voicemailToolbar_);
    remove_from_toolbar(toolbar, imToolbar_);
    remove_from_toolbar(toolbar, screenshareToolbar_);
    remove_from_toolbar(toolbar, holdToolbar_);
    remove_from_toolbar(toolbar, offHoldToolbar_);
    remove_from_toolbar(toolbar, newCallWidget_);

    if (addrbook) {
        remove_from_toolbar(toolbar, GTK_TOOL_ITEM(contactButton_));
        gtk_widget_set_sensitive(contactButton_, FALSE);
        gtk_widget_set_tooltip_text(contactButton_, _("No address book selected"));
    }
*/
    // New call widget always present
    add_to_toolbar(toolbar, newCallWidget_, 0);
/*
    // Add the history button and set it to sensitive if enabled
    if (g_settings_get_boolean(client->settings, "history-enabled")) {
        add_to_toolbar(toolbar, historyButton_, -1);
        gtk_widget_set_sensitive(historyButton_, TRUE);
    }
*/

    // If addressbook support has been enabled and all addressbooks are loaded, display the icon
    if (addrbook && addrbook->is_ready() &&
        g_settings_get_boolean(client->settings, "use-evolution-addressbook")) {
        add_to_toolbar(toolbar, GTK_TOOL_ITEM(contactButton_), -1);

        // Make the icon clickable only if at least one address book is active
        if (addrbook->is_active()) {
            gtk_widget_set_sensitive(contactButton_, TRUE);
            gtk_widget_set_tooltip_text(contactButton_, _("Address book"));
        }
    }

    callable_obj_t * selectedCall = NULL;
    conference_obj_t * selectedConf = NULL;

    const gboolean instant_messaging_enabled = g_settings_get_boolean(client->settings, "instant-messaging-enabled");

    return;

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
    gtk_toolbar_set_icon_size (GTK_TOOLBAR(client->toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
}

GtkBuilder *
uibuilder_new(SFLPhoneClient *client)
{
    /* Create an accel group for window's shortcuts */
    gchar *path = g_build_filename(SFLPHONE_UIDIR_UNINSTALLED, "./uibuilder.xml", NULL);
    guint builder_id;
    GError *error = NULL;
    GtkBuilder * builder = gtk_builder_new();

    if (g_file_test(path, G_FILE_TEST_EXISTS))
        builder_id = gtk_builder_add_from_file(builder, path, &error);
    else {
        g_free(path);
        path = g_build_filename(SFLPHONE_UIDIR, "./uibuilder.xml", NULL);

        if (!g_file_test(path, G_FILE_TEST_EXISTS))
            goto fail;

        builder_id = gtk_builder_add_from_file(builder, path, &error);
    }

    if ((error) || !builder_id)
        goto fail;

    g_free(path);

    g_action_map_add_action_entries (G_ACTION_MAP (client), action_entries, G_N_ELEMENTS (action_entries), client);
    //TODO: uimanager.c:1200
    // - add address book
    // - translation domain

    active_calltree_tab = current_calls_tab;

    g_debug("New buildre created.");
    return builder;

fail:
    if (error)
        g_error_free(error);

    g_free(path);
    return NULL;
}

void
uibuilder_create_menus(GtkBuilder *ui_builder, SFLPhoneClient *client)
{
    gtk_application_set_menubar (GTK_APPLICATION (client),
            G_MENU_MODEL (gtk_builder_get_object (ui_builder, "menu-bar")));
}

void
uibuilder_create_toolbar(GtkBuilder *ui_builder, SFLPhoneClient *client)
{
    GtkWidget *toolbar = GTK_WIDGET (gtk_builder_get_object (ui_builder, "toolbar"));
    guint i = 0;

    /*
    //TODO: fatcorize tool items:
    toolbar_items[TOOLBAR_ITEM_CALL_NEW] = toolbar_item_create ("app.call-new", GTK_STOCK_DIAL, _("New Call"), _("Start a new call"));

    for (i = 0; i < G_N_ELEMENTS (toolbar_items); ++i)
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolbar_items[i], i);
        */

    newCallWidget_ = toolbar_item_create (G_ACTION_CALL_NEW, GTK_STOCK_DIAL, _("New Call"), _("Start a new call"));
    pickUpWidget_ = toolbar_item_create (G_ACTION_CALL_PICKUP, GTK_STOCK_PICKUP, _("Pick up"), _("Pick up the phone"));
    hangUpWidget_ = toolbar_item_create (G_ACTION_CALL_HANGUP, GTK_STOCK_HANGUP, _("Hang up"), _("Hang up he phone"));
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), newCallWidget_, i++);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), pickUpWidget_, i++);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), hangUpWidget_, i++);


    gtk_toolbar_set_icon_size (GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style (GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    client->toolbar = toolbar;
}
