/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <addressbook.h>
#include <searchbar.h>
#include <string.h>
#include <addressbook-config.h>

static void
handler_async_search (GList *, gpointer);

/**
 * Perform a search on address book
 */
void
addressbook_search (GtkEntry* entry)
{

    const gchar* query = gtk_entry_get_text (GTK_ENTRY (entry));
    DEBUG ("Addressbook: Search %s", query);


    AddressBook_Config *addressbook_config;

    activateWaitingLayer();

    addressbook_config_load_parameters (&addressbook_config);


    search_async_by_contacts (gtk_entry_get_text (GTK_ENTRY (entry)), addressbook_config->max_results, &handler_async_search, addressbook_config);

}

/**
 * Return addressbook state
 */
gboolean
addressbook_is_enabled()
{
    AddressBook_Config *addressbook_config;

    // Load the address book parameters
    addressbook_config_load_parameters (&addressbook_config);

    return (guint) addressbook_config->enable;
}

/**
 * Return addressbook state
 */
gboolean
addressbook_is_ready()
{
    return books_ready();
}

/**
 * Return TRUE if at least one addressbook is active
 */
gboolean
addressbook_is_active()
{
    return books_active();
}

/**
 * Get active addressbook from config.
 */
static void
addressbook_config_books()
{

    gchar **config_book_uid;
    book_data_t *book_data;
    gchar **list;

    // Retrieve list of books
    list = (gchar **) dbus_get_addressbook_list();

    if (list) {

        for (config_book_uid = list; *config_book_uid; config_book_uid++) {

            // Get corresponding book data
            book_data = books_get_book_data_by_uid (*config_book_uid);

            // If book_data exists
            if (!book_data) {
                ERROR ("Addressbook: Error: Could not open book (%s:%d)", __FILE__, __LINE__);
		continue;
	    }

            book_data->active = TRUE;
        }

        g_strfreev (list);
    }

    // Update buttons
    // update_actions ();
}

/**
 * Good method to get books_data
 */
GSList *
addressbook_get_books_data()
{
    DEBUG ("Addressboook: Get books data");

    fill_books_data();
    addressbook_config_books();

    return books_data;
}

/**
 * Initialize books.
 * Set active/inactive status depending on config.
 */
void
addressbook_init()
{
    DEBUG ("Addressbook: Initialize addressbook");

    fill_books_data();
    addressbook_config_books();

    // Call books initialization
    init ();
}

/**
 * Callback called after all book have been processed
 */
static void
handler_async_search (GList *hits, gpointer user_data)
{

    GList *i;
    GdkPixbuf *photo = NULL;
    AddressBook_Config *addressbook_config;
    callable_obj_t *j;

    DEBUG ("Addressbook: callback async search");

    // freeing calls
    while ( (j = (callable_obj_t *) g_queue_pop_tail (contacts->callQueue)) != NULL) {
        free_callable_obj_t (j);
    }

    // Retrieve the address book parameters
    addressbook_config = (AddressBook_Config*) user_data;

    // reset previous results
    calltree_reset (contacts);
    calllist_reset (contacts);

    for (i = hits; i != NULL; i = i->next) {

        Hit *entry;
        entry = i->data;

        if (entry) {
            // Get the photo
            if (addressbook_display (addressbook_config,
                                     ADDRESSBOOK_DISPLAY_CONTACT_PHOTO))
                photo = entry->photo;

            // Create entry for business phone information
            if (addressbook_display (addressbook_config,
                                     ADDRESSBOOK_DISPLAY_PHONE_BUSINESS))
                calllist_add_contact (entry->name, entry->phone_business,
                                      CONTACT_PHONE_BUSINESS, photo);

            // Create entry for home phone information
            if (addressbook_display (addressbook_config,
                                     ADDRESSBOOK_DISPLAY_PHONE_HOME))
                calllist_add_contact (entry->name, entry->phone_home,
                                      CONTACT_PHONE_HOME, photo);

            // Create entry for mobile phone information
            if (addressbook_display (addressbook_config,
                                     ADDRESSBOOK_DISPLAY_PHONE_MOBILE))
                calllist_add_contact (entry->name, entry->phone_mobile,
                                      CONTACT_PHONE_MOBILE, photo);
        }

        free_hit (entry);
    }

    g_list_free (hits);

    // Deactivate waiting image
    deactivateWaitingLayer();


    gtk_widget_grab_focus (GTK_WIDGET (contacts->view));
}

