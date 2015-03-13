/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "addrbookfactory.h"
#include "addressbook-config.h"
#include "calltab.h"
#include "calltree.h"

#include <glib.h>
#include <dlfcn.h>

AddrBookHandle *addrbook = NULL;

/**
 * Callback called after all book have been processed
 */
static void
handler_async_search(GList *hits, gpointer user_data)
{
    AddressBook_Config *addressbook_config = user_data;

    gtk_tree_store_clear(contacts_tab->store);
    calllist_reset(contacts_tab);

    for (GList *i = hits; i != NULL; i = i->next) {
        GdkPixbuf *photo = NULL;
        Hit *entry = i->data;

        if (!entry)
            continue;

        if (addressbook_config->display_contact_photo)
            photo = entry->photo;

        if (addressbook_config->search_phone_business)
            calllist_add_contact(entry->name, entry->phone_business,
                                 CONTACT_PHONE_BUSINESS, photo);

        if (addressbook_config->search_phone_home)
            calllist_add_contact(entry->name, entry->phone_home,
                                 CONTACT_PHONE_HOME, photo);

        if (addressbook_config->search_phone_mobile)
            calllist_add_contact(entry->name, entry->phone_mobile,
                                 CONTACT_PHONE_MOBILE, photo);

        g_free(entry->name);
        g_free(entry->phone_business);
        g_free(entry->phone_home);
        g_free(entry->phone_mobile);
        g_free(entry);
    }

    g_list_free(hits);
    gtk_widget_grab_focus(GTK_WIDGET(contacts_tab->view));
}

void abook_init()
{
    /* Clear any existing error */
    dlerror();

    const gchar *addrbook_path = PLUGINS_DIR "/libevladdrbook.so";
    /* FIXME: handle should be unloaded with dlclose on exit */
    void *handle = dlopen(addrbook_path, RTLD_LAZY);

    if (handle == NULL) {
        g_debug("Did not load addressbook from path %s:%s", addrbook_path, dlerror());
        return;
    }

    addrbook = g_new0(AddrBookHandle, 1);
    /* Keep the handle around to dlclose it later */
    addrbook->handle = handle;

#define LOAD(func) do {                                         \
        addrbook-> func = dlsym(handle, "addressbook_" #func);  \
        if (addrbook-> func == NULL) {                          \
            g_warning("Couldn't load " # func ":%s", dlerror());\
            dlclose(handle);                                    \
            g_free(addrbook);                                   \
            addrbook = NULL;                                    \
            return;                                             \
        }                                                       \
    } while(0)


    LOAD(init);
    LOAD(is_ready);
    LOAD(is_active);
    LOAD(search);
    LOAD(get_books_data);
    LOAD(get_book_data_by_uid);
    LOAD(set_current_book);
    LOAD(set_search_type);

    addrbook->search_cb = handler_async_search;
}

void
free_addressbook()
{
    if (!addrbook)
        return;

    if (addrbook->handle)
        dlclose(addrbook->handle);

    g_free(addrbook);
}
