/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "config.h"
#include "actions.h"
#include "dbus.h"
#include "calltree.h"
#include "calltab.h"
#include "logger.h"
#include "preferencesdialog.h"
#include "dialpad.h"
#include "mainwindow.h"
#include "sliders.h"
#include "contacts/searchbar.h"
#include "statusicon.h" /* for set_minimized */
#include "assistant.h"
#include "widget/minidialog.h"
#include "uimanager.h"
#include "unused.h"
#include "config/audioconf.h"

#include "eel-gconf-extensions.h"

#include <sys/stat.h>
#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>

/** Local variables */
static GtkUIManager *ui_manager;
static GtkAccelGroup *accelGroup;
static GtkWidget *window;
static GtkWidget *subvbox;
static GtkWidget *vbox;
static GtkWidget *dialpad;
static GtkWidget *speaker_control;
static GtkWidget *mic_control;
static GtkWidget *statusBar;

static gchar *status_current_message;
static GMutex *gmutex;

static gboolean focus_is_on_searchbar;

void
focus_on_searchbar_out()
{
    focus_is_on_searchbar = FALSE;
}

void
focus_on_searchbar_in()
{
    focus_is_on_searchbar = TRUE;
}

/**
 * Handle main window resizing
 */
static gboolean window_configure_cb(GtkWidget *win UNUSED, GdkEventConfigure *event)
{
    eel_gconf_set_integer(CONF_MAIN_WINDOW_WIDTH, event->width);
    eel_gconf_set_integer(CONF_MAIN_WINDOW_HEIGHT, event->height);

    int pos_x, pos_y;
    gtk_window_get_position(GTK_WINDOW(window), &pos_x, &pos_y);
    eel_gconf_set_integer(CONF_MAIN_WINDOW_POSITION_X, pos_x);
    eel_gconf_set_integer(CONF_MAIN_WINDOW_POSITION_Y, pos_y);

    return FALSE;
}


/**
 * Minimize the main window.
 */
static gboolean
on_delete(GtkWidget * widget UNUSED, gpointer data UNUSED)
{
    if (eel_gconf_get_integer(SHOW_STATUSICON)) {
        gtk_widget_hide(get_main_window());
        set_minimized(TRUE);
    } else {
        g_mutex_free(gmutex);
        sflphone_quit();
    }

    return TRUE;
}

/** Ask the user if he wants to hangup current calls */
gboolean
main_window_ask_quit()
{
    gchar * question;

    if (calllist_get_size(current_calls_tab) == 1)
        question = _("There is one call in progress.");
    else
        question = _("There are calls in progress.");

    GtkWidget *dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(window),
                        GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s\n%s",
                        question, _("Do you still want to quit?"));

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    return (response == GTK_RESPONSE_NO) ? FALSE : TRUE ;
}

static gboolean
on_key_released(GtkWidget *widget UNUSED, GdkEventKey *event, gpointer user_data UNUSED)
{
    if (focus_is_on_searchbar)
        return TRUE;

    if (event->keyval == GDK_KEY_Return) {
        if (active_calltree_tab == current_calls_tab) {
            sflphone_keypad(event->keyval, event->string);
            return TRUE;
        } else if (active_calltree_tab == history_tab)
            return FALSE;
    }

    // If a modifier key is pressed, it's a shortcut, pass along
    if (event->state & GDK_CONTROL_MASK || event->state & GDK_MOD1_MASK ||
            event->keyval == '<' ||
            event->keyval == '>' ||
            event->keyval == '\"'||
            event->keyval == GDK_KEY_Tab ||
            event->keyval == GDK_KEY_Return ||
            event->keyval == GDK_KEY_Left ||
            event->keyval == GDK_KEY_Up ||
            event->keyval == GDK_KEY_Right ||
            event->keyval == GDK_KEY_Down ||
            (event->keyval >= GDK_KEY_F1 && event->keyval <= GDK_KEY_F12) ||
            event->keyval == ' ')
        return FALSE;
    else
        sflphone_keypad(event->keyval, event->string);

    return TRUE;
}

void
create_main_window()
{
    focus_is_on_searchbar = FALSE;

    // Get configuration stored in gconf
    int width =  eel_gconf_get_integer(CONF_MAIN_WINDOW_WIDTH);
    int height =  eel_gconf_get_integer(CONF_MAIN_WINDOW_HEIGHT);
    int position_x =  eel_gconf_get_integer(CONF_MAIN_WINDOW_POSITION_X);
    int position_y =  eel_gconf_get_integer(CONF_MAIN_WINDOW_POSITION_Y);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);
    gtk_window_set_title(GTK_WINDOW(window), "SFLphone VoIP Client");
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    struct stat st;

    if (!stat(LOGO, &st))
        gtk_window_set_default_icon_from_file(LOGO, NULL);

    gtk_window_set_position(GTK_WINDOW(window) , GTK_WIN_POS_MOUSE);

    /* Connect the destroy event of the window with our on_destroy function
     * When the window is about to be destroyed we get a notificaiton and
     * stop the main GTK loop
     */
    g_signal_connect(G_OBJECT(window), "delete-event",
                     G_CALLBACK(on_delete), NULL);

    g_signal_connect(G_OBJECT(window), "key-release-event",
                     G_CALLBACK(on_key_released), NULL);

    g_signal_connect_object(G_OBJECT(window), "configure-event",
                            G_CALLBACK(window_configure_cb), NULL, 0);

    gtk_widget_set_name(window, "mainwindow");

    ui_manager = uimanager_new();

    if (!ui_manager) {
        ERROR("Could not load xml GUI\n");
        exit(1);
    }

    /* Create an accel group for window's shortcuts */
    gtk_window_add_accel_group(GTK_WINDOW(window),
                               gtk_ui_manager_get_accel_group(ui_manager));

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0 /*spacing*/);
    subvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5 /*spacing*/);

    GtkWidget *widget = create_menus(ui_manager);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE /*expand*/, TRUE /*fill*/,
                       0 /*padding*/);

    widget = create_toolbar_actions(ui_manager);
    // Do not override GNOME user settings
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE /*expand*/, TRUE /*fill*/,
                       0 /*padding*/);

    gtk_box_pack_start(GTK_BOX(vbox), current_calls_tab->tree, TRUE /*expand*/,
                       TRUE /*fill*/, 0 /*padding*/);
    gtk_box_pack_start(GTK_BOX(vbox), history_tab->tree, TRUE /*expand*/,
                       TRUE /*fill*/, 0 /*padding*/);
    gtk_box_pack_start(GTK_BOX(vbox), contacts_tab->tree, TRUE /*expand*/,
                       TRUE /*fill*/, 0 /*padding*/);

    g_signal_connect_object(G_OBJECT(window), "configure-event",
                            G_CALLBACK(window_configure_cb), NULL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), subvbox, FALSE /*expand*/,
                       FALSE /*fill*/, 0 /*padding*/);

    if (SHOW_VOLUME) {
        speaker_control = create_slider("speaker");
        gtk_box_pack_end(GTK_BOX(subvbox), speaker_control, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(speaker_control);
        mic_control = create_slider("mic");
        gtk_box_pack_end(GTK_BOX(subvbox), mic_control, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(mic_control);
    }

    if (eel_gconf_get_boolean(CONF_SHOW_DIALPAD)) {
        dialpad = create_dialpad();
        gtk_box_pack_end(GTK_BOX(subvbox), dialpad, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(dialpad);
    }

    /* Status bar */
    statusBar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), statusBar, FALSE /*expand*/,
                       TRUE /*fill*/, 0 /*padding*/);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(window);

    /* dont't show the history */
    gtk_widget_hide(history_tab->tree);

    /* dont't show the contact list */
    gtk_widget_hide(contacts_tab->tree);

    /* don't show waiting layer */
    gtk_widget_hide(waitingLayer);

    gmutex = g_mutex_new();

    g_timeout_add_seconds(1, calltree_update_clock, NULL);

    // Configuration wizard
    if (account_list_get_size() == 1)
        build_wizard();

    // Restore position according to the configuration stored in gconf
    gtk_window_move(GTK_WINDOW(window), position_x, position_y);
}

GtkAccelGroup *
get_accel_group()
{
    return accelGroup;
}

GtkWidget *
get_main_window()
{
    return window;
}

void
main_window_dialpad(gboolean state)
{
    if (state) {
        dialpad = create_dialpad();
        gtk_box_pack_end(GTK_BOX(subvbox), dialpad, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(dialpad);
    } else
        gtk_container_remove(GTK_CONTAINER(subvbox), dialpad);
}

void
main_window_volume_controls(gboolean state)
{
    if (state) {
        speaker_control = create_slider("speaker");
        gtk_box_pack_end(GTK_BOX(subvbox), speaker_control, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(speaker_control);
        mic_control = create_slider("mic");
        gtk_box_pack_end(GTK_BOX(subvbox), mic_control, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(mic_control);
    } else {
        gtk_container_remove(GTK_CONTAINER(subvbox), speaker_control);
        gtk_container_remove(GTK_CONTAINER(subvbox), mic_control);
    }
}

void
statusbar_push_message(const gchar * const left_hand_message, const gchar * const right_hand_message, guint id)
{
    g_mutex_lock(gmutex);

    g_free(status_current_message);
    // store the left hand message so that it can be reused in case of clock update
    status_current_message = g_strdup(left_hand_message);

    // Format message according to right hand member
    gchar *message_to_display;

    if (right_hand_message)
        message_to_display = g_strdup_printf("%s           %s",
                                             left_hand_message, right_hand_message);
    else
        message_to_display = g_strdup(left_hand_message);

    // Push into the statusbar
    gtk_statusbar_push(GTK_STATUSBAR(statusBar), id, message_to_display);

    g_free(message_to_display);

    g_mutex_unlock(gmutex);
}

void
statusbar_pop_message(guint id)
{
    gtk_statusbar_pop(GTK_STATUSBAR(statusBar), id);
}

void
statusbar_update_clock(const gchar * const msg)
{
    g_mutex_lock(gmutex);
    gchar *message = g_strdup(status_current_message);
    g_mutex_unlock(gmutex);

    if (message) {
        statusbar_pop_message(__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message(message, msg, __MSG_ACCOUNT_DEFAULT);
    }

    g_free(message);
}


static void
destroy_error_dialog_cb(GtkWidget *dialog UNUSED, GtkWidget *win)
{
    gtk_widget_destroy(win);
}

static void
add_error_dialog(GtkWidget *dialog)
{
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(win), dialog);

    g_signal_connect_after(dialog, "destroy", (GCallback)destroy_error_dialog_cb, win);

    gtk_widget_show(win);
}

void
main_window_zrtp_not_supported(callable_obj_t * c)
{
    gchar* warning_enabled = "";

    account_t *account_details = account_list_get_by_id(c->_accountID);

    if (account_details != NULL) {
        warning_enabled = g_hash_table_lookup(account_details->properties,
                                              ACCOUNT_ZRTP_NOT_SUPP_WARNING);
        DEBUG("Warning Enabled %s", warning_enabled);
    } else {
        DEBUG("Account is null callID %s", c->_callID);
        GHashTable * properties = sflphone_get_ip2ip_properties();

        if (properties)
            warning_enabled = g_hash_table_lookup (properties,
                                                   ACCOUNT_ZRTP_NOT_SUPP_WARNING);
    }

    if (g_strcasecmp(warning_enabled, "true") == 0) {
        PidginMiniDialog *mini_dialog;
        gchar *desc = g_markup_printf_escaped(
                          _("ZRTP is not supported by peer %s\n"), c->_peer_number);
        mini_dialog = pidgin_mini_dialog_new(
                          _("Secure Communication Unavailable"), desc,
                          GTK_STOCK_DIALOG_WARNING);
        g_free(desc);
        pidgin_mini_dialog_add_button(mini_dialog, _("Continue"), NULL, NULL);
        pidgin_mini_dialog_add_button(mini_dialog, _("Stop Call"),
                                      sflphone_hang_up, NULL);

        add_error_dialog(GTK_WIDGET(mini_dialog));
    }
}

void
main_window_zrtp_negotiation_failed(const gchar* const callID, const gchar* const reason,
                                    const gchar* const severity)
{
    gchar* peer_number = "(number unknown)";
    callable_obj_t * c = NULL;
    c = calllist_get_call(current_calls_tab, callID);

    if (c != NULL)
        peer_number = c->_peer_number;

    gchar *desc = g_markup_printf_escaped(_("A %s error forced the call with "
                                            "%s to fall under unencrypted "
                                            "mode.\nExact reason: %s\n"),
                                          severity, peer_number, reason);
    PidginMiniDialog *mini_dialog = pidgin_mini_dialog_new(_("ZRTP negotiation failed"), desc,
                                    GTK_STOCK_DIALOG_WARNING);
    g_free(desc);
    pidgin_mini_dialog_add_button(mini_dialog, _("Continue"), NULL, NULL);
    pidgin_mini_dialog_add_button(mini_dialog, _("Stop Call"), sflphone_hang_up,
                                  NULL);

    add_error_dialog(GTK_WIDGET(mini_dialog));
}

void
main_window_confirm_go_clear(callable_obj_t * c)
{
    gchar *desc = g_markup_printf_escaped(
                      _("%s wants to stop using secure communication. Confirm will resume conversation without SRTP.\n"),
                      c->_peer_number);
    PidginMiniDialog *mini_dialog = pidgin_mini_dialog_new(_("Confirm Go Clear"), desc,
                                    GTK_STOCK_STOP);
    g_free(desc);
    pidgin_mini_dialog_add_button(mini_dialog, _("Confirm"),
                                  (PidginMiniDialogCallback) dbus_set_confirm_go_clear, NULL);
    pidgin_mini_dialog_add_button(mini_dialog, _("Stop Call"), sflphone_hang_up,
                                  NULL);

    add_error_dialog(GTK_WIDGET(mini_dialog));
}
