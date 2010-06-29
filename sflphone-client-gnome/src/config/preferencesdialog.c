/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include <gtk/gtk.h>

#include <accountlist.h>
#include <accountconfigdialog.h>
#include <actions.h>
#include <config.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <audioconf.h>
#include <addressbook-config.h>
#include <shortcuts-config.h>
#include <hooks-config.h>
#include <utils.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Local variables
 */
gboolean accDialogOpen = FALSE;
gboolean dialogOpen = FALSE;
gboolean ringtoneEnabled = TRUE;

GtkWidget * localPortSpinBox;
GtkWidget * localAddressCombo;

GtkWidget * history_value;

GtkWidget * status;

GtkWidget *showstatusicon;
GtkWidget *starthidden;
GtkWidget *popupwindow;
GtkWidget *neverpopupwindow;

static int history_limit;
static gboolean history_enabled = TRUE;

static void
set_md5_hash_cb (GtkWidget *widget UNUSED, gpointer data UNUSED)
{

  gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  dbus_set_md5_credential_hashing (enabled);
}

static void
start_hidden (void)
{
  dbus_start_hidden ();
}

static void
set_popup_mode (GtkWidget *widget, gpointer *userdata)
{
  if (dbus_popup_mode () || gtk_toggle_button_get_active (
      GTK_TOGGLE_BUTTON (widget)))
    dbus_switch_popup_mode ();
}

void
set_notif_level ()
{
  dbus_set_notify ();
}

static void
history_limit_cb (GtkSpinButton *button, void *ptr)
{
  history_limit = gtk_spin_button_get_value_as_int ((GtkSpinButton *) (ptr));
}

static void
history_enabled_cb (GtkWidget *widget)
{
  history_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  gtk_widget_set_sensitive (GTK_WIDGET (history_value), history_enabled);

  // Toggle it through D-Bus
  dbus_set_history_enabled ();
}

void
clean_history (void)
{
  calllist_clean_history ();
}

void showstatusicon_cb (GtkWidget *widget, gpointer data) {

  gboolean currentstatus = FALSE;

  // data contains the previous value of dbus_is_status_icon_enabled () - ie before the click.
  currentstatus = (gboolean) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  // Update the widget states
  gtk_widget_set_sensitive (GTK_WIDGET (popupwindow), currentstatus);
  gtk_widget_set_sensitive (GTK_WIDGET (neverpopupwindow), currentstatus);
  gtk_widget_set_sensitive (GTK_WIDGET (starthidden), currentstatus);

  currentstatus ?       show_status_icon () : hide_status_icon ();

  // Update through D-Bus
  dbus_enable_status_icon (currentstatus ? "true" : "false");
}


GtkWidget*
create_general_settings ()
{

  GtkWidget *ret, *notifAll, *trayItem, *frame, *checkBoxWidget, *label, *table;
  gboolean statusicon = FALSE;

  // Load history configuration
  history_load_configuration ();

  // Main widget
  ret = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER(ret), 10);

  // Notifications Frame
  gnome_main_section_new_with_table (_("Desktop Notifications"), &frame,
      &table, 2, 1);
  gtk_box_pack_start (GTK_BOX(ret), frame, FALSE, FALSE, 0);

  // Notification All
  notifAll = gtk_check_button_new_with_mnemonic (_("_Enable notifications"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(notifAll), dbus_get_notify ());
  g_signal_connect(G_OBJECT( notifAll ) , "clicked" , G_CALLBACK( set_notif_level ) , NULL );
  gtk_table_attach (GTK_TABLE(table), notifAll, 0, 1, 0, 1, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  // System Tray option frame
  gnome_main_section_new_with_table (_("System Tray Icon"), &frame, &table, 4,
      1);
  gtk_box_pack_start (GTK_BOX(ret), frame, FALSE, FALSE, 0);

  if (g_strcasecmp (dbus_is_status_icon_enabled (), "true") == 0)
      statusicon = TRUE;
  else
    statusicon = FALSE;

  showstatusicon = gtk_check_button_new_with_mnemonic (
      _("Show SFLphone in the system tray"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(showstatusicon), statusicon);
  g_signal_connect (G_OBJECT (showstatusicon) , "clicked" , G_CALLBACK (showstatusicon_cb), NULL);
  gtk_table_attach (GTK_TABLE (table), showstatusicon, 0, 1, 0, 1, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  popupwindow = gtk_radio_button_new_with_mnemonic (NULL,
      _("_Popup main window on incoming call"));
  g_signal_connect(G_OBJECT (popupwindow), "toggled", G_CALLBACK (set_popup_mode), NULL);
  gtk_table_attach (GTK_TABLE(table), popupwindow, 0, 1, 1, 2, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  neverpopupwindow = gtk_radio_button_new_with_mnemonic_from_widget (
      GTK_RADIO_BUTTON (popupwindow), _("Ne_ver popup main window"));
  gtk_table_attach (GTK_TABLE(table), neverpopupwindow, 0, 1, 2, 3, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  // Toggle according to the user configuration
  dbus_popup_mode () ? gtk_toggle_button_set_active (
      GTK_TOGGLE_BUTTON (popupwindow), TRUE) : gtk_toggle_button_set_active (
      GTK_TOGGLE_BUTTON (neverpopupwindow), TRUE);

  starthidden = gtk_check_button_new_with_mnemonic (
      _("Hide SFLphone window on _startup"));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(starthidden),
      dbus_is_start_hidden ());
  g_signal_connect(G_OBJECT (starthidden) , "clicked" , G_CALLBACK( start_hidden ) , NULL);
  gtk_table_attach (GTK_TABLE(table), starthidden, 0, 1, 3, 4, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  // Update the widget states
  gtk_widget_set_sensitive (GTK_WIDGET (popupwindow),statusicon);
  gtk_widget_set_sensitive (GTK_WIDGET (neverpopupwindow),statusicon);
  gtk_widget_set_sensitive (GTK_WIDGET (starthidden),statusicon);

  // HISTORY CONFIGURATION
  gnome_main_section_new_with_table (_("Calls History"), &frame, &table, 3, 1);
  gtk_box_pack_start (GTK_BOX(ret), frame, FALSE, FALSE, 0);

  checkBoxWidget = gtk_check_button_new_with_mnemonic (
      _("_Keep my history for at least"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkBoxWidget),
      history_enabled);
  g_signal_connect (G_OBJECT (checkBoxWidget) , "clicked" , G_CALLBACK (history_enabled_cb) , NULL);
  gtk_table_attach (GTK_TABLE(table), checkBoxWidget, 0, 1, 0, 1, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  history_value = gtk_spin_button_new_with_range (1, 99, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(history_value), history_limit);
  g_signal_connect( G_OBJECT (history_value) , "value-changed" , G_CALLBACK (history_limit_cb) , history_value);
  gtk_widget_set_sensitive (GTK_WIDGET (history_value),
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkBoxWidget)));
  gtk_table_attach (GTK_TABLE(table), history_value, 1, 2, 0, 1, GTK_EXPAND
      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

  label = gtk_label_new (_("days"));
  gtk_table_attach (GTK_TABLE(table), label, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL,
      GTK_EXPAND | GTK_FILL, 0, 5);

  gtk_widget_show_all (ret);

  return ret;
}

void
save_configuration_parameters (void)
{

  // Address book config
  addressbook_config_save_parameters ();
  hooks_save_parameters ();

  // History config
  dbus_set_history_limit (history_limit);

  // Direct IP calls config
  // dbus_set_ip2ip_details (directIpCallsProperties);
}

void
history_load_configuration ()
{
  history_limit = dbus_get_history_limit ();
  history_enabled = TRUE;
  if (g_strcasecmp (dbus_get_history_enabled (), "false") == 0)
    history_enabled = FALSE;
}

/**
 * Show configuration window with tabs
 */
void
show_preferences_dialog ()
{
  GtkDialog * dialog;
  GtkWidget * notebook;
  GtkWidget * tab;
  guint result;

  dialogOpen = TRUE;

  dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Preferences"),
          GTK_WINDOW(get_main_window()),
          GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_STOCK_CLOSE,
          GTK_RESPONSE_ACCEPT,
          NULL));

  // Set window properties
  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_window_set_default_size (GTK_WINDOW(dialog), 600, 400);
  gtk_container_set_border_width (GTK_CONTAINER(dialog), 0);

  // Create tabs container
  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(notebook), 10);
  gtk_widget_show (notebook);

  // General settings tab
  tab = create_general_settings ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), tab, gtk_label_new (
      _("General")));
  gtk_notebook_page_num (GTK_NOTEBOOK(notebook), tab);

  // Audio tab
  tab = create_audio_configuration ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), tab, gtk_label_new (
      _("Audio")));
  gtk_notebook_page_num (GTK_NOTEBOOK(notebook), tab);

  // Addressbook tab
  tab = create_addressbook_settings ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), tab, gtk_label_new (
      _("Address Book")));
  gtk_notebook_page_num (GTK_NOTEBOOK(notebook), tab);

  // Hooks tab
  tab = create_hooks_settings ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), tab, gtk_label_new (
      _("Hooks")));
  gtk_notebook_page_num (GTK_NOTEBOOK(notebook), tab);

  // Shortcuts tab
  tab = create_shortcuts_settings();
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Shortcuts")));
  gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);

  result = gtk_dialog_run (dialog);

  save_configuration_parameters ();
  update_actions ();

  dialogOpen = FALSE;

  gtk_widget_destroy (GTK_WIDGET(dialog));
}

