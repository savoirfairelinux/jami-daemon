/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "icons/icon_factory.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbus.h"
#include "statusicon.h"
#include "addrbookfactory.h"
#include "preferencesdialog.h"
#include "addressbook-config.h"
#include "shortcuts-config.h"
#include "hooks-config.h"
#include "audioconf.h"
#ifdef SFL_VIDEO
#include "videoconf.h"
#endif
#include "uimanager.h"
#include "mainwindow.h"

/**
 * Local variables
 */
static GtkWidget * history_value;

static GtkWidget *popupwindow;
static GtkWidget *neverpopupwindow;

static GtkWidget *iconview;
static GtkWidget * notebook;

enum {
    PIXBUF_COL,
    TEXT_COL,
    PAGE_NUMBER
};

// history preference parameters
static int history_limit;
static gboolean history_enabled = TRUE;

// instant messaging preference parameters
static gboolean instant_messaging_enabled = TRUE;

static void
set_popup_mode(GtkToggleButton *widget, SFLPhoneClient *client)
{
    const gboolean currentstate = g_settings_get_boolean(client->settings, "popup-main-window");

    if (currentstate || gtk_toggle_button_get_active(widget))
        g_settings_set_boolean(client->settings, "popup-main-window", !currentstate);
}

void
set_notif_level(G_GNUC_UNUSED GtkWidget *widget, SFLPhoneClient *client)
{
    const gboolean current_state = g_settings_get_boolean(client->settings, "notify-all");
    g_settings_set_boolean(client->settings, "notify-all", !current_state);
}

static void
history_limit_cb(GtkSpinButton *button)
{
    history_limit = gtk_spin_button_get_value_as_int(button);
}

static void
history_enabled_cb(GtkWidget *widget, SFLPhoneClient *client)
{
    history_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    gtk_widget_set_sensitive(GTK_WIDGET(history_value), history_enabled);

    // Toggle it through D-Bus
    g_settings_set_boolean(client->settings, "history-enabled", history_enabled);
}

static void
instant_messaging_enabled_cb(GtkToggleButton *widget, SFLPhoneClient *client)
{
    instant_messaging_enabled = gtk_toggle_button_get_active(widget);
    g_settings_set_boolean(client->settings, "instant-messaging-enabled", instant_messaging_enabled);
}

void showstatusicon_cb(GtkToggleButton *widget, SFLPhoneClient *client)
{
    // data contains the previous value of dbus_is_status_icon_enabled () - ie before the click.
    gboolean currentstatus = gtk_toggle_button_get_active(widget);

    // Update the widget states
    gtk_widget_set_sensitive(GTK_WIDGET(popupwindow), currentstatus);
    gtk_widget_set_sensitive(GTK_WIDGET(neverpopupwindow), currentstatus);

    currentstatus ? show_status_icon(client) : hide_status_icon();

    // Update through D-Bus
    g_settings_set_boolean(client->settings, "show-status-icon", currentstatus);
}

static void
history_load_configuration(SFLPhoneClient *client)
{
    history_limit = dbus_get_history_limit();
    history_enabled = g_settings_get_boolean(client->settings, "history-enabled");
}

static void
instant_messaging_load_configuration(SFLPhoneClient *client)
{
    instant_messaging_enabled = g_settings_get_boolean(client->settings, "instant-messaging-enabled");
}

static void
win_to_front_cb(GtkToggleButton *widget, gpointer data)
{
    SFLPhoneClient *client = (SFLPhoneClient *) data;
    const gboolean window_to_front = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_settings_set_boolean(client->settings, "bring-window-to-front", window_to_front);
}

GtkWidget*
create_general_settings(SFLPhoneClient *client)
{
    GtkWidget *ret, *notifAll, *frame, *checkBoxWidget, *label, *showstatusicon;
    gboolean statusicon;

    // Load history configuration
    history_load_configuration(client);

    // Load instant messaging configuration
    instant_messaging_load_configuration(client);

    // Main widget
    ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    // Notifications Frame
    GtkWidget *grid;
    gnome_main_section_new_with_grid(_("Desktop Notifications"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    // Notification All
    notifAll = gtk_check_button_new_with_mnemonic(_("_Enable notifications"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(notifAll), g_settings_get_boolean(client->settings, "notify-all"));
    g_signal_connect(G_OBJECT(notifAll), "clicked", G_CALLBACK(set_notif_level), client);
    gtk_grid_attach(GTK_GRID(grid), notifAll, 0, 0, 1, 1);

    // Window Behaviour frame
    gnome_main_section_new_with_grid(_("Window Behaviour"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    // Whether or not to bring window to foreground on incoming call
    const gboolean win_to_front = g_settings_get_boolean(client->settings, "bring-window-to-front");

    GtkWidget *win_to_front_button =
        gtk_check_button_new_with_mnemonic(_("Bring SFLphone to foreground on incoming calls"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win_to_front_button), win_to_front);
    g_signal_connect(G_OBJECT(win_to_front_button), "toggled",
            G_CALLBACK(win_to_front_cb), client);
    gtk_grid_attach(GTK_GRID(grid), win_to_front_button, 0, 0, 1, 1);

    // System Tray option frame
    gnome_main_section_new_with_grid(_("System Tray Icon (Legacy)"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    // Whether or not displaying an icon in the system tray
    statusicon = g_settings_get_boolean(client->settings, "show-status-icon");

    showstatusicon = gtk_check_button_new_with_mnemonic(
                         _("Show SFLphone in the system tray"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showstatusicon), statusicon);
    g_signal_connect(G_OBJECT(showstatusicon), "clicked", G_CALLBACK(showstatusicon_cb), client);
    gtk_grid_attach(GTK_GRID(grid), showstatusicon, 0, 0, 1, 1);

    popupwindow = gtk_radio_button_new_with_mnemonic(NULL,
                  _("_Popup main window on incoming call"));
    g_signal_connect(G_OBJECT(popupwindow), "toggled", G_CALLBACK(set_popup_mode), client);
    gtk_grid_attach(GTK_GRID(grid), popupwindow, 0, 1, 1, 1);

    neverpopupwindow = gtk_radio_button_new_with_mnemonic_from_widget(
                           GTK_RADIO_BUTTON(popupwindow), _("Ne_ver popup main window"));
    gtk_grid_attach(GTK_GRID(grid), neverpopupwindow, 0, 2, 1, 1);

    // Toggle according to the user configuration
    g_settings_get_boolean(client->settings, "popup-main-window") ? gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(popupwindow),
        TRUE) :
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(neverpopupwindow),
        TRUE);

    // Update the widget states
    gtk_widget_set_sensitive(GTK_WIDGET(popupwindow),statusicon);
    gtk_widget_set_sensitive(GTK_WIDGET(neverpopupwindow),statusicon);

    // HISTORY CONFIGURATION
    gnome_main_section_new_with_grid(_("Calls History"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    checkBoxWidget = gtk_check_button_new_with_mnemonic(
                         _("_Keep my history for at least"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkBoxWidget),
                                 history_enabled);
    g_signal_connect(G_OBJECT(checkBoxWidget), "clicked", G_CALLBACK(history_enabled_cb), client);
    gtk_grid_attach(GTK_GRID(grid), checkBoxWidget, 0, 0, 1, 1);

    history_value = gtk_spin_button_new_with_range(1, 99, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(history_value), history_limit);
    g_signal_connect(G_OBJECT(history_value), "value-changed", G_CALLBACK(history_limit_cb), NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(history_value),
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkBoxWidget)));
    gtk_grid_attach(GTK_GRID(grid), history_value, 1, 0, 1, 1);

    label = gtk_label_new(_("days"));
    gtk_grid_attach(GTK_GRID(grid), label, 2, 0, 1, 1);

    // INSTANT MESSAGING CONFIGURATION
    gnome_main_section_new_with_grid(_("Instant Messaging"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    checkBoxWidget = gtk_check_button_new_with_mnemonic(
                         _("Enable instant messaging"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkBoxWidget),
                                 instant_messaging_enabled);
    g_signal_connect(G_OBJECT(checkBoxWidget), "clicked", G_CALLBACK(instant_messaging_enabled_cb), client);
    gtk_grid_attach(GTK_GRID(grid), checkBoxWidget, 0, 0, 1, 1);

    gtk_widget_show_all(ret);

    return ret;
}

void
save_configuration_parameters(SFLPhoneClient *client)
{
    if (addrbook)
        addressbook_config_save_parameters(client->settings);

    hooks_save_parameters(client);

    dbus_set_history_limit(history_limit);
}

void
selection_changed_cb(GtkIconView *view, G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GList *list;
    gint page;

    model = gtk_icon_view_get_model(view);
    list = gtk_icon_view_get_selected_items(view);

    if (list == NULL)
        return;

    if (g_list_length(list) > 1)
        return;

    gtk_tree_model_get_iter(model, &iter, list->data);
    gtk_tree_model_get(model, &iter, PAGE_NUMBER, &page, -1);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page);
    g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(list);
}

/*
 * Get an 48x48 icon from the default theme or fallback to an application icon.
 */
static GdkPixbuf *get_icon(const gchar *name, GtkWidget *widget)
{
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(theme, name, 48, 0, NULL);
    if (!pixbuf)
        pixbuf = gtk_widget_render_icon_pixbuf(widget, name, GTK_ICON_SIZE_DIALOG);

    return pixbuf;
}

static GtkTreeModel* create_model(GtkWidget *widget)
{
    static const struct {
        gchar* icon_descr;
        gchar* icon_name;
        gint page_number;
    } browser_entries_full[] = {
        {"General", GTK_STOCK_PREFS, 0},
        {"Audio", GTK_STOCK_PREFS_AUDIO, 1},
#ifdef SFL_VIDEO
        {"Video", GTK_STOCK_PREFS_VIDEO, 2},
        {"Hooks", GTK_STOCK_PREFS_HOOK, 3},
        {"Shortcuts", GTK_STOCK_PREFS_SHORTCUT, 4},
        {"Address Book", GTK_STOCK_PREFS_ADDRESSBOOK, 5},
#else
        {"Hooks", GTK_STOCK_PREFS_HOOK, 2},
        {"Shortcuts", GTK_STOCK_PREFS_SHORTCUT, 3},
        {"Address Book", GTK_STOCK_PREFS_ADDRESSBOOK, 4},
#endif
    };
    GdkPixbuf *pixbuf;
    GtkTreeIter iter;

    GtkListStore *store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);
    gint nb_entries = sizeof(browser_entries_full) / sizeof(browser_entries_full[0]);
    /* Skip address book entry if that plugin is not installed */
    if (!addrbook)
        --nb_entries;

    for (gint i = 0; i < nb_entries; ++i) {
        gtk_list_store_append (store, &iter);
        pixbuf = get_icon(browser_entries_full[i].icon_name, widget);
        gtk_list_store_set(store, &iter,
                           PIXBUF_COL, pixbuf,
                           TEXT_COL, _(browser_entries_full[i].icon_descr),
                           PAGE_NUMBER, browser_entries_full[i].page_number,
                           -1);
        if (pixbuf)
            g_object_unref(pixbuf);
    }

    return GTK_TREE_MODEL(store);
}

/* Callback used to catch the destroy event send by pressing escape key or the
 * close button in the preference dialog */
static void
dialog_destroy_cb(G_GNUC_UNUSED GtkWidget *widget,
        G_GNUC_UNUSED gpointer user_data)
{
#ifdef SFL_VIDEO
    gchar ** str = dbus_get_call_list();

    /* we stop the video only if we are not currently in a call */
    if (str == NULL || *str == NULL) {
        dbus_stop_video_camera();
    }

    g_strfreev(str);
#endif
}

/**
 * Show configuration window with tabs
 */
guint
show_preferences_dialog(SFLPhoneClient *client)
{
    GtkDialog *dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("Preferences"),
                                   GTK_WINDOW(client->win),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_STOCK_CLOSE,
                                   GTK_RESPONSE_ACCEPT,
                                   NULL));

    // Set window properties
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    // Create tree view
    iconview = gtk_icon_view_new_with_model(create_model(hbox));
    g_object_set (iconview,
                  "selection-mode", GTK_SELECTION_BROWSE,
                  "text-column", TEXT_COL,
                  "pixbuf-column", PIXBUF_COL,
                  "columns", 1,
                  "margin", 10,
                  NULL);


    /* Connect the callback when the dialog is destroy, ie: pressing escape key or the
     * close button */
    g_signal_connect(GTK_WIDGET(dialog), "destroy", G_CALLBACK(dialog_destroy_cb), NULL);

    // Connect the callback when clicking on an item
    g_signal_connect(G_OBJECT(iconview), "selection-changed", G_CALLBACK(selection_changed_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), iconview, TRUE, TRUE, 0);

    // Create tabs container
    notebook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
    gtk_box_pack_end(GTK_BOX(hbox), notebook, TRUE, TRUE, 0);
    GtkWidget *box = gtk_dialog_get_content_area(dialog);
    gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);
    gtk_widget_show_all(box);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
    gtk_widget_show(notebook);

    // General settings tab
    GtkWidget *tab = create_general_settings(client);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("General")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Audio tab
    tab = create_audio_configuration(client);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Audio")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

#ifdef SFL_VIDEO
    // Video tab
    tab = create_video_configuration();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Video")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
#endif

    // Hooks tab
    tab = create_hooks_settings(client);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Hooks")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Shortcuts tab
    tab = create_shortcuts_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Shortcuts")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    if (addrbook) {
        // Addressbook tab
        tab = create_addressbook_settings(client);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Address Book")));
        gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
    }

    // By default, general settings
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
    // Highlight the corresponding icon
    gtk_icon_view_select_path(GTK_ICON_VIEW(iconview), gtk_tree_path_new_first());

    guint result = gtk_dialog_run(dialog);

    save_configuration_parameters(client);
    update_actions(client);

    gtk_widget_destroy(GTK_WIDGET(dialog));
    return result;
}
