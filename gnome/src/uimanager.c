/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "config.h"
#include "preferencesdialog.h"
#include "logger.h"
#include "dbus/dbus.h"
#include "mainwindow.h"
#include "assistant.h"
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gprintf.h>

#include "uimanager.h"
#include "statusicon.h"
#include "widget/imwidget.h"
#include "eel-gconf-extensions.h"

#include "config/audioconf.h"
#include "unused.h"
#include "uimanager.h"
#include "statusicon.h"

#include "contacts/addrbookfactory.h"
#include "contacts/calltab.h"
#include "config/addressbook-config.h"

#include "eel-gconf-extensions.h"

#include "accountlist.h"
#include "config/accountlistconfigdialog.h"

#include <sys/stat.h>

#include <sliders.h>

void show_edit_number(callable_obj_t *call);

static GtkWidget *toolbar_;

static guint transferButtonConnId_; //The button toggled signal connection ID
static guint recordButtonConnId_; //The button toggled signal connection ID
static guint muteCallButtonId_; //The button toggled signal connection ID

static GtkAction * pickUpAction_;
static GtkWidget * pickUpWidget_;
static GtkAction * newCallAction_;
static GtkWidget * newCallWidget_;
static GtkAction * hangUpAction_;
static GtkWidget * hangUpWidget_;
static GtkWidget * holdMenu_;
static GtkWidget * holdToolbar_;
static GtkWidget * offHoldToolbar_;
static GtkWidget * transferToolbar_;
static GtkAction * copyAction_;
static GtkAction * pasteAction_;
static GtkAction * recordAction_;
static GtkAction * muteAction_;
static GtkWidget * recordWidget_;
static GtkWidget * muteWidget_;
static GtkAction * voicemailAction_;
static GtkWidget * voicemailToolbar_;
static GtkWidget * imToolbar_;
static GtkAction * imAction_;
static GtkWidget * playRecordWidget_;
static GtkWidget * stopRecordWidget_;

static GtkWidget * editable_num_;
static GtkWidget * edit_dialog_;

enum {
    CALLTREE_CALLS, CALLTREE_HISTORY, CALLTREE_CONTACTS
};

static void
remove_from_toolbar(GtkWidget *widget)
{
    /* We must ensure that a widget is a child of a container
     * before removing it. */
    if (gtk_widget_get_parent(widget) == toolbar_)
        gtk_container_remove(GTK_CONTAINER(toolbar_), widget);
}

static bool
is_non_empty(const char *str)
{
    return str && strlen(str) > 0;
}

/* Inserts an item in a toolbar at a given position, making sure that the index
 * is valid, that it does not exceed the number of elements */
static void add_to_toolbar(GtkWidget *toolbar, GtkWidget *item, int pos)
{
    g_assert(gtk_toolbar_get_n_items(GTK_TOOLBAR(toolbar)) >= pos);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), pos);
}

static void
call_mute(void)
{
    DEBUG("UIManager: Mute call button pressed");
    sflphone_mute_call();
}

void
update_actions()
{
    int pos = 0;

    gtk_action_set_sensitive(newCallAction_, TRUE);
    gtk_action_set_sensitive(pickUpAction_, FALSE);
    gtk_action_set_sensitive(hangUpAction_, FALSE);
    gtk_action_set_sensitive(imAction_, FALSE);

    g_object_ref(hangUpWidget_);
    g_object_ref(recordWidget_);
    g_object_ref(muteWidget_);
    g_object_ref(holdToolbar_);
    g_object_ref(offHoldToolbar_);

    if (addrbook)
        g_object_ref(contactButton_);

    g_object_ref(historyButton_);
    g_object_ref(transferToolbar_);
    g_object_ref(voicemailToolbar_);
    g_object_ref(imToolbar_);

    remove_from_toolbar(hangUpWidget_);
    remove_from_toolbar(recordWidget_);
    remove_from_toolbar(muteWidget_);
    remove_from_toolbar(transferToolbar_);
    remove_from_toolbar(historyButton_);

    if (addrbook)
        remove_from_toolbar(contactButton_);

    remove_from_toolbar(voicemailToolbar_);
    remove_from_toolbar(imToolbar_);

    gtk_widget_set_sensitive(holdMenu_, FALSE);
    gtk_widget_set_sensitive(holdToolbar_, FALSE);
    gtk_widget_set_sensitive(offHoldToolbar_, FALSE);
    gtk_action_set_sensitive(recordAction_, FALSE);
    gtk_action_set_sensitive(muteAction_, FALSE);
    gtk_widget_set_sensitive(recordWidget_, FALSE);
    gtk_widget_set_sensitive(muteWidget_, FALSE);
    gtk_action_set_sensitive(copyAction_, FALSE);

    if (addrbook)
        gtk_widget_set_sensitive(contactButton_, FALSE);

    gtk_widget_set_sensitive(historyButton_, FALSE);

    if (addrbook)
        gtk_widget_set_tooltip_text(contactButton_, _("No address book selected"));

    remove_from_toolbar(holdToolbar_);
    remove_from_toolbar(offHoldToolbar_);
    remove_from_toolbar(newCallWidget_);
    remove_from_toolbar(pickUpWidget_);

    add_to_toolbar(toolbar_, newCallWidget_, 0);

    remove_from_toolbar(playRecordWidget_);
    remove_from_toolbar(stopRecordWidget_);

    if (eel_gconf_get_integer(HISTORY_ENABLED)) {
        add_to_toolbar(toolbar_, historyButton_, -1);
        gtk_widget_set_sensitive(historyButton_, TRUE);
    }

    GtkToolItem *separator = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar_), separator, -1);
    // add mute button
    add_to_toolbar(toolbar_, muteWidget_, -1);
    gtk_action_set_sensitive(muteAction_, TRUE);

    // If addressbook support has been enabled and all addressbooks are loaded, display the icon
    if (addrbook && addrbook->is_ready() && addressbook_config_load_parameters()->enable) {
        add_to_toolbar(toolbar_, contactButton_, -1);

        // Make the icon clickable only if at least one address book is active
        if (addrbook->is_active()) {
            gtk_widget_set_sensitive(contactButton_, TRUE);
            gtk_widget_set_tooltip_text(contactButton_, _("Address book"));
        }
    }

    callable_obj_t * selectedCall = calltab_get_selected_call(active_calltree_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(active_calltree_tab);

    gboolean instant_messaging_enabled = TRUE;

    if (eel_gconf_key_exists(INSTANT_MESSAGING_ENABLED))
        instant_messaging_enabled = eel_gconf_get_integer(INSTANT_MESSAGING_ENABLED);

    if (selectedCall) {
        DEBUG("UIManager: Update actions for call %s", selectedCall->_callID);

        // update icon in systray
        show_status_hangup_icon();

        gtk_action_set_sensitive(copyAction_, TRUE);

        switch (selectedCall->_state) {
            case CALL_STATE_INCOMING:
                {
                    DEBUG("UIManager: Call State Incoming");
                    // Make the button toolbar clickable
                    gtk_action_set_sensitive(pickUpAction_, TRUE);
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    // Replace the dial button with the hangup button
                    g_object_ref(newCallWidget_);
                    remove_from_toolbar(newCallWidget_);
                    pos = 0;
                    add_to_toolbar(toolbar_, pickUpWidget_, pos++);
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    break;
                }
            case CALL_STATE_HOLD:
                {
                    DEBUG("UIManager: Call State Hold");
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    gtk_widget_set_sensitive(holdMenu_, TRUE);
                    gtk_widget_set_sensitive(offHoldToolbar_, TRUE);
                    gtk_widget_set_sensitive(newCallWidget_, TRUE);

                    // Replace the hold button with the off-hold button
                    pos = 1;
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    add_to_toolbar(toolbar_, offHoldToolbar_, pos++);

                    if (instant_messaging_enabled) {
                        gtk_action_set_sensitive(imAction_, TRUE);
                        add_to_toolbar(toolbar_, imToolbar_, pos++);
                    }

                    break;
                }
            case CALL_STATE_RINGING:
                {
                    DEBUG("UIManager: Call State Ringing");
                    gtk_action_set_sensitive(pickUpAction_, TRUE);
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    pos = 1;
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    break;
                }
            case CALL_STATE_DIALING:
                {
                    DEBUG("UIManager: Call State Dialing");
                    gtk_action_set_sensitive(pickUpAction_, TRUE);

                    if (active_calltree_tab == current_calls_tab)
                        gtk_action_set_sensitive(hangUpAction_, TRUE);

                    g_object_ref(newCallWidget_);
                    remove_from_toolbar(newCallWidget_);
                    pos = 0;
                    add_to_toolbar(toolbar_, pickUpWidget_, pos++);

                    if (active_calltree_tab == current_calls_tab)
                        add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    else if (active_calltree_tab == history_tab) {
                        if (is_non_empty(selectedCall->_recordfile)) {
                            if (selectedCall->_record_is_playing)
                                add_to_toolbar(toolbar_, stopRecordWidget_, pos++);
                            else
                                add_to_toolbar(toolbar_, playRecordWidget_, pos++);
                        }
                    }
                    break;
                }
            case CALL_STATE_CURRENT:
                {
                    DEBUG("UIManager: Call State Current");
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    pos = 1;
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    gtk_widget_set_sensitive(holdMenu_, TRUE);
                    gtk_widget_set_sensitive(holdToolbar_, TRUE);
                    gtk_widget_set_sensitive(transferToolbar_, TRUE);
                    gtk_action_set_sensitive(recordAction_, TRUE);
                    add_to_toolbar(toolbar_, holdToolbar_, pos++);
                    add_to_toolbar(toolbar_, transferToolbar_, pos++);
                    add_to_toolbar(toolbar_, recordWidget_, pos++);
                    g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), FALSE);
                    g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                    g_signal_handler_block(recordWidget_, recordButtonConnId_);
                    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(recordWidget_), FALSE);
                    g_signal_handler_unblock(recordWidget_, recordButtonConnId_);

                    if (instant_messaging_enabled) {
                        gtk_action_set_sensitive(imAction_, TRUE);
                        add_to_toolbar(toolbar_, imToolbar_, pos++);
                    }

                    break;
                }

            case CALL_STATE_RECORD:
                {
                    DEBUG("UIManager: Call State Record");
                    pos = 1;
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    gtk_widget_set_sensitive(holdMenu_, TRUE);
                    gtk_widget_set_sensitive(holdToolbar_, TRUE);
                    gtk_widget_set_sensitive(transferToolbar_, TRUE);
                    gtk_action_set_sensitive(recordAction_, TRUE);
                    add_to_toolbar(toolbar_, holdToolbar_, pos++);
                    add_to_toolbar(toolbar_, transferToolbar_, pos++);
                    add_to_toolbar(toolbar_, recordWidget_, pos++);
                    g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), FALSE);
                    g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                    g_signal_handler_block(recordWidget_, recordButtonConnId_);
                    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(recordWidget_), TRUE);
                    g_signal_handler_unblock(recordWidget_, recordButtonConnId_);

                    if (instant_messaging_enabled) {
                        gtk_action_set_sensitive(imAction_, TRUE);
                        add_to_toolbar(toolbar_, imToolbar_, pos++);
                    }

                    break;
                }
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
                {
                    pos = 1;
                    DEBUG("UIManager: Call State Busy/Failure");
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    break;
                }
            case CALL_STATE_TRANSFER:
                {
                    pos = 1;
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    add_to_toolbar(toolbar_, transferToolbar_, pos++);
                    g_signal_handler_block(transferToolbar_, transferButtonConnId_);
                    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_), TRUE);
                    g_signal_handler_unblock(transferToolbar_, transferButtonConnId_);
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    gtk_widget_set_sensitive(holdMenu_, TRUE);
                    gtk_widget_set_sensitive(holdToolbar_, TRUE);
                    gtk_widget_set_sensitive(transferToolbar_, TRUE);
                    break;
                }
            default:
                ERROR("UIMAnager: Error: Unknown state in action update!");
                break;
        }

    } else if (selectedConf) {

        DEBUG("UIManager: Update actions for conference");

        // update icon in systray
        show_status_hangup_icon();

        switch (selectedConf->_state) {

            case CONFERENCE_STATE_ACTIVE_ATTACHED:
            case CONFERENCE_STATE_ACTIVE_DETACHED:
                DEBUG("UIManager: Conference State Active");

                if (active_calltree_tab == current_calls_tab) {
                    gtk_action_set_sensitive(hangUpAction_, TRUE);
                    gtk_widget_set_sensitive(holdToolbar_, TRUE);
                    gtk_action_set_sensitive(recordAction_, TRUE);
                    pos = 1;
                    add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                    add_to_toolbar(toolbar_, holdToolbar_, pos++);
                    add_to_toolbar(toolbar_, recordWidget_, pos++);

                    if (instant_messaging_enabled) {
                        gtk_action_set_sensitive(imAction_, TRUE);
                        add_to_toolbar(toolbar_, imToolbar_, pos);
                    }
                } else if (active_calltree_tab == history_tab) {
                    if (is_non_empty(selectedConf->_recordfile)) {
                        pos = 2;
                        if (selectedConf->_record_is_playing)
                            add_to_toolbar(toolbar_, stopRecordWidget_, pos);
                        else
                            add_to_toolbar(toolbar_, playRecordWidget_, pos);
                    }
                }

                break;
            case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
            case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD: {
                pos = 1;
                DEBUG("UIManager: Conference State Record");
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                gtk_widget_set_sensitive(holdToolbar_, TRUE);
                gtk_action_set_sensitive(recordAction_, TRUE);
                add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                add_to_toolbar(toolbar_, holdToolbar_, pos++);
                add_to_toolbar(toolbar_, recordWidget_, pos++);

                if (instant_messaging_enabled) {
                    gtk_action_set_sensitive(imAction_, TRUE);
                    add_to_toolbar(toolbar_, imToolbar_, pos);
                }

                break;
            }
            case CONFERENCE_STATE_HOLD:
            case CONFERENCE_STATE_HOLD_RECORD: {
                DEBUG("UIManager: Conference State Hold");
                pos = 1;
                gtk_action_set_sensitive(hangUpAction_, TRUE);
                gtk_widget_set_sensitive(offHoldToolbar_, TRUE);
                gtk_action_set_sensitive(recordAction_, TRUE);
                add_to_toolbar(toolbar_, hangUpWidget_, pos++);
                add_to_toolbar(toolbar_, offHoldToolbar_, pos++);
                add_to_toolbar(toolbar_, recordWidget_, pos++);

                if (instant_messaging_enabled) {
                    gtk_action_set_sensitive(imAction_, TRUE);
                    add_to_toolbar(toolbar_, imToolbar_, pos);
                }

                break;
            }
            default:
                WARN("UIManager: Error: Should not happen in action update!");
                break;
        }
    } else {
        // update icon in systray
        hide_status_hangup_icon();

        if (account_list_get_size() > 0 && current_account_has_mailbox()) {
            add_to_toolbar(toolbar_, voicemailToolbar_, -1);
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
        gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(voicemailToolbar_),
                                      "mail-message-new");
    else
        gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(voicemailToolbar_),
                                      "mail-read");

    gtk_tool_button_set_label(GTK_TOOL_BUTTON(voicemailToolbar_), messages);
    g_free(messages);
}

static void
volume_bar_cb(GtkToggleAction *togglemenuitem, gpointer user_data UNUSED)
{
    gboolean toggled = gtk_toggle_action_get_active(togglemenuitem);

    if (toggled == SHOW_VOLUME)
        return;

    main_window_volume_controls(toggled);

    if (toggled || SHOW_VOLUME)
        eel_gconf_set_integer(SHOW_VOLUME_CONTROLS, toggled);
}

static void
dialpad_bar_cb(GtkToggleAction *togglemenuitem, gpointer user_data UNUSED)
{
    gboolean toggled = gtk_toggle_action_get_active(togglemenuitem);
    gboolean conf_dialpad = eel_gconf_get_boolean(CONF_SHOW_DIALPAD);

    if (toggled == conf_dialpad)
        return;

    main_window_dialpad(toggled);

    if (toggled || conf_dialpad)
        eel_gconf_set_boolean(CONF_SHOW_DIALPAD, toggled);
}

static void
help_contents_cb(GtkAction *action UNUSED)
{
    GError *error = NULL;
    gtk_show_uri(NULL, "ghelp:sflphone", GDK_CURRENT_TIME, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

static void
help_about(void * foo UNUSED)
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
        "Alexandre Savard <alexandre.savard@savoirfairelinux.com>", NULL
    };
    static const gchar *artists[] = {
        "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>",
        "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>", NULL
    };

    gtk_show_about_dialog(GTK_WINDOW(get_main_window()),
            "artists", artists,
            "authors", authors,
            "comments", _("SFLphone is a VoIP client compatible with SIP and IAX2 protocols."),
            "copyright", "Copyright © 2004-2011 Savoir-faire Linux Inc.",
            "name", PACKAGE,
            "title", _("About SFLphone"),
            "version", VERSION,
            "website", "http://www.sflphone.org",
            NULL);
}

/* ----------------------------------------------------------------- */

static void
call_new_call(void * foo UNUSED)
{
    DEBUG("UIManager: New call button pressed");
    sflphone_new_call();
}

static void
call_quit(void * foo UNUSED)
{
    sflphone_quit();
}

static void
call_minimize(void * foo UNUSED)
{
    if (eel_gconf_get_integer(SHOW_STATUSICON)) {
        gtk_widget_hide(get_main_window());
        set_minimized(TRUE);
    } else
        sflphone_quit();
}

static void
switch_account(GtkWidget* item, gpointer data UNUSED)
{
    account_t* acc = g_object_get_data(G_OBJECT(item), "account");
    DEBUG("%s" , acc->accountID);
    account_list_set_current(acc);
    status_bar_display_account();
}

static void
call_hold(void* foo UNUSED)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    DEBUG("UIManager: Hold button pressed");

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
call_im(void* foo UNUSED)
{
    callable_obj_t *selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t *selectedConf = calltab_get_selected_conf(current_calls_tab);

    if (calltab_get_selected_type(current_calls_tab) == A_CALL) {
        if (selectedCall) {
            if (!selectedCall->_im_widget)
                selectedCall->_im_widget = im_widget_display(selectedCall->_callID);
        } else
            WARN("Sorry. Instant messaging is not allowed outside a call\n");
    } else {
        if (selectedConf) {
            if (!selectedConf->_im_widget)
                selectedConf->_im_widget = im_widget_display(selectedConf->_confID);
        } else
            WARN("Sorry. Instant messaging is not allowed outside a call\n");
    }
}

static void
conference_hold(void* foo UNUSED)
{
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    DEBUG("UIManager: Hold button pressed for conference");

    if (selectedConf == NULL) {
        ERROR("UIManager: No conference selected");
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
call_pick_up(void * foo UNUSED)
{
    DEBUG("UIManager: Pick up");

    if (calllist_get_size(current_calls_tab) > 0) {
        sflphone_pick_up();
    } else if (calllist_get_size(active_calltree_tab) > 0) {
        callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);

        if (selectedCall) {
            callable_obj_t *new_call = create_new_call(CALL, CALL_STATE_DIALING, "", "", "",
                                       selectedCall->_peer_number);
            calllist_add_call(current_calls_tab, new_call);
            calltree_add_call(current_calls_tab, new_call, NULL);
            sflphone_place_call(new_call);
            calltree_display(current_calls_tab);
        } else {
            sflphone_new_call();
            calltree_display(current_calls_tab);
        }
    } else {
        sflphone_new_call();
        calltree_display(current_calls_tab);
    }
}

static void
call_hang_up(void)
{
    DEBUG("UIManager: Hang up button pressed(call)");
    /*
     * [#3020]	Restore the record toggle button
     *			We set it to FALSE, as when we hang up a call, the recording is stopped.
     */

    sflphone_hang_up();
}

static void
conference_hang_up(void)
{
    DEBUG("UIManager: Hang up button pressed(conference)");
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    if (selectedConf)
        dbus_hang_up_conference(selectedConf);
}

static void
call_record(void)
{
    DEBUG("UIManager: Record button pressed");
    sflphone_rec_call();
}

static void
start_playback_record_cb(void)
{
    DEBUG("UIManager: Start playback button pressed");

    callable_obj_t *selectedCall = calltab_get_selected_call(history_tab);

    if (selectedCall == NULL) {
        ERROR("UIManager: Error: No selected object in playback record callback");
        return;
    }

    DEBUG("UIManager: Start selected call file playback %s", selectedCall->_recordfile);
    selectedCall->_record_is_playing = dbus_start_recorded_file_playback(selectedCall->_recordfile);

    update_actions();
}

static void
stop_playback_record_cb(void)
{
    DEBUG("UIManager: Stop playback button pressed");

    callable_obj_t *selectedCall = calltab_get_selected_call(history_tab);

    if (selectedCall == NULL) {
        ERROR("UIManager: Error: No selected object in history treeview");
        return;
    }

    if (selectedCall) {
        if (selectedCall->_recordfile == NULL) {
            ERROR("UIManager: Error: Record file is NULL");
            return;
        }

        dbus_stop_recorded_file_playback(selectedCall->_recordfile);
        DEBUG("UIManager: Stop selected call file playback %s", selectedCall->_recordfile);
        selectedCall->_record_is_playing = FALSE;
    }

    update_actions();
}

static void
call_configuration_assistant(void * foo UNUSED)
{
    build_wizard();
}

static void
remove_from_history(void * foo UNUSED)
{
    callable_obj_t* call = calltab_get_selected_call(history_tab);

    DEBUG("UIManager: Remove the call from the history");

    if (call == NULL) {
        ERROR("UIManager: Error: Call is NULL");
        return;
    }

    calllist_remove_from_history(call);
}

static void
call_back(void * foo UNUSED)
{
    callable_obj_t *selected_call = calltab_get_selected_call(active_calltree_tab);

    DEBUG("UIManager: Call back");

    if (selected_call == NULL) {
        ERROR("UIManager: Error: No selected call");
        return;
    }

    callable_obj_t *new_call = create_new_call(CALL, CALL_STATE_DIALING, "",
                               "", selected_call->_display_name,
                               selected_call->_peer_number);

    calllist_add_call(current_calls_tab, new_call);
    calltree_add_call(current_calls_tab, new_call, NULL);
    sflphone_place_call(new_call);
    calltree_display(current_calls_tab);
}

static void
edit_preferences(void * foo UNUSED)
{
    show_preferences_dialog();
}

static void
edit_accounts(void * foo UNUSED)
{
    show_account_list_config_dialog();
}

// The menu Edit/Copy should copy the current selected call's number
static void
edit_copy(void * foo UNUSED)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);

    DEBUG("UIManager: Edit/Copy");

    if (selectedCall == NULL) {
        ERROR("UIManager: Error: No selected call", selectedCall);
        return;
    }

    DEBUG("UIManager: Clipboard number: %s\n", selectedCall->_peer_number);
    gtk_clipboard_set_text(clip, selectedCall->_peer_number,
                           strlen(selectedCall->_peer_number));
}

// The menu Edit/Paste should paste the clipboard into the current selected call
static void
edit_paste(void * foo UNUSED)
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
                DEBUG("TO: %s\n", old);
                selectedCall->_peer_number = g_strconcat(old, no, NULL);
                g_free(old);

                if (selectedCall->_state == CALL_STATE_DIALING)
                    selectedCall->_peer_info = g_strconcat("\"\" <",
                                                           selectedCall->_peer_number, ">", NULL);

                calltree_update_call(current_calls_tab, selectedCall);
            }
            break;
            case CALL_STATE_RINGING:
            case CALL_STATE_INCOMING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
            case CALL_STATE_HOLD: { // Create a new call to hold the new text
                selectedCall = sflphone_new_call();

                gchar *old = selectedCall->_peer_number;
                selectedCall->_peer_number = g_strconcat(old, no, NULL);
                g_free(old);
                DEBUG("TO: %s", selectedCall->_peer_number);

                g_free(selectedCall->_peer_info);
                selectedCall->_peer_info = g_strconcat("\"\" <",
                                                       selectedCall->_peer_number, ">", NULL);

                calltree_update_call(current_calls_tab, selectedCall);
            }
            break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_RECORD:
            default: {
                for (unsigned i = 0; i < strlen(no); i++) {
                    gchar * oneNo = g_strndup(&no[i], 1);
                    DEBUG("<%s>", oneNo);
                    dbus_play_dtmf(oneNo);

                    gchar * temp = g_strconcat(selectedCall->_peer_number,
                                               oneNo, NULL);
                    g_free(selectedCall->_peer_info);
                    selectedCall->_peer_info = get_peer_info(temp, selectedCall->_display_name);
                    g_free(temp);
                    g_free(oneNo);
                    calltree_update_call(current_calls_tab, selectedCall);
                }
            }
            break;
        }
    } else { // There is no current call, create one
        selectedCall = sflphone_new_call();

        gchar * old = selectedCall->_peer_number;
        selectedCall->_peer_number = g_strconcat(old, no, NULL);
        g_free(old);
        DEBUG("UIManager: TO: %s", selectedCall->_peer_number);

        g_free(selectedCall->_peer_info);
        selectedCall->_peer_info = g_strconcat("\"\" <",
                                               selectedCall->_peer_number, ">", NULL);
        calltree_update_call(current_calls_tab, selectedCall);
    }

    g_free(no);
}

static void
clear_history(void)
{
    calllist_clean_history();
    dbus_clear_history();
}

/**
 * Transfer the line
 */
static void
call_transfer_cb()
{
    if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(transferToolbar_)))
        sflphone_set_transfer();
    else
        sflphone_unset_transfer();
}

static void
call_mailbox_cb(void)
{
    account_t *current = account_list_get_current();

    if (current == NULL) // Should not happens
        return;

    const gchar * const to = g_hash_table_lookup(current->properties, ACCOUNT_MAILBOX);
    const gchar * const account_id = g_strdup(current->accountID);

    callable_obj_t *mailbox_call = create_new_call(CALL, CALL_STATE_DIALING,
                                   "", account_id,
                                   _("Voicemail"), to);
    DEBUG("TO : %s" , mailbox_call->_peer_number);
    calllist_add_call(current_calls_tab, mailbox_call);
    calltree_add_call(current_calls_tab, mailbox_call, NULL);
    update_actions();
    sflphone_place_call(mailbox_call);
    calltree_display(current_calls_tab);
}

static void
toggle_history_cb(GtkToggleAction *action, gpointer user_data UNUSED)
{
    if (gtk_toggle_action_get_active(action))
        calltree_display(history_tab);
    else
        calltree_display(current_calls_tab);
}

static void
toggle_addressbook_cb(GtkToggleAction *action, gpointer user_data UNUSED)
{
    if (gtk_toggle_action_get_active(action))
        calltree_display(contacts_tab);
    else
        calltree_display(current_calls_tab);
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
    {
        "StartPlaybackRecord", GTK_STOCK_MEDIA_PLAY,  N_("_Playback record"), NULL,
        N_("Playback recorded file"), G_CALLBACK(start_playback_record_cb)
    },
    {
        "StopPlaybackRecord", GTK_STOCK_MEDIA_PAUSE, N_("_Stop playback"), NULL,
        N_("Stop recorded file playback"), G_CALLBACK(stop_playback_record_cb)
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

static void register_custom_stock_icon(void) {

    static gboolean registered = FALSE;

    if (!registered) {
        GdkPixbuf *pixbuf;
        GtkIconFactory *factory;

        static GtkStockItem items[] = {
            { "SFLPHONE_MUTE_CALL",
              "_GTK!",
              0, 0, NULL }
        };

        registered = TRUE;

        /* Register our stock items */
        gtk_stock_add (items, G_N_ELEMENTS (items));

        /* Add our custom icon factory to the list of defaults */
        factory = gtk_icon_factory_new ();
        gtk_icon_factory_add_default (factory);

        /* demo_find_file() looks in the current directory first,
         * so you can run gtk-demo without installing GTK, then looks
         * in the location where the file is installed.
         */
        pixbuf = NULL;
        pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/mic.svg", NULL);
        if(pixbuf == NULL) {
            DEBUG("Error could not create mic.svg pixbuf");
        }

        /* Register icon to accompany stock item */
        if (pixbuf != NULL) {
            GtkIconSet *icon_set;
            GdkPixbuf *transparent;

            /* The gtk-logo-rgb icon has a white background, make it transparent */
            transparent = gdk_pixbuf_add_alpha (pixbuf, TRUE, 0xff, 0xff, 0xff);

            icon_set = gtk_icon_set_new_from_pixbuf (transparent);
            gtk_icon_factory_add (factory, "SFLPHONE_MUTE_CALL", icon_set);
            gtk_icon_set_unref (icon_set);
            g_object_unref (pixbuf);
            g_object_unref (transparent);
        }
        else
            g_warning ("failed to load GTK logo for toolbar");

        /* Drop our reference to the factory, GTK will hold a reference. */
        g_object_unref (factory);
    }
}

static const GtkToggleActionEntry toggle_menu_entries[] = {
    { "Transfer", GTK_STOCK_TRANSFER, N_("_Transfer"), "<control>T", N_("Transfer the call"), NULL, TRUE },
    { "Record", GTK_STOCK_MEDIA_RECORD, N_("_Record"), "<control>R", N_("Record the current conversation"), NULL, TRUE },
    { "Mute", "SFLPHONE_MUTE_CALL", N_("_Mute"), "<control>M", N_("Mute microphone for this call"), G_CALLBACK(call_mute), FALSE },
    { "Toolbar", NULL, N_("_Show toolbar"), "<control>T", N_("Show the toolbar"), NULL, TRUE },
    { "Dialpad", NULL, N_("_Dialpad"), "<control>D", N_("Show the dialpad"), G_CALLBACK(dialpad_bar_cb), TRUE },
    { "VolumeControls", NULL, N_("_Volume controls"), "<control>V", N_("Show the volume controls"), G_CALLBACK(volume_bar_cb), TRUE },
    { "History", "appointment-soon", N_("_History"), NULL, N_("Calls history"), G_CALLBACK(toggle_history_cb), FALSE },
    { "Addressbook", GTK_STOCK_ADDRESSBOOK, N_("_Address book"), NULL, N_("Address book"), G_CALLBACK(toggle_addressbook_cb), FALSE },
};

GtkUIManager *uimanager_new(void)
{
    gint nb_entries = addrbook ? 8 : 7;

    GtkWidget *window = get_main_window();
    GtkUIManager *ui_manager = gtk_ui_manager_new();

    /* Register new icons as GTK_STOCK_ITEMS */
    register_custom_stock_icon();

    /* Create an accel group for window's shortcuts */
    gchar *path = g_build_filename(SFLPHONE_UIDIR_UNINSTALLED, "./ui.xml", NULL);
    guint manager_id;
    GError *error = NULL;

    if (g_file_test(path, G_FILE_TEST_EXISTS))
        manager_id = gtk_ui_manager_add_ui_from_file(ui_manager, path, &error);
    else {
        g_free(path);
        path = g_build_filename(SFLPHONE_UIDIR, "./ui.xml", NULL);

        if (!g_file_test(path, G_FILE_TEST_EXISTS))
            goto fail;

        manager_id = gtk_ui_manager_add_ui_from_file(ui_manager, path, &error);
    }

    if (error)
        goto fail;

    g_free(path);

    if (addrbook) {
        // These actions must be loaded dynamically and is not specified in the xml description
        gtk_ui_manager_add_ui(ui_manager, manager_id, "/ViewMenu",
                              "Addressbook",
                              "Addressbook",
                              GTK_UI_MANAGER_MENUITEM, FALSE);
        gtk_ui_manager_add_ui(ui_manager, manager_id,  "/ToolbarActions",
                              "AddressbookToolbar",
                              "Addressbook",
                              GTK_UI_MANAGER_TOOLITEM, FALSE);
    }

    GtkActionGroup *action_group = gtk_action_group_new("SFLphoneWindowActions");
    // To translate label and tooltip entries
    gtk_action_group_set_translation_domain(action_group, "sflphone-client-gnome");
    gtk_action_group_add_actions(action_group, menu_entries, G_N_ELEMENTS(menu_entries), window);
    gtk_action_group_add_toggle_actions(action_group, toggle_menu_entries, nb_entries, window);
    //gtk_action_group_add_radio_actions(action_group, radio_menu_entries, G_N_ELEMENTS(radio_menu_entries), CALLTREE_CALLS, G_CALLBACK(calltree_switch_cb), window);
    gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);

    return ui_manager;

fail:

    if (error)
        g_error_free(error);

    g_free(path);
    return NULL;
}

static void
edit_number_cb(GtkWidget *widget UNUSED, gpointer user_data)
{
    show_edit_number((callable_obj_t*) user_data);
}

void
add_registered_accounts_to_menu(GtkWidget *menu)
{
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_widget_show(separator);

    for (unsigned i = 0; i != account_list_get_size(); i++) {
        account_t *acc = account_list_get_nth(i);

        // Display only the registered accounts
        if (g_strcasecmp(account_state_name(acc->state), account_state_name(
                             ACCOUNT_STATE_REGISTERED)) == 0) {
            gchar *alias = g_strconcat(g_hash_table_lookup(acc->properties, ACCOUNT_ALIAS),
                                       " - ",
                                       g_hash_table_lookup(acc->properties, ACCOUNT_TYPE),
                                       NULL);
            GtkWidget *menu_items = gtk_check_menu_item_new_with_mnemonic(alias);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_object_set_data(G_OBJECT(menu_items), "account", acc);
            g_free(alias);
            account_t *current = account_list_get_current();

            if (current) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
                                               g_strcasecmp(acc->accountID, current->accountID) == 0);
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

void
show_popup_menu(GtkWidget *my_widget, GdkEventButton *event)
{
    // TODO update the selection to make sure the call under the mouse is the call selected
    gboolean pickup = FALSE, hangup = FALSE, hold = FALSE, copy = FALSE, record = FALSE, im = FALSE, mute = FALSE;
    gboolean accounts = FALSE;

    // conference type boolean
    gboolean hangup_or_hold_conf = FALSE;

    callable_obj_t * selectedCall = NULL;
    conference_obj_t * selectedConf = NULL;

    if (calltab_get_selected_type(current_calls_tab) == A_CALL) {
        DEBUG("UIManager: Menus: Selected a call");
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
                case CALL_STATE_RECORD:
                case CALL_STATE_CURRENT:
                    hangup = TRUE;
                    hold = TRUE;
                    record = TRUE;
                    im = TRUE;
                    mute = TRUE;
                    break;
                case CALL_STATE_BUSY:
                case CALL_STATE_FAILURE:
                    hangup = TRUE;
                    break;
                default:
                    WARN("UIManager: Should not happen in show_popup_menu for calls!")
                    ;
                    break;
            }
        }
    } else {
        DEBUG("UIManager: Menus: selected a conf");
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
                    WARN("UIManager: Should not happen in show_popup_menu for conferences!")
                    ;
                    break;
            }
        }
    }

    GtkWidget *menu = gtk_menu_new();

    if (calltab_get_selected_type(current_calls_tab) == A_CALL) {
        DEBUG("UIManager: Build call menu");
        if (copy) {
            GtkWidget *menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY,
                                    get_accel_group());
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(edit_copy),
                             NULL);
            gtk_widget_show(menu_items);
        }

        GtkWidget *paste = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE,
                           get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste);
        g_signal_connect(G_OBJECT(paste), "activate", G_CALLBACK(edit_paste),
                         NULL);
        gtk_widget_show(paste);

        if (pickup || hangup || hold) {
            GtkWidget *menu_items = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            gtk_widget_show(menu_items);
        }

        if (pickup) {
            GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
            GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_accept.svg");
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_pick_up),
                             NULL);
            gtk_widget_show(menu_items);
        }

        if (hangup) {
            GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
            GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_hangup.svg");
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_hang_up),
                             NULL);
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
            GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_MEDIA_RECORD,
                               GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_record),
                             NULL);
            gtk_widget_show(menu_items);
        }

        if (mute) {
            GtkWidget *menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Mute"));
            GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_MEDIA_RECORD,
                               GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
            g_signal_connect(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(call_mute),
                             NULL);
            gtk_widget_show(menu_items);
        }



        if (im) {
            // do not display message if instant messaging is disabled
            gboolean instant_messaging_enabled = TRUE;

            if (eel_gconf_key_exists(INSTANT_MESSAGING_ENABLED))
                instant_messaging_enabled = eel_gconf_get_integer(INSTANT_MESSAGING_ENABLED);

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
                                 NULL);
                gtk_widget_show(menu_items);
            }
        }

    } else {
        DEBUG("UIManager: Build conf menus");

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
show_popup_menu_history(GtkWidget *my_widget, GdkEventButton *event)
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
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(call_back), NULL);
        gtk_widget_show(menu_items);
    }

    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_widget_show(separator);

    if (edit) {
        GtkWidget *menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_EDIT,
                                get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(edit_number_cb), selectedCall);
        gtk_widget_show(menu_items);
    }

    if (add_remove_button) {
        GtkWidget *menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE,
                                get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_items);
        g_signal_connect(G_OBJECT(menu_items), "activate", G_CALLBACK(remove_from_history), NULL);
        gtk_widget_show(menu_items);
    }

    if (accounts)
        add_registered_accounts_to_menu(menu);

    menu_popup_wrapper(menu, my_widget, event);
}


void
show_popup_menu_contacts(GtkWidget *my_widget, GdkEventButton *event)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(contacts_tab);

    GtkWidget *menu = gtk_menu_new();

    if (selectedCall) {
        GtkWidget *new_call = gtk_image_menu_item_new_with_mnemonic(_("_New call"));
        GtkWidget *image = gtk_image_new_from_file(ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(new_call), image);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_call);
        g_signal_connect(new_call, "activate", G_CALLBACK(call_back), NULL);
        gtk_widget_show(new_call);

        GtkWidget *edit = gtk_image_menu_item_new_from_stock(GTK_STOCK_EDIT,
                          get_accel_group());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), edit);
        g_signal_connect(edit, "activate", G_CALLBACK(edit_number_cb), selectedCall);
        gtk_widget_show(edit);

        add_registered_accounts_to_menu(menu);
    }

    menu_popup_wrapper(menu, my_widget, event);
}

static void
ok_cb(GtkWidget *widget UNUSED, gpointer userdata)
{
    // Change the number of the selected call before calling
    const gchar * const new_number = gtk_entry_get_text(GTK_ENTRY(editable_num_));
    callable_obj_t *original =(callable_obj_t*) userdata;

    // Create the new call
    callable_obj_t *modified_call = create_new_call(CALL, CALL_STATE_DIALING, "", original->_accountID,
                                    original->_display_name, new_number);

    // Update the internal data structure and the GUI
    calllist_add_call(current_calls_tab, modified_call);
    calltree_add_call(current_calls_tab, modified_call, NULL);
    sflphone_place_call(modified_call);
    calltree_display(current_calls_tab);

    // Close the contextual menu
    gtk_widget_destroy(edit_dialog_);
}

static void
on_delete(GtkWidget * widget)
{
    gtk_widget_destroy(widget);
}

void
show_edit_number(callable_obj_t *call)
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
        ERROR("This a bug, the call should be defined. menus.c line 1051");

    gtk_box_pack_start(GTK_BOX(hbox), editable_num_, TRUE, TRUE, 0);

    // Set a custom image for the button
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(ICONS_DIR "/outgoing.svg", 32, 32,
                        TRUE, NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    GtkWidget *ok = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(ok), image);
    gtk_box_pack_start(GTK_BOX(hbox), ok, TRUE, TRUE, 0);
    g_signal_connect(ok, "clicked", G_CALLBACK(ok_cb), call);

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

GtkWidget *
create_menus(GtkUIManager *ui_manager)
{
    GtkWidget *menu_bar = gtk_ui_manager_get_widget(ui_manager, "/MenuBar");
    if(menu_bar == NULL) {
        ERROR("Could not create menu bar");
    }

    pickUpAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/CallMenu/PickUp");
    if(pickUpAction_ == NULL) {
        ERROR("Could not create pick up action");
    }

    newCallAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/CallMenu/NewCall");
    if(newCallAction_ == NULL) {
        ERROR("Could not create new call action");
    }

    hangUpAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/CallMenu/HangUp");
    if(hangUpAction_ == NULL) {
        ERROR("Could not create hangup action");
    }

    holdMenu_ = gtk_ui_manager_get_widget(ui_manager, "/MenuBar/CallMenu/OnHoldMenu");
    if(holdMenu_ == NULL) {
        ERROR("Could not create hold menu widget");
    }

    recordAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/CallMenu/Record");
    if(recordAction_ == NULL) {
        ERROR("Could not create record action");
    }

    muteAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/CallMenu/Mute");
    if(muteAction_ == NULL) {
        ERROR("Could not create mute call action");
    }

    imAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/CallMenu/InstantMessaging");
    if(imAction_ == NULL) {
        ERROR("Could not create instant messaging action");
    }

    copyAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/EditMenu/Copy");
    if(copyAction_ == NULL) {
        ERROR("Could not create copy action");
    }

    pasteAction_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/EditMenu/Paste");
    if(pasteAction_ == NULL) {
        ERROR("Could not create paste action");
    }

    volumeToggle_ = gtk_ui_manager_get_action(ui_manager, "/MenuBar/ViewMenu/VolumeControls");
    if(volumeToggle_ == NULL) {
        ERROR("Could not create volume toggle action");
    }

    // Set the toggle buttons
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(gtk_ui_manager_get_action(ui_manager, "/MenuBar/ViewMenu/Dialpad")), eel_gconf_get_boolean(CONF_SHOW_DIALPAD));
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(volumeToggle_),(gboolean) SHOW_VOLUME);
    gtk_action_set_sensitive(volumeToggle_, must_show_alsa_conf());
    gtk_action_set_sensitive(gtk_ui_manager_get_action(ui_manager, "/MenuBar/ViewMenu/Toolbar"), FALSE);

    /* Add the loading icon at the right of the toolbar. It is used for addressbook searches. */
    waitingLayer = create_waiting_icon();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), waitingLayer);

    return menu_bar;
}

GtkWidget *
create_toolbar_actions(GtkUIManager *ui_manager)
{
    toolbar_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions");

    holdToolbar_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/OnHoldToolbar");
    if(holdToolbar_ == NULL) {
        ERROR("Could not create on hold toolbar widget");
    }

    offHoldToolbar_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/OffHoldToolbar");
    if(offHoldToolbar_ == NULL) {
        ERROR("Could not create off hold toolbar widget");
    }

    transferToolbar_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/TransferToolbar");
    if(transferToolbar_ == NULL) {
        ERROR("Could not create transfer toolbar widget");
    }

    voicemailAction_ = gtk_ui_manager_get_action(ui_manager, "/ToolbarActions/Voicemail");
    if(voicemailAction_ == NULL) {
        ERROR("Could not create voicemail action");
    }

    voicemailToolbar_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/VoicemailToolbar");
    if(voicemailToolbar_ == NULL) {
        ERROR("Could not create voicemail toolbar widget");
    }

    newCallWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/NewCallToolbar");
    if(newCallWidget_ == NULL) {
        ERROR("Could not create new call widget");
    }

    pickUpWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/PickUpToolbar");
    if(pickUpWidget_ == NULL) {
        ERROR("Could not create pick up toolbar widget");
    }

    hangUpWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/HangUpToolbar");
    if(hangUpWidget_ == NULL) {
        ERROR("Could not create hang up toolbar widget");
    }

    recordWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/RecordToolbar");
    if(recordWidget_ == NULL) {
        ERROR("Could not create record toolbar widget");
    }

    muteWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/MuteToolbar");
    if(muteWidget_ == NULL) {
        ERROR("Could not create mute call widget");
    }

    imToolbar_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/InstantMessagingToolbar");
    if(imToolbar_ == NULL) {
        ERROR("Could not create instant messaging widget");
    }

    historyButton_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/HistoryToolbar");
    if(historyButton_ == NULL) {
        ERROR("Could not create history button widget");
    }

    playRecordWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/StartPlaybackRecordToolbar");
    if(playRecordWidget_ == NULL) {
        ERROR("Could not create play record widget");
    }

    stopRecordWidget_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/StopPlaybackRecordToolbar");
    if(stopRecordWidget_ == NULL) {
        ERROR("Could not create stop record widget");
    }

    if (addrbook)
        contactButton_ = gtk_ui_manager_get_widget(ui_manager, "/ToolbarActions/AddressbookToolbar");

    // Set the handler ID for the transfer
    transferButtonConnId_ = g_signal_connect(G_OBJECT(transferToolbar_), "toggled", G_CALLBACK(call_transfer_cb), NULL);
    recordButtonConnId_ = g_signal_connect(G_OBJECT(recordWidget_), "toggled", G_CALLBACK(call_record), NULL);
    // muteCallButtonId_ = g_signal_connect(G_OBJECT(muteWidget_), "toggled", G_CALLBACK(call_mute), NULL);
    active_calltree_tab = current_calls_tab;

    return toolbar_;
}
