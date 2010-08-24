/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <config.h>
#include <preferencesdialog.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <assistant.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gprintf.h>
#include <libgnome/gnome-help.h>
#include <eel-gconf-extensions.h>
#include "uimanager.h"
#include "statusicon.h"
#include "contacts/addressbook.h"
#include "accountlist.h"
#include "config/accountlistconfigdialog.h"

void show_edit_number (callable_obj_t *call);

static GtkWidget *toolbar;

guint transfertButtonConnId; //The button toggled signal connection ID
guint recordButtonConnId; //The button toggled signal connection ID

GtkAction * pickUpAction;
GtkWidget * pickUpWidget;
GtkAction * newCallAction;
GtkWidget * newCallWidget;
GtkAction * hangUpAction;
GtkWidget * hangUpWidget;
GtkWidget * holdMenu;
GtkWidget * holdToolbar;
GtkWidget * offHoldToolbar;
GtkWidget * transferToolbar;
GtkAction * copyAction;
GtkAction * pasteAction;
GtkAction * recordAction;
GtkWidget * recordWidget;
GtkAction * voicemailAction;
GtkWidget * voicemailToolbar;

GtkWidget * editable_num;
GtkDialog * edit_dialog;

enum {
    CALLTREE_CALLS, CALLTREE_HISTORY, CALLTREE_CONTACTS
};

static gboolean
is_inserted (GtkWidget* button, GtkWidget *current_toolbar)
{
    return (GTK_WIDGET (button)->parent == GTK_WIDGET (current_toolbar));
}

void
update_actions()
{

    DEBUG ("UIManager: Update action");

    gtk_action_set_sensitive (GTK_ACTION (newCallAction), TRUE);
    gtk_action_set_sensitive (GTK_ACTION (pickUpAction), FALSE);
    gtk_action_set_sensitive (GTK_ACTION (hangUpAction), FALSE);

    g_object_ref (hangUpWidget);
    g_object_ref (recordWidget);
    g_object_ref (holdToolbar);
    g_object_ref (offHoldToolbar);
    g_object_ref (contactButton);
    g_object_ref (historyButton);
    g_object_ref (transferToolbar);
    g_object_ref (voicemailToolbar);

    if (is_inserted (GTK_WIDGET (hangUpWidget), GTK_WIDGET (toolbar))) {
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (hangUpWidget));
    }

    if (is_inserted (GTK_WIDGET (recordWidget), GTK_WIDGET (toolbar))) {
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (recordWidget));
    }

    if (is_inserted (GTK_WIDGET (transferToolbar), GTK_WIDGET (toolbar))) {
        gtk_container_remove (GTK_CONTAINER (toolbar),
                              GTK_WIDGET (transferToolbar));
    }

    if (is_inserted (GTK_WIDGET (historyButton), GTK_WIDGET (toolbar))) {
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (historyButton));
    }

    if (is_inserted (GTK_WIDGET (contactButton), GTK_WIDGET (toolbar))) {
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (contactButton));
    }

    if (is_inserted (GTK_WIDGET (voicemailToolbar), GTK_WIDGET (toolbar))) {
        gtk_container_remove (GTK_CONTAINER (toolbar),
                              GTK_WIDGET (voicemailToolbar));
    }

    gtk_widget_set_sensitive (GTK_WIDGET (holdMenu), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (offHoldToolbar), FALSE);
    gtk_action_set_sensitive (GTK_ACTION (recordAction), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (recordWidget), FALSE);
    gtk_action_set_sensitive (GTK_ACTION (copyAction), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (contactButton), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (historyButton), FALSE);
    gtk_widget_set_tooltip_text (GTK_WIDGET (contactButton),
                                 _ ("No address book selected"));

    if (is_inserted (GTK_WIDGET (holdToolbar), GTK_WIDGET (toolbar)))
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (holdToolbar));

    if (is_inserted (GTK_WIDGET (offHoldToolbar), GTK_WIDGET (toolbar)))
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (offHoldToolbar));

    if (is_inserted (GTK_WIDGET (newCallWidget), GTK_WIDGET (toolbar)))
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (newCallWidget));

    if (is_inserted (GTK_WIDGET (pickUpWidget), GTK_WIDGET (toolbar)))
        gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (pickUpWidget));

    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (newCallWidget), 0);


    if (eel_gconf_get_integer (HISTORY_ENABLED)) {
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (historyButton), -1);
        gtk_widget_set_sensitive (GTK_WIDGET (historyButton), TRUE);
    }

    // If addressbook support has been enabled and all addressbooks are loaded, display the icon
    if (addressbook_is_enabled() && addressbook_is_ready()) {
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (contactButton),
                            -1);

        // Make the icon clickable only if at least one address book is active
        if (addressbook_is_active()) {
            gtk_widget_set_sensitive (GTK_WIDGET (contactButton), TRUE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (contactButton),
                                         _ ("Address book"));
        }
    }

    // g_signal_handler_block (GTK_OBJECT (recordWidget), recordButtonConnId);
    // gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (recordWidget), FALSE);
    // g_signal_handler_unblock (GTK_OBJECT (recordWidget), recordButtonConnId);

    callable_obj_t * selectedCall = calltab_get_selected_call (active_calltree);
    conference_obj_t * selectedConf = calltab_get_selected_conf (active_calltree);

    if (selectedCall) {
        // update icon in systray
        show_status_hangup_icon();

        gtk_action_set_sensitive (GTK_ACTION (copyAction), TRUE);

        switch (selectedCall->_state) {
            case CALL_STATE_INCOMING:
                // Make the button toolbar clickable
                gtk_action_set_sensitive (GTK_ACTION (pickUpAction), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                // Replace the dial button with the hangup button
                g_object_ref (newCallWidget);
                gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (newCallWidget));
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (pickUpWidget),
                                    0);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget),
                                    1);
                break;
            case CALL_STATE_HOLD:
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdMenu), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (offHoldToolbar), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (newCallWidget), TRUE);
                // Replace the hold button with the off-hold button
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget),
                                    1);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
                                    GTK_TOOL_ITEM (offHoldToolbar), 2);
                break;
            case CALL_STATE_RINGING:
                gtk_action_set_sensitive (GTK_ACTION (pickUpAction), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget),
                                    1);
                break;
            case CALL_STATE_DIALING:
                gtk_action_set_sensitive (GTK_ACTION (pickUpAction), TRUE);

                if (active_calltree == current_calls)
                    gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);

                //gtk_action_set_sensitive( GTK_ACTION(newCallMenu),TRUE);
                g_object_ref (newCallWidget);
                gtk_container_remove (GTK_CONTAINER (toolbar), GTK_WIDGET (newCallWidget));
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (pickUpWidget), 0);

                if (active_calltree == current_calls)
                    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);

                break;
            case CALL_STATE_CURRENT:
                DEBUG ("UIManager: Call State Current");
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_widget_set_sensitive (GTK_WIDGET (holdMenu), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (transferToolbar), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (recordAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (holdToolbar), 2);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (transferToolbar), 3);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (recordWidget), 4);
                gtk_signal_handler_block (GTK_OBJECT (transferToolbar), transfertButtonConnId);
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (transferToolbar), FALSE);
                gtk_signal_handler_unblock (transferToolbar, transfertButtonConnId);
                g_signal_handler_block (GTK_OBJECT (recordWidget), recordButtonConnId);
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (recordWidget), FALSE);
                g_signal_handler_unblock (GTK_OBJECT (recordWidget), recordButtonConnId);
                break;

            case CALL_STATE_RECORD:
                DEBUG ("UIManager: Call State Record");
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_widget_set_sensitive (GTK_WIDGET (holdMenu), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (transferToolbar), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (recordAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (holdToolbar), 2);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (transferToolbar), 3);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (recordWidget), 4);
                gtk_signal_handler_block (GTK_OBJECT (transferToolbar), transfertButtonConnId);
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (transferToolbar), FALSE);
                gtk_signal_handler_unblock (transferToolbar, transfertButtonConnId);
                g_signal_handler_block (GTK_OBJECT (recordWidget), recordButtonConnId);
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (recordWidget), TRUE);
                g_signal_handler_unblock (GTK_OBJECT (recordWidget), recordButtonConnId);
                break;
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                break;
            case CALL_STATE_TRANSFERT:
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (transferToolbar), 2);
                gtk_signal_handler_block (GTK_OBJECT (transferToolbar), transfertButtonConnId);
                gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (transferToolbar), TRUE);
                gtk_signal_handler_unblock (transferToolbar, transfertButtonConnId);
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdMenu), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (transferToolbar), TRUE);
                break;
            default:
                WARN ("Should not happen in update_actions()!");
                break;
        }
    } else if (selectedConf) {

        // update icon in systray
        show_status_hangup_icon();

        switch (selectedConf->_state) {

            case CONFERENCE_STATE_ACTIVE_ATACHED:
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (recordAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (holdToolbar), 2);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (recordWidget), 3);
                break;

            case CONFERENCE_STATE_ACTIVE_DETACHED:
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (recordAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (holdToolbar), 2);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (recordWidget), 3);
                break;

            case CONFERENCE_STATE_RECORD:
                DEBUG ("UIManager: Conference state record");
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (holdToolbar), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (recordAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (holdToolbar), 2);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (recordWidget), 3);
                break;

            case CONFERENCE_STATE_HOLD:
                gtk_action_set_sensitive (GTK_ACTION (hangUpAction), TRUE);
                gtk_widget_set_sensitive (GTK_WIDGET (offHoldToolbar), TRUE);
                gtk_action_set_sensitive (GTK_ACTION (recordAction), TRUE);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (hangUpWidget), 1);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (offHoldToolbar), 2);
                gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (recordWidget), 3);
                break;

            default:
                WARN ("Should not happen in update_action()!");
                break;

        }
    }

    else {

        // update icon in systray
        hide_status_hangup_icon();

        if (account_list_get_size() > 0 && current_account_has_mailbox()) {
            gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
                                GTK_TOOL_ITEM (voicemailToolbar), -2);
            update_voicemail_status();
        }
    }
}

void
update_voicemail_status (void)
{
    gchar *messages = "";
    messages = g_markup_printf_escaped (_ ("Voicemail (%i)"),
                                        current_account_get_message_number());
    (current_account_has_new_message()) ? gtk_tool_button_set_icon_name (
        GTK_TOOL_BUTTON (voicemailToolbar), "mail-message-new")
    : gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (voicemailToolbar),
                                     "mail-read");
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (voicemailToolbar), messages);
    g_free (messages);
}

static void
volume_bar_cb (GtkToggleAction *togglemenuitem, gpointer user_data UNUSED)
{
    gboolean toggled = gtk_toggle_action_get_active (togglemenuitem);

    if (toggled == SHOW_VOLUME)
        return;

    main_window_volume_controls (toggled);

    if (toggled || SHOW_VOLUME)
        eel_gconf_set_integer (SHOW_VOLUME_CONTROLS, toggled);
}

static void
dialpad_bar_cb (GtkToggleAction *togglemenuitem, gpointer user_data UNUSED)
{
    gboolean toggled = gtk_toggle_action_get_active (togglemenuitem);
    gboolean conf_dialpad = eel_gconf_get_boolean (CONF_SHOW_DIALPAD);

    if (toggled == conf_dialpad)
        return;

    main_window_dialpad (toggled);

    if (toggled || conf_dialpad)
        eel_gconf_set_boolean (CONF_SHOW_DIALPAD, toggled); //dbus_set_dialpad (toggled);

}

static void
help_contents_cb (GtkAction *action UNUSED)
{
    GError *error = NULL;

    gnome_help_display ("sflphone.xml", NULL, &error);

    if (error != NULL) {
        g_warning ("%s", error->message);
        g_error_free (error);
    }
}

static void
help_about (void * foo UNUSED)
{
    gchar
    *authors[] = {
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
    gchar *artists[] = { "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>",
                         "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>", NULL
                       };

    gtk_show_about_dialog (GTK_WINDOW (get_main_window()), "artists", artists,
                           "authors", authors, "comments",
                           _ ("SFLphone is a VoIP client compatible with SIP and IAX2 protocols."),
                           "copyright", "Copyright © 2004-2009 Savoir-faire Linux Inc.", "name",
                           PACKAGE, "title", _ ("About SFLphone"), "version", VERSION, "website",
                           "http://www.sflphone.org", NULL);

}

/* ----------------------------------------------------------------- */

static void
call_new_call (void * foo UNUSED)
{
    DEBUG ("UIManager: New call button pressed");

    sflphone_new_call();
}

static void
call_quit (void * foo UNUSED)
{
    sflphone_quit();
}

static void
call_minimize (void * foo UNUSED)
{

    if (eel_gconf_get_integer (SHOW_STATUSICON)) {
        gtk_widget_hide (GTK_WIDGET (get_main_window()));
        set_minimized (TRUE);
    } else {
        sflphone_quit ();
    }
}

static void
switch_account (GtkWidget* item, gpointer data UNUSED)
{
    account_t* acc = g_object_get_data (G_OBJECT (item), "account");
    DEBUG ("%s" , acc->accountID);
    account_list_set_current (acc);
    status_bar_display_account();
}

static void
call_hold (void* foo UNUSED)
{
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    conference_obj_t * selectedConf = calltab_get_selected_conf();

    DEBUG ("UIManager: Hold button pressed (call)");

    if (selectedCall) {
        if (selectedCall->_state == CALL_STATE_HOLD) {
            sflphone_off_hold();
        } else {
            sflphone_on_hold();
        }
    } else if (selectedConf) {

        switch (selectedConf->_state) {

            case CONFERENCE_STATE_HOLD: {
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATACHED;
                sflphone_conference_off_hold (selectedConf);
            }
            break;

            case CONFERENCE_STATE_ACTIVE_ATACHED:
            case CONFERENCE_STATE_ACTIVE_DETACHED: {
                selectedConf->_state = CONFERENCE_STATE_HOLD;
                sflphone_conference_on_hold (selectedConf);
            }
            break;
            default:
                break;
        }

    }
}

static void
conference_hold (void* foo UNUSED)
{
    conference_obj_t * selectedConf = calltab_get_selected_conf();

    DEBUG ("UIManager: Hold button pressed (conference)");

    switch (selectedConf->_state) {
        case CONFERENCE_STATE_HOLD: {
            selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATACHED;
            sflphone_conference_off_hold (selectedConf);
        }
        break;

        case CONFERENCE_STATE_ACTIVE_ATACHED:
        case CONFERENCE_STATE_ACTIVE_DETACHED: {
            selectedConf->_state = CONFERENCE_STATE_HOLD;
            sflphone_conference_on_hold (selectedConf);
        }
        break;
        default:
            break;
    }
}

static void
call_pick_up (void * foo UNUSED)
{
    DEBUG ("UIManager: Call button pressed");
    callable_obj_t * selectedCall;
    callable_obj_t* new_call;

    selectedCall = calltab_get_selected_call (active_calltree);

    if (calllist_get_size (current_calls) > 0)
        sflphone_pick_up();

    else if (calllist_get_size (active_calltree) > 0) {
        if (selectedCall) {
            create_new_call (CALL, CALL_STATE_DIALING, "", "", "",
                             selectedCall->_peer_number, &new_call);

            calllist_add (current_calls, new_call);
            calltree_add_call (current_calls, new_call, NULL);
            sflphone_place_call (new_call);
            calltree_display (current_calls);
        } else {
            sflphone_new_call();
            calltree_display (current_calls);
        }
    } else {
        sflphone_new_call();
        calltree_display (current_calls);
    }
}

static void
call_hang_up (void)
{

    DEBUG ("UIManager: Hang up button pressed (call)");
    /*
     * [#3020]	Restore the record toggle button
     *			We set it to FALSE, as when we hang up a call, the recording is stopped.
     */
    gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (recordWidget), FALSE);

    sflphone_hang_up();

}

static void
conference_hang_up (void)
{
    DEBUG ("UIManager: Hang up button pressed (conference)");

    sflphone_conference_hang_up();
}

static void
call_record (void)
{
    DEBUG ("UIManager: Record button pressed");

    sflphone_rec_call();
}

static void
call_configuration_assistant (void * foo UNUSED)
{
#if GTK_CHECK_VERSION(2,10,0)
    build_wizard();
#endif
}

static void
remove_from_history (void * foo UNUSED)
{
    callable_obj_t* c = calltab_get_selected_call (history);

    if (c) {
        DEBUG ("UIManager: Remove the call from the history");
        calllist_remove_from_history (c);
    }
}

static void
call_back (void * foo UNUSED)
{
    callable_obj_t *selected_call, *new_call;

    selected_call = calltab_get_selected_call (active_calltree);

    if (selected_call) {
        create_new_call (CALL, CALL_STATE_DIALING, "", "",
                         selected_call->_peer_name, selected_call->_peer_number, &new_call);

        calllist_add (current_calls, new_call);
        calltree_add_call (current_calls, new_call, NULL);
        sflphone_place_call (new_call);
        calltree_display (current_calls);
    }
}

static void
edit_preferences (void * foo UNUSED)
{
    show_preferences_dialog();
}

static void
edit_accounts (void * foo UNUSED)
{
    show_account_list_config_dialog();
}

// The menu Edit/Copy should copy the current selected call's number
static void
edit_copy (void * foo UNUSED)
{
    GtkClipboard* clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    gchar * no = NULL;

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_TRANSFERT:
            case CALL_STATE_DIALING:
            case CALL_STATE_RINGING:
                no = selectedCall->_peer_number;
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_HOLD:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
            case CALL_STATE_INCOMING:
            default:
                no = selectedCall->_peer_number;
                break;
        }

        DEBUG ("UIManager: Clipboard number: %s\n", no);
        gtk_clipboard_set_text (clip, no, strlen (no));
    }

}

// The menu Edit/Paste should paste the clipboard into the current selected call
static void
edit_paste (void * foo UNUSED)
{

    GtkClipboard* clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    gchar * no = gtk_clipboard_wait_for_text (clip);

    if (no && selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_TRANSFERT:
            case CALL_STATE_DIALING:
                // Add the text to the number
            {
                gchar * before;
                before = selectedCall->_peer_number;
                DEBUG ("TO: %s\n", before);
                selectedCall->_peer_number = g_strconcat (before, no, NULL);

                if (selectedCall->_state == CALL_STATE_DIALING) {
                    selectedCall->_peer_info = g_strconcat ("\"\" <",
                                                            selectedCall->_peer_number, ">", NULL);
                }

                calltree_update_call (current_calls, selectedCall, NULL);
            }
            break;
            case CALL_STATE_RINGING:
            case CALL_STATE_INCOMING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
            case CALL_STATE_HOLD: { // Create a new call to hold the new text
                selectedCall = sflphone_new_call();

                selectedCall->_peer_number = g_strconcat (selectedCall->_peer_number,
                                             no, NULL);
                DEBUG ("TO: %s", selectedCall->_peer_number);

                selectedCall->_peer_info = g_strconcat ("\"\" <",
                                                        selectedCall->_peer_number, ">", NULL);

                calltree_update_call (current_calls, selectedCall, NULL);
            }
            break;
            case CALL_STATE_CURRENT:
            default: {
                unsigned int i;

                for (i = 0; i < strlen (no); i++) {
                    gchar * oneNo = g_strndup (&no[i], 1);
                    DEBUG ("<%s>", oneNo);
                    dbus_play_dtmf (oneNo);

                    gchar * temp = g_strconcat (selectedCall->_peer_number, oneNo,
                                                NULL);
                    selectedCall->_peer_info = get_peer_info (temp,
                                               selectedCall->_peer_name);
                    // g_free(temp);
                    calltree_update_call (current_calls, selectedCall, NULL);

                }
            }
            break;
        }

    } else { // There is no current call, create one
        selectedCall = sflphone_new_call();

        gchar * before = selectedCall->_peer_number;
        selectedCall->_peer_number = g_strconcat (selectedCall->_peer_number, no,
                                     NULL);
        g_free (before);
        DEBUG ("UIManager: TO: %s", selectedCall->_peer_number);

        g_free (selectedCall->_peer_info);
        selectedCall->_peer_info = g_strconcat ("\"\" <",
                                                selectedCall->_peer_number, ">", NULL);
        calltree_update_call (current_calls, selectedCall, NULL);
    }

}

static void
clear_history (void)
{
    if (calllist_get_size (history) != 0) {
        calllist_clean_history();
    }
}

/**
 * Transfert the line
 */
static void
call_transfer_cb()
{
    gboolean active = gtk_toggle_tool_button_get_active (
                          GTK_TOGGLE_TOOL_BUTTON (transferToolbar));
    active ? sflphone_set_transfert() : sflphone_unset_transfert();
}

static void
call_mailbox_cb (void)
{
    account_t* current;
    callable_obj_t *mailbox_call;
    gchar *to, *account_id;

    current = account_list_get_current();

    if (current == NULL) // Should not happens
        return;

    to = g_strdup (g_hash_table_lookup (current->properties, ACCOUNT_MAILBOX));
    account_id = g_strdup (current->accountID);

    create_new_call (CALL, CALL_STATE_DIALING, "", account_id, _ ("Voicemail"), to,
                     &mailbox_call);
    DEBUG ("TO : %s" , mailbox_call->_peer_number);
    calllist_add (current_calls, mailbox_call);
    calltree_add_call (current_calls, mailbox_call, NULL);
    update_actions();
    sflphone_place_call (mailbox_call);
    calltree_display (current_calls);
}

static void
toggle_history_cb (GtkToggleAction *action, gpointer user_data UNUSED)
{
    gboolean toggle;
    toggle = gtk_toggle_action_get_active (action);
    (toggle) ? calltree_display (history) : calltree_display (current_calls);
}

static void
toggle_addressbook_cb (GtkToggleAction *action, gpointer user_data UNUSED)
{
    gboolean toggle;
    toggle = gtk_toggle_action_get_active (action);
    (toggle) ? calltree_display (contacts) : calltree_display (current_calls);
}

static const GtkActionEntry menu_entries[] = {

    // Call Menu
    { "Call", NULL, N_ ("Call"), NULL, NULL, NULL },
    { "NewCall", GTK_STOCK_DIAL, N_ ("_New call"), "<control>N", N_ ("Place a new call"), G_CALLBACK (call_new_call) },
    { "PickUp", GTK_STOCK_PICKUP, N_ ("_Pick up"), NULL, N_ ("Answer the call"), G_CALLBACK (call_pick_up) },
    { "HangUp", GTK_STOCK_HANGUP, N_ ("_Hang up"), "<control>S", N_ ("Finish the call"), G_CALLBACK (call_hang_up) },
    { "OnHold", GTK_STOCK_ONHOLD, N_ ("O_n hold"), "<control>P", N_ ("Place the call on hold"), G_CALLBACK (call_hold) },
    { "OffHold", GTK_STOCK_OFFHOLD, N_ ("O_ff hold"), "<control>P", N_ ("Place the call off hold"), G_CALLBACK (call_hold) },
    { "AccountAssistant", NULL, N_ ("Configuration _Assistant"), NULL, N_ ("Run the configuration assistant"), G_CALLBACK (call_configuration_assistant) },
    { "Voicemail", "mail-read", N_ ("Voicemail"), NULL, N_ ("Call your voicemail"), G_CALLBACK (call_mailbox_cb) },
    { "Close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Minimize to system tray"), G_CALLBACK (call_minimize) },
    { "Quit", GTK_STOCK_CLOSE, N_ ("_Quit"), "<control>Q", N_ ("Quit the program"), G_CALLBACK (call_quit) },

    // Edit Menu
    { "Edit", NULL, N_ ("_Edit"), NULL, NULL, NULL },
    { "Copy", GTK_STOCK_COPY, N_ ("_Copy"), "<control>C", N_ ("Copy the selection"), G_CALLBACK (edit_copy) },
    { "Paste", GTK_STOCK_PASTE, N_ ("_Paste"), "<control>V", N_ ("Paste the clipboard"), G_CALLBACK (edit_paste) },
    { "ClearHistory", GTK_STOCK_CLEAR, N_ ("Clear _history"), NULL, N_ ("Clear the call history"), G_CALLBACK (clear_history) },
    { "Accounts", NULL, N_ ("_Accounts"), NULL, N_ ("Edit your accounts"), G_CALLBACK (edit_accounts) },
    { "Preferences", GTK_STOCK_PREFERENCES, N_ ("_Preferences"), NULL, N_ ("Change your preferences"), G_CALLBACK (edit_preferences) },

    // View Menu
    { "View", NULL, N_ ("_View"), NULL, NULL, NULL },

    // Help menu
    { "Help", NULL, N_ ("_Help"), NULL, NULL, NULL },
    { "HelpContents", GTK_STOCK_HELP, N_ ("Contents"), "F1", N_ ("Open the manual"), G_CALLBACK (help_contents_cb) },
    { "About", GTK_STOCK_ABOUT, NULL, NULL, N_ ("About this application"), G_CALLBACK (help_about) }
};

static const GtkToggleActionEntry toggle_menu_entries[] = {

    { "Transfer", GTK_STOCK_TRANSFER, N_ ("_Transfer"), "<control>T", N_ ("Transfer the call"), NULL, TRUE },
    { "Record", GTK_STOCK_MEDIA_RECORD, N_ ("_Record"), "<control>R", N_ ("Record the current conversation"), NULL, TRUE },
    { "Toolbar", NULL, N_ ("_Show toolbar"), "<control>T", N_ ("Show the toolbar"), NULL, TRUE },
    { "Dialpad", NULL, N_ ("_Dialpad"), "<control>D", N_ ("Show the dialpad"), G_CALLBACK (dialpad_bar_cb), TRUE },
    { "VolumeControls", NULL, N_ ("_Volume controls"), "<control>V", N_ ("Show the volume controls"), G_CALLBACK (volume_bar_cb), TRUE },
    { "History", "appointment-soon", N_ ("_History"), NULL, N_ ("Calls history"), G_CALLBACK (toggle_history_cb), FALSE },
    { "Addressbook", GTK_STOCK_ADDRESSBOOK, N_ ("_Address book"), NULL, N_ ("Address book"), G_CALLBACK (toggle_addressbook_cb), FALSE }
};

gboolean
uimanager_new (GtkUIManager **_ui_manager)
{

    GtkUIManager *ui_manager;
    GtkActionGroup *action_group;
    GtkWidget *window;
    gchar *path;
    GError *error = NULL;

    window = get_main_window();
    ui_manager = gtk_ui_manager_new();

    /* Create an accel group for window's shortcuts */
    path = g_build_filename (SFLPHONE_UIDIR_UNINSTALLED, "./ui.xml", NULL);

    if (g_file_test (path, G_FILE_TEST_EXISTS)) {
        gtk_ui_manager_add_ui_from_file (ui_manager, path, &error);

        if (error != NULL) {
            g_error_free (error);
            return FALSE;
        }

        g_free (path);
    } else {
        path = g_build_filename (SFLPHONE_UIDIR, "./ui.xml", NULL);

        if (g_file_test (path, G_FILE_TEST_EXISTS)) {
            gtk_ui_manager_add_ui_from_file (ui_manager, path, &error);

            if (error != NULL) {
                g_error_free (error);
                return FALSE;
            }

            g_free (path);
        } else
            return FALSE;
    }

    action_group = gtk_action_group_new ("SFLphoneWindowActions");
    // To translate label and tooltip entries
    gtk_action_group_set_translation_domain (action_group, "sflphone-client-gnome");
    gtk_action_group_add_actions (action_group, menu_entries,
                                  G_N_ELEMENTS (menu_entries), window);
    gtk_action_group_add_toggle_actions (action_group, toggle_menu_entries,
                                         G_N_ELEMENTS (toggle_menu_entries), window);
    //gtk_action_group_add_radio_actions (action_group, radio_menu_entries, G_N_ELEMENTS (radio_menu_entries), CALLTREE_CALLS, G_CALLBACK (calltree_switch_cb), window);
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

    *_ui_manager = ui_manager;

    return TRUE;
}

static void
edit_number_cb (GtkWidget *widget UNUSED, gpointer user_data)
{

    show_edit_number ( (callable_obj_t*) user_data);
}

void
add_registered_accounts_to_menu (GtkWidget *menu)
{

    GtkWidget *menu_items;
    unsigned int i;
    account_t* acc, *current;
    gchar* alias;

    menu_items = gtk_separator_menu_item_new();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);

    for (i = 0; i < account_list_get_size(); i++) {
        acc = account_list_get_nth (i);

        // Display only the registered accounts
        if (g_strcasecmp (account_state_name (acc->state), account_state_name (
                              ACCOUNT_STATE_REGISTERED)) == 0) {
            alias = g_strconcat (g_hash_table_lookup (acc->properties,
                                 ACCOUNT_ALIAS), " - ", g_hash_table_lookup (acc->properties,
                                         ACCOUNT_TYPE), NULL);
            menu_items = gtk_check_menu_item_new_with_mnemonic (alias);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_object_set_data (G_OBJECT (menu_items), "account", acc);
            g_free (alias);
            current = account_list_get_current();

            if (current) {
                gtk_check_menu_item_set_active (
                    GTK_CHECK_MENU_ITEM (menu_items),
                    (g_strcasecmp (acc->accountID, current->accountID) == 0) ? TRUE
                    : FALSE);
            }

            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (switch_account),
                              NULL);
            gtk_widget_show (menu_items);
        } // fi
    }

}

void
show_popup_menu (GtkWidget *my_widget, GdkEventButton *event)
{
    // TODO update the selection to make sure the call under the mouse is the call selected

    // call type boolean
    gboolean pickup = FALSE, hangup = FALSE, hold = FALSE, copy = FALSE, record =
                                          FALSE, detach = FALSE;
    gboolean accounts = FALSE;

    // conference type boolean
    gboolean hangup_conf = FALSE, hold_conf = FALSE;

    callable_obj_t * selectedCall = NULL;
    conference_obj_t * selectedConf;

    if (calltab_get_selected_type (current_calls) == A_CALL) {
        DEBUG ("UIManager: Menus: Selected a call");
        selectedCall = calltab_get_selected_call (current_calls);

        if (selectedCall) {
            copy = TRUE;

            switch (selectedCall->_state) {
                case CALL_STATE_INCOMING:
                    pickup = TRUE;
                    hangup = TRUE;
                    detach = TRUE;
                    break;
                case CALL_STATE_HOLD:
                    hangup = TRUE;
                    hold = TRUE;
                    detach = TRUE;
                    break;
                case CALL_STATE_RINGING:
                    hangup = TRUE;
                    detach = TRUE;
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
                    detach = TRUE;
                    break;
                case CALL_STATE_BUSY:
                case CALL_STATE_FAILURE:
                    hangup = TRUE;
                    break;
                default:
                    WARN ("UIManager: Should not happen in show_popup_menu for calls!")
                    ;
                    break;
            }
        }
    } else {
        DEBUG ("UIManager: Menus: selected a conf");
        selectedConf = calltab_get_selected_conf();

        if (selectedConf) {
            switch (selectedConf->_state) {
                case CONFERENCE_STATE_ACTIVE_ATACHED:
                    hangup_conf = TRUE;
                    hold_conf = TRUE;
                    break;
                case CONFERENCE_STATE_ACTIVE_DETACHED:
                    break;
                case CONFERENCE_STATE_HOLD:
                    hangup_conf = TRUE;
                    hold_conf = TRUE;
                    break;
                default:
                    WARN ("UIManager: Should not happen in show_popup_menu for conferences!")
                    ;
                    break;
            }
        }

    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new();

    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);
    if (calltab_get_selected_type (current_calls) == A_CALL) {
        DEBUG ("UIManager: Build call menu");

        if (copy) {
            menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY,
                         get_accel_group());
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (edit_copy),
                              NULL);
            gtk_widget_show (menu_items);
        }

        menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE,
                     get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",
                          G_CALLBACK (edit_paste),
                          NULL);
        gtk_widget_show (menu_items);

        if (pickup || hangup || hold) {
            menu_items = gtk_separator_menu_item_new();
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            gtk_widget_show (menu_items);
        }

        if (pickup) {

            menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_Pick up"));
            image = gtk_image_new_from_file (ICONS_DIR "/icon_accept.svg");
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (call_pick_up),
                              NULL);
            gtk_widget_show (menu_items);
        }

        if (hangup) {
            menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_Hang up"));
            image = gtk_image_new_from_file (ICONS_DIR "/icon_hangup.svg");
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (call_hang_up),
                              NULL);
            gtk_widget_show (menu_items);
        }

        if (hold) {
            menu_items = gtk_check_menu_item_new_with_mnemonic (_ ("On _Hold"));
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_items),
                                            (selectedCall->_state == CALL_STATE_HOLD ? TRUE : FALSE));
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (call_hold),
                              NULL);
            gtk_widget_show (menu_items);
        }

        if (record) {
            menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_Record"));
            image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_RECORD,
                                              GTK_ICON_SIZE_MENU);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (call_record),
                              NULL);
            gtk_widget_show (menu_items);
        }

    } else {
        DEBUG ("UIManager: Build call menus");

        if (hangup_conf) {
            menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_Hang up"));
            image = gtk_image_new_from_file (ICONS_DIR "/icon_hangup.svg");
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (conference_hang_up),
                              NULL);
            gtk_widget_show (menu_items);
        }

        if (hold_conf) {
            menu_items = gtk_check_menu_item_new_with_mnemonic (_ ("On _Hold"));
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_items),
                                            (selectedCall->_state == CALL_STATE_HOLD ? TRUE : FALSE));
            g_signal_connect (G_OBJECT (menu_items), "activate",
                              G_CALLBACK (conference_hold),
                              NULL);
            gtk_widget_show (menu_items);
        }
    }

    if (accounts) {
        add_registered_accounts_to_menu (menu);
    }

    if (event) {
        button = event->button;
        event_time = event->time;
    } else {
        button = 0;
        event_time = gtk_get_current_event_time();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}

void
show_popup_menu_history (GtkWidget *my_widget, GdkEventButton *event)
{

    DEBUG ("UIManager: Show popup menu history");

    gboolean pickup = FALSE;
    gboolean remove = FALSE;
    gboolean edit = FALSE;
    gboolean accounts = FALSE;

    callable_obj_t * selectedCall = calltab_get_selected_call (history);

    if (selectedCall) {
        remove = TRUE;
        pickup = TRUE;
        edit = TRUE;
        accounts = TRUE;
    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new();
    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);

    if (pickup) {

        menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_Call back"));
        image = gtk_image_new_from_file (ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (call_back), NULL);
        gtk_widget_show (menu_items);
    }

    menu_items = gtk_separator_menu_item_new();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);

    if (edit) {
        menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT,
                     get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (edit_number_cb), selectedCall);
        gtk_widget_show (menu_items);
    }

    if (remove) {
        menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE,
                     get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate", G_CALLBACK (remove_from_history), NULL);
        gtk_widget_show (menu_items);
    }

    if (accounts) {
        add_registered_accounts_to_menu (menu);
    }

    if (event) {
        button = event->button;
        event_time = event->time;
    } else {
        button = 0;
        event_time = gtk_get_current_event_time();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}

/*
void
show_popup_menu_addressbook (GtkWidget *my_widget, GdkEventButton *event)
{

    if (selectedCall) {
        remove = TRUE;
        pickup = TRUE;
        edit = TRUE;
        accounts = TRUE;
    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new();
    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);

    if (pickup) {

        menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_Call back"));
        image = gtk_image_new_from_file (ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (call_back), NULL);
        gtk_widget_show (menu_items);
    }

    menu_items = gtk_separator_menu_item_new();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);

    if (edit) {
        menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT,
                     get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (edit_number_cb), selectedCall);
        gtk_widget_show (menu_items);
    }

    if (remove) {
        menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE,
                     get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate", G_CALLBACK (remove_from_history), NULL);
        gtk_widget_show (menu_items);
    }

    if (accounts) {
        add_registered_accounts_to_menu (menu);
    }

    if (event) {
        button = event->button;
        event_time = event->time;
    } else {
        button = 0;
        event_time = gtk_get_current_event_time();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}
*/

void
show_popup_menu_contacts (GtkWidget *my_widget, GdkEventButton *event)
{

    gboolean pickup = FALSE;
    gboolean accounts = FALSE;
    gboolean edit = FALSE;

    callable_obj_t * selectedCall = calltab_get_selected_call (contacts);

    if (selectedCall) {
        pickup = TRUE;
        accounts = TRUE;
        edit = TRUE;
    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new();
    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);

    if (pickup) {

        menu_items = gtk_image_menu_item_new_with_mnemonic (_ ("_New call"));
        image = gtk_image_new_from_file (ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_items), image);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (call_back), NULL);
        gtk_widget_show (menu_items);
    }

    if (edit) {
        menu_items = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT,
                     get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (edit_number_cb), selectedCall);
        gtk_widget_show (menu_items);
    }

    if (accounts) {
        add_registered_accounts_to_menu (menu);
    }

    if (event) {
        button = event->button;
        event_time = event->time;
    } else {
        button = 0;
        event_time = gtk_get_current_event_time();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}

static void
ok_cb (GtkWidget *widget UNUSED, gpointer userdata)
{

    gchar *new_number;
    callable_obj_t *modified_call, *original;

    // Change the number of the selected call before calling
    new_number = (gchar*) gtk_entry_get_text (GTK_ENTRY (editable_num));
    original = (callable_obj_t*) userdata;

    // Create the new call
    create_new_call (CALL, CALL_STATE_DIALING, "", g_strdup (original->_accountID),
                     original->_peer_name, g_strdup (new_number), &modified_call);

    // Update the internal data structure and the GUI
    calllist_add (current_calls, modified_call);
    calltree_add_call (current_calls, modified_call, NULL);
    sflphone_place_call (modified_call);
    calltree_display (current_calls);

    // Close the contextual menu
    gtk_widget_destroy (GTK_WIDGET (edit_dialog));
}

static void
on_delete (GtkWidget * widget)
{
    gtk_widget_destroy (widget);
}

void
show_edit_number (callable_obj_t *call)
{

    GtkWidget *ok, *hbox, *image;
    GdkPixbuf *pixbuf;

    edit_dialog = GTK_DIALOG (gtk_dialog_new());

    // Set window properties
    gtk_window_set_default_size (GTK_WINDOW (edit_dialog), 300, 20);
    gtk_window_set_title (GTK_WINDOW (edit_dialog), _ ("Edit phone number"));
    gtk_window_set_resizable (GTK_WINDOW (edit_dialog), FALSE);

    g_signal_connect (G_OBJECT (edit_dialog), "delete-event", G_CALLBACK (on_delete), NULL);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (edit_dialog->vbox), hbox, TRUE, TRUE, 0);

    // Set the number to be edited
    editable_num = gtk_entry_new();
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text (GTK_WIDGET (editable_num),
                                 _ ("Edit the phone number before making a call"));
#endif

    if (call)
        gtk_entry_set_text (GTK_ENTRY (editable_num), g_strdup (call->_peer_number));
    else
        ERROR ("This a bug, the call should be defined. menus.c line 1051");

    gtk_box_pack_start (GTK_BOX (hbox), editable_num, TRUE, TRUE, 0);

    // Set a custom image for the button
    pixbuf = gdk_pixbuf_new_from_file_at_scale (ICONS_DIR "/outgoing.svg", 32, 32,
             TRUE, NULL);
    image = gtk_image_new_from_pixbuf (pixbuf);
    ok = gtk_button_new();
    gtk_button_set_image (GTK_BUTTON (ok), image);
    gtk_box_pack_start (GTK_BOX (hbox), ok, TRUE, TRUE, 0);
    g_signal_connect (G_OBJECT (ok), "clicked", G_CALLBACK (ok_cb), call);

    gtk_widget_show_all (edit_dialog->vbox);

    gtk_dialog_run (edit_dialog);

}

GtkWidget*
create_waiting_icon()
{
    GtkWidget * waiting_icon;
    waiting_icon = gtk_image_menu_item_new_with_label ("");
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (waiting_icon),
                                   gtk_image_new_from_animation (gdk_pixbuf_animation_new_from_file (
                                                                     ICONS_DIR "/wait-on.gif", NULL)));
    gtk_menu_item_set_right_justified (GTK_MENU_ITEM (waiting_icon), TRUE);

    return waiting_icon;
}

void
create_menus (GtkUIManager *ui_manager, GtkWidget **widget)
{

    GtkWidget * menu_bar;

    menu_bar = gtk_ui_manager_get_widget (ui_manager, "/MenuBar");
    pickUpAction = gtk_ui_manager_get_action (ui_manager, "/MenuBar/CallMenu/PickUp");
    newCallAction = gtk_ui_manager_get_action (ui_manager, "/MenuBar/CallMenu/NewCall");
    hangUpAction = gtk_ui_manager_get_action (ui_manager, "/MenuBar/CallMenu/HangUp");
    holdMenu = gtk_ui_manager_get_widget (ui_manager, "/MenuBar/CallMenu/OnHoldMenu");
    recordAction = gtk_ui_manager_get_action (ui_manager, "/MenuBar/CallMenu/Record");
    copyAction = gtk_ui_manager_get_action (ui_manager, "/MenuBar/EditMenu/Copy");
    pasteAction = gtk_ui_manager_get_action (ui_manager, "/MenuBar/EditMenu/Paste");
    volumeToggle = gtk_ui_manager_get_action (ui_manager, "/MenuBar/ViewMenu/VolumeControls");

    // Set the toggle buttons
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (gtk_ui_manager_get_action (ui_manager, "/MenuBar/ViewMenu/Dialpad")), eel_gconf_get_boolean (CONF_SHOW_DIALPAD));
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (volumeToggle), (gboolean) SHOW_VOLUME);
    gtk_action_set_sensitive (GTK_ACTION (volumeToggle), SHOW_ALSA_CONF);

    // Disable it right now
    gtk_action_set_sensitive (GTK_ACTION (gtk_ui_manager_get_action (ui_manager, "/MenuBar/ViewMenu/Toolbar")), FALSE);

    waitingLayer = create_waiting_icon ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), waitingLayer);

    *widget = menu_bar;
}

void
create_toolbar_actions (GtkUIManager *ui_manager, GtkWidget **widget)
{
    toolbar = gtk_ui_manager_get_widget (ui_manager, "/ToolbarActions");

    holdToolbar = gtk_ui_manager_get_widget (ui_manager,
                  "/ToolbarActions/OnHoldToolbar");
    offHoldToolbar = gtk_ui_manager_get_widget (ui_manager,
                     "/ToolbarActions/OffHoldToolbar");
    transferToolbar = gtk_ui_manager_get_widget (ui_manager,
                      "/ToolbarActions/TransferToolbar");
    voicemailAction = gtk_ui_manager_get_action (ui_manager,
                      "/ToolbarActions/Voicemail");
    voicemailToolbar = gtk_ui_manager_get_widget (ui_manager,
                       "/ToolbarActions/VoicemailToolbar");
    newCallWidget = gtk_ui_manager_get_widget (ui_manager,
                    "/ToolbarActions/NewCallToolbar");
    pickUpWidget = gtk_ui_manager_get_widget (ui_manager,
                   "/ToolbarActions/PickUpToolbar");
    hangUpWidget = gtk_ui_manager_get_widget (ui_manager,
                   "/ToolbarActions/HangUpToolbar");
    recordWidget = gtk_ui_manager_get_widget (ui_manager,
                   "/ToolbarActions/RecordToolbar");
    historyButton = gtk_ui_manager_get_widget (ui_manager,
                    "/ToolbarActions/HistoryToolbar");
    contactButton = gtk_ui_manager_get_widget (ui_manager,
                    "/ToolbarActions/AddressbookToolbar");

    // Set the handler ID for the transfer
    transfertButtonConnId
    = g_signal_connect (G_OBJECT (transferToolbar), "toggled", G_CALLBACK (call_transfer_cb), NULL);
    recordButtonConnId
    = g_signal_connect (G_OBJECT (recordWidget), "toggled", G_CALLBACK (call_record), NULL);
    active_calltree = current_calls;

    *widget = toolbar;
}
