/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <searchbar.h>
#include <calltree.h>
#include <contacts/addressbook/eds.h>

GtkWidget * searchbox;
GtkWidget * addressbookentry;

static GtkWidget *menu = NULL;

/**
 * Searchbar icons
 */
GdkPixbuf *incoming_pixbuf = NULL;
GdkPixbuf *outgoing_pixbuf = NULL;
GdkPixbuf *missed_pixbuf = NULL;


void searchbar_addressbook_activated (GtkEntry *entry, gchar *arg1 UNUSED, gpointer data UNUSED)
{
    DEBUG ("Searchbar: Entry activated");

    addressbook_search (entry);
}

void searchbar_entry_changed (GtkEntry* entry, gchar* arg1 UNUSED, gpointer data UNUSED)
{
    DEBUG ("Searchbar: Entry changed");

    if (active_calltree == contacts) {
        // Search made only when text entry is activated
        // addressbook_search (entry);
    } else if (active_calltree == history) {
        history_search (HistorySearchType);
    }
}

#if GTK_CHECK_VERSION(2,16,0)

static void cbox_changed_cb (GtkWidget *widget, gpointer user_data)
{
    gchar *name;

    name = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

    set_current_addressbook (name);

    addressbook_search (GTK_ENTRY (addressbookentry));
}

static void select_addressbook (GtkWidget *item, GtkEntry  *entry)
{
    DEBUG ("Searchbar: Selected item label %s", gtk_menu_item_get_label (item));

    set_current_addressbook (gtk_menu_item_get_label (item));

    gchar *name = get_current_addressbook();
    gchar *searching = g_strjoin ("", "Click to select addressbook\n Searching in ", name, NULL);

    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (addressbookentry), GTK_ENTRY_ICON_PRIMARY,
                                     searching);

    addressbook_search (GTK_ENTRY (addressbookentry));

    g_free (searching);
}

static void search_all (GtkWidget *item UNUSED, GtkEntry  *entry)
{
    HistorySearchType = SEARCH_ALL;

    gtk_entry_set_icon_from_stock (entry, GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
                                     g_markup_printf_escaped ("%s\n%s",
                                                              _ ("Search all"),
                                                              _ ("Click here to change the search type")));

    history_search (HistorySearchType);
}

static void search_by_missed (GtkWidget *item UNUSED, GtkEntry  *entry)
{
    HistorySearchType = SEARCH_MISSED;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, missed_pixbuf);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
                                     g_markup_printf_escaped ("%s\n%s",
                                                              _ ("Search by missed call"),
                                                              _ ("Click here to change the search type")));
    history_search (HistorySearchType);
}

static void search_by_incoming (GtkWidget *item UNUSED, GtkEntry *entry)
{
    HistorySearchType = SEARCH_INCOMING;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, incoming_pixbuf);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
                                     g_markup_printf_escaped ("%s\n%s",
                                                              _ ("Search by incoming call"),
                                                              _ ("Click here to change the search type")));
    history_search (HistorySearchType);
}

static void search_by_outgoing (GtkWidget *item UNUSED, GtkEntry  *entry)
{
    HistorySearchType = SEARCH_OUTGOING;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, outgoing_pixbuf);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
                                     g_markup_printf_escaped ("%s\n%s",
                                                              _ ("Search by outgoing call"),
                                                              _ ("Click here to change the search type")));
    history_search (HistorySearchType);
}

static void icon_press_cb (GtkEntry *entry, gint position, GdkEventButton *event, gpointer data UNUSED)
{
    DEBUG ("Searchbar: Icon pressed");

    if (position == GTK_ENTRY_ICON_PRIMARY && active_calltree == history)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        event->button, event->time);
    else if (position == GTK_ENTRY_ICON_PRIMARY && active_calltree == contacts) {
        //GtkWidget *addrbook_menu = addressbook_menu_new();
        //gtk_menu_popup (GTK_MENU (addrbook_menu), NULL, NULL, NULL, NULL,
        //                event->button, event->time);
    } else
        gtk_entry_set_text (entry, "");
}

static void text_changed_cb (GtkEntry *entry, GParamSpec *pspec UNUSED)
{
    gboolean has_text;

    has_text = gtk_entry_get_text_length (entry) > 0;
    gtk_entry_set_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY, has_text);
}

#endif

GtkWidget *addressbook_menu_new (void)
{

    GtkWidget *menu, *item;

    GSList *book_list_iterator;
    book_data_t *book_data;
    GSList *books_data = addressbook_get_books_data();

    // Create the menu
    menu = gtk_menu_new ();
    gtk_menu_attach_to_widget (GTK_MENU (menu), contacts->searchbar, NULL);

    // Populate menu
    for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
            = book_list_iterator->next) {
        book_data = (book_data_t *) book_list_iterator->data;
        item = gtk_menu_item_new_with_label (book_data->name);
        g_signal_connect (item, "activate", G_CALLBACK (select_addressbook), searchbox);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    gtk_widget_show_all (menu);

    return menu;
}

void
focus_on_searchbar_out()
{
    DEBUG ("set_focus_on_searchbar_out");
    // gtk_widget_grab_focus(GTK_WIDGET(sw));
    focus_is_on_searchbar = FALSE;
}

void
focus_on_searchbar_in()
{
    DEBUG ("set_focus_on_searchbar_in");
    // gtk_widget_grab_focus(GTK_WIDGET(sw));
    focus_is_on_searchbar = TRUE;
}

void searchbar_init (calltab_t *tab)
{
    if (g_strcasecmp (tab->_name, CONTACTS) == 0) {
        addressbook_init();
    } else if (g_strcasecmp (tab->_name, HISTORY) == 0) {
        history_init();
    } else
        ERROR ("searchbar.c - searchbar_init should not happen within this widget\n");
}

GtkWidget* history_searchbar_new (void)
{

    GtkWidget *ret, *item, *image;

    ret = gtk_hbox_new (FALSE, 0);

#if GTK_CHECK_VERSION(2,16,0)

    searchbox = gtk_entry_new();
    gtk_entry_set_icon_from_stock (GTK_ENTRY (searchbox), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);

    missed_pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/missed.svg", NULL);
    incoming_pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/incoming.svg", NULL);
    outgoing_pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/outgoing.svg", NULL);

    // Set the clean insensitive
    text_changed_cb (GTK_ENTRY (searchbox), NULL);

    g_signal_connect (searchbox, "icon-press", G_CALLBACK (icon_press_cb), NULL);
    g_signal_connect (searchbox, "notify::text", G_CALLBACK (text_changed_cb), NULL);
    //g_signal_connect (searchbox, "activate", G_CALLBACK (activate_cb), NULL);

    // Set up the search icon
    search_all (NULL, GTK_ENTRY (searchbox));

    // Create the menu
    menu = gtk_menu_new ();
    gtk_menu_attach_to_widget (GTK_MENU (menu), searchbox, NULL);

    image = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
    item = gtk_image_menu_item_new_with_label ("Search all");
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    g_signal_connect (item, "activate", G_CALLBACK (search_all), searchbox);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_image_menu_item_new_with_label ("Search by missed call");
    image = gtk_image_new_from_file (ICONS_DIR "/missed.svg");
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    g_signal_connect (item, "activate", G_CALLBACK (search_by_missed), searchbox);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_image_menu_item_new_with_label ("Search by incoming call");
    image = gtk_image_new_from_file (ICONS_DIR "/incoming.svg");
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    g_signal_connect (item, "activate", G_CALLBACK (search_by_incoming), searchbox);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_image_menu_item_new_with_label ("Search by outgoing call");
    image = gtk_image_new_from_file (ICONS_DIR "/outgoing.svg");
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    g_signal_connect (item, "activate", G_CALLBACK (search_by_outgoing), searchbox);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);

#else
    searchbox = sexy_icon_entry_new();
    image = gtk_image_new_from_stock (GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
    sexy_icon_entry_set_icon (SEXY_ICON_ENTRY (searchbox), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE (image));
    sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (searchbox));
#endif

    g_signal_connect_after (GTK_ENTRY (searchbox), "changed", G_CALLBACK (searchbar_entry_changed), NULL);
    g_signal_connect_after (G_OBJECT (searchbox), "focus-in-event",
                            G_CALLBACK (focus_on_searchbar_in), NULL);
    g_signal_connect_after (G_OBJECT (searchbox), "focus-out-event",
                            G_CALLBACK (focus_on_searchbar_out), NULL);

    gtk_box_pack_start (GTK_BOX (ret), searchbox, TRUE, TRUE, 0);
    history_set_searchbar_widget (searchbox);

    return ret;
}

GtkWidget* contacts_searchbar_new ()
{

    GtkWidget *ret;
    GtkWidget *cbox;
    int count;

    ret = gtk_hbox_new (FALSE, 0);

    // Create combo box to select current addressbook
    cbox = gtk_combo_box_new_text();

    GSList *book_list_iterator;
    book_data_t *book_data;
    GSList *books_data = addressbook_get_books_data();

    // Populate menu
    count = 0;

    for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
            = book_list_iterator->next) {
        book_data = (book_data_t *) book_list_iterator->data;
        gtk_combo_box_append_text (cbox, book_data->name);

        if (book_data->isdefault)
            gtk_combo_box_set_active (cbox, count);

        count++;
    }

    g_signal_connect (cbox, "changed", G_CALLBACK (cbox_changed_cb), NULL);

#if GTK_CHECK_VERSION(2,16,0)

    GdkPixbuf *pixbuf;

    gchar *current_addressbook = get_current_addressbook();
    gchar *tooltip_text = g_strjoin ("", "Click to select addressbook\n Searching in ", current_addressbook, NULL);

    addressbookentry = gtk_entry_new();
    gtk_entry_set_icon_from_stock (GTK_ENTRY (addressbookentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
    pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/stock_person.svg", NULL);
    gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (addressbookentry), GTK_ENTRY_ICON_PRIMARY, pixbuf);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (addressbookentry), GTK_ENTRY_ICON_PRIMARY,
                                     tooltip_text);


    // Set the clean insensitive
    text_changed_cb (GTK_ENTRY (addressbookentry), NULL);

    g_signal_connect (addressbookentry, "notify::text", G_CALLBACK (text_changed_cb), NULL);
    g_signal_connect (addressbookentry, "icon-press", G_CALLBACK (icon_press_cb), NULL);

#else

    GtkWidget *image;

    addressbookentry = sexy_icon_entry_new();
    image = gtk_image_new_from_stock (GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
    sexy_icon_entry_set_icon (SEXY_ICON_ENTRY (addressbookentry), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE (image));
    sexy_icon_entry_add_clear_button (SEXY_ICON_ENTRY (addressbookentry));
#endif

    gtk_entry_set_activates_default (GTK_ENTRY (addressbookentry), TRUE);
    g_signal_connect_after (GTK_ENTRY (addressbookentry), "activate", G_CALLBACK (searchbar_addressbook_activated), NULL);

    g_signal_connect_after (GTK_ENTRY (addressbookentry), "changed", G_CALLBACK (searchbar_entry_changed), NULL);

    g_signal_connect_after (G_OBJECT (addressbookentry), "focus-in-event",
                            G_CALLBACK (focus_on_searchbar_in), NULL);
    g_signal_connect_after (G_OBJECT (addressbookentry), "focus-out-event",
                            G_CALLBACK (focus_on_searchbar_out), NULL);


    gtk_box_pack_start (GTK_BOX (ret), cbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (ret), addressbookentry, TRUE, TRUE, 0);

    g_free (tooltip_text);

    return ret;
}

void activateWaitingLayer()
{
    gtk_widget_show (waitingLayer);
}

void deactivateWaitingLayer()
{
    gtk_widget_hide (waitingLayer);
}

SearchType get_current_history_search_type (void)
{
    return HistorySearchType;
}
