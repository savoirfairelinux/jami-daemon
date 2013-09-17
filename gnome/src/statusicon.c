/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
#include <gtk/gtk.h>
#include "actions.h"
#include "mainwindow.h"
#include "accountlist.h"
#include "statusicon.h"

static GtkStatusIcon *status;
static GtkWidget *show_menu_item, *hangup_menu_item;

void
set_minimized(gboolean state)
{
    if (show_menu_item)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), !state);
}

void
popup_main_window(SFLPhoneClient *client)
{
    gtk_widget_show(client->win);
    set_minimized(FALSE);
}

void
show_status_hangup_icon(SFLPhoneClient *client)
{
    if (g_settings_get_boolean(client->settings, "popup-main-window")) {
        gtk_widget_show(client->win);
        gtk_window_move(GTK_WINDOW(client->win),
                        g_settings_get_int(client->settings, "window-position-x"),
                        g_settings_get_int(client->settings, "window-position-y"));
        set_minimized(FALSE);
    }
}

void
hide_status_hangup_icon()
{
    if (status) {
        g_debug("Hide Hangup in Systray");
        gtk_widget_hide(GTK_WIDGET(hangup_menu_item));
    }
}

static void
status_quit(SFLPhoneClient *client)
{
    sflphone_quit(FALSE, client);
}

static void
status_hangup(G_GNUC_UNUSED GtkWidget *widget, SFLPhoneClient *client)
{
    sflphone_hang_up(client);
}

void
status_icon_unminimize()
{
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), TRUE);
}

void
show_hide(GtkWidget *widget, SFLPhoneClient *client)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
        gtk_widget_show(client->win);
        gtk_window_move(GTK_WINDOW(client->win),
                        g_settings_get_int(client->settings, "window-position-x"),
                        g_settings_get_int(client->settings, "window-position-y"));
        set_minimized(FALSE);
    } else {
        gtk_widget_hide(client->win);
        set_minimized(TRUE);
    }
}

void
status_click(G_GNUC_UNUSED GtkStatusIcon *status_icon, G_GNUC_UNUSED gpointer foo)
{
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item),
                                   !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(show_menu_item)));
}

static void menu(GtkStatusIcon *status_icon, guint button, guint activate_time, GtkWidget * menu_widget)
{
    gtk_menu_popup(GTK_MENU(menu_widget), NULL, NULL, gtk_status_icon_position_menu,
                   status_icon, button, activate_time);
}

static GtkWidget*
create_menu(SFLPhoneClient *client)
{
    GtkWidget * menu_widget;
    GtkWidget * menu_items;
    GtkWidget * image;

    menu_widget = gtk_menu_new();

    show_menu_item = gtk_check_menu_item_new_with_mnemonic(_("_Show main window"));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), TRUE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), show_menu_item);
    g_signal_connect(G_OBJECT(show_menu_item), "toggled",
                     G_CALLBACK(show_hide),
                     client);

    hangup_menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
    image = gtk_image_new_from_file(ICONS_DIR "/icon_hangup.svg");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(hangup_menu_item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), hangup_menu_item);
    g_signal_connect(G_OBJECT(hangup_menu_item), "activate",
                     G_CALLBACK(status_hangup),
                     client);

    menu_items = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), menu_items);

    menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT,
                 get_accel_group());
    g_signal_connect_swapped(G_OBJECT(menu_items), "activate",
                             G_CALLBACK(status_quit),
                             client);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), menu_items);

    gtk_widget_show_all(menu_widget);

    return menu_widget;
}

void
show_status_icon(SFLPhoneClient *client)
{
    status = gtk_status_icon_new_from_file(LOGO);
    g_signal_connect(G_OBJECT(status), "activate",
                     G_CALLBACK(status_click),
                     NULL);
    g_signal_connect(G_OBJECT(status), "popup-menu",
                     G_CALLBACK(menu),
                     create_menu(client));

    statusicon_set_tooltip();
}

void hide_status_icon(void)
{
    g_object_unref(status);
    status = NULL;
}


void
statusicon_set_tooltip()
{
    if (status) {
        // Add a tooltip to the system tray icon
        int count = account_list_get_registered_accounts();
        gchar *accounts = g_markup_printf_escaped(n_("%i active account", "%i active accounts", count), count);
        gchar *tip = g_markup_printf_escaped("%s - %s", _("SFLphone"), accounts);
        g_free(accounts);
        gtk_status_icon_set_tooltip_markup(status, tip);
        g_free(tip);
    }
}

void
status_tray_icon_blink(gboolean active)
{
    if (status)
        gtk_status_icon_set_from_file(status, active ? LOGO_NOTIF : LOGO);
}

void
status_tray_icon_online(gboolean online)
{
    if (status)
        gtk_status_icon_set_from_file(status, online ? LOGO : LOGO_OFFLINE);
}

GtkStatusIcon*
get_status_icon(void)
{
    return status;
}
