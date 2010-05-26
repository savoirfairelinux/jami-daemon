/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <gtk/gtk.h>
#include <actions.h>
#include <mainwindow.h>
#include <accountlist.h>
#include <statusicon.h>

#if GTK_CHECK_VERSION(2,10,0)
GtkStatusIcon *status;
GtkWidget *show_menu_item, *hangup_menu_item;
gboolean __minimized = MINIMIZED;

void
popup_main_window(void)
{
  if (__POPUP_WINDOW)
    {
      gtk_widget_show(get_main_window());
      gtk_window_move(GTK_WINDOW (get_main_window ()),
          dbus_get_window_position_x(), dbus_get_window_position_y());
      set_minimized(FALSE);
    }
}

void
show_status_hangup_icon()
{
  if (status) {
    DEBUG("Show Hangup in Systray");
    gtk_widget_show(GTK_WIDGET(hangup_menu_item));
  }
}

void
hide_status_hangup_icon()
{
  if (status) {
    DEBUG("Hide Hangup in Systray");
    gtk_widget_hide(GTK_WIDGET(hangup_menu_item));
  }
}

void
status_quit(void * foo UNUSED)
{
  sflphone_quit();
}

void
status_hangup()
{
  sflphone_hang_up();
}

void
status_icon_unminimize()
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), TRUE);
}

gboolean
main_widget_minimized()
{
  return __minimized;
}

void
show_hide(void)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(show_menu_item)))
    {
      gtk_widget_show(GTK_WIDGET(get_main_window()));
      gtk_window_move(GTK_WINDOW (get_main_window ()),
          dbus_get_window_position_x(), dbus_get_window_position_y());
      set_minimized(!MINIMIZED);
    }
  else
    {
      gtk_widget_hide(GTK_WIDGET(get_main_window()));
      set_minimized(MINIMIZED);
    }
}

void
status_click(GtkStatusIcon *status_icon UNUSED, void * foo UNUSED)
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item),
      !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(show_menu_item)));
}

void
menu(GtkStatusIcon *status_icon, guint button, guint activate_time,
    GtkWidget * menu)
{
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, gtk_status_icon_position_menu,
      status_icon, button, activate_time);
}

GtkWidget*
create_menu()
{
  GtkWidget * menu;
  GtkWidget * menu_items;
  GtkWidget * image;

  menu = gtk_menu_new();

  show_menu_item
      = gtk_check_menu_item_new_with_mnemonic(_("_Show main window"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), TRUE);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), show_menu_item);
  g_signal_connect(G_OBJECT (show_menu_item), "toggled",
      G_CALLBACK (show_hide),
      NULL);

  hangup_menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
  image = gtk_image_new_from_file(ICONS_DIR "/icon_hangup.svg");
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(hangup_menu_item), image);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), hangup_menu_item);
  g_signal_connect(G_OBJECT (hangup_menu_item), "activate",
      G_CALLBACK (status_hangup),
      NULL);

  menu_items = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), menu_items);

  menu_items = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT,
      get_accel_group());
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (status_quit),
      NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL (menu), menu_items);

  gtk_widget_show_all(menu);

  return menu;
}

void
show_status_icon()
{
  status = gtk_status_icon_new_from_file(LOGO);
  g_signal_connect (G_OBJECT (status), "activate",
      G_CALLBACK (status_click),
      NULL);
  g_signal_connect (G_OBJECT (status), "popup-menu",
      G_CALLBACK (menu),
      create_menu());

  statusicon_set_tooltip();
}

void hide_status_icon (void) {

    g_object_unref (status);
    status = NULL;
}


void
statusicon_set_tooltip()
{
  int count;
  gchar *tip;

  if(status) {

    // Add a tooltip to the system tray icon
    count = account_list_get_registered_accounts();
    tip = g_markup_printf_escaped("%s - %s", _("SFLphone"),
        g_markup_printf_escaped(n_("%i active account", "%i active accounts", count), count));
    gtk_status_icon_set_tooltip(status, tip);
    g_free(tip);

  }

}

void
status_tray_icon_blink(gboolean active)
{
  if (status) {
  // Set a different icon to notify of an event
  active ? gtk_status_icon_set_from_file(status, LOGO_NOTIF)
      : gtk_status_icon_set_from_file(status, LOGO);
  }
}

void
status_tray_icon_online(gboolean online)
{
  if (status) {
  // Set a different icon to notify of an event
  online ? gtk_status_icon_set_from_file(status, LOGO)
      : gtk_status_icon_set_from_file(status, LOGO_OFFLINE);
  }
}

GtkStatusIcon*
get_status_icon(void)
{
  return status;
}

void
set_minimized(gboolean state)
{
  __minimized = state;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), !state);
}

#endif
