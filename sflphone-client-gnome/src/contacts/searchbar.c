/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
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
 */

#include <searchbar.h>
#include <calltree.h>

GtkWidget * searchbox;

static GtkWidget *menu = NULL;

/**
 * Searchbar icons
 */
GdkPixbuf *incoming_pixbuf = NULL;
GdkPixbuf *outgoing_pixbuf = NULL;
GdkPixbuf *missed_pixbuf = NULL;

void searchbar_entry_changed (GtkEntry* entry, gchar* arg1 UNUSED, gpointer data UNUSED)
{
    if (active_calltree == contacts) {
        addressbook_search (entry);
    }
    else if (active_calltree == history) {
        history_search (HistorySearchType);
    }
}

#if GTK_CHECK_VERSION(2,16,0)

static void search_all (GtkWidget *item, GtkEntry  *entry)
{
    HistorySearchType = SEARCH_ALL;

    gtk_entry_set_icon_from_stock (entry, GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
            _("Search all\n"
            "Click here to change the search type"));

    history_search (HistorySearchType);
} 

static void search_by_missed (GtkWidget *item, GtkEntry  *entry)
{
    HistorySearchType = SEARCH_MISSED;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, missed_pixbuf);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
            _("Search by missed call\n"
            "Click here to change the search type"));

    history_search (HistorySearchType);
} 

static void search_by_incoming (GtkWidget *item, GtkEntry *entry)
{
    HistorySearchType = SEARCH_INCOMING;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, incoming_pixbuf);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
            _("Search by incoming call\n"
            "Click here to change the search type"));

    history_search (HistorySearchType);
} 

static void search_by_outgoing (GtkWidget *item, GtkEntry  *entry)
{
    HistorySearchType = SEARCH_OUTGOING;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, outgoing_pixbuf);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY,
            _("Search by outgoing call\n"
            "Click here to change the search type"));

    history_search (HistorySearchType);
} 

static void icon_press_cb (GtkEntry *entry, gint position, GdkEventButton *event, gpointer data)
{
    if (position == GTK_ENTRY_ICON_PRIMARY && active_calltree == history)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 
                event->button, event->time);
    else 
        gtk_entry_set_text (entry, "");
}

static void text_changed_cb (GtkEntry *entry, GParamSpec *pspec)
{
    gboolean has_text;

    has_text = gtk_entry_get_text_length (entry) > 0;
    gtk_entry_set_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY, has_text);
}

#endif

void
focus_on_searchbar_out(){
    DEBUG("set_focus_on_searchbar_out \n");
    // gtk_widget_grab_focus(GTK_WIDGET(sw));
    focus_is_on_searchbar = FALSE;
}

void
focus_on_searchbar_in(){
    DEBUG("set_focus_on_searchbar_in \n");
    // gtk_widget_grab_focus(GTK_WIDGET(sw));
    focus_is_on_searchbar = TRUE;
}

void searchbar_init(calltab_t *tab)
{
    if (g_strcasecmp (tab->_name, CONTACTS) == 0) 
    {
        addressbook_init();
    }
    else if (g_strcasecmp (tab->_name, HISTORY) == 0) 
    {
        history_init();
    }
    else
        ERROR ("searchbar.c - searchbar_init should not happen within this widget\n");
}

GtkWidget* history_searchbar_new (void)
{

    GtkWidget *ret, *item, *image;

    ret = gtk_hbox_new(FALSE, 0);

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
    image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
    sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(searchbox), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
    sexy_icon_entry_add_clear_button( SEXY_ICON_ENTRY(searchbox) );
#endif

    g_signal_connect_after(GTK_ENTRY(searchbox), "changed", G_CALLBACK(searchbar_entry_changed), NULL);
    g_signal_connect_after (G_OBJECT (searchbox), "focus-in-event",
            G_CALLBACK (focus_on_searchbar_in), NULL);
    g_signal_connect_after (G_OBJECT (searchbox), "focus-out-event",
            G_CALLBACK (focus_on_searchbar_out), NULL);

    gtk_box_pack_start(GTK_BOX(ret), searchbox, TRUE, TRUE, 0);
    history_set_searchbar_widget(searchbox);

    return ret;
}

GtkWidget* contacts_searchbar_new () {

    GtkWidget *ret;

    ret = gtk_hbox_new(FALSE, 0);

#if GTK_CHECK_VERSION(2,16,0)

    GdkPixbuf *pixbuf; 

    searchbox = gtk_entry_new();
    gtk_entry_set_icon_from_stock (GTK_ENTRY (searchbox), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
    pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/stock_person.svg", NULL);
    gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (searchbox), GTK_ENTRY_ICON_PRIMARY, pixbuf);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (searchbox), GTK_ENTRY_ICON_PRIMARY,
            "Search contacts\n"
            "GNOME evolution backend");


    // Set the clean insensitive
    text_changed_cb (GTK_ENTRY (searchbox), NULL);

    g_signal_connect (searchbox, "notify::text", G_CALLBACK (text_changed_cb), NULL);
    g_signal_connect (searchbox, "icon-press", G_CALLBACK (icon_press_cb), NULL);

#else

    GtkWidget *image;

    searchbox = sexy_icon_entry_new();
    image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
    sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(searchbox), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
    sexy_icon_entry_add_clear_button( SEXY_ICON_ENTRY(searchbox) );
#endif

    g_signal_connect_after(GTK_ENTRY(searchbox), "changed", G_CALLBACK(searchbar_entry_changed), NULL);

    g_signal_connect_after (G_OBJECT (searchbox), "focus-in-event",
            G_CALLBACK (focus_on_searchbar_in), NULL);
    g_signal_connect_after (G_OBJECT (searchbox), "focus-out-event",
            G_CALLBACK (focus_on_searchbar_out), NULL);

    gtk_box_pack_start(GTK_BOX(ret), searchbox, TRUE, TRUE, 0);

    return ret;
}

void activateWaitingLayer() {
    gtk_widget_show(waitingLayer);
}

void deactivateWaitingLayer() {
    gtk_widget_hide(waitingLayer);
}

SearchType get_current_history_search_type (void)
{
    return HistorySearchType;
}
