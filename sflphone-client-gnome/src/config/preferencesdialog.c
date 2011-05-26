/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "eel-gconf-extensions.h"

#include "accountconfigdialog.h"
#include "addressbook-config.h"
#include "shortcuts-config.h"
#include "audioconf.h"
#include "videoconf.h"
#include "hooks-config.h"
#include "audioconf.h"
#include "videoconf.h"
#include "uimanager.h"

/**
 * Local variables
 */
gboolean accDialogOpen = FALSE;
gboolean dialogOpen = FALSE;
gboolean ringtoneEnabled = TRUE;


GtkWidget * status;
GtkWidget * history_value;

GtkWidget *starthidden;
GtkWidget *popupwindow;
GtkWidget *neverpopupwindow;

GtkWidget *treeView;
GtkWidget *iconview;
GtkCellRenderer *renderer;
GtkTreeViewColumn *column;
GtkTreeSelection *selection;
GtkWidget * notebook;


enum {
    PIXBUF_COL,
    TEXT_COL,
    PAGE_NUMBER
};

typedef struct {
    gchar* icon_descr;
    gchar* icon_name;
    gint page_number;
} browser_t;

// history preference parameters
static int history_limit;
static gboolean history_enabled = TRUE;

// instant messaging preference parameters
static gboolean instant_messaging_enabled = TRUE;

static void
start_hidden (void)
{
    gboolean currentstate = eel_gconf_get_integer (START_HIDDEN);
    eel_gconf_set_integer (START_HIDDEN, !currentstate);
}

static void
set_popup_mode (GtkWidget *widget, gpointer *userdata UNUSED)
{
    gboolean currentstate = eel_gconf_get_integer (POPUP_ON_CALL);

    if (currentstate || gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        eel_gconf_set_integer (POPUP_ON_CALL, !currentstate);
    }
}

void
set_notif_level ()
{
    gboolean current_state = eel_gconf_get_integer (NOTIFY_ALL);
    eel_gconf_set_integer (NOTIFY_ALL, !current_state);
}

static void
history_limit_cb (GtkSpinButton *button UNUSED, void *ptr)
{
    history_limit = gtk_spin_button_get_value_as_int ( (GtkSpinButton *) (ptr));
}

static void
history_enabled_cb (GtkWidget *widget)
{
    history_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    gtk_widget_set_sensitive (GTK_WIDGET (history_value), history_enabled);

    // Toggle it through D-Bus
    eel_gconf_set_integer (HISTORY_ENABLED, !eel_gconf_get_integer (HISTORY_ENABLED));
}

static void
instant_messaging_enabled_cb (GtkWidget *widget)
{
    instant_messaging_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    eel_gconf_set_integer (INSTANT_MESSAGING_ENABLED, !eel_gconf_get_integer (INSTANT_MESSAGING_ENABLED));
}

void
clean_history (void)
{
    calllist_clean_history ();
}

void showstatusicon_cb (GtkWidget *widget, gpointer data UNUSED)
{

    gboolean currentstatus = FALSE;

    // data contains the previous value of dbus_is_status_icon_enabled () - ie before the click.
    currentstatus = (gboolean) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    // Update the widget states
    gtk_widget_set_sensitive (GTK_WIDGET (popupwindow), currentstatus);
    gtk_widget_set_sensitive (GTK_WIDGET (neverpopupwindow), currentstatus);
    gtk_widget_set_sensitive (GTK_WIDGET (starthidden), currentstatus);

    currentstatus ?       show_status_icon () : hide_status_icon ();

    // Update through D-Bus
    eel_gconf_set_integer (SHOW_STATUSICON, currentstatus);
}


GtkWidget*
create_general_settings ()
{

    GtkWidget *ret, *notifAll, *frame, *checkBoxWidget, *label, *table, *showstatusicon;
    gboolean statusicon;

    // Load history configuration
    history_load_configuration ();

    // Load instant messaging configuration
    instant_messaging_load_configuration();

    // Main widget
    ret = gtk_vbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (ret), 10);

    // Notifications Frame
    gnome_main_section_new_with_table (_ ("Desktop Notifications"), &frame,
                                       &table, 2, 1);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    // Notification All
    notifAll = gtk_check_button_new_with_mnemonic (_ ("_Enable notifications"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (notifAll), eel_gconf_get_integer (NOTIFY_ALL));
    g_signal_connect (G_OBJECT (notifAll) , "clicked" , G_CALLBACK (set_notif_level) , NULL);
    gtk_table_attach (GTK_TABLE (table), notifAll, 0, 1, 0, 1, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // System Tray option frame
    gnome_main_section_new_with_table (_ ("System Tray Icon"), &frame, &table, 4,
                                       1);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    // Whether or not displaying an icon in the system tray
    statusicon = eel_gconf_get_integer (SHOW_STATUSICON);

    showstatusicon = gtk_check_button_new_with_mnemonic (
                         _ ("Show SFLphone in the system tray"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showstatusicon), statusicon);
    g_signal_connect (G_OBJECT (showstatusicon) , "clicked" , G_CALLBACK (showstatusicon_cb), NULL);
    gtk_table_attach (GTK_TABLE (table), showstatusicon, 0, 1, 0, 1, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    popupwindow = gtk_radio_button_new_with_mnemonic (NULL,
                  _ ("_Popup main window on incoming call"));
    g_signal_connect (G_OBJECT (popupwindow), "toggled", G_CALLBACK (set_popup_mode), NULL);
    gtk_table_attach (GTK_TABLE (table), popupwindow, 0, 1, 1, 2, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    neverpopupwindow = gtk_radio_button_new_with_mnemonic_from_widget (
                           GTK_RADIO_BUTTON (popupwindow), _ ("Ne_ver popup main window"));
    gtk_table_attach (GTK_TABLE (table), neverpopupwindow, 0, 1, 2, 3, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // Toggle according to the user configuration
    eel_gconf_get_integer (POPUP_ON_CALL) ? gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (popupwindow),
        TRUE) :
    gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (neverpopupwindow),
        TRUE);

    starthidden = gtk_check_button_new_with_mnemonic (
                      _ ("Hide SFLphone window on _startup"));

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (starthidden),
                                  eel_gconf_get_integer (START_HIDDEN));
    g_signal_connect (G_OBJECT (starthidden) , "clicked" , G_CALLBACK (start_hidden) , NULL);
    gtk_table_attach (GTK_TABLE (table), starthidden, 0, 1, 3, 4, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // Update the widget states
    gtk_widget_set_sensitive (GTK_WIDGET (popupwindow),statusicon);
    gtk_widget_set_sensitive (GTK_WIDGET (neverpopupwindow),statusicon);
    gtk_widget_set_sensitive (GTK_WIDGET (starthidden),statusicon);

    // HISTORY CONFIGURATION
    gnome_main_section_new_with_table (_ ("Calls History"), &frame, &table, 3, 1);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    checkBoxWidget = gtk_check_button_new_with_mnemonic (
                         _ ("_Keep my history for at least"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkBoxWidget),
                                  history_enabled);
    g_signal_connect (G_OBJECT (checkBoxWidget) , "clicked" , G_CALLBACK (history_enabled_cb) , NULL);
    gtk_table_attach (GTK_TABLE (table), checkBoxWidget, 0, 1, 0, 1, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    history_value = gtk_spin_button_new_with_range (1, 99, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (history_value), history_limit);
    g_signal_connect (G_OBJECT (history_value) , "value-changed" , G_CALLBACK (history_limit_cb) , history_value);
    gtk_widget_set_sensitive (GTK_WIDGET (history_value),
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkBoxWidget)));
    gtk_table_attach (GTK_TABLE (table), history_value, 1, 2, 0, 1, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    label = gtk_label_new (_ ("days"));
    gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL,
                      GTK_EXPAND | GTK_FILL, 0, 5);

    // INSTANT MESSAGING CONFIGURATION
    gnome_main_section_new_with_table (_ ("Instant Messaging"), &frame, &table, 1, 1);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    checkBoxWidget = gtk_check_button_new_with_mnemonic (
                         _ ("Enable instant messaging"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkBoxWidget),
                                  instant_messaging_enabled);
    g_signal_connect (G_OBJECT (checkBoxWidget) , "clicked" , G_CALLBACK (instant_messaging_enabled_cb) , NULL);
    gtk_table_attach (GTK_TABLE (table), checkBoxWidget, 0, 1, 0, 1, GTK_EXPAND
                      | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

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
    history_enabled = eel_gconf_get_integer (HISTORY_ENABLED);

}

void
instant_messaging_load_configuration ()
{
    instant_messaging_enabled = eel_gconf_get_integer (INSTANT_MESSAGING_ENABLED);

}


void
selection_changed_cb (GtkIconView *view, gpointer user_data UNUSED)
{

    GtkTreeModel *model;
    GtkTreeIter iter;
    GList *list;
    gint page;

    model = gtk_icon_view_get_model (view);
    list = gtk_icon_view_get_selected_items (view);

    if (list == NULL)
        return;

    if (g_list_length (list) > 1)
        return;

    gtk_tree_model_get_iter (model, &iter, list->data);
    gtk_tree_model_get (model, &iter, PAGE_NUMBER, &page, -1);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
    g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (list);
}



/**
 * Show configuration window with tabs
 */
void
show_preferences_dialog ()
{
    GtkDialog * dialog;
    GtkWidget * hbox;
    GtkWidget * tab;
    guint result;

    dialogOpen = TRUE;

    dialog = GTK_DIALOG (gtk_dialog_new_with_buttons (_ ("Preferences"),
                         GTK_WINDOW (get_main_window()),
                         GTK_DIALOG_DESTROY_WITH_PARENT,
                         GTK_STOCK_CLOSE,
                         GTK_RESPONSE_ACCEPT,
                         NULL));

    // Set window properties
    gtk_dialog_set_has_separator (dialog, FALSE);
    gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 400);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 0);

    hbox = gtk_hbox_new (FALSE, 10);

    // Create tree view
    iconview = gtk_icon_view_new_with_model (createModel ());
    g_object_set (iconview,
                  "selection-mode", GTK_SELECTION_BROWSE,
                  "text-column", TEXT_COL,
                  "pixbuf-column", PIXBUF_COL,
                  "columns", 1,
                  "margin", 10,
                  NULL);
    // Connect the callback when clicking on an item
    g_signal_connect (G_OBJECT (iconview), "selection-changed", G_CALLBACK (selection_changed_cb), NULL);
    gtk_box_pack_start (GTK_BOX (hbox), iconview, TRUE, TRUE, 0);

    // Create tabs container
    notebook = gtk_notebook_new ();
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
    gtk_box_pack_end (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (dialog->vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show_all (dialog->vbox);
    gtk_container_set_border_width (GTK_CONTAINER (notebook), 10);
    gtk_widget_show (notebook);

    // General settings tab
    tab = create_general_settings ();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tab, gtk_label_new (
                                  _ ("General")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab);

    // Audio tab
    tab = create_audio_configuration ();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tab, gtk_label_new (
                                  _ ("Audio")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab);

    // Video tab
    tab = create_video_configuration ();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tab, gtk_label_new (
                                  _ ("Video")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab);

    // Addressbook tab
    tab = create_addressbook_settings ();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tab, gtk_label_new (
                                  _ ("Address Book")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab);

    // Hooks tab
    tab = create_hooks_settings ();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tab, gtk_label_new (
                                  _ ("Hooks")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab);

    // Shortcuts tab
    tab = create_shortcuts_settings();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tab, gtk_label_new (_ ("Shortcuts")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab);

    // By default, general settings
    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
    // Highlight the corresponding icon
    gtk_icon_view_select_path (GTK_ICON_VIEW (iconview), gtk_tree_path_new_first ());

    result = gtk_dialog_run (dialog);

    save_configuration_parameters ();
    update_actions ();

    dialogOpen = FALSE;

    gtk_widget_destroy (GTK_WIDGET (dialog));
}


GtkTreeModel* createModel()
{
    enum {ENTRIES = 6} ;
    browser_t browser_entries[ENTRIES] = {
        {_ ("General"), "preferences-system", 0},
        {_ ("Audio"), "multimedia-volume-control", 1},
        {_ ("Video"), "multimedia-volume-control", 2},
        {_ ("Address Book"), "address-book-new", 3},
        {_ ("Hooks"), "gnome-globe", 4},
        {_ ("Shortcuts"), "preferences-desktop-keyboard", 5}
    };

    GdkPixbuf *pixbuf;
    GtkTreeIter iter;
    GtkListStore *store;
    GError *error = NULL;
    gint i;

    store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);
    GtkIconTheme* theme = gtk_icon_theme_get_default();

    for (i = 0; i < ENTRIES; i++) {

        gtk_list_store_append (store, &iter);

        pixbuf = gtk_icon_theme_load_icon (theme, browser_entries[i].icon_name, 48, 0, &error);

        gtk_list_store_set (store, &iter,
                            PIXBUF_COL, pixbuf,
                            TEXT_COL, browser_entries[i].icon_descr,
                            PAGE_NUMBER, browser_entries[i].page_number,
                            -1);

        if (pixbuf != NULL) {
            gdk_pixbuf_unref (pixbuf);
        } else {
            DEBUG ("Couldn't load icon: %s", error->message);
            g_clear_error (&error);
        }
    }

    return GTK_TREE_MODEL (store);
}
