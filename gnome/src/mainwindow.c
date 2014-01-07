/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "account_schema.h"
#include "actions.h"
#include "dbus.h"
#include "calltree.h"
#include "calltab.h"
#include "preferencesdialog.h"
#include "dialpad.h"
#include "mainwindow.h"
#include "sliders.h"
#include "contacts/searchbar.h"
#include "statusicon.h" /* for set_minimized */
#include "assistant.h"
#include "widget/minidialog.h"
#include "uimanager.h"
#include "config/audioconf.h"
#include "str_utils.h"
#include "seekslider.h"
#include "messaging/message_tab.h"

#include <glib/gi18n.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>


/** Local variables */
static GtkAccelGroup *accelGroup;
static GtkWidget *subvbox;
static GtkWidget *vbox;
static GtkWidget *dialpad;
static GtkWidget *speaker_control;
static GtkWidget *mic_control;
static GtkWidget *statusBar;
static GtkWidget *seekslider = NULL;

static gchar *status_current_message;

static gboolean focus_is_on_searchbar = FALSE;

static gboolean pause_grabber = FALSE;

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
 * Save the vpaned size
 */
static void
on_messaging_paned_position_change(GtkPaned* paned, G_GNUC_UNUSED GtkScrollType scroll_type, SFLPhoneClient *client)
{
    const gint height = gtk_paned_get_position(paned);
    g_settings_set_int(client->settings, "message-tab-height", height);
    set_message_tab_height(paned, height);
}

/**
 * Handle main window resizing
 */
static gboolean window_configure_cb(GtkWidget *win, GdkEventConfigure *event, SFLPhoneClient *client)
{
    g_settings_set_int(client->settings, "window-width", event->width);
    g_settings_set_int(client->settings, "window-height", event->height);

    gint height = 0;
    gint width  = 0;
    gtk_widget_get_size_request(get_tab_box(),&width,&height);

    gint pos_x, pos_y;
    gtk_window_get_position(GTK_WINDOW(win), &pos_x, &pos_y);
    g_settings_set_int(client->settings, "window-position-x", pos_x);
    g_settings_set_int(client->settings, "window-position-y", pos_y);

    return FALSE;
}

/**
 * Minimize the main window.
 */
static gboolean
on_delete(SFLPhoneClient *client)
{
    sflphone_quit(FALSE, client);
    return TRUE;
}

/** Ask the user if he wants to hangup current calls */
gboolean
main_window_ask_quit(SFLPhoneClient *client)
{
    gchar * question;

    if (calllist_get_size(current_calls_tab) == 1)
        question = _("There is one call in progress.");
    else
        question = _("There are calls in progress.");
    g_debug("Currently %d calls in progress", calllist_get_size(current_calls_tab));

    GtkWidget *dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(client->win),
                        GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s\n%s",
                        question, _("Do you still want to quit?"));

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    return (response == GTK_RESPONSE_NO) ? FALSE : TRUE ;
}

static gboolean
on_key_released(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event, SFLPhoneClient *client)
{
    if (!pause_grabber) {
        if (focus_is_on_searchbar)
           return TRUE;

        if (event->keyval == GDK_KEY_Return) {
           if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
                 sflphone_keypad(event->keyval, event->string, client);
                 return TRUE;
           } else if (calltab_has_name(active_calltree_tab, HISTORY))
                 return FALSE;
        }

        // If a modifier key is pressed, it's a shortcut, pass along
        if (event->state & GDK_CONTROL_MASK ||
            event->state & GDK_MOD1_MASK ||
            event->keyval == '<' ||
            event->keyval == '>' ||
            event->keyval == '\"'||
            event->keyval == GDK_KEY_Tab ||
            event->keyval == GDK_KEY_Return ||
            event->keyval == GDK_KEY_Left ||
            event->keyval == GDK_KEY_Up ||
            event->keyval == GDK_KEY_Right ||
            event->keyval == GDK_KEY_Down ||
            (event->keyval >= GDK_KEY_Shift_L  && event->keyval <= GDK_KEY_Hyper_R) ||
            (event->keyval >= GDK_KEY_ISO_Lock && event->keyval <= GDK_KEY_ISO_Enter) ||
            (event->keyval >= GDK_KEY_F1 && event->keyval <= GDK_KEY_F12) ||
            event->keyval == ' ')
            return FALSE;
        else
            sflphone_keypad(event->keyval, event->string, client);

        return TRUE;
    }
    return FALSE;
}

static void pack_main_window_start(GtkBox *box, GtkWidget *widget, gboolean expand, gboolean fill, guint padding)
{
    if(box == NULL) {
        g_warning("Box is NULL while packing main window");
        return;
    }

    if(widget == NULL) {
        g_warning("Widget is NULL while packing the mainwindow");
        return;
    }

    GtkWidget *alignment =  gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 6, 6);
    gtk_container_add(GTK_CONTAINER(alignment), widget);
    gtk_box_pack_start(box, alignment, expand, fill, padding);
}

void
main_window_bring_to_front(SFLPhoneClient *client, guint32 timestamp)
{
    /* Window should not be in focus, in case user was entering sensitive
     * information in another application */
    gtk_window_present_with_time(GTK_WINDOW(client->win), timestamp);
}


GtkWidget*
create_status_bar()
{
    GtkWidget *bar = gtk_statusbar_new();
    return bar;
}


void
create_main_window(SFLPhoneClient *client)
{
    // Get configuration stored in GSettings
    int width =  g_settings_get_int(client->settings, "window-width");
    int height =  g_settings_get_int(client->settings, "window-height");
    int position_x =  g_settings_get_int(client->settings, "window-position-x");
    int position_y =  g_settings_get_int(client->settings, "window-position-y");

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_container_set_border_width(GTK_CONTAINER(window), 0);

    gtk_window_set_title(GTK_WINDOW(window), "SFLphone VoIP Client");
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    struct stat st;

    if (!stat(LOGO, &st))
        gtk_window_set_default_icon_from_file(LOGO, NULL);

    gtk_window_set_position(GTK_WINDOW(window) , GTK_WIN_POS_MOUSE);
    gtk_widget_set_name(window, "mainwindow");

    /* Connect the destroy event of the window with our on_destroy function
     * When the window is about to be destroyed we get a notificaiton and
     * stop the main GTK loop
     */
    g_signal_connect_swapped(G_OBJECT(window), "delete-event",
                             G_CALLBACK(on_delete), client);

    g_signal_connect(G_OBJECT(window), "key-release-event",
                     G_CALLBACK(on_key_released), client);

    g_signal_connect(G_OBJECT(window), "configure-event",
                     G_CALLBACK(window_configure_cb), client);


    GtkUIManager *ui_manager = uimanager_new(client);
    if (!ui_manager) {
        g_warning("Could not load xml GUI\n");
        exit(1);
    }

    /* Create an accel group for window's shortcuts */
    gtk_window_add_accel_group(GTK_WINDOW(window), gtk_ui_manager_get_accel_group(ui_manager));

    /* Instantiate vbox, subvbox as homogeneous */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);

    subvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(subvbox), FALSE);

    /*Create the messaging tab container*/
    GtkWidget *tab_widget = get_tab_box();
    gtk_widget_show (tab_widget);

    /* Populate the main window */
    GtkWidget *widget = create_menus(ui_manager, client);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);

    create_toolbar_actions(ui_manager, client);
    pack_main_window_start(GTK_BOX(vbox), client->toolbar, FALSE, TRUE, 0);

    /* Setup call main widget*/
    GtkWidget *vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    current_calls_tab->mainwidget = vpaned;

    const gint messaging_height = g_settings_get_int(client->settings, "message-tab-height");
    set_message_tab_height(GTK_PANED(vpaned), messaging_height);

    gtk_widget_show (vpaned);
    gtk_box_pack_start(GTK_BOX(vbox), vpaned, TRUE, TRUE, 0);

    /* Setup history main widget */
    GtkWidget *history_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    history_tab->mainwidget = history_vbox;
    gtk_box_set_homogeneous(GTK_BOX(history_vbox), FALSE);
    gtk_box_pack_start(GTK_BOX(history_vbox), history_tab->tree, TRUE, TRUE, 0);

    /* Add tree views */
    gtk_box_pack_start(GTK_BOX(vbox), contacts_tab->tree, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), history_vbox, TRUE, TRUE, 0);
    gtk_paned_pack1 (GTK_PANED (vpaned), current_calls_tab->tree, TRUE, FALSE);
    gtk_paned_pack2 (GTK_PANED (vpaned), tab_widget, FALSE, FALSE);

    g_signal_connect(G_OBJECT(vpaned), "notify::position", G_CALLBACK(on_messaging_paned_position_change), client);

    /* Add playback scale and setup history tab */
    seekslider = GTK_WIDGET(sfl_seekslider_new());
    pack_main_window_start(GTK_BOX(history_vbox), seekslider, FALSE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), subvbox, FALSE, FALSE, 0);

    speaker_control = create_slider("speaker");
    mic_control = create_slider("mic");
    g_object_ref(speaker_control);
    g_object_ref(mic_control);

    if (g_settings_get_boolean(client->settings, "show-dialpad")) {
        dialpad = create_dialpad(client);
        gtk_box_pack_end(GTK_BOX(subvbox), dialpad, FALSE, TRUE, 0);
        gtk_widget_show_all(dialpad);
    }

    if (must_show_volume(client)) {
        gtk_box_pack_end(GTK_BOX(subvbox), speaker_control, FALSE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(subvbox), mic_control, FALSE, TRUE, 0);
        gtk_widget_show_all(speaker_control);
        gtk_widget_show_all(mic_control);
    } else {
        gtk_widget_hide(speaker_control);
        gtk_widget_hide(mic_control);
    }


    /* Status bar */
    statusBar = create_status_bar();
    pack_main_window_start(GTK_BOX(vbox), statusBar, FALSE, TRUE, 0);

    /* Add to main window */
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(window);

    /* dont't show the history */
    gtk_widget_hide(history_vbox);

    /* dont't show the contact list */
    gtk_widget_hide(contacts_tab->tree);

    /* show playback scale only if a recorded call is selected */
    sfl_seekslider_set_display(SFL_SEEKSLIDER(seekslider), SFL_SEEKSLIDER_DISPLAY_PLAY);
    gtk_widget_set_sensitive(seekslider, FALSE);
    main_window_hide_playback_scale();

    /* don't show waiting layer */
    gtk_widget_hide(waitingLayer);

    g_timeout_add_seconds(1, calltree_update_clock, NULL);

    // Configuration wizard
    if (account_list_get_size() == 1)
        build_wizard();

    // Restore position according to the configuration stored in gconf
    gtk_window_move(GTK_WINDOW(window), position_x, position_y);
    client->win = window;
}

GtkAccelGroup *
get_accel_group()
{
    return accelGroup;
}

void
main_window_dialpad(gboolean state, SFLPhoneClient *client)
{
    if (state) {
        dialpad = create_dialpad(client);
        gtk_box_pack_end(GTK_BOX(subvbox), dialpad, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(dialpad);
    } else if (GTK_IS_WIDGET(dialpad))
        gtk_container_remove(GTK_CONTAINER(subvbox), dialpad);
}

void
main_window_volume_controls(gboolean state)
{
    if (state) {
        // speaker_control = create_slider("speaker");
        gtk_box_pack_end(GTK_BOX(subvbox), speaker_control, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(speaker_control);
        // mic_control = create_slider("mic");
        gtk_box_pack_end(GTK_BOX(subvbox), mic_control, FALSE /*expand*/,
                         TRUE /*fill*/, 0 /*padding*/);
        gtk_widget_show_all(mic_control);
    } else {
        if (GTK_IS_WIDGET(speaker_control))
            gtk_container_remove(GTK_CONTAINER(subvbox), speaker_control);
        if (GTK_IS_WIDGET(mic_control))
            gtk_container_remove(GTK_CONTAINER(subvbox), mic_control);
    }
}

void
statusbar_push_message(const gchar * const left_hand_message, const gchar * const right_hand_message, guint id)
{
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
}

void
statusbar_pop_message(guint id)
{
    gtk_statusbar_pop(GTK_STATUSBAR(statusBar), id);
}

void
statusbar_update_clock(const gchar * msg)
{
    gchar *message = g_strdup(status_current_message);

    if (message) {
        statusbar_pop_message(__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message(message, msg, __MSG_ACCOUNT_DEFAULT);
    }

    g_free(message);
}


static void
destroy_error_dialog_cb(G_GNUC_UNUSED GtkWidget *dialog, GtkWidget *win)
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
main_window_zrtp_not_supported(callable_obj_t * c, SFLPhoneClient *client)
{
    gchar* warning_enabled = "";

    account_t *account = account_list_get_by_id(c->_accountID);

    if (account != NULL) {
        warning_enabled = account_lookup(account,
                                         CONFIG_ZRTP_NOT_SUPP_WARNING);
        g_debug("Warning Enabled %s", warning_enabled);
    } else {
        g_debug("Account is null callID %s", c->_callID);
        GHashTable * properties = sflphone_get_ip2ip_properties();

        if (properties)
            warning_enabled = g_hash_table_lookup(properties,
                                                  CONFIG_ZRTP_NOT_SUPP_WARNING);
    }

    if (utf8_case_equal(warning_enabled, "true")) {
        PidginMiniDialog *mini_dialog;
        gchar *desc = g_markup_printf_escaped(
                          _("ZRTP is not supported by peer %s\n"), c->_peer_number);
        mini_dialog = pidgin_mini_dialog_new(
                          _("Secure Communication Unavailable"), desc,
                          GTK_STOCK_DIALOG_WARNING);
        g_free(desc);
        pidgin_mini_dialog_add_button(mini_dialog, _("Continue"), NULL, NULL);
        pidgin_mini_dialog_add_button(mini_dialog, _("Stop Call"),
                                      (PidginMiniDialogCallback) sflphone_hang_up, client);

        add_error_dialog(GTK_WIDGET(mini_dialog));
    }
}

void
main_window_zrtp_negotiation_failed(const gchar* const callID, const gchar* const reason,
                                    const gchar* const severity, SFLPhoneClient *client)
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
    pidgin_mini_dialog_add_button(mini_dialog, _("Stop Call"), (PidginMiniDialogCallback) sflphone_hang_up,
                                  client);

    add_error_dialog(GTK_WIDGET(mini_dialog));
}

void
main_window_confirm_go_clear(callable_obj_t * c, SFLPhoneClient *client)
{
    gchar *desc = g_markup_printf_escaped(
                      _("%s wants to stop using secure communication. Confirm will resume conversation without SRTP.\n"),
                      c->_peer_number);
    PidginMiniDialog *mini_dialog = pidgin_mini_dialog_new(_("Confirm Go Clear"), desc,
                                    GTK_STOCK_STOP);
    g_free(desc);
    pidgin_mini_dialog_add_button(mini_dialog, _("Confirm"),
                                  (PidginMiniDialogCallback) dbus_set_confirm_go_clear, NULL);
    pidgin_mini_dialog_add_button(mini_dialog, _("Stop Call"), (PidginMiniDialogCallback) sflphone_hang_up,
                                  client);

    add_error_dialog(GTK_WIDGET(mini_dialog));
}

gboolean
main_window_update_playback_scale(const gchar *file_path, guint current, guint size)
{
    if (sfl_seekslider_has_path(SFL_SEEKSLIDER(seekslider), file_path)) {
        sfl_seekslider_update_scale(SFL_SEEKSLIDER(seekslider), current, size);
        return TRUE;
    }
    return FALSE;
}

void
main_window_set_playback_scale_sensitive()
{
    gtk_widget_set_sensitive(seekslider, TRUE);
}

void
main_window_set_playback_scale_unsensitive()
{
    gtk_widget_set_sensitive(seekslider, FALSE);
}

void
main_window_show_playback_scale()
{
    gtk_widget_show(seekslider);
}

void
main_window_hide_playback_scale()
{
    gtk_widget_hide(seekslider);
    main_window_reset_playback_scale();
}

void
main_window_reset_playback_scale()
{
    sfl_seekslider_reset((SFLSeekSlider *)seekslider);
}


void
main_window_pause_keygrabber(gboolean value)
{
    pause_grabber = value;
}

void
main_window_update_seekslider(const gchar *recordfile)
{
    g_object_set(G_OBJECT(seekslider), "file-path", recordfile, NULL);
}
