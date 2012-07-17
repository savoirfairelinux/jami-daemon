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
 *e_book
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

#include <glib.h>
#include <string.h>
#include <pango/pango.h>
#include "eds.h"
#include <libedataserver/e-source.h>

#include "config.h"

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
static GStaticMutex books_data_mutex = G_STATIC_MUTEX_INIT;

/**
 * Current selected addressbook's uri and uid, initialized with default
 */
static gchar *current_uri = NULL;
static gchar *current_uid = NULL;
static gchar *current_name = "Default";

static EBookQueryTest current_test = E_BOOK_QUERY_BEGINS_WITH;

/**
 * Public way to know if we can perform a search
 */
gboolean
books_ready()
{
    g_static_mutex_lock(&books_data_mutex);

    gboolean ret = books_data && g_slist_length (books_data) > 0;

    g_static_mutex_unlock(&books_data_mutex);

    return ret ;
}

/**
 * Public way to know if we enable at least one address book
 */
gboolean
books_active()
{
    gboolean ret = FALSE;

    g_static_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter; iter = iter->next)
        if (((book_data_t *) iter->data)->active) {
            ret = TRUE;
            break;
        }

    g_static_mutex_unlock(&books_data_mutex);

    return ret;
}
/**
 * Get a specific book data by UID
 */
book_data_t *
books_get_book_data_by_uid (gchar *uid)
{
    book_data_t *ret = NULL;

    g_static_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next)
        if (!strcmp (((book_data_t *)iter->data)->uid, uid) ) {
            ret = iter->data;
            break;
        }

    g_static_mutex_unlock(&books_data_mutex);

    return ret;
}


/**
 * Create a query which looks for the specified string in a contact's full name, email addresses and
 * nick name.
 */
static EBookQuery*
create_query (const char* s, EBookQueryTest test, AddressBook_Config *conf)
{
    EBookQuery *queries[4];
    int cpt = 0;

    queries[cpt++] = e_book_query_field_test (E_CONTACT_FULL_NAME, test, s);

    if (conf->search_phone_home)
        queries[cpt++] = e_book_query_field_test (E_CONTACT_PHONE_HOME, test, s);

    if (conf->search_phone_business)
        queries[cpt++] = e_book_query_field_test (E_CONTACT_PHONE_BUSINESS, test, s);

    if (conf->search_phone_mobile)
        queries[cpt++] = e_book_query_field_test (E_CONTACT_PHONE_MOBILE, test, s);

    return e_book_query_or (cpt, queries, TRUE);
}

/**
 * Retrieve the contact's picture
 */
static GdkPixbuf*
pixbuf_from_contact (EContact *contact)
{
    GdkPixbuf *pixbuf = NULL;
    EContactPhoto *photo = e_contact_get (contact, E_CONTACT_PHOTO);

    if (!photo)
        return NULL;

    GdkPixbufLoader *loader;

    loader = gdk_pixbuf_loader_new();

    if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
        if (gdk_pixbuf_loader_write (loader, (guchar *) photo->data.inlined.data, photo->data.inlined.length, NULL)) {
            pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
        }
    }

    e_contact_photo_free (photo);

    if (!pixbuf)
        return NULL;

    // check size and resize if needed
    gint width = gdk_pixbuf_get_width (pixbuf);
    gint height = gdk_pixbuf_get_height (pixbuf);
    double scale = 32 / (double) ((height > width) ? height : width);

    if (scale < 1.0) {
        GdkPixbuf *tmp = gdk_pixbuf_scale_simple (pixbuf, width * scale, height
                                       * scale, GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        pixbuf = tmp;
    }

    return pixbuf;
}

/**
 * Final callback after all books have been processed.
 */
static void
view_finish_callback (EBookView *book_view, Search_Handler_And_Data *had)
{
    SearchAsyncHandler had_handler = had->search_handler;
    GList *had_hits = had->hits;
    gpointer had_user_data = had->user_data;

    g_free (had);

    if (book_view != NULL)
        g_object_unref (book_view);

    // Call display callback
    had_handler (had_hits, had_user_data);
}

/**
 * Callback called after a contact have been found in EDS by search_async_by_contacts.
 */
#ifdef LIBEDATASERVER_VERSION_2_32
static void
eds_query_result_cb (EBook *book, const GError *error, GList *contacts, gpointer user_data)
{
    if (error)
        return;
#else
static void
eds_query_result_cb (EBook *book, EBookStatus status, GList *contacts, gpointer user_data)
{
    if (status != E_BOOK_ERROR_OK)
        return;
#endif

    Search_Handler_And_Data *had = (Search_Handler_And_Data *) user_data;

    if (contacts == NULL) {
        had->search_handler (NULL, user_data);
        return;
    }

    // make sure we have a new list of hits
    had->hits = NULL;

    for (GList *l = contacts; l; l = g_list_next (l)) {
        Hit *hit = g_new (Hit, 1);
        EContact *contact = E_CONTACT(l->data);

        hit->photo = pixbuf_from_contact(contact);
        hit->phone_business = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_PHONE_BUSINESS));
        hit->phone_home     = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_PHONE_HOME));
        hit->phone_mobile   = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_PHONE_MOBILE));
        hit->name           = g_strdup((char*)e_contact_get_const(contact, E_CONTACT_NAME_OR_ORG));

        had->hits = g_list_append (had->hits, hit);

        if (--had->max_results_remaining <= 0)
            break;
    }

    view_finish_callback (NULL, had);

    g_object_unref (book);
}



/**
 * Callback for asynchronous open of books
 */
#ifdef LIBEDATASERVER_VERSION_2_32
void
eds_async_open_callback (EBook *book, const GError *error, gpointer closure)
{
    if(error)
        return;
#else
static void
eds_async_open_callback (EBook *book, EBookStatus status, gpointer closure)
{
    if(status == E_BOOK_ERROR_OK)
        return;

#endif

    Search_Handler_And_Data *had = (Search_Handler_And_Data *) closure;

    if (!e_book_is_opened (book))
        e_book_open (book, FALSE, NULL);

#ifdef LIBEDATASERVER_VERSION_2_32
    e_book_get_contacts_async (book, had->equery, eds_query_result_cb, had);
#else
    e_book_async_get_contacts (book, had->equery, eds_query_result_cb, had);
#endif

}

/**
 * Initialize address book
 */
void
init_eds ()
{
    g_static_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next) {
        book_data_t *book_data = (book_data_t *) iter->data;

        if (book_data->isdefault) {
            current_uri = book_data->uri;
            current_uid = book_data->uid;
            current_name = book_data->name;
        }
    }

    g_static_mutex_unlock(&books_data_mutex);
}

/**
 * Fill book data
 */
void
fill_books_data ()
{
    ESourceList *source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");

    if (source_list == NULL)
        return;

    GSList *list = e_source_list_peek_groups (source_list);

    if (list == NULL) {
        g_object_unref (source_list);
        return;
    }

    g_static_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next) {
        book_data_t *book_data = (book_data_t *) iter->data;

        g_free (book_data->name);
        g_free (book_data->uid);
        g_free (book_data->uri);
    }
    books_data = NULL;

    for (GSList *l = list; l != NULL; l = l->next) {
        ESourceGroup *group = l->data;

        for (GSList *m = e_source_group_peek_sources (group); m != NULL; m = m->next) {
            ESource *source = m->data;

            book_data_t *book_data = g_new (book_data_t, 1);
            book_data->active = FALSE;
            book_data->name = g_strdup (e_source_peek_name (source));
            book_data->uid = g_strdup (e_source_peek_uid (source));

            const gchar *prop = e_source_get_property (source, "default");
            book_data->isdefault = (prop && !strcmp(prop, "true"));
            book_data->uri = g_strconcat(e_source_group_peek_base_uri (group), e_source_peek_relative_uri (source), NULL);

            books_data = g_slist_prepend (books_data, book_data);
        }
    }

    g_static_mutex_unlock(&books_data_mutex);

    g_object_unref (source_list);
}

void
determine_default_addressbook()
{
    g_static_mutex_lock(&books_data_mutex);

    gboolean default_found = FALSE;

    for (GSList *elm = books_data; elm ; elm = g_slist_next (elm)) {
        book_data_t *book_data = elm->data;

        if (book_data->isdefault) {
            current_uri = book_data->uri;
            current_uid = book_data->uid;
            current_name = book_data->name;
            default_found = TRUE;
            break;
        }
    }

    if (!default_found)
        for (GSList *elm = books_data; elm ; elm = g_slist_next (elm)) {
            book_data_t *book_data = elm->data;

            if (book_data->active) {
                book_data->isdefault = TRUE;
                current_uri = book_data->uri;
                current_uid = book_data->uid;
                current_name = book_data->name;
                break;
            }
        }

    g_static_mutex_unlock(&books_data_mutex);
}

void
search_async_by_contacts (const char *query, int max_results, SearchAsyncHandler handler, gpointer user_data)
{
    if (!*query) {
        handler (NULL, user_data);
        return;
    }

    Search_Handler_And_Data *had = g_new (Search_Handler_And_Data, 1);

    // initialize search data
    had->search_handler = handler;
    had->user_data = user_data;
    had->hits = NULL;
    had->max_results_remaining = max_results;
    had->equery = create_query (query, current_test, (AddressBook_Config *) (user_data));

    EBook *book = e_book_new_from_uri(current_uri, NULL);
    if (!book)
        return;

#ifdef LIBEDATASERVER_VERSION_2_32
    e_book_open_async (book, TRUE, eds_async_open_callback, had);
#else
    e_book_async_open(book, TRUE, eds_async_open_callback, had);
#endif
}

void
set_current_addressbook (const gchar *name)
{
    if(name == NULL)
        return;

    g_static_mutex_lock(&books_data_mutex);

    for (GSList *iter = books_data; iter != NULL; iter = iter->next) {
        book_data_t *book_data = (book_data_t *) iter->data;
        if (strcmp (book_data->name, name) == 0) {
            current_uri = book_data->uri;
            current_uid = book_data->uid;
            current_name = book_data->name;
        }
    }

    g_static_mutex_unlock(&books_data_mutex);
}


void
set_current_addressbook_test (EBookQueryTest test)
{
    current_test = test;
}

GSList *
get_books_data()
{
    return books_data;
}
