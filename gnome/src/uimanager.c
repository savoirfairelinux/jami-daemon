/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com
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

#include "str_utils.h"
#include "actions.h"
#include "preferencesdialog.h"
#include "icons/icon_factory.h"
#include "dbus/dbus.h"
#include "mainwindow.h"
#include "assistant.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "uimanager.h"
#include "statusicon.h"
#include "config/audioconf.h"
#include "uimanager.h"
#include "statusicon.h"

#include "contacts/addrbookfactory.h"
#include "contacts/calltab.h"
#include "config/addressbook-config.h"

#include "accountlist.h"
#include "account_schema.h"
#include "config/accountlistconfigdialog.h"

#include "messaging/message_tab.h"

#include <sys/stat.h>

#include "sliders.h"

#ifdef SFL_PRESENCE
#include "presencewindow.h"
#include "presence.h"
#endif

typedef struct
{
    callable_obj_t *call;
    SFLPhoneClient *client;
} OkData;

void show_edit_number(callable_obj_t *call, SFLPhoneClient *client);

// store the signal ID in case we need to
// intercept this signal
static guint transferButtonConnId_;
static guint recordButtonConnId_;

static GtkAction * pickUpAction_;
static GtkAction * newCallAction_;
static GtkAction * hangUpAction_;
static GtkAction * copyAction_;
static GtkAction * pasteAction_;
static GtkAction * recordAction_;
static GtkAction * imAction_;
#ifdef SFL_VIDEO
static GtkAction * screenshareAction_;
#endif

static GtkWidget * pickUpWidget_;
static GtkWidget * newCallWidget_;
static GtkWidget * hangUpWidget_;
static GtkWidget * holdMenu_;
static GtkWidget * holdToolbar_;
static GtkWidget * offHoldToolbar_;
static GtkWidget * transferToolbar_;
static GtkWidget * recordWidget_;
static GtkWidget * voicemailToolbar_;
static GtkWidget * imToolbar_;
static GtkWidget * screenshareToolbar_;

static GtkWidget * editable_num_;
static GtkWidget * edit_dialog_;

// GtkToolItem *separator_;

static void
remove_from_toolbar(GtkWidget *toolbar, GtkWidget *widget)
{
    /* We must ensure that a widget is a child of a container
     * before removing it. */
    if (gtk_widget_get_parent(widget) == toolbar)
        gtk_container_remove(GTK_CONTAINER(toolbar), widget);
}

static gboolean
is_non_empty(const char *str)
{
    return str && strlen(str) > 0;
}

/* Inserts an item in a toolbar at a given position.
 * If the index exceeds the number of elements, the widget is simply appended */
static void add_to_toolbar(GtkWidget *toolbar, GtkWidget *item, gint pos)
{
    if (gtk_toolbar_get_n_items(GTK_TOOLBAR(toolbar)) < pos)
        pos = -1;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), pos);
}

#ifdef SFL_VIDEO
static gboolean
is_video_call(callable_obj_t *call)
{
    gboolean video_enabled = FALSE;
    account_t *account = account_list_get_by_id(call->_accountID);

    if (account) {
        gchar *ptr = (gchar *) account_lookup(account, CONFIG_VIDEO_ENABLED);
        video_enabled = g_strcmp0(ptr, "true") == 0;
    }

    return video_enabled;
}
#endif

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

    gtk_action_set_sensitive(copyAction_, TRUE);

    GtkWidget *toolbar = client->toolbar;

    switch (selectedCall->_state) {
        case CALL_STATE_INCOMING:
        {
                g_debug("Call State Incoming");
                // Make the button toolbar clickable
                gtk_action_set_sensitive(pickUpAction_, TRUE);
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                // Replace the dial button with the hangup button
                g_object_ref(newCallWidget_);
                remove_from_toolbar(toolbar, newCallWidget_);
                pos = 0;
                add_to_toolbar(toolbar, pickUpWidget_, pos++);
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                break;
        }
        case CALL_STATE_HOLD:
        {
                g_debug("Call State Hold");
                gtk_action_set_sensitive(hangUpAction_, TRUE);
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

                break;
        }
        case CALL_STATE_RINGING:
        {
                g_debug("Call State Ringing");
                gtk_action_set_sensitive(pickUpAction_, TRUE);
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                pos = 1;
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                break;
        }
        case CALL_STATE_DIALING:
        {
                g_debug("Call State Dialing");
                gtk_action_set_sensitive(pickUpAction_, TRUE);

                if (calltab_has_name(active_calltree_tab, CURRENT_CALLS))
                    gtk_action_set_sensitive(hangUpAction_, TRUE);

                g_object_ref(newCallWidget_);
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
                g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                g_signal_handler_block(recordWidget_, recordButtonConnId_);

                gtk_action_set_sensitive(hangUpAction_, TRUE);
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
                break;
        }

        case CALL_STATE_BUSY:
        case CALL_STATE_FAILURE:
        {
                pos = 1;
                g_debug("Call State Busy/Failure");
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                break;
        }
        case CALL_STATE_TRANSFER:
        {
                pos = 1;
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                gtk_widget_set_sensitive(holdMenu_, TRUE);
                gtk_widget_set_sensitive(holdToolbar_, TRUE);
                gtk_widget_set_sensitive(transferToolbar_, TRUE);
                gtk_widget_set_sensitive(transferToolbar_, TRUE);

                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                add_to_toolbar(toolbar, transferToolbar_, pos++);
                g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), TRUE);
                g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                break;
        }
        default:
            g_warning("Unknown state in action update!");
            break;
    }
}

static void
update_toolbar_for_conference(conference_obj_t * selectedConf, gboolean instant_messaging_enabled, SFLPhoneClient *client)
{
    int pos = 0;

    GtkWidget *toolbar = client->toolbar;

    // update icon in systray
    show_status_hangup_icon(client);

    switch (selectedConf->_state) {

        case CONFERENCE_STATE_ACTIVE_ATTACHED:
        case CONFERENCE_STATE_ACTIVE_DETACHED:
            g_debug("Conference State Active");
            g_signal_handler_block(recordWidget_, recordButtonConnId_);
            if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                gtk_widget_set_sensitive(holdToolbar_, TRUE);
                gtk_widget_set_sensitive(recordWidget_, TRUE);
                pos = 1;
                add_to_toolbar(toolbar, hangUpWidget_, pos++);
                add_to_toolbar(toolbar, holdToolbar_, pos++);
                add_to_toolbar(toolbar, recordWidget_, pos++);

                if (instant_messaging_enabled) {
                    gtk_action_set_sensitive(imAction_, TRUE);
                    add_to_toolbar(toolbar, imToolbar_, pos);
                }
                main_window_hide_playback_scale();
            } else if (calltab_has_name(active_calltree_tab, HISTORY)) {
                main_window_show_playback_scale();
                if (is_non_empty(selectedConf->_recordfile))
                    main_window_set_playback_scale_sensitive();
                else
                    main_window_set_playback_scale_unsensitive();
            } else {
                main_window_hide_playback_scale();
            }
            g_signal_handler_unblock(recordWidget_, recordButtonConnId_);
            break;
        case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
        case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD: {
            g_signal_handler_block(recordWidget_, recordButtonConnId_);
            pos = 1;
            g_debug("Conference State Record");
            gtk_action_set_sensitive(hangUpAction_, TRUE);
            gtk_widget_set_sensitive(holdToolbar_, TRUE);
            gtk_widget_set_sensitive(recordWidget_, TRUE);
            add_to_toolbar(toolbar, hangUpWidget_, pos++);
            add_to_toolbar(toolbar, holdToolbar_, pos++);
            add_to_toolbar(toolbar, recordWidget_, pos++);

            if (instant_messaging_enabled) {
                gtk_action_set_sensitive(imAction_, TRUE);
                add_to_toolbar(toolbar, imToolbar_, pos);
            }
            g_signal_handler_unblock(recordWidget_, recordButtonConnId_);
            break;
        }
        case CONFERENCE_STATE_HOLD:
        case CONFERENCE_STATE_HOLD_RECORD: {
            g_debug("Conference State Hold");
            g_signal_handler_block(recordWidget_, recordButtonConnId_);
            pos = 1;
            gtk_action_set_sensitive(hangUpAction_, TRUE);
            gtk_widget_set_sensitive(offHoldToolbar_, TRUE);
            gtk_widget_set_sensitive(recordWidget_, TRUE);
            add_to_toolbar(toolbar, hangUpWidget_, pos++);
            add_to_toolbar(toolbar, offHoldToolbar_, pos++);
            add_to_toolbar(toolbar, recordWidget_, pos++);

            if (instant_messaging_enabled) {
                gtk_action_set_sensitive(imAction_, TRUE);
                add_to_toolbar(toolbar, imToolbar_, pos);
            }
            g_signal_handler_unblock(recordWidget_, recordButtonConnId_);

            break;
        }
        default:
            g_warning("Should not happen in action update!");
            break;
    }

}

void
update_actions(SFLPhoneClient *client)
{
    gtk_action_set_sensitive(newCallAction_, TRUE);
    gtk_action_set_sensitive(pickUpAction_, FALSE);
    gtk_action_set_sensitive(hangUpAction_, FALSE);
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

    // Increment the reference counter
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

    if (addrbook)
        g_object_ref(contactButton_);

    // Make sure the toolbar is reinitialized
    // Widget will be added according to the state
    // of the selected call
    GtkWidget *toolbar = client->toolbar;
    remove_from_toolbar(toolbar, hangUpWidget_);
    remove_from_toolbar(toolbar, recordWidget_);
    remove_from_toolbar(toolbar, transferToolbar_);
    remove_from_toolbar(toolbar, historyButton_);
    remove_from_toolbar(toolbar, voicemailToolbar_);
    remove_from_toolbar(toolbar, imToolbar_);
    remove_from_toolbar(toolbar, screenshareToolbar_);
    remove_from_toolbar(toolbar, holdToolbar_);
    remove_from_toolbar(toolbar, offHoldToolbar_);
    remove_from_toolbar(toolbar, newCallWidget_);
    remove_from_toolbar(toolbar, pickUpWidget_);

    if (addrbook) {
        remove_from_toolbar(toolbar, contactButton_);
        gtk_widget_set_sensitive(contactButton_, FALSE);
        gtk_widget_set_tooltip_text(contactButton_, _("No address book selected"));
    }

    // New call widget always present
    add_to_toolbar(toolbar, newCallWidget_, 0);

    // Add the history button and set it to sensitive if enabled
    if (g_settings_get_boolean(client->settings, "history-enabled")) {
        add_to_toolbar(toolbar, historyButton_, -1);
        gtk_widget_set_sensitive(historyButton_, TRUE);
    }

    // If addressbook support has been enabled and all addressbooks are loaded, display the icon
    if (addrbook && addrbook->is_ready() &&
        g_settings_get_boolean(client->settings, "use-evolution-addressbook")) {
        add_to_toolbar(toolbar, contactButton_, -1);

        // Make the icon clickable only if at least one address book is active
        if (addrbook->is_active()) {
            gtk_widget_set_sensitive(contactButton_, TRUE);
            gtk_widget_set_tooltip_text(contactButton_, _("Address book"));
        }
    }

    callable_obj_t * selectedCall = NULL;
    conference_obj_t * selectedConf = NULL;

    const gboolean instant_messaging_enabled = g_settings_get_boolean(client->settings, "instant-messaging-enabled");

    if (!calllist_empty(active_calltree_tab)) {
        selectedCall = calltab_get_selected_call(active_calltree_tab);
        selectedConf = calltab_get_selected_conf(active_calltree_tab);
    }

    if (selectedCall) {
        update_toolbar_for_call(selectedCall, instant_messaging_enabled, client);
    } else if (selectedConf) {
        update_toolbar_for_conference(selectedConf, instant_messaging_enabled, client);
    } else {
        // update icon in systray
        hide_status_hangup_icon();

        if (account_list_get_size() > 0 && current_account_has_mailbox()) {
            add_to_toolbar(toolbar, voicemailToolbar_, -1);
            update_voicemail_status();
        }
    }
}

void
update_voicemail_status()
{
    gchar *messages = g_markup_printf_escaped(_("Voicemail(%i)"),
                      current_account_get_message_number());

    if (current_account_has_new_message())
        gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(voicemailToolbar_),
                                      GTK_STOCK_NEWVOICEMAIL);
    else
        gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(voicemailToolbar_),
                                      GTK_STOCK_VOICEMAIL);

    gtk_tool_button_set_label(GTK_TOOL_BUTTON(voicemailToolbar_), messages);
    g_free(messages);
}

static void
volume_bar_cb(GtkToggleAction *togglemenuitem, SFLPhoneClient *client)
{
    gboolean toggled = gtk_toggle_action_get_active(togglemenuitem);

    const gboolean show_volume = must_show_volume(client);

    main_window_volume_controls(toggled);

    if (toggled != show_volume)
        g_settings_set_boolean(client->settings, "show-volume-controls", toggled);
}

static void
dialpad_bar_cb(GtkToggleAction *togglemenuitem, SFLPhoneClient *client)
{
    const gboolean toggled = gtk_toggle_action_get_active(togglemenuitem);

    main_window_dialpad(toggled, client);

    const gboolean conf_dialpad = g_settings_get_boolean(client->settings, "show-dialpad");
    if (toggled != conf_dialpad)
        g_settings_set_boolean(client->settings, "show-dialpad", toggled);
}

#ifdef SFL_PRESENCE
static void
toggle_presence_window_cb(GtkToggleAction *togglemenuitem, SFLPhoneClient *client)
{
    const gboolean toggled = gtk_toggle_action_get_active(togglemenuitem);
    if (toggled)
        create_presence_window(client, togglemenuitem);
    else
        destroy_presence_window();
}
#endif

static void
help_contents_cb(G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED gpointer data)
{
    GError *error = NULL;
    gtk_show_uri(NULL, "ghelp:sflphone", GDK_CURRENT_TIME, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

static void
help_about(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    static const gchar *authors[] = {
        "Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>",
        "Jean-Philippe Barrette-LaPierre",
        "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>",
        "Julien Bonjean <julien.bonjean@savoirfairelinux.com>",
        "Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>",
        "Laurielle Lea",
        "Yun Liu <yun.liu@savoirfairelinux.com>",
        "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>",
        "Yan Morin <yan.morin@savoirfairelinux.com>",
        "Jérôme Oufella <jerome.oufella@savoirfairelinux.com>",
        "Julien Plissonneau Duquene <julien.plissonneau.duquene@savoirfairelinux.com>",
        "Alexandre Savard <alexandre.savard@savoirfairelinux.com>",
        "Tristan Matthews <tristan.matthews@savoirfairelinux.com>",
        "Rafaël Carré <rafael.carre@savoirfairelinux.com>",
        "Vivien Didelot <vivien.didelot@savoirfairelinux.com>",
        "Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>",
        "Partick Keroulas <patrick.keroulas@savoirfairelinux.com>",
        NULL
    };
    static const gchar *artists[] = {
        "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>",
        "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>", NULL
    };

    gtk_show_about_dialog(GTK_WINDOW(client->win),
            "artists", artists,
            "authors", authors,
            "comments", _("SFLphone is a VoIP client compatible with SIP and IAX2 protocols."),
            "copyright", "Copyright © 2004-2014 Savoir-faire Linux Inc.",
            "name", PACKAGE_NAME,
            "title", _("About SFLphone"),
            "version", PACKAGE_VERSION,
            "website", "http://www.sflphone.org",
            NULL);
}

/* ----------------------------------------------------------------- */

static void
call_new_call(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    g_debug("New call button pressed");
    sflphone_new_call(client);
}

static void
call_quit(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    sflphone_quit(FALSE, client);
}

static void
call_minimize(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    if (g_settings_get_boolean(client->settings, "show-status-icon")) {
        gtk_widget_hide(client->win);
        set_minimized(TRUE);
    } else
        sflphone_quit(FALSE, client);
}

static void
switch_account(GtkWidget* item, G_GNUC_UNUSED gpointer data)
{
    account_t* acc = g_object_get_data(G_OBJECT(item), "account");
    g_debug("%s" , acc->accountID);
    account_list_set_current(acc);
    status_bar_display_account();
}

static void
call_hold(G_GNUC_UNUSED GtkAction * action, G_GNUC_UNUSED gpointer data)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    g_debug("Hold button pressed");

    if (selectedCall) {
        if (selectedCall->_state == CALL_STATE_HOLD)
            sflphone_off_hold();
        else
            sflphone_on_hold();
    } else if (selectedConf) {
        switch (selectedConf->_state) {
            case CONFERENCE_STATE_HOLD:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED;
                dbus_unhold_conference(selectedConf);
                break;
            case CONFERENCE_STATE_HOLD_RECORD:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
                dbus_unhold_conference(selectedConf);
                break;

            case CONFERENCE_STATE_ACTIVE_ATTACHED:
            case CONFERENCE_STATE_ACTIVE_DETACHED:
                selectedConf->_state = CONFERENCE_STATE_HOLD;
                dbus_hold_conference(selectedConf);
                break;
            case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
            case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
                selectedConf->_state = CONFERENCE_STATE_HOLD_RECORD;
                dbus_hold_conference(selectedConf);
                break;
            default:
                break;
        }
    }
}

static void
call_im(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    callable_obj_t *selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t *selectedConf = calltab_get_selected_conf(current_calls_tab);

    if (calltab_get_selected_type(current_calls_tab) == A_CALL) {
        if (selectedCall)
            create_messaging_tab(selectedCall, client);
        else
            g_warning("Sorry. Instant messaging is not allowed outside a call\n");
    } else {
        if (selectedConf)
            create_messaging_tab_conf(selectedConf, client);
        else
            g_warning("Sorry. Instant messaging is not allowed outside a call\n");
    }
}

static void
call_screenshare(G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED SFLPhoneClient *client)
{
#ifdef SFL_VIDEO
    sflphone_toggle_screenshare();
#endif
}

static gchar *
choose_file(void)
{
    gchar *uri = NULL;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(NULL, NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            _("_Cancel"), GTK_RESPONSE_CANCEL,
            _("_Open"), GTK_RESPONSE_ACCEPT,
            NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        uri = gtk_file_chooser_get_uri(chooser);
    }

    gtk_widget_destroy(dialog);
    return uri;
}

#ifdef SFL_VIDEO
static void
call_switch_video_input(G_GNUC_UNUSED GtkWidget *widget, gchar *device)
{
    gchar *resource;

    if (g_strcmp0(device, _("Screen")) == 0) {
        resource = sflphone_get_display();
    } else if (g_strcmp0(device, _("Choose file...")) == 0) {
        resource = choose_file();
        if (!resource)
            return;
    } else {
        dbus_video_set_default_device(device);
        resource = g_strconcat("v4l2://", device, NULL);
    }

    sflphone_switch_video_input(resource);
    g_free(resource);
}
#endif

static void
conference_hold(G_GNUC_UNUSED gpointer foo)
{
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    g_debug("Hold button pressed for conference");

    if (selectedConf == NULL) {
        g_warning("No conference selected");
        return;
    }

    switch (selectedConf->_state) {
        case CONFERENCE_STATE_HOLD:
            selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED;
            dbus_unhold_conference(selectedConf);
            break;
        case CONFERENCE_STATE_HOLD_RECORD:
            selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
            dbus_unhold_conference(selectedConf);
            break;
        case CONFERENCE_STATE_ACTIVE_ATTACHED:
        case CONFERENCE_STATE_ACTIVE_DETACHED:
            selectedConf->_state = CONFERENCE_STATE_HOLD;
            dbus_hold_conference(selectedConf);
            break;
        case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
        case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
            selectedConf->_state = CONFERENCE_STATE_HOLD_RECORD;
            dbus_hold_conference(selectedConf);
        default:
            break;
    }
}

static void
call_pick_up(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
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
call_hang_up(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    g_debug("Hang up button pressed(call)");
    /*
     * [#3020]	Restore the record toggle button
     *			We set it to FALSE, as when we hang up a call, the recording is stopped.
     */

    sflphone_hang_up(client);
}

static void
conference_hang_up(void)
{
    g_debug("Hang up button pressed(conference)");
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    if (selectedConf)
        dbus_hang_up_conference(selectedConf);
}

static void
call_record(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    g_debug("Record button pressed");
    /* Ensure that button is set to correct state, but suppress signal */
    g_signal_handler_block(recordWidget_, recordButtonConnId_);
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(recordWidget_), sflphone_rec_call(client));
    g_signal_handler_unblock(recordWidget_, recordButtonConnId_);
}

static void
call_configuration_assistant(G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED gpointer data)
{
    build_wizard();
}

typedef struct
{
    callable_obj_t *call;
    SFLPhoneClient *client;
} EditNumberData;

static void
remove_from_history(G_GNUC_UNUSED GtkAction *action, EditNumberData *data)
{
    if (data->call == NULL) {
        g_warning("Call is NULL");
        return;
    }

    calllist_remove_from_history(data->call, data->client);
}

static void
call_back(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    callable_obj_t *selected_call = calltab_get_selected_call(active_calltree_tab);

    g_debug("Call back");

    if (selected_call == NULL) {
        g_warning("No selected call");
        return;
    }

    callable_obj_t *new_call = create_new_call(CALL, CALL_STATE_DIALING, "",
                               "", selected_call->_display_name,
                               selected_call->_peer_number);

    calllist_add_call(current_calls_tab, new_call);
    calltree_add_call(current_calls_tab, new_call, NULL);
    sflphone_place_call(new_call, client);
    calltree_display(current_calls_tab, client);
}

static void
edit_preferences(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    show_preferences_dialog(client);
}

static void
edit_accounts(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    show_account_list_config_dialog(client);
}

// The menu Edit/Copy should copy the current selected call's number
static void
edit_copy(G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED gpointer data)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);

    g_return_if_fail(selectedCall != NULL);

    g_debug("Clipboard number: %s\n", selectedCall->_peer_number);
    gtk_clipboard_set_text(clip, selectedCall->_peer_number,
                           strlen(selectedCall->_peer_number));
}

// The menu Edit/Paste should paste the clipboard into the current selected call
static void
edit_paste(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    gchar * no = gtk_clipboard_wait_for_text(clip);

    if (no && selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_TRANSFER:
            case CALL_STATE_DIALING: {
                /* Add the text to the number */
                gchar *old = selectedCall->_peer_number;
                g_debug("TO: %s\n", old);
                selectedCall->_peer_number = g_strconcat(old, no, NULL);
                g_free(old);

                if (selectedCall->_state == CALL_STATE_DIALING)
                    selectedCall->_peer_info = g_strconcat("\"\" <",
                                                           selectedCall->_peer_number, ">", NULL);

                calltree_update_call(current_calls_tab, selectedCall, client);
            }
            break;
            case CALL_STATE_RINGING:
            case CALL_STATE_INCOMING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
            case CALL_STATE_HOLD: { // Create a new call to hold the new text
                selectedCall = sflphone_new_call(client);

                gchar *old = selectedCall->_peer_number;
                selectedCall->_peer_number = g_strconcat(old, no, NULL);
                g_free(old);
                g_debug("TO: %s", selectedCall->_peer_number);

                g_free(selectedCall->_peer_info);
                selectedCall->_peer_info = g_strconcat("\"\" <",
                                                       selectedCall->_peer_number, ">", NULL);

                calltree_update_call(current_calls_tab, selectedCall, client);
            }
            break;
            case CALL_STATE_CURRENT:
            default: {
                for (unsigned i = 0; i < strlen(no); i++) {
                    gchar * oneNo = g_strndup(&no[i], 1);
                    g_debug("<%s>", oneNo);
                    dbus_play_dtmf(oneNo);

                    gchar * temp = g_strconcat(selectedCall->_peer_number,
                                               oneNo, NULL);
                    g_free(selectedCall->_peer_info);
                    selectedCall->_peer_info = get_peer_info(temp, selectedCall->_display_name);
                    g_free(temp);
                    g_free(oneNo);
                    calltree_update_call(current_calls_tab, selectedCall, client);
                }
            }
            break;
        }
    } else { // There is no current call, create one
        selectedCall = sflphone_new_call(client);

        gchar * old = selectedCall->_peer_number;
        selectedCall->_peer_number = g_strconcat(old, no, NULL);
        g_free(old);
        g_debug("TO: %s", selectedCall->_peer_number);

        g_free(selectedCall->_peer_info);
        selectedCall->_peer_info = g_strconcat("\"\" <",
                                               selectedCall->_peer_number, ">", NULL);
        calltree_update_call(current_calls_tab, selectedCall, client);
    }

    g_free(no);
}

static void
clear_history(G_GNUC_UNUSED GtkAction *action, G_GNUC_UNUSED SFLPhoneClient *client)
{
    calllist_clean_history();
    dbus_clear_history();
}

/**
 * Transfer the line
 */
static void
call_transfer_cb(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_)))
        sflphone_set_transfer(client);
    else
        sflphone_unset_transfer(client);
}

static void
call_mailbox_cb(G_GNUC_UNUSED GtkAction *action, SFLPhoneClient *client)
{
    account_t *current = account_list_get_current();

    if (current == NULL) // Should not happen
        return;

    const gchar * const to = g_hash_table_lookup(current->properties, CONFIG_ACCOUNT_MAILBOX);
    const gchar * const account_id = g_strdup(current->accountID);

    callable_obj_t *mailbox_call = create_new_call(CALL, CALL_STATE_DIALING,
                                   "", account_id,
                                   _("Voicemail"), to);
    g_debug("TO : %s" , mailbox_call->_peer_number);
    calllist_add_call(current_calls_tab, mailbox_call);
    calltree_add_call(current_calls_tab, mailbox_call, NULL);
    update_actions(client);
    sflphone_place_call(mailbox_call, client);
    calltree_display(current_calls_tab, client);
}

static void
reset_scrollwindow_position(calltab_t *tab)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(tab->tree));
    /* Calltree scrolled window is first element in list */
    GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(children->data);
    g_list_free(children);
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(scrolled_window);
    gtk_adjustment_set_value(adjustment, 0.0);
}

static void
toggle_history_cb(GtkToggleAction *action, SFLPhoneClient *client)
{
    if (gtk_toggle_action_get_active(action)) {
        /* Ensure that latest call is visible in history without scrolling */
        reset_scrollwindow_position(history_tab);
        calltree_display(history_tab, client);
        main_window_show_playback_scale();
    } else {
        calltree_display(current_calls_tab, client);
        main_window_hide_playback_scale();
    }
}

static void
toggle_addressbook_cb(GtkToggleAction *action, SFLPhoneClient *client)
{
    if (gtk_toggle_action_get_active(action)) {
        calltree_display(contacts_tab, client);
        main_window_hide_playback_scale();
    }
    else {
        calltree_display(current_calls_tab, client);
        main_window_show_playback_scale();
    }
}

static const GtkActionEntry menu_entries[] = {
    // Call Menu
    { "Call", NULL, N_("Call"), NULL, NULL, NULL},
    {
        "NewCall", GTK_STOCK_DIAL, N_("_New call"), "<control>N",
        N_("Place a new call"), G_CALLBACK(call_new_call)
    },
    {
        "PickUp", GTK_STOCK_PICKUP, N_("_Pick up"), NULL,
        N_("Answer the call"), G_CALLBACK(call_pick_up)
    },
    {
        "HangUp", GTK_STOCK_HANGUP, N_("_Hang up"), "<control>S",
        N_("Finish the call"), G_CALLBACK(call_hang_up)
    },
    {
        "OnHold", GTK_STOCK_ONHOLD, N_("O_n hold"), "<control>P",
        N_("Place the call on hold"), G_CALLBACK(call_hold)
    },
    {
        "OffHold", GTK_STOCK_OFFHOLD, N_("O_ff hold"), "<control>P",
        N_("Place the call off hold"), G_CALLBACK(call_hold)
    },
    {
        "InstantMessaging", GTK_STOCK_IM, N_("Send _message"), "<control>M",
        N_("Send message"), G_CALLBACK(call_im)
    },
    {
        "ScreenSharing", GTK_STOCK_SCREENSHARING, N_("Share screen"), "<control>X",
        N_("Share screen"), G_CALLBACK(call_screenshare)
    },
    {
        "AccountAssistant", NULL, N_("Configuration _Assistant"), NULL,
        N_("Run the configuration assistant"), G_CALLBACK(call_configuration_assistant)
    },
    {
        "Voicemail", "mail-read", N_("Voicemail"), NULL,
        N_("Call your voicemail"), G_CALLBACK(call_mailbox_cb)
    },
    {
        "Close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
        N_("Minimize to system tray"), G_CALLBACK(call_minimize)
    },
    {
        "Quit", GTK_STOCK_CLOSE, N_("_Quit"), "<control>Q",
        N_("Quit the program"), G_CALLBACK(call_quit)
    },

    // Edit Menu
    { "Edit", NULL, N_("_Edit"), NULL, NULL, NULL },
    {
        "Copy", GTK_STOCK_COPY, N_("_Copy"), "<control>C",
        N_("Copy the selection"), G_CALLBACK(edit_copy)
    },
    {
        "Paste", GTK_STOCK_PASTE, N_("_Paste"), "<control>V",
        N_("Paste the clipboard"), G_CALLBACK(edit_paste)
    },
    {
        "ClearHistory", GTK_STOCK_CLEAR, N_("Clear _history"), NULL,
        N_("Clear the call history"), G_CALLBACK(clear_history)
    },
    {
        "Accounts", NULL, N_("_Accounts"), NULL,
        N_("Edit your accounts"), G_CALLBACK(edit_accounts)
    },
    {
        "Preferences", GTK_STOCK_PREFERENCES, N_("_Preferences"), NULL,
        N_("Change your preferences"), G_CALLBACK(edit_preferences)
    },

    // View Menu
    { "View", NULL, N_("_View"), NULL, NULL, NULL},

    // Help menu
    { "Help", NULL, N_("_Help"), NULL, NULL, NULL },
    { "HelpContents", GTK_STOCK_HELP, N_("Contents"), "F1",
      N_("Open the manual"), G_CALLBACK(help_contents_cb) },
    { "About", GTK_STOCK_ABOUT, NULL, NULL,
      N_("About this application"), G_CALLBACK(help_about) }
};

static const GtkToggleActionEntry toggle_menu_entries[] = {
    { "Transfer", GTK_STOCK_TRANSFER, N_("_Transfer"), "<control>T", N_("Transfer the call"), NULL, TRUE },
    { "Record", GTK_STOCK_RECORD, N_("_Record"), "<control>R", N_("Record the current conversation"), NULL, TRUE },
    { "Toolbar", NULL, N_("_Show toolbar"), "<control>T", N_("Show the toolbar"), NULL, TRUE },
    { "Dialpad", NULL, N_("_Dialpad"), "<control>D", N_("Show the dialpad"), G_CALLBACK(dialpad_bar_cb), TRUE },
    { "VolumeControls", NULL, N_("_Volume controls"), "<control>V", N_("Show the volume controls"), G_CALLBACK(volume_bar_cb), TRUE },
    { "History", GTK_STOCK_HISTORY, N_("_History"), NULL, N_("Call history"), G_CALLBACK(toggle_history_cb), FALSE },
    { "Addressbook", GTK_STOCK_ADDRESSBOOK, N_("_Address book"), NULL, N_("Address book"), G_CALLBACK(toggle_addressbook_cb), FALSE },
#ifdef SFL_PRESENCE
    { "Buddies", NULL, N_("_Buddy list"), NULL, N_("Display the buddy list"), G_CALLBACK(toggle_presence_window_cb), FALSE},
#endif
};

GtkUIManager *uimanager_new(SFLPhoneClient *client)
{
    const gint nb_entries = sizeof(toggle_menu_entries) / sizeof(toggle_menu_entries[0]);

    GtkUIManager *ui = gtk_ui_manager_new();

    /* Create an accel group for window's shortcuts */
    gchar *path = g_build_filename(SFLPHONE_UIDIR_UNINSTALLED, "./ui.xml", NULL);
    guint manager_id;
    GError *error = NULL;

    if (g_file_test(path, G_FILE_TEST_EXISTS))
        manager_id = gtk_ui_manager_add_ui_from_file(ui, path, &error);
    else {
        g_free(path);
        path = g_build_filename(SFLPHONE_UIDIR, "./ui.xml", NULL);

        if (!g_file_test(path, G_FILE_TEST_EXISTS))
            goto fail;

        manager_id = gtk_ui_manager_add_ui_from_file(ui, path, &error);
    }

    if (error)
        goto fail;

    g_free(path);

    if (addrbook) {
        // These actions must be loaded dynamically and is not specified in the xml description
        gtk_ui_manager_add_ui(ui, manager_id, "/ViewMenu",
                              "Addressbook",
                              "Addressbook",
                              GTK_UI_MANAGER_MENUITEM, FALSE);
        gtk_ui_manager_add_ui(ui, manager_id,  "/ToolbarActions",
                              "AddressbookToolbar",
                              "Addressbook",
                              GTK_UI_MANAGER_TOOLITEM, FALSE);
    }

    GtkActionGroup *action_group = gtk_action_group_new("SFLphoneWindowActions");
    // To translate label and tooltip entries
    gtk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions(action_group, menu_entries, G_N_ELEMENTS(menu_entries), client);
    gtk_action_group_add_toggle_actions(action_group, toggle_menu_entries, nb_entries, client);
    gtk_ui_manager_insert_action_group(ui, action_group, 0);

    return ui;

fail:

    if (error)
        g_error_free(error);

    g_free(path);
    return NULL;
}

static void
edit_number_cb(G_GNUC_UNUSED GtkWidget *widget, EditNumberData *data)
{
    show_edit_number(data->call, data->client);
    g_free(data);
}

#ifdef SFL_PRESENCE
void
add_presence_subscription_cb(G_GNUC_UNUSED GtkWidget * widget, G_GNUC_UNUSED calltab_t * tab)
{
    callable_obj_t * c = calltab_get_selected_call(tab);
    buddy_t * b = presence_buddy_create();

    presence_callable_to_buddy(c, b);

    g_debug("Presence : trying to create a new subscription (%s,%s)", b->uri, b->acc);

    // popup
    if(show_buddy_info_dialog(_("Add new buddy"), b))
    {
        presence_buddy_list_add_buddy(b);
        update_presence_view();
    }
    else
        presence_buddy_delete(b);
}
#endif

void
add_registered_accounts_to_menu(GtkWidget *menu)
{
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_widget_show(separator);

    for (unsigned i = 0; i != account_list_get_size(); i++) {
        account_t *acc = account_list_get_nth(i);

        // Display only the registered accounts
        if (utf8_case_equal(account_state_name(acc->state),
                            account_state_name(ACCOUNT_STATE_REGISTERED))) {
            gchar *alias = g_strconcat(g_hash_table_lookup(acc->properties, CONFIG_ACCOUNT_ALIAS),
                                       " - ",
                                       g_hash_table_lookup(acc->properties, CONFIG_ACCOUNT_TYPE),
                                       NULL);
            GtkWidget *menu_items = gtk_check_menu_item_new_with_mnemonic(alias);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_object_set_data(G_OBJECT(menu_items), "account", acc);
            g_free(alias);
            account_t *current = account_list_get_current();

            if (current) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
                                               utf8_case_equal(acc->accountID, current->accountID));
            }

            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(switch_account),
                             NULL);
            gtk_widget_show(menu_items);
        }
    }
}

static void menu_popup_wrapper(GtkWidget *menu, GtkWidget *my_widget, GdkEventButton *event)
{
    gtk_menu_attach_to_widget(GTK_MENU(menu), my_widget, NULL);

    if (event)
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button,
                       event->time);
    else
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0,
                       gtk_get_current_event_time());
}

#ifdef SFL_VIDEO
static void
append_video_input_to_submenu(GtkWidget *submenu, const gchar *device)
{
    GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(_(device));
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    g_signal_connect(G_OBJECT(item), "activate",
            G_CALLBACK(call_switch_video_input), (gpointer) device);
    gtk_widget_show(item);
}
#endif

void
show_popup_menu(GtkWidget *my_widget, GdkEventButton *event, SFLPhoneClient *client)
{
    // TODO update the selection to make sure the call under the mouse is the call selected
    gboolean pickup = FALSE, hangup = FALSE, hold = FALSE, copy = FALSE, record = FALSE, im = FALSE;
#ifdef SFL_VIDEO
    gboolean video_sources = FALSE;
#endif
    gboolean accounts = FALSE;

    // conference type boolean
    gboolean hangup_or_hold_conf = FALSE;

    callable_obj_t * selectedCall = NULL;
    conference_obj_t * selectedConf = NULL;

    if (calltab_get_selected_type(current_calls_tab) == A_CALL) {
        g_debug("Menus: Selected a call");
        selectedCall = calltab_get_selected_call(current_calls_tab);

        if (selectedCall) {
            copy = TRUE;

            switch (selectedCall->_state) {
                case CALL_STATE_INCOMING:
                    pickup = TRUE;
                    hangup = TRUE;
                    break;
                case CALL_STATE_HOLD:
                    hangup = TRUE;
                    hold = TRUE;
                    break;
                case CALL_STATE_RINGING:
                    hangup = TRUE;
                    break;
                case CALL_STATE_DIALING:
                    pickup = TRUE;
                    hangup = TRUE;
                    accounts = TRUE;
                    break;
                case CALL_STATE_CURRENT:
                    hangup = TRUE;
                    hold = TRUE;
                    record = TRUE;
                    im = TRUE;
#ifdef SFL_VIDEO
                    video_sources = is_video_call(selectedCall);
#endif
                    break;
                case CALL_STATE_BUSY:
                case CALL_STATE_FAILURE:
                    hangup = TRUE;
                    break;
                default:
                    g_warning("Should not happen in show_popup_menu for calls!")
                    ;
                    break;
            }
        }
    } else {
        g_debug("Menus: selected a conf");
        selectedConf = calltab_get_selected_conf(active_calltree_tab);

        if (selectedConf) {
            switch (selectedConf->_state) {
                case CONFERENCE_STATE_ACTIVE_ATTACHED:
                case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
                    hangup_or_hold_conf = TRUE;
                    break;
                case CONFERENCE_STATE_ACTIVE_DETACHED:
                case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
                    break;
                case CONFERENCE_STATE_HOLD:
                case CONFERENCE_STATE_HOLD_RECORD:
                    hangup_or_hold_conf = TRUE;
                    break;
                default:
                    g_warning("Should not happen in show_popup_menu for conferences!")
                    ;
                    break;
            }
        }
    }

    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_accel_group(GTK_MENU(menu), get_accel_group());

    if (calltab_get_selected_type(current_calls_tab) == A_CALL) {
        g_debug("Build call menu");
        if (copy) {
            GtkWidget *copy_item = gtk_menu_item_new_with_mnemonic(_("_Copy"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
            g_signal_connect(G_OBJECT(copy_item), "activate",
                             G_CALLBACK(edit_copy),
                             NULL);
            gtk_widget_show(copy_item);
        }

        GtkWidget *paste = gtk_menu_item_new_with_mnemonic(_("_Paste"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste);
        g_signal_connect(G_OBJECT(paste), "activate", G_CALLBACK(edit_paste),
                         client);
        gtk_widget_show(paste);

        if (pickup || hangup || hold) {
            GtkWidget *menu_items = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            gtk_widget_show(menu_items);
        }

        if (pickup) {
            GtkWidget *pickup_item = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
            GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_accept.svg");
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(pickup_item), image);
            gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(pickup_item), TRUE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), pickup_item);
            g_signal_connect(G_OBJECT(pickup_item), "activate",
                             G_CALLBACK(call_pick_up),
                             client);
            gtk_widget_show(pickup_item);
        }

        if (hangup) {
            GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
            GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_hangup.svg");
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_hang_up),
                             client);
            gtk_widget_show(menu_items);
        }

        if (hold) {
            GtkWidget *menu_items = gtk_check_menu_item_new_with_mnemonic(_("On _Hold"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
                                           (selectedCall->_state == CALL_STATE_HOLD));
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_hold),
                             NULL);
            gtk_widget_show(menu_items);
        }

        if (record) {
            GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Record"));
            GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_RECORD,
                               GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_record),
                             client);
            gtk_widget_show(menu_items);
        }

        if (im) {
            // do not display message if instant messaging is disabled
            const gboolean instant_messaging_enabled = g_settings_get_boolean(client->settings, "instant-messaging-enabled");

            if (instant_messaging_enabled) {
                GtkWidget *menu_items = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
                gtk_widget_show(menu_items);

                menu_items = gtk_image_menu_item_new_with_mnemonic(_("Send _message"));
                GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_IM, GTK_ICON_SIZE_MENU);
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
                g_signal_connect(G_OBJECT(menu_items), "activate",
                                 G_CALLBACK(call_im),
                                 selectedCall);
                gtk_widget_show(menu_items);
            }
        }

#ifdef SFL_VIDEO
        if (video_sources) {
            GtkWidget *video_sep, *video_item, *video_menu;

            video_sep = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), video_sep);
            gtk_widget_show(video_sep);

            /* Add a video sources menu */
            video_item = gtk_image_menu_item_new_with_mnemonic(_("Video sources"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), video_item);
            gtk_widget_show(video_item);

            video_menu = gtk_menu_new();
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(video_item), video_menu);

            /* Append sources to the submenu */
            gchar **video_list = dbus_video_get_device_list();
            while (*video_list) {
                append_video_input_to_submenu(video_menu, *video_list);
                //g_free(*video_list);
                video_list++;
            }
            /* Add the special X11 device */
            append_video_input_to_submenu(video_menu, _("Screen"));
            append_video_input_to_submenu(video_menu, _("Choose file..."));
        }
#endif
    } else {
        g_debug("Build conf menus");

        if (hangup_or_hold_conf) {
            GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
            GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_hangup.svg");
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(conference_hang_up),
                             NULL);
            gtk_widget_show(menu_items);

            menu_items = gtk_check_menu_item_new_with_mnemonic(_("On _Hold"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
                                           (selectedConf->_state == CONFERENCE_STATE_HOLD ? TRUE : FALSE));
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(conference_hold),
                             NULL);
            gtk_widget_show(menu_items);
        }
    }

    if (accounts)
        add_registered_accounts_to_menu(menu);

    menu_popup_wrapper(menu, my_widget, event);
}

void
show_popup_menu_history(GtkWidget *my_widget, GdkEventButton *event, SFLPhoneClient *client)
{
    gboolean pickup = FALSE;
    gboolean add_remove_button = FALSE;
    gboolean edit = FALSE;
    gboolean accounts = FALSE;

    callable_obj_t * selectedCall = calltab_get_selected_call(history_tab);

    if (selectedCall) {
        add_remove_button = TRUE;
        pickup = TRUE;
        edit = TRUE;
        accounts = TRUE;
    }

    GtkWidget *menu = gtk_menu_new();

    if (pickup) {
        GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Call back"));
        GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(call_back), client);
        gtk_widget_show(menu_items);
    }

#ifdef SFL_PRESENCE
    if (selectedCall) {
        GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("Follow status"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
        if(!presence_buddy_list_get())
            gtk_widget_set_sensitive(menu_items, FALSE);
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(add_presence_subscription_cb), history_tab);
        gtk_widget_show(menu_items);
    }
#endif

    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_widget_show(separator);

    if (edit) {
        GtkWidget *menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_EDIT,
                                get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
        EditNumberData *edit_number_data = g_new0(EditNumberData, 1);
        edit_number_data->call = selectedCall;
        edit_number_data->client = client;
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(edit_number_cb), edit_number_data);
        gtk_widget_show(menu_items);
    }

    if (add_remove_button) {
        GtkWidget *menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE,
                                get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);

        EditNumberData *edit_number_data = g_new0(EditNumberData, 1);
        edit_number_data->call = selectedCall;
        edit_number_data->client = client;
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(remove_from_history), edit_number_data);
        gtk_widget_show(menu_items);
    }

    if (accounts)
        add_registered_accounts_to_menu(menu);

    menu_popup_wrapper(menu, my_widget, event);
}


void
show_popup_menu_contacts(GtkWidget *my_widget, GdkEventButton *event, SFLPhoneClient *client)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(contacts_tab);

    GtkWidget *menu = gtk_menu_new();

    if (selectedCall) {
        GtkWidget *new_call = gtk_image_menu_item_new_with_mnemonic(_("_New call"));
        GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(new_call), image);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_call);
        g_signal_connect(new_call, "activate", G_CALLBACK(call_back), client);
        gtk_widget_show(new_call);

#ifdef SFL_PRESENCE
        GtkWidget *presence = gtk_image_menu_item_new_with_mnemonic(_("Follow status"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), presence);

        if(!presence_buddy_list_get())
            gtk_widget_set_sensitive(presence, FALSE);

        g_signal_connect(G_OBJECT(presence), "activate", G_CALLBACK(add_presence_subscription_cb), contacts_tab);
        gtk_widget_show(presence);
#endif

        GtkWidget *edit = gtk_image_menu_item_new_from_stock(GTK_STOCK_EDIT,
                          get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), edit);
        EditNumberData *edit_number_data = g_new0(EditNumberData, 1);
        edit_number_data->call = selectedCall;
        edit_number_data->client = client;
        g_signal_connect(edit, "activate", G_CALLBACK(edit_number_cb), edit_number_data);
        gtk_widget_show(edit);

        add_registered_accounts_to_menu(menu);
    }

    menu_popup_wrapper(menu, my_widget, event);
}

static void
ok_cb(G_GNUC_UNUSED GtkWidget *widget, OkData *ok_data)
{
    // Change the number of the selected call before calling
    const gchar * const new_number = gtk_entry_get_text(GTK_ENTRY(editable_num_));
    callable_obj_t *original = ok_data->call;

    // Create the new call
    callable_obj_t *modified_call = create_new_call(CALL, CALL_STATE_DIALING, "", original->_accountID,
                                    original->_display_name, new_number);

    // Update the internal data structure and the GUI
    calllist_add_call(current_calls_tab, modified_call);
    calltree_add_call(current_calls_tab, modified_call, NULL);
    SFLPhoneClient *client = ok_data->client;
    sflphone_place_call(modified_call, client);
    calltree_display(current_calls_tab, client);

    // Close the contextual menu
    gtk_widget_destroy(edit_dialog_);
    g_free(ok_data);
}

static void
on_delete(GtkWidget * widget)
{
    gtk_widget_destroy(widget);
}

void
show_edit_number(callable_obj_t *call, SFLPhoneClient *client)
{
    edit_dialog_ = gtk_dialog_new();

    // Set window properties
    gtk_window_set_default_size(GTK_WINDOW(edit_dialog_), 300, 20);
    gtk_window_set_title(GTK_WINDOW(edit_dialog_), _("Edit phone number"));
    gtk_window_set_resizable(GTK_WINDOW(edit_dialog_), FALSE);

    g_signal_connect(G_OBJECT(edit_dialog_), "delete-event", G_CALLBACK(on_delete), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(edit_dialog_))), hbox, TRUE, TRUE, 0);

    // Set the number to be edited
    editable_num_ = gtk_entry_new();
    gtk_widget_set_tooltip_text(editable_num_,
                                _("Edit the phone number before making a call"));

    if (call)
        gtk_entry_set_text(GTK_ENTRY(editable_num_), call->_peer_number);
    else
        g_warning("This a bug, the call should be defined. menus.c line 1051");

    gtk_box_pack_start(GTK_BOX(hbox), editable_num_, TRUE, TRUE, 0);

    // Set a custom image for the button
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(ICONS_DIR "/outgoing.svg", 32, 32,
                        TRUE, NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    GtkWidget *ok = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(ok), image);
    gtk_box_pack_start(GTK_BOX(hbox), ok, TRUE, TRUE, 0);
    OkData * ok_data = g_new0(OkData, 1);
    ok_data->call = call;
    ok_data->client = client;
    g_signal_connect(ok, "clicked", G_CALLBACK(ok_cb), ok_data);

    gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(edit_dialog_)));

    gtk_dialog_run(GTK_DIALOG(edit_dialog_));
}

static GtkWidget*
create_waiting_icon()
{
    GtkWidget * waiting_icon = gtk_image_menu_item_new_with_label("");
    struct stat st;

    if (!stat(ICONS_DIR "/wait-on.gif", &st))
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(waiting_icon),
                                      gtk_image_new_from_animation(gdk_pixbuf_animation_new_from_file(
                                              ICONS_DIR "/wait-on.gif", NULL)));

    /* Deprecated:
     * gtk_menu_item_set_right_justified(GTK_MENU_ITEM(waiting_icon), TRUE); */

    return waiting_icon;
}

static GtkWidget *
get_widget(GtkUIManager *ui, const gchar *ui_path)
{
    GtkWidget *result = gtk_ui_manager_get_widget(ui, ui_path);
    if (result == NULL)
        g_warning("Could not get %s widget", ui_path);
    return result;
}

static GtkAction*
get_action(GtkUIManager *ui, const gchar *ui_path)
{
    GtkAction *result = gtk_ui_manager_get_action(ui, ui_path);
    if (result == NULL)
        g_warning("Could not get %s action", ui_path);
    return result;
}

GtkWidget *
create_menus(GtkUIManager *ui, SFLPhoneClient *client)
{
    GtkWidget *menu_bar = get_widget(ui, "/MenuBar");
    pickUpAction_ = get_action(ui, "/MenuBar/CallMenu/PickUp");
    newCallAction_ = get_action(ui, "/MenuBar/CallMenu/NewCall");
    hangUpAction_ = get_action(ui, "/MenuBar/CallMenu/HangUp");
    holdMenu_ = get_widget(ui, "/MenuBar/CallMenu/OnHoldMenu");
    recordAction_ = get_action(ui, "/MenuBar/CallMenu/Record");
    imAction_ = get_action(ui, "/MenuBar/CallMenu/InstantMessaging");
#ifdef SFL_VIDEO
    screenshareAction_ = get_action(ui, "/MenuBar/CallMenu/ScreenSharing");
#endif
    copyAction_ = get_action(ui, "/MenuBar/EditMenu/Copy");
    pasteAction_ = get_action(ui, "/MenuBar/EditMenu/Paste");
    volumeToggle_ = get_action(ui, "/MenuBar/ViewMenu/VolumeControls");
    // Set the toggle buttons
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(get_action(ui, "/MenuBar/ViewMenu/Dialpad")), g_settings_get_boolean(client->settings, "show-dialpad"));
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(volumeToggle_), must_show_volume(client));
    gtk_action_set_sensitive(volumeToggle_, TRUE);
    gtk_action_set_sensitive(get_action(ui, "/MenuBar/ViewMenu/Toolbar"), FALSE);

#ifdef SFL_PRESENCE
    gtk_action_set_sensitive(get_action(ui, "/MenuBar/ViewMenu/Buddies"), TRUE);
#endif

    /* Add the loading icon at the right of the toolbar. It is used for addressbook searches. */
    waitingLayer = create_waiting_icon();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), waitingLayer);

    return menu_bar;
}

void
create_toolbar_actions(GtkUIManager *ui, SFLPhoneClient *client)
{
    client->toolbar = get_widget(ui, "/ToolbarActions");
    holdToolbar_ = get_widget(ui, "/ToolbarActions/OnHoldToolbar");
    offHoldToolbar_ = get_widget(ui, "/ToolbarActions/OffHoldToolbar");
    transferToolbar_ = get_widget(ui, "/ToolbarActions/TransferToolbar");
    voicemailToolbar_ = get_widget(ui, "/ToolbarActions/VoicemailToolbar");
    newCallWidget_ = get_widget(ui, "/ToolbarActions/NewCallToolbar");
    pickUpWidget_ = get_widget(ui, "/ToolbarActions/PickUpToolbar");
    hangUpWidget_ = get_widget(ui, "/ToolbarActions/HangUpToolbar");
    recordWidget_ = get_widget(ui, "/ToolbarActions/RecordToolbar");
    imToolbar_ = get_widget(ui, "/ToolbarActions/InstantMessagingToolbar");
    screenshareToolbar_ = get_widget(ui, "/ToolbarActions/ScreenSharingToolbar");
/* Hide this widget if video support is disabled */
#ifndef SFL_VIDEO
    remove_from_toolbar(client->toolbar, screenshareToolbar_);
#endif
    historyButton_ = get_widget(ui, "/ToolbarActions/HistoryToolbar");
    if (addrbook)
        contactButton_ = get_widget(ui, "/ToolbarActions/AddressbookToolbar");

    // Set the handler ID for the transfer
    g_assert(transferToolbar_);
    transferButtonConnId_ = g_signal_connect(G_OBJECT(transferToolbar_), "toggled", G_CALLBACK(call_transfer_cb), client);
    g_assert(recordWidget_);
    recordButtonConnId_ = g_signal_connect(G_OBJECT(recordWidget_), "toggled", G_CALLBACK(call_record), client);
    active_calltree_tab = current_calls_tab;
}
