/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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
#include "searchbar.h"
#include "calltree.h"
#include "calltab.h"
#include "dbus.h"
#include "mainwindow.h"
#include "config/addressbook-config.h"
#include "contacts/addressbook.h"
#include "contacts/addrbookfactory.h"

static GtkWidget * searchbox;
static GtkWidget * addressbookentry;
static SearchType HistorySearchType;

static GtkWidget * cbox;
static GtkListStore * liststore;

static gint cboxSignalId;

static GtkWidget *menu;

/**
 * Searchbar icons
 */
static GdkPixbuf *incoming_pixbuf;
static GdkPixbuf *outgoing_pixbuf;
static GdkPixbuf *missed_pixbuf;

static void
searchbar_addressbook_activated(GtkEntry *entry, gpointer data)
{
    if (addrbook)
        addrbook->search(addrbook->search_cb, entry,
                addressbook_config_load_parameters(data));
}

static void
searchbar_entry_changed(G_GNUC_UNUSED GtkEntry* entry, G_GNUC_UNUSED gchar* arg1, G_GNUC_UNUSED gpointer data)
{
    g_debug("Searchbar: Entry changed");
    if (calltab_has_name(active_calltree_tab, HISTORY))
        history_search();
}

static gchar *
get_combobox_active_text(GtkWidget *widget)
{
    GtkTreeIter iter;
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter))
        return NULL;
    GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
    gchar *string = NULL;
    /* this will return a strdup'd string of the text for the active
     * entry, which must be freed */
    gtk_tree_model_get(model, &iter, 0, &string, -1);
    return string;
}

static void
update_current_addressbook(GtkWidget *widget, GSettings *settings)
{
    if (!addrbook)
        return;
    gchar *string = get_combobox_active_text(widget);
    if (string) {
        addrbook->set_current_book(string);
        g_free(string);
    }

    AddressBook_Config *addressbook_config = addressbook_config_load_parameters(settings);
    addrbook->search(addrbook->search_cb, GTK_ENTRY(addressbookentry), addressbook_config);
}

static void
cbox_changed_cb(GtkWidget *widget, gpointer user_data)
{
    if (!addrbook)
        return;
    update_current_addressbook(widget, user_data);
}

void
set_focus_on_addressbook_searchbar()
{
    gtk_widget_grab_focus(addressbookentry);
}

void
update_searchbar_addressbook_list(GSettings *settings)
{
    GSList *books_data = NULL;

    if (addrbook)
        books_data = addrbook->get_books_data();

    if (books_data == NULL) {
        g_warning("Searchbar: No books data found");
        return;
    }

    g_debug("Searchbar: Update addressbook list");

    // we must disconnect signal from the cbox while updating its content
    g_signal_handler_disconnect(cbox, cboxSignalId);

    // store the current active text
    gchar *activeText = get_combobox_active_text(cbox);

    if (activeText == NULL)
        activeText = g_strdup("");

    gtk_list_store_clear(liststore);

    // Populate menu
    gboolean activeIsSet = FALSE;

    GtkTreeIter iter, activeIter;
    for (GSList *book_list_iterator = books_data; book_list_iterator != NULL;
            book_list_iterator = book_list_iterator->next) {
        book_data_t *book_data = (book_data_t *) book_list_iterator->data;

        if (book_data && book_data->active) {

            gtk_list_store_append(liststore, &iter);
            gtk_list_store_set(liststore, &iter, 0, book_data->name, -1);

            if (g_strcmp0(book_data->name, activeText) == 0) {
                activeIter = iter;
                activeIsSet = TRUE;
            }
        }
    }

    if (addrbook) {
        if (activeIsSet) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(cbox), &activeIter);
            addrbook->set_current_book(activeText);
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), 0);
            update_current_addressbook(cbox, settings);
        }
    }

    g_free(activeText);
    cboxSignalId = g_signal_connect(G_OBJECT(cbox), "changed",
            G_CALLBACK(cbox_changed_cb), settings);
}

static gboolean
label_matches(const gchar *label, GtkWidget *item)
{
    return g_strcmp0(label, gtk_menu_item_get_label(GTK_MENU_ITEM(item))) == 0;
}

static void
select_search_type(GtkWidget *item, GSettings *settings)
{
    if (!addrbook)
        return;

    g_debug("Searchbar: %s", gtk_menu_item_get_label(GTK_MENU_ITEM(item)));

    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(addressbookentry), GTK_ENTRY_ICON_PRIMARY,
                                    gtk_menu_item_get_label(GTK_MENU_ITEM(item)));


    if (label_matches("Search is", item))
        addrbook->set_search_type(ABOOK_QUERY_IS);
    else if (label_matches("Search begins with", item))
        addrbook->set_search_type(ABOOK_QUERY_BEGINS_WITH);
    else if (label_matches("Search contains", item))
        addrbook->set_search_type(ABOOK_QUERY_CONTAINS);

    addrbook->search(addrbook->search_cb, GTK_ENTRY(addressbookentry),
                     addressbook_config_load_parameters(settings));
}

static void
update_search_entry(GtkEntry *entry, const gchar *search_str,
                    const gchar *click_str, SearchType type)
{
    HistorySearchType = type;
    gchar *markup = g_markup_printf_escaped("%s\n%s", search_str, click_str);
    gtk_entry_set_icon_tooltip_text(entry, GTK_ENTRY_ICON_PRIMARY, markup);
    g_free(markup);
    history_search();
}

static void
search_all(G_GNUC_UNUSED GtkWidget *item, GtkEntry *entry)
{
    gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
    update_search_entry(entry, _("Search all"),
                        _("Click here to change the search type"), SEARCH_ALL);
}

static void
search_by_missed(G_GNUC_UNUSED GtkWidget *item, GtkEntry *entry)
{
    gtk_entry_set_icon_from_pixbuf(entry, GTK_ENTRY_ICON_PRIMARY, missed_pixbuf);
    update_search_entry(entry, _("Search by missed call"),
                        _("Click here to change the search type"), SEARCH_MISSED);
}

static void
search_by_incoming(G_GNUC_UNUSED GtkWidget *item, GtkEntry *entry)
{
    gtk_entry_set_icon_from_pixbuf(entry, GTK_ENTRY_ICON_PRIMARY, incoming_pixbuf);
    update_search_entry(entry, _("Search by incoming call"),
                        _("Click here to change the search type"), SEARCH_INCOMING);
}

static void
search_by_outgoing(G_GNUC_UNUSED GtkWidget *item, GtkEntry *entry)
{
    gtk_entry_set_icon_from_pixbuf(entry, GTK_ENTRY_ICON_PRIMARY, outgoing_pixbuf);
    update_search_entry(entry, _("Search by outgoing call"),
                        _("Click here to change the search type"), SEARCH_OUTGOING);
}

static void
icon_press_cb(GtkEntry *entry, gint position, GdkEventButton *event, gpointer data)
{
    g_debug("Searchbar: Icon pressed");

    if (position == GTK_ENTRY_ICON_PRIMARY)
       if (calltab_has_name(active_calltree_tab, HISTORY))
           gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL,
                          NULL, event->button, event->time);
       else if (calltab_has_name(active_calltree_tab, CONTACTS))
           gtk_menu_popup(GTK_MENU(addressbook_menu_new(data)), NULL, NULL, NULL,
                          NULL, event->button, event->time);
       else
           gtk_entry_set_text(entry, "");
    else
        gtk_entry_set_text(entry, "");
}

static void
text_changed_cb(GtkEntry *entry, G_GNUC_UNUSED GParamSpec *pspec)
{
    const gboolean has_text = gtk_entry_get_text_length(entry) > 0;
    gtk_entry_set_icon_sensitive(entry, GTK_ENTRY_ICON_SECONDARY, has_text);
}


GtkWidget *
addressbook_menu_new(GSettings *settings)
{
    // Create the menu
    GtkWidget *menu_widget = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(menu_widget), contacts_tab->searchbar, NULL);

    // Populate menu
    GtkWidget *item = gtk_menu_item_new_with_label("Search is");
    g_signal_connect(item, "activate", G_CALLBACK(select_search_type), settings);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), item);

    item = gtk_menu_item_new_with_label("Search begins with");
    g_signal_connect(item, "activate", G_CALLBACK(select_search_type), settings);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), item);

    item = gtk_menu_item_new_with_label("Search contains");
    g_signal_connect(item, "activate", G_CALLBACK(select_search_type), settings);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), item);

    gtk_widget_show_all(menu_widget);

    return menu_widget;
}


GtkWidget*
history_searchbar_new(GSettings *settings)
{
    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    searchbox = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(searchbox), GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");

    missed_pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/missed.svg", NULL);
    incoming_pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/incoming.svg", NULL);
    outgoing_pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/outgoing.svg", NULL);

    // Set the clean insensitive
    text_changed_cb(GTK_ENTRY(searchbox), NULL);

    g_signal_connect(searchbox, "icon-press", G_CALLBACK(icon_press_cb), settings);
    g_signal_connect(searchbox, "notify::text", G_CALLBACK(text_changed_cb), NULL);

    // Set up the search icon
    search_all(NULL, GTK_ENTRY(searchbox));

    // Create the menu
    menu = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(menu), searchbox, NULL);

    GtkWidget *image = gtk_image_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_MENU);
    GtkWidget *item = gtk_image_menu_item_new_with_label("Search all");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(item, "activate", G_CALLBACK(search_all), searchbox);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_label(_("Search by missed call"));
    image = gtk_image_new_from_file(ICONS_DIR "/missed.svg");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(item, "activate", G_CALLBACK(search_by_missed), searchbox);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_label(_("Search by incoming call"));
    image = gtk_image_new_from_file(ICONS_DIR "/incoming.svg");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(item, "activate", G_CALLBACK(search_by_incoming), searchbox);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_label(_("Search by outgoing call"));
    image = gtk_image_new_from_file(ICONS_DIR "/outgoing.svg");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(item, "activate", G_CALLBACK(search_by_outgoing), searchbox);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);

    g_signal_connect_after(GTK_ENTRY(searchbox), "changed", G_CALLBACK(searchbar_entry_changed), NULL);
    g_signal_connect_after(G_OBJECT(searchbox), "focus-in-event",
                           G_CALLBACK(focus_on_searchbar_in), NULL);
    g_signal_connect_after(G_OBJECT(searchbox), "focus-out-event",
                           G_CALLBACK(focus_on_searchbar_out), NULL);

    gtk_box_pack_start(GTK_BOX(ret), searchbox, TRUE, TRUE, 0);
    history_set_searchbar_widget(searchbox);

    return ret;
}

GtkWidget*
contacts_searchbar_new(GSettings *settings)
{
    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    liststore = gtk_list_store_new(1, G_TYPE_STRING);

    // Create combo box to select current addressbook
    if (!addrbook)
        return NULL;

    addrbook->init();

    // Populate menu
    int count = 0;
    gboolean activeIsSet = FALSE;

    GtkTreeIter iter, activeIter;
    GSList *books_data = addrbook->get_books_data();
    for (GSList *book_list_iterator = books_data; book_list_iterator != NULL;
         book_list_iterator = book_list_iterator->next) {
        book_data_t *book_data = (book_data_t *) book_list_iterator->data;

        g_debug("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ %s", book_data->name);

        if (book_data->active) {

            gtk_list_store_append(liststore, &iter);
            gtk_list_store_set(liststore, &iter, 0, book_data->name, -1);

            activeIter = iter;
            activeIsSet = TRUE;
            count++;
        }
    }

    cbox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(liststore));

    if (activeIsSet)
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(cbox), &activeIter);
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), 0);

    GtkWidget *align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 2, 6, 6);
    gtk_container_add(GTK_CONTAINER(align), cbox);

    gint cbox_width, cbox_height;
    gtk_widget_get_size_request(GTK_WIDGET(cbox), &cbox_width, &cbox_height);
    gtk_widget_set_size_request(GTK_WIDGET(cbox), cbox_width, 26);

    cboxSignalId = g_signal_connect(G_OBJECT(cbox), "changed",
            G_CALLBACK(cbox_changed_cb), settings);

    GtkCellRenderer *cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cbox), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cbox), cell, "text", 0, NULL);

    gchar *tooltip_text = g_strdup("Search is");

    addressbookentry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(addressbookentry), GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(addressbookentry), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(addressbookentry), GTK_ENTRY_ICON_PRIMARY,
                                    tooltip_text);

    // Set the clean insensitive
    text_changed_cb(GTK_ENTRY(addressbookentry), NULL);

    g_signal_connect(addressbookentry, "notify::text", G_CALLBACK(text_changed_cb), NULL);
    g_signal_connect(addressbookentry, "icon-press", G_CALLBACK(icon_press_cb), settings);

    gtk_entry_set_activates_default(GTK_ENTRY(addressbookentry), TRUE);
    g_signal_connect_after(GTK_ENTRY(addressbookentry), "activate",
            G_CALLBACK(searchbar_addressbook_activated), settings);

    g_signal_connect_after(GTK_ENTRY(addressbookentry), "changed", G_CALLBACK(searchbar_entry_changed), NULL);

    g_signal_connect_after(G_OBJECT(addressbookentry), "focus-in-event",
                           G_CALLBACK(focus_on_searchbar_in), NULL);
    g_signal_connect_after(G_OBJECT(addressbookentry), "focus-out-event",
                           G_CALLBACK(focus_on_searchbar_out), NULL);


    gtk_box_pack_start(GTK_BOX(ret), align, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(ret), addressbookentry, TRUE, TRUE, 0);

    g_free(tooltip_text);

    update_current_addressbook(cbox, settings);

    return ret;
}

SearchType get_current_history_search_type()
{
    return HistorySearchType;
}
