/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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
#include <actions.h>
#include <calltree.h>
#include <calltab.h>
#include <preferencesdialog.h>
#include <dialpad.h>
#include <mainwindow.h>
#include <sliders.h>
#include <contacts/searchbar.h>
#include <assistant.h>
#include <widget/gtkscrollbook.h>
#include <widget/minidialog.h>
#include "uimanager.h"

#include <gtk/gtk.h>
#include <eel-gconf-extensions.h>

/** Local variables */
GtkUIManager *ui_manager = NULL;
GtkAccelGroup * accelGroup = NULL;
GtkWidget * window = NULL;
GtkWidget * subvbox = NULL;
GtkWidget * vbox = NULL;
GtkWidget * dialpad = NULL;
GtkWidget * speaker_control = NULL;
GtkWidget * mic_control = NULL;
GtkWidget * statusBar = NULL;
GtkWidget * filterEntry = NULL;
PidginScrollBook *embedded_error_notebook;

gchar *status_current_message = NULL;
// pthread_mutex_t statusbar_message_mutex;
GMutex *gmutex;

/**
 * Handle main window resizing
 */
static gboolean window_configure_cb (GtkWidget *win UNUSED, GdkEventConfigure *event)
{

    int pos_x, pos_y;

    eel_gconf_set_integer (CONF_MAIN_WINDOW_WIDTH, event->width);
    eel_gconf_set_integer (CONF_MAIN_WINDOW_HEIGHT, event->height);

    gtk_window_get_position (GTK_WINDOW (window), &pos_x, &pos_y);
    eel_gconf_set_integer (CONF_MAIN_WINDOW_POSITION_X, pos_x);
    eel_gconf_set_integer (CONF_MAIN_WINDOW_POSITION_Y, pos_y);

    return FALSE;
}


/**
 * Minimize the main window.
 */
static gboolean
on_delete (GtkWidget * widget UNUSED, gpointer data UNUSED)
{

    if (eel_gconf_get_integer (SHOW_STATUSICON)) {
        gtk_widget_hide (GTK_WIDGET (get_main_window()));
        set_minimized (TRUE);
    } else {
        sflphone_quit ();
    }

    // pthread_mutex_destroy (&statusbar_message_mutex);
    g_mutex_free (gmutex);
    return TRUE;
}

/** Ask the user if he wants to hangup current calls */
gboolean
main_window_ask_quit ()
{
    guint count = calllist_get_size (current_calls);
    GtkWidget * dialog;
    gint response;
    gchar * question;

    if (count == 1) {
        question = _ ("There is one call in progress.");
    } else {
        question = _ ("There are calls in progress.");
    }

    dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
             GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s\n%s",
             question, _ ("Do you still want to quit?"));

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return (response == GTK_RESPONSE_NO) ? FALSE : TRUE ;


}

static gboolean
on_key_released (GtkWidget *widget, GdkEventKey *event, gpointer user_data UNUSED)
{
    DEBUG ("On key released from Main Window : %s", gtk_widget_get_name (widget));

    if (focus_is_on_searchbar == FALSE) {
        // If a modifier key is pressed, it's a shortcut, pass along
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_MOD1_MASK
                || event->keyval == 60 || // <
                event->keyval == 62 || // >
                event->keyval == 34 || // "
                event->keyval == 65289 || // tab
                event->keyval == 65361 || // left arrow
                event->keyval == 65362 || // up arrow
                event->keyval == 65363 || // right arrow
                event->keyval == 65364 || // down arrow
                event->keyval >= 65470 || // F-keys
                event->keyval == 32 // space
           )
            return FALSE;
        else
            sflphone_keypad (event->keyval, event->string);
    }

    return TRUE;
}

void
focus_on_mainwindow_out ()
{
    //  gtk_widget_grab_focus(GTK_WIDGET(window));

}

void
focus_on_mainwindow_in ()
{
    //  gtk_widget_grab_focus(GTK_WIDGET(window));
}

void
create_main_window ()
{
    GtkWidget *widget;
    GError *error = NULL;
    gboolean ret;
    const char *window_title = "SFLphone VoIP Client";
    int width, height, position_x, position_y;

    focus_is_on_calltree = FALSE;
    focus_is_on_searchbar = FALSE;

    // Get configuration stored in gconf
    width =  eel_gconf_get_integer (CONF_MAIN_WINDOW_WIDTH);
    height =  eel_gconf_get_integer (CONF_MAIN_WINDOW_HEIGHT);
    position_x =  eel_gconf_get_integer (CONF_MAIN_WINDOW_POSITION_X);
    position_y =  eel_gconf_get_integer (CONF_MAIN_WINDOW_POSITION_Y);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_title (GTK_WINDOW (window), window_title);
    gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    gtk_window_set_default_icon_from_file (LOGO,
                                           NULL);
    gtk_window_set_position (GTK_WINDOW (window) , GTK_WIN_POS_MOUSE);

    /* Connect the destroy event of the window with our on_destroy function
     * When the window is about to be destroyed we get a notificaiton and
     * stop the main GTK loop
     */
    g_signal_connect (G_OBJECT (window), "delete-event",
                      G_CALLBACK (on_delete), NULL);

    g_signal_connect (G_OBJECT (window), "key-release-event",
                      G_CALLBACK (on_key_released), NULL);

    g_signal_connect_after (G_OBJECT (window), "focus-in-event",
                            G_CALLBACK (focus_on_mainwindow_in), NULL);

    g_signal_connect_after (G_OBJECT (window), "focus-out-event",
                            G_CALLBACK (focus_on_mainwindow_out), NULL);

    g_signal_connect_object (G_OBJECT (window), "configure-event",
                             G_CALLBACK (window_configure_cb), NULL, 0);

    gtk_widget_set_name (window, "mainwindow");

    ret = uimanager_new (&ui_manager);

    if (!ret) {
        ERROR ("Could not load xml GUI\n");
        g_error_free (error);
        exit (1);
    }

    /* Create an accel group for window's shortcuts */
    gtk_window_add_accel_group (GTK_WINDOW (window),
                                gtk_ui_manager_get_accel_group (ui_manager));

    vbox = gtk_vbox_new (FALSE /*homogeneous*/, 0 /*spacing*/);
    subvbox = gtk_vbox_new (FALSE /*homogeneous*/, 5 /*spacing*/);

    create_menus (ui_manager, &widget);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE /*expand*/, TRUE /*fill*/,
                        0 /*padding*/);

    create_toolbar_actions (ui_manager, &widget);
    // Do not override GNOME user settings
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE /*expand*/, TRUE /*fill*/,
                        0 /*padding*/);

    gtk_box_pack_start (GTK_BOX (vbox), current_calls->tree, TRUE /*expand*/,
                        TRUE /*fill*/, 0 /*padding*/);
    gtk_box_pack_start (GTK_BOX (vbox), history->tree, TRUE /*expand*/,
                        TRUE /*fill*/, 0 /*padding*/);
    gtk_box_pack_start (GTK_BOX (vbox), contacts->tree, TRUE /*expand*/,
                        TRUE /*fill*/, 0 /*padding*/);

    g_signal_connect_object (G_OBJECT (window), "configure-event",
                             G_CALLBACK (window_configure_cb), NULL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), subvbox, FALSE /*expand*/,
                        FALSE /*fill*/, 0 /*padding*/);

    embedded_error_notebook = PIDGIN_SCROLL_BOOK (pidgin_scroll_book_new());
    gtk_box_pack_start (GTK_BOX (subvbox), GTK_WIDGET (embedded_error_notebook),
                        FALSE, FALSE, 0);

    if (SHOW_VOLUME) {
        speaker_control = create_slider ("speaker");
        gtk_box_pack_end (GTK_BOX (subvbox), speaker_control, FALSE /*expand*/,
                          TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all (speaker_control);
        mic_control = create_slider ("mic");
        gtk_box_pack_end (GTK_BOX (subvbox), mic_control, FALSE /*expand*/,
                          TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all (mic_control);
    }


    if (eel_gconf_get_boolean (CONF_SHOW_DIALPAD)) {
        dialpad = create_dialpad();
        gtk_box_pack_end (GTK_BOX (subvbox), dialpad, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all (dialpad);
    }

    /* Status bar */
    statusBar = gtk_statusbar_new ();
    gtk_box_pack_start (GTK_BOX (vbox), statusBar, FALSE /*expand*/,
                        TRUE /*fill*/, 0 /*padding*/);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all (window);

    /* dont't show the history */
    gtk_widget_hide (history->tree);

    /* dont't show the contact list */
    gtk_widget_hide (contacts->tree);

    searchbar_init (history);
    searchbar_init (contacts);

    /* don't show waiting layer */
    gtk_widget_hide (waitingLayer);

    // pthread_mutex_init (&statusbar_message_mutex, NULL);
    gmutex = g_mutex_new();

    // Configuration wizard
    if (account_list_get_size () == 1) {
#if GTK_CHECK_VERSION(2,10,0)
        build_wizard ();
#else
        GtkWidget * dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
                             GTK_DIALOG_DESTROY_WITH_PARENT,
                             GTK_MESSAGE_INFO,
                             GTK_BUTTONS_YES_NO,
                             "<b><big>Welcome to SFLphone!</big></b>\n\nThere are no VoIP accounts configured, would you like to edit the preferences now?");

        int response = gtk_dialog_run (GTK_DIALOG (dialog));

        gtk_widget_destroy (GTK_WIDGET (dialog));

        if (response == GTK_RESPONSE_YES) {
            show_preferences_dialog();
        }

#endif
    }

    // Restore position according to the configuration stored in gconf
    gtk_window_move (GTK_WINDOW (window), position_x, position_y);
}

GtkAccelGroup *
get_accel_group ()
{
    return accelGroup;
}

GtkWidget *
get_main_window ()
{
    return window;
}

void
main_window_message (GtkMessageType type, gchar * markup)
{

    GtkWidget * dialog = gtk_message_dialog_new_with_markup (
                             GTK_WINDOW (get_main_window()), GTK_DIALOG_MODAL
                             | GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_CLOSE, NULL);

    gtk_window_set_title (GTK_WINDOW (dialog), _ ("SFLphone Error"));
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
main_window_error_message (gchar * markup)
{
    main_window_message (GTK_MESSAGE_ERROR, markup);
}

void
main_window_warning_message (gchar * markup)
{
    main_window_message (GTK_MESSAGE_WARNING, markup);
}

void
main_window_info_message (gchar * markup)
{
    main_window_message (GTK_MESSAGE_INFO, markup);
}

void
main_window_dialpad (gboolean state)
{

    g_print ("main_window_dialpad\n");

    if (state) {
        dialpad = create_dialpad ();
        gtk_box_pack_end (GTK_BOX (subvbox), dialpad, FALSE /*expand*/,
                          TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all (dialpad);
    } else {
        gtk_container_remove (GTK_CONTAINER (subvbox), dialpad);
    }
}

void
main_window_volume_controls (gboolean state)
{
    if (state) {
        speaker_control = create_slider ("speaker");
        gtk_box_pack_end (GTK_BOX (subvbox), speaker_control, FALSE /*expand*/,
                          TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all (speaker_control);
        mic_control = create_slider ("mic");
        gtk_box_pack_end (GTK_BOX (subvbox), mic_control, FALSE /*expand*/,
                          TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all (mic_control);
    } else {
        gtk_container_remove (GTK_CONTAINER (subvbox), speaker_control);
        gtk_container_remove (GTK_CONTAINER (subvbox), mic_control);
    }
}

void
statusbar_push_message (const gchar *left_hand_message, const gchar *right_hand_message, guint id)
{
    // The actual message to be push in the statusbar
    gchar *message_to_display;

    g_mutex_lock (gmutex);
    // pthread_mutex_lock (&statusbar_message_mutex);

    g_free (status_current_message);
    // store the left hand message so that it can be reused in case of clock update
    status_current_message = g_strdup (left_hand_message);

    // Format message according to right hand member
    if (right_hand_message)
        message_to_display = g_strdup_printf ("%s           %s",
                                              left_hand_message, right_hand_message);
    else
        message_to_display = g_strdup (left_hand_message);

    // Push into the statusbar
    gtk_statusbar_push (GTK_STATUSBAR (statusBar), id, message_to_display);

    g_free (message_to_display);

    // pthread_mutex_unlock (&statusbar_message_mutex);
    g_mutex_unlock (gmutex);
}

void
statusbar_pop_message (guint id)
{
    gtk_statusbar_pop (GTK_STATUSBAR (statusBar), id);
}

void
statusbar_update_clock (gchar *msg)
{
    gchar *message = NULL;

    if (!msg) {
        statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message (message, NULL, __MSG_ACCOUNT_DEFAULT);
    }


    // pthread_mutex_lock (&statusbar_message_mutex);
    g_mutex_lock (gmutex);
    message = g_strdup (status_current_message);
    // pthread_mutex_unlock (&statusbar_message_mutex);
    g_mutex_unlock (gmutex);

    if (message) {
        statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message (message, msg, __MSG_ACCOUNT_DEFAULT);
    }

    g_free (message);
    message = NULL;

}

static void
add_error_dialog (GtkWidget *dialog, callable_obj_t * call)
{
    gtk_container_add (GTK_CONTAINER (embedded_error_notebook), dialog);
    call_add_error (call, dialog);
}

static void
destroy_error_dialog_cb (GtkObject *dialog, callable_obj_t * call)
{
    call_remove_error (call, dialog);
}

void
main_window_zrtp_not_supported (callable_obj_t * c)
{
    account_t* account_details = NULL;
    gchar* warning_enabled = "";

    account_details = account_list_get_by_id (c->_accountID);

    if (account_details != NULL) {
        warning_enabled = g_hash_table_lookup (account_details->properties,
                                               ACCOUNT_ZRTP_NOT_SUPP_WARNING);
        DEBUG ("Warning Enabled %s", warning_enabled);
    } else {
        DEBUG ("Account is null callID %s", c->_callID);
        GHashTable * properties = NULL;
        sflphone_get_ip2ip_properties (&properties);

        if (properties != NULL) {
            warning_enabled = g_hash_table_lookup (properties,
                                                   ACCOUNT_ZRTP_NOT_SUPP_WARNING);
        }
    }

    if (g_strcasecmp (warning_enabled, "true") == 0) {
        PidginMiniDialog *mini_dialog;
        gchar *desc = g_markup_printf_escaped (
                          _ ("ZRTP is not supported by peer %s\n"), c->_peer_number);
        mini_dialog = pidgin_mini_dialog_new (
                          _ ("Secure Communication Unavailable"), desc,
                          GTK_STOCK_DIALOG_WARNING);
        pidgin_mini_dialog_add_button (mini_dialog, _ ("Continue"), NULL, NULL);
        pidgin_mini_dialog_add_button (mini_dialog, _ ("Stop Call"),
                                       sflphone_hang_up, NULL);

        g_signal_connect_after (mini_dialog, "destroy", (GCallback) destroy_error_dialog_cb, c);

        add_error_dialog (GTK_WIDGET (mini_dialog), c);
    }
}

void
main_window_zrtp_negotiation_failed (const gchar* callID, const gchar* reason,
                                     const gchar* severity)
{
    gchar* peer_number = "(number unknown)";
    callable_obj_t * c = NULL;
    c = calllist_get (current_calls, callID);

    if (c != NULL) {
        peer_number = c->_peer_number;
    }

    PidginMiniDialog *mini_dialog;
    gchar
    *desc =
        g_markup_printf_escaped (
            _ ("A %s error forced the call with %s to fall under unencrypted mode.\nExact reason: %s\n"),
            severity, peer_number, reason);
    mini_dialog = pidgin_mini_dialog_new (_ ("ZRTP negotiation failed"), desc,
                                          GTK_STOCK_DIALOG_WARNING);
    pidgin_mini_dialog_add_button (mini_dialog, _ ("Continue"), NULL, NULL);
    pidgin_mini_dialog_add_button (mini_dialog, _ ("Stop Call"), sflphone_hang_up,
                                   NULL);

    g_signal_connect_after (mini_dialog, "destroy", (GCallback) destroy_error_dialog_cb, c);

    add_error_dialog (GTK_WIDGET (mini_dialog), c);
}

void
main_window_confirm_go_clear (callable_obj_t * c)
{
    PidginMiniDialog *mini_dialog;
    gchar
    *desc =
        g_markup_printf_escaped (
            _ ("%s wants to stop using secure communication. Confirm will resume conversation without SRTP.\n"),
            c->_peer_number);
    mini_dialog = pidgin_mini_dialog_new (_ ("Confirm Go Clear"), desc,
                                          GTK_STOCK_STOP);
    pidgin_mini_dialog_add_button (mini_dialog, _ ("Confirm"),
                                   (PidginMiniDialogCallback) sflphone_set_confirm_go_clear, NULL);
    pidgin_mini_dialog_add_button (mini_dialog, _ ("Stop Call"), sflphone_hang_up,
                                   NULL);

    add_error_dialog (GTK_WIDGET (mini_dialog), c);
}

