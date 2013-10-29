/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  File originally copied from evolution module of deskbar-applet 2.24.1
 *   Authors :
 *    Nigel Tao <nigel.tao@myrealbox.com>
 *    Raphaël Slinckx <raphael@slinckx.net>
 *    Mikkel Kamstrup Erlandsen <kamstrup@daimi.au.dk>
 *    Sebastian Pölsterl <marduk@k-d-w.org>
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

#include "config.h"

#include <glib.h>
#include <string.h>
#include <pango/pango.h>
#include "eds.h"
#if EDS_CHECK_VERSION(3,5,3)
#include <libedataserver/libedataserver.h>
#else /* < EDS 3.5.3  */
#include <libedataserver/e-source.h>
#include <libebook/e-book-client.h>
#endif

#define GCC_VERSION (__GNUC__ * 10000 \
        + __GNUC_MINOR__ * 100 \
        + __GNUC_PATCHLEVEL__)
/* Test for GCC < 4.7.0 */
#if GCC_VERSION < 40700
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

/**
 * Structure used to store search callback and data
 */
typedef struct _Search_Handler_And_Data {
    SearchAsyncHandler search_handler;
    gpointer user_data;
    GList *hits;
    int max_results_remaining;
    EBookQuery *equery;
} Search_Handler_And_Data;

/**
 * The global addressbook list
 */
static GSList *books_data = NULL;
static GMutex books_data_mutex;

/**
 * Current selected addressbook's uri and uid, initialized with default
 */
#if !EDS_CHECK_VERSION(3,5,3)
static gchar *current_uri = NULL;
#endif
static gchar *current_uid = NULL;

static EBookQueryTest current_test = E_BOOK_QUERY_BEGINS_WITH;

/**
 * Public way to know if we can perform a search
 */
gboolean
books_ready()
{
    g_mutex_lock(&books_data_mutex);

    gboolean ret = books_data && g_slist_length(books_data) > 0;

    g_mutex_unlock(&books_data_mutex);

    return ret ;
}

/**
 * Public way to know if we enable at least one address book
 */
gboolean
books_active()
{
    gboolean ret = FALSE;

    g_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter; iter = iter->next)
        if (((book_data_t *) iter->data)->active) {
            ret = TRUE;
            break;
        }

    g_mutex_unlock(&books_data_mutex);

    return ret;
}
/**
 * Get a specific book data by UID
 */
book_data_t *
books_get_book_data_by_uid(gchar *uid)
{
    book_data_t *ret = NULL;

    g_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next)
        if (!strcmp(((book_data_t *)iter->data)->uid, uid) ) {
            ret = iter->data;
            break;
        }

    g_mutex_unlock(&books_data_mutex);

    return ret;
}


/**
 * Create a query which looks for the specified string in a contact's full name, email addresses and
 * nick name.
 */
static EBookQuery*
create_query(const char* s, EBookQueryTest test, AddressBook_Config *conf)
{
    EBookQuery *queries[4];
    int cpt = 0;

    queries[cpt++] = e_book_query_field_test(E_CONTACT_FULL_NAME, test, s);

    if (!conf || conf->search_phone_home)
        queries[cpt++] = e_book_query_field_test(E_CONTACT_PHONE_HOME, test, s);

    if (!conf || conf->search_phone_business)
        queries[cpt++] = e_book_query_field_test(E_CONTACT_PHONE_BUSINESS, test, s);

    if (!conf || conf->search_phone_mobile)
        queries[cpt++] = e_book_query_field_test(E_CONTACT_PHONE_MOBILE, test, s);

    return e_book_query_or(cpt, queries, TRUE);
}

/**
 * Create a query which looks any contact with a phone number
 */
static EBookQuery*
create_query_all_phones(AddressBook_Config *conf)
{
    EBookQuery *queries[3];
    int cpt = 0;

    if (!conf || conf->search_phone_home)
        queries[cpt++] = e_book_query_field_exists(E_CONTACT_PHONE_HOME);

    if (!conf || conf->search_phone_business)
        queries[cpt++] = e_book_query_field_exists(E_CONTACT_PHONE_BUSINESS);

    if (!conf || conf->search_phone_mobile)
        queries[cpt++] = e_book_query_field_exists(E_CONTACT_PHONE_MOBILE);

    return e_book_query_or(cpt, queries, TRUE);
}

/**
 * Retrieve the contact's picture
 */
static GdkPixbuf*
pixbuf_from_contact(EContact *contact)
{
    GdkPixbuf *pixbuf = NULL;
    EContactPhoto *photo = e_contact_get(contact, E_CONTACT_PHOTO);

    if (!photo)
        return NULL;

    GdkPixbufLoader *loader;

    loader = gdk_pixbuf_loader_new();

    if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
        if (gdk_pixbuf_loader_write(loader, (guchar *) photo->data.inlined.data, photo->data.inlined.length, NULL)) {
            pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        }
    }

    e_contact_photo_free(photo);

    if (!pixbuf) {
        return NULL;
    }

    // check size and resize if needed
    gint width = gdk_pixbuf_get_width(pixbuf);
    gint height = gdk_pixbuf_get_height(pixbuf);
    double scale = 32 / (double) ((height > width) ? height : width);

    if (scale < 1.0) {
        GdkPixbuf *tmp = gdk_pixbuf_scale_simple(pixbuf, width * scale, height
                                       * scale, GDK_INTERP_BILINEAR);
        g_object_unref(pixbuf);
        pixbuf = tmp;
    }

    return pixbuf;
}

/**
 * Final callback after all books have been processed.
 */
static void
view_finish_callback(Search_Handler_And_Data *had)
{
    SearchAsyncHandler had_handler = had->search_handler;
    GList *had_hits = had->hits;
    gpointer had_user_data = had->user_data;

    g_free(had);

    // Call display callback
    had_handler(had_hits, had_user_data);
}

/**
 * Callback called after a contact have been found in EDS by search_async_by_contacts.
 */

static void
eds_query_result_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
    EBookClient *book_client = E_BOOK_CLIENT(object);
    if (!book_client)
        return;
    GSList *contacts;
    if (!e_book_client_get_contacts_finish(book_client, result, &contacts, NULL))
        return;
    Search_Handler_And_Data *had = (Search_Handler_And_Data *) user_data;

    // make sure we have a new list of hits
    had->hits = NULL;

    for (GSList *l = contacts; l; l = l->next) {
        Hit *hit = g_new0(Hit, 1);
        EContact *contact = E_CONTACT(l->data);

        hit->photo = pixbuf_from_contact(contact);
        hit->phone_business = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_PHONE_BUSINESS));
        hit->phone_home     = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_PHONE_HOME));
        hit->phone_mobile   = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_PHONE_MOBILE));
        hit->name           = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_NAME_OR_ORG));

        had->hits = g_list_append(had->hits, hit);

        if (--had->max_results_remaining <= 0)
            break;
    }
    g_slist_foreach(contacts, (GFunc) g_object_unref, NULL);
    g_slist_free(contacts);

    view_finish_callback(had);

    g_object_unref(object);
}


/**
 * Callback for asynchronous opening of book client
 */
static void
client_open_async_callback(GObject *client, GAsyncResult *result, gpointer closure)
{
    if (!client)
        return;
    if (!e_client_open_finish(E_CLIENT(client), result, NULL))
        return;

    Search_Handler_And_Data *had = (Search_Handler_And_Data *) closure;
    gchar *query_str = e_book_query_to_string(had->equery);
    e_book_client_get_contacts(E_BOOK_CLIENT(client), query_str, NULL, eds_query_result_cb, closure);
    g_free(query_str);
}

/**
 * Initialize address book
 */
void
init_eds()
{
    g_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next) {
        book_data_t *book_data = (book_data_t *) iter->data;

#if !EDS_CHECK_VERSION(3,5,3)
        current_uri = book_data->uri;
#endif
        current_uid = book_data->uid;
    }

    g_mutex_unlock(&books_data_mutex);
}

static GSList *
free_books_data(GSList *list)
{
    for (GSList *iter = list; iter != NULL; iter = iter->next) {
        book_data_t *book_data = (book_data_t *) iter->data;

        g_free(book_data->name);
        g_free(book_data->uid);
        g_free(book_data->uri);
    }
    return NULL;
}

/**
 * Fill books data
 */
#if EDS_CHECK_VERSION(3,5,3)

static book_data_t *
create_book_data_from_source(ESource *source)
{
    book_data_t *book_data = g_new0(book_data_t, 1);
    book_data->active = e_source_get_enabled(source);
    book_data->name = g_strdup(e_source_get_display_name(source));
    book_data->uri = g_strdup(""); // No longer used
    book_data->uid = g_strdup(e_source_get_uid(source));
    return book_data;
}

static ESourceRegistry *
get_registry()
{
    static ESourceRegistry * registry;
    if (registry == NULL)
        registry = e_source_registry_new_sync(NULL, NULL);
    return registry;
}

void
fill_books_data()
{
    // FIXME: add error handling
    ESourceRegistry *registry = get_registry();
    const gchar *extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
    GList *list = e_source_registry_list_sources(registry, extension_name);

    g_mutex_lock(&books_data_mutex);

    books_data = free_books_data(books_data);

    for (GList *l = list; l != NULL; l = l->next) {
        if (l->data) {
            ESource *source = l->data;
            book_data_t *book_data = create_book_data_from_source(source);
            books_data = g_slist_prepend(books_data, book_data);
        }
    }

    g_mutex_unlock(&books_data_mutex);

    /* Free each source in the list and the list itself */
    g_list_free_full(list, g_object_unref);
}

#else /* EDS < 3.5.3 */

static book_data_t *
create_book_data_from_source(ESource *source, ESourceGroup *group)
{
    book_data_t *book_data = g_new0(book_data_t, 1);
    book_data->active = TRUE;
    book_data->name = g_strdup(e_source_peek_name(source));
    book_data->uid = g_strdup(e_source_peek_uid(source));

    book_data->uri = g_strconcat(e_source_group_peek_base_uri(group), e_source_peek_relative_uri(source), NULL);
    return book_data;
}


void
fill_books_data()
{
    ESourceList *source_list = e_source_list_new_for_gconf_default("/apps/evolution/addressbook/sources");

    if (source_list == NULL)
        return;

    GSList *list = e_source_list_peek_groups(source_list);

    if (list == NULL) {
        g_object_unref(source_list);
        return;
    }

    g_mutex_lock(&books_data_mutex);

    books_data = free_books_data(books_data);

    for (GSList *l = list; l != NULL; l = l->next) {
        ESourceGroup *group = l->data;

        for (GSList *m = e_source_group_peek_sources(group); m != NULL; m = m->next) {
            ESource *source = m->data;
            book_data_t *book_data = create_book_data_from_source(source, group);
            books_data = g_slist_prepend(books_data, book_data);
        }
    }

    g_mutex_unlock(&books_data_mutex);

    g_object_unref(source_list);
}
#endif

// FIXME: should be obtained by e_source_registry_ref_default_address_book ()
void
determine_default_addressbook()
{
    g_mutex_lock(&books_data_mutex);

    /* Just grabbing first addressbook as default */
    for (GSList *elm = books_data; elm ; elm = g_slist_next(elm)) {
        book_data_t *book_data = elm->data;

#if !EDS_CHECK_VERSION(3,5,3)
        current_uri = book_data->uri;
#endif
        current_uid = book_data->uid;
        break;
    }

    for (GSList *elm = books_data; elm ; elm = g_slist_next(elm)) {
        book_data_t *book_data = elm->data;

        if (book_data->active) {
#if !EDS_CHECK_VERSION(3,5,3)
            current_uri = book_data->uri;
#endif
            current_uid = book_data->uid;
            break;
        }
    }

    g_mutex_unlock(&books_data_mutex);
}

void
search_async_by_contacts(const char *query, int max_results, SearchAsyncHandler handler, gpointer user_data)
{
    Search_Handler_And_Data *had = g_new0(Search_Handler_And_Data, 1);

    // initialize search data
    had->search_handler = handler;
    had->user_data = user_data;
    had->hits = NULL;
    had->max_results_remaining = max_results;
    if (!g_strcmp0(query, ""))
        had->equery = create_query_all_phones(user_data);
    else
        had->equery = create_query(query, current_test, user_data);


#if EDS_CHECK_VERSION(3,5,3)
    ESourceRegistry *registry = get_registry();
    ESource *source = e_source_registry_ref_source(registry, current_uid);
    EBookClient *book_client = e_book_client_new(source, NULL);
    g_object_unref(source);
#else /* < EDS 3.5.3 */
    EBookClient *book_client = e_book_client_new_from_uri(current_uri, NULL);
#endif
    if (!book_client)
        return;
    e_client_open(E_CLIENT(book_client), TRUE, NULL, client_open_async_callback, had);

}

void
set_current_addressbook(const gchar *name)
{
    if(name == NULL)
        return;

    g_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next) {
        book_data_t *book_data = (book_data_t *) iter->data;
        if (g_strcmp0(book_data->name, name) == 0) {
#if !EDS_CHECK_VERSION(3,5,3)
            current_uri = book_data->uri;
#endif
            current_uid = book_data->uid;
        }
    }

    g_mutex_unlock(&books_data_mutex);
}


void
set_current_addressbook_test(EBookQueryTest test)
{
    current_test = test;
}

GSList *
get_books_data()
{
    return books_data;
}
