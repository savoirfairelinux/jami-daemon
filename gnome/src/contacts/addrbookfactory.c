/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "addrbookfactory.h"
#include "addressbook-config.h"


#include <glib.h>
#include <dlfcn.h>

static void handler_async_search(GList *hits, gpointer user_data);

AddrBookFactory addressbookFactory = { .addrbook = NULL };

AddrBookFactory *abookfactory_get_factory(void) {
    return &addressbookFactory;
}

gboolean abookfactory_is_addressbook_loaded(void) {
   return (addressbookFactory.addrbook != NULL) ? TRUE : FALSE;
}

void abookfactory_init_factory() {
    abookfactory_load_module(&addressbookFactory);
} 

void abookfactory_scan_directory(AddrBookFactory *factory UNUSED) {
    
}

void abookfactory_load_module(AddrBookFactory *factory)
{
    gchar *plugindir = PLUGINS_DIR;
    gchar *pluginpath = g_strdup_printf("%s/libevladdrbook.so", plugindir);

    DEBUG("AddressbookFactory: Loading addressbook: %s", pluginpath);

    void *handle = dlopen(pluginpath, RTLD_LAZY);
    g_free(pluginpath);
    if(handle == NULL) {
        ERROR("AddressbookFactory: Error: Could not load addressbook");
        return;
    }

    AddrBookHandle *ab = g_malloc(sizeof(AddrBookHandle));

    ab->init = dlsym(handle, "addressbook_init");
    if(ab->init == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook_init function");
 
    ab->is_ready = dlsym(handle, "addressbook_is_ready");
    if(ab->is_ready == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_is_ready function");

    ab->is_enabled = dlsym(handle, "addressbook_is_enabled");
    if(ab->is_enabled == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_is_enabled function");

    ab->is_active = dlsym(handle, "addressbook_is_active");
    if(ab->is_active == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_is_active function");

    ab->search = dlsym(handle, "addressbook_search");
    if(ab->search == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_search function");

    ab->get_books_data = dlsym(handle, "addressbook_get_books_data");
    if(ab->get_books_data == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_get_books_data function");

    ab->get_book_data_by_uid = dlsym(handle, "addressbook_get_book_data_by_uid");
    if(ab->get_book_data_by_uid == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_get_books_data_by_uid function");

    ab->set_current_book = dlsym(handle, "addressbook_set_current_book");
    if(ab->set_current_book == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_ser_current_book");

    ab->set_search_type = dlsym(handle, "addressbook_set_search_type");
    if(ab->set_search_type == NULL)
        ERROR("AddressbookFactory: Error: Could not load addressbook addressbook_set_search_type");

    ab->search_cb = handler_async_search;

    DEBUG("AddressbookFactory: Loading done");
    factory->addrbook = ab;
}

void
free_hit (Hit *h)
{
    g_free (h->name);
    g_free (h->phone_business);
    g_free (h->phone_home);
    g_free (h->phone_mobile);
    g_free (h);
}

/**
 * Callback called after all book have been processed
 */
static void
handler_async_search (GList *hits, gpointer user_data)
{
    GList *i;

    // Retrieve the address book parameters
    AddressBook_Config *addressbook_config = user_data;

    // reset previous results
    calltree_reset (contacts);
    calllist_reset (contacts);

    for (i = hits; i != NULL; i = i->next) {
        GdkPixbuf *photo = NULL;
        Hit *entry = i->data;
        if (!entry)
          continue;

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

        // Create entry for mobile phone iddnformation
        if (addressbook_display (addressbook_config,
                                 ADDRESSBOOK_DISPLAY_PHONE_MOBILE))
            calllist_add_contact (entry->name, entry->phone_mobile,
                                  CONTACT_PHONE_MOBILE, photo);

        free_hit (entry);
    }

    g_list_free (hits);

    // Deactivate waiting image
    // deactivateWaitingLayer();


    gtk_widget_grab_focus (GTK_WIDGET (contacts->view));
}
