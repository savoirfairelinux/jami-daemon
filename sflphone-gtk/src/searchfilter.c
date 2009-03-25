/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
 *
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
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
 */

#include <string.h>

#include <searchfilter.h>
#include <calltree.h>
#include <contactlist/eds.h>
#include "addressbook-config.h"

static void handler_async_search (GList *hits, gpointer user_data UNUSED);

GtkTreeModel* create_filter (GtkTreeModel* child) {

    GtkTreeModel* ret;

    ret = gtk_tree_model_filter_new(child, NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ret), is_visible, NULL, NULL);
    return GTK_TREE_MODEL(ret);

}

gboolean is_visible (GtkTreeModel* model, GtkTreeIter* iter, gpointer data UNUSED) {

    if( SHOW_SEARCHBAR )
    {
        GValue val;

        gchar* text = NULL;
        gchar* search = (gchar*)gtk_entry_get_text(GTK_ENTRY(filter_entry_history));
        memset (&val, 0, sizeof(val));
        gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter, 1, &val);
        if(G_VALUE_HOLDS_STRING(&val)){
            text = (gchar *)g_value_get_string(&val);
        }
        if(text != NULL && g_ascii_strncasecmp(search, _("Search"), 6) != 0){
            return g_regex_match_simple(search, text, G_REGEX_CASELESS, 0);
        }
        g_value_unset (&val);
        return TRUE;
    }
    return TRUE;

}

static void handler_async_search (GList *hits, gpointer user_data) {

    GList *i;
    GdkPixbuf *photo = NULL;
    AddressBook_Config *addressbook_config;
    call_t *j;

    // freeing calls
    while((j = (call_t *)g_queue_pop_tail (contacts->callQueue)) != NULL)
    {
        free_call_t(j);
    }

    // Retrieve the address book parameters
    addressbook_config = (AddressBook_Config*) user_data;

    // reset previous results
    reset_call_tree(contacts);
    call_list_reset(contacts);

    for (i = hits; i != NULL; i = i->next)
    {
        Hit *entry;
        entry = i->data;
        if (entry)
        {
            /* Get the photo */
            if (addressbook_display (addressbook_config, ADDRESSBOOK_DISPLAY_CONTACT_PHOTO))
                photo = entry->photo;
            /* Create entry for business phone information */
            if (addressbook_display (addressbook_config, ADDRESSBOOK_DISPLAY_PHONE_BUSINESS))
                create_new_entry_in_contactlist (entry->name, entry->phone_business, CONTACT_PHONE_BUSINESS, photo);
            /* Create entry for home phone information */
            if (addressbook_display (addressbook_config, ADDRESSBOOK_DISPLAY_PHONE_HOME))
                create_new_entry_in_contactlist (entry->name, entry->phone_home, CONTACT_PHONE_HOME, photo);
            /* Create entry for mobile phone information */
            if (addressbook_display (addressbook_config, ADDRESSBOOK_DISPLAY_PHONE_MOBILE))
                create_new_entry_in_contactlist (entry->name, entry->phone_mobile, CONTACT_PHONE_MOBILE, photo);
        }
        free_hit(entry);
    }
    g_list_free(hits);

    // Deactivate waiting image
    deactivateWaitingLayer();
}

void filter_entry_changed (GtkEntry* entry, gchar* arg1 UNUSED, gpointer data UNUSED) {

    AddressBook_Config *addressbook_config;

    /* Switch to the address book when the focus is on the search bar */
    // if (active_calltree != contacts)
    //     display_calltree (contacts);


    /* We want to search in the contact list */
    if (active_calltree == contacts) {
        // Activate waiting layer
        activateWaitingLayer();

        // Load the address book parameters
        addressbook_load_parameters (&addressbook_config);

        // Start the asynchronous search as soon as we have an entry */
        search_async (gtk_entry_get_text (GTK_ENTRY (entry)), addressbook_config->max_results, &handler_async_search, addressbook_config);
    }

    else if (active_calltree == history) {
        // Filter the displayed calls

    }

}

void
filter_entry_changed_history(GtkEntry* entry UNUSED, gchar* arg1 UNUSED, gpointer data UNUSED)
{
  g_print("--- filter_entry_changed_history --- \n");

  if (active_calltree != history)
        display_calltree (history);

  // gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton), TRUE);
  gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(histfilter));
  // gtk_tree_view_set_model(GTK_TREE_VIEW(history->view), GTK_TREE_MODEL(histfilter));
}


void clear_filter_entry_if_default (GtkWidget* widget, gpointer user_data UNUSED) {

    if(g_ascii_strncasecmp(gtk_entry_get_text(GTK_ENTRY(widget)), _("Search"), 6) == 0)
        gtk_entry_set_text(GTK_ENTRY(widget), "");

}

GtkWidget* create_filter_entry_contact() {

    GtkWidget* image;
    GtkWidget* ret = gtk_hbox_new(FALSE, 0);

    filter_entry_contact = sexy_icon_entry_new();
    image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
    sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(filter_entry_contact), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
    sexy_icon_entry_add_clear_button( SEXY_ICON_ENTRY(filter_entry_contact) );
    gtk_entry_set_text(GTK_ENTRY(filter_entry_contact), _("Search"));
    g_signal_connect(GTK_ENTRY(filter_entry_contact), "changed", G_CALLBACK(filter_entry_changed), NULL);
    g_signal_connect(GTK_ENTRY(filter_entry_contact), "grab-focus", G_CALLBACK(clear_filter_entry_if_default), NULL);

    gtk_box_pack_start(GTK_BOX(ret), filter_entry_contact, TRUE, TRUE, 0);

    return ret;
}


GtkWidget* create_filter_entry_history() {

    g_print("--- create_filter_entry_history --- \n");

    GtkWidget* image;
    GtkWidget* ret = gtk_hbox_new(FALSE, 0);

    filter_entry_history = sexy_icon_entry_new();
    image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
    sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(filter_entry_history), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
    sexy_icon_entry_add_clear_button( SEXY_ICON_ENTRY(filter_entry_history) );
    gtk_entry_set_text(GTK_ENTRY(filter_entry_history), _("Search"));
    g_signal_connect(GTK_ENTRY(filter_entry_history), "changed", G_CALLBACK(filter_entry_changed_history), NULL);
    g_signal_connect(GTK_ENTRY(filter_entry_history), "grab-focus", G_CALLBACK(clear_filter_entry_if_default), NULL);

    gtk_box_pack_start(GTK_BOX(ret), filter_entry_history, TRUE, TRUE, 0);

    return ret;
}

void activateWaitingLayer() {
  gtk_widget_show(waitingLayer);
}

void deactivateWaitingLayer() {
  gtk_widget_hide(waitingLayer);
}
