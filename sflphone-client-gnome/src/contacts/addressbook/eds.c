/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <glib.h>
#include <glib/gstring.h>
#include <string.h>
#include <pango/pango.h>
#include "eds.h"
#include <addressbook-config.h>

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
 * Size of image that will be displayed in contact list
 */
static int pixbuf_size = 32;

/**
 * Remaining books to open (asynchronous)
 */
int remaining_books_to_open;

/**
 * Current selected addressbook's uri and uid, initialized with default
 */
static gchar *current_uri = NULL;
static gchar *current_uid = NULL;
static gchar *current_name = "Default";

static EBookQueryTest current_test = E_BOOK_QUERY_BEGINS_WITH;

/**
 * Freeing a hit instance
 */
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
 * Public way to know if we can perform a search
 */
gboolean
books_ready()
{
    return (g_slist_length (books_data) > 0);
}

/**
 * Public way to know if we enable at least one address book
 */
gboolean
books_active()
{
    GSList *book_list_iterator;
    book_data_t *book_data;

    if(!books_data) {
	DEBUG("Addressbook: No books data (%s:%d)", __FILE__, __LINE__);
        return FALSE;
    }

    // Iterate throw the list
    for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
            = book_list_iterator->next) {
        book_data = (book_data_t *) book_list_iterator->data;

        if (book_data->active)
            return TRUE;
    }

    // If no result
    return FALSE;
}
/**
 * Get a specific book data by UID
 */
book_data_t *
books_get_book_data_by_uid (gchar *uid)
{
    GSList *book_list_iterator;
    book_data_t *book_data;

    if(!books_data) {
	DEBUG("Addressbook: No books data (%s:%d)", __FILE__, __LINE__);
	return NULL;
    }

    DEBUG ("Addressbook: Get book data by uid: %s", uid);

    // Iterate throw the list
    for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
            = book_list_iterator->next) {
        book_data = (book_data_t *) book_list_iterator->data;

        if (strcmp (book_data->uid, uid) == 0) {
            DEBUG ("Addressbook: Book %s found", uid);
            return book_data;
        }
    }

    DEBUG ("Addressbook: Could not found Book %s", uid);
    // If no result
    return NULL;
}


/**
 * Create a query which looks for the specified string in a contact's full name, email addresses and
 * nick name.
 */
static EBookQuery*
create_query (const char* s, EBookQueryTest test, AddressBook_Config *conf)
{

    EBookQuery *equery;
    EBookQuery *queries[4];

    // Create the query
    int cpt = 0;

    // We could also use E_BOOK_QUERY_IS or E_BOOK_QUERY_BEGINS_WITH instead of E_BOOK_QUERY_CONTAINS
    queries[cpt++] = e_book_query_field_test (E_CONTACT_FULL_NAME, test, s);

    if (conf->search_phone_home)
        queries[cpt++] = e_book_query_field_test (E_CONTACT_PHONE_HOME, test, s);

    if (conf->search_phone_business)
        queries[cpt++] = e_book_query_field_test (E_CONTACT_PHONE_BUSINESS, test, s);

    if (conf->search_phone_mobile)
        queries[cpt++] = e_book_query_field_test (E_CONTACT_PHONE_MOBILE, test, s);

    equery = e_book_query_or (cpt, queries, TRUE);

    return equery;
}

/**
 * Retrieve the contact's picture
 */
static GdkPixbuf*
pixbuf_from_contact (EContact *contact)
{

    GdkPixbuf *pixbuf = NULL;
    EContactPhoto *photo = e_contact_get (contact, E_CONTACT_PHOTO);

    if (photo) {
        GdkPixbufLoader *loader;

        loader = gdk_pixbuf_loader_new();

        if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
            if (gdk_pixbuf_loader_write (loader,
                                         (guchar *) photo->data.inlined.data, photo->data.inlined.length,
                                         NULL))
                pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
        }

        // If pixbuf has been found, check size and resize if needed
        if (pixbuf) {
            GdkPixbuf *tmp;
            gint width = gdk_pixbuf_get_width (pixbuf);
            gint height = gdk_pixbuf_get_height (pixbuf);
            double scale = 1.0;

            if (height > width) {
                scale = pixbuf_size / (double) height;
            } else {
                scale = pixbuf_size / (double) width;
            }

            if (scale < 1.0) {
                tmp = gdk_pixbuf_scale_simple (pixbuf, width * scale, height
                                               * scale, GDK_INTERP_BILINEAR);
                g_object_unref (pixbuf);
                pixbuf = tmp;
            }
        }

        e_contact_photo_free (photo);
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

    DEBUG ("Addressbook: View finish");

    if (book_view != NULL)
        g_object_unref (book_view);

    // Call display callback
    had_handler (had_hits, had_user_data);
}


/**
 * Callback called after a contact have been found in EDS by search_async_by_contacts.
 */
static void
eds_query_result_cb (EBook *book, EBookStatus status, GList *contacts, gpointer user_data)
{

    DEBUG ("Addressbook: Search Result callback called");

    if (status != E_BOOK_ERROR_OK)
        ERROR ("Addressbook: Error: Status not OK on search callback");

    Search_Handler_And_Data *had = (Search_Handler_And_Data *) user_data;

    if (!contacts) {
        DEBUG ("Addressbook: No contact found");
        had->search_handler (NULL, user_data);
        return;
    }

    GList *l = NULL;

    if (status == E_BOOK_ERROR_OK) {

        gchar *number;

        // make sure we have a new list of hits
        had->hits = NULL;

        l = contacts;

        while (l) {

            Hit *hit = g_new (Hit, 1);

            if (hit) {

                // Get the photo contact
                hit->photo = pixbuf_from_contact (E_CONTACT (l->data));

                // Get business phone information
                fetch_information_from_contact (E_CONTACT (l->data), E_CONTACT_PHONE_BUSINESS, &number);
                hit->phone_business = g_strdup (number);

                // Get home phone information
                fetch_information_from_contact (E_CONTACT (l->data), E_CONTACT_PHONE_HOME, &number);
                hit->phone_home = g_strdup (number);

                // Get mobile phone information
                fetch_information_from_contact (E_CONTACT (l->data), E_CONTACT_PHONE_MOBILE, &number);
                hit->phone_mobile = g_strdup (number);

                hit->name = g_strdup ( (char *) e_contact_get_const (E_CONTACT (l->data), E_CONTACT_NAME_OR_ORG));

                if (!hit->name)
                    hit->name = "";

                /*
                        DEBUG ("Addressbook: Contact Found");
                        DEBUG ("Addressbook:     Full Name %s", hit->name);
                        DEBUG ("Addressbook:     Phone Home %s", hit->phone_home);
                        DEBUG ("Addressbook:     Phone Business %s", hit->phone_business);
                        DEBUG ("Addressbook:     Phone Mobile %s", hit->phone_mobile);
                */

                if (hit)
                    had->hits = g_list_append (had->hits, hit);


                // DEBUG ("Addressbook: max_result_remaining %d", had->max_results_remaining);
                had->max_results_remaining--;

                if (had->max_results_remaining <= 0)
                    break;
            }

            l = g_list_next (l);

        }

        view_finish_callback (NULL, had);
    }

    g_object_unref (book);

}



/**
 * Callback for asynchronous open of books
 */
static void
eds_async_open_callback (EBook *book, EBookStatus status, gpointer closure)
{
    DEBUG ("Addressbook: Open book callback");

    Search_Handler_And_Data *had = (Search_Handler_And_Data *) closure;

    if (status == E_BOOK_ERROR_OK) {

        if (!e_book_is_opened (book)) {
            // We must open the addressbook
            e_book_open (book, FALSE, NULL);
        }

        if (e_book_async_get_contacts (book, had->equery, eds_query_result_cb, had))
            ERROR ("Addressbook: Error: While querying addressbook");

    } else {
        WARN ("Addressbook: Got error when opening book");
        gchar *state_string = NULL;

        switch (status) {
            case E_BOOK_ERROR_INVALID_ARG :
                state_string = g_strdup ("E_BOOK_ERROR_INVALID_ARG");
                break;
            case E_BOOK_ERROR_BUSY :
                state_string = g_strdup ("E_BOOK_ERROR_BUSY");
                break;
            case E_BOOK_ERROR_REPOSITORY_OFFLINE :
                state_string = g_strdup ("E_BOOK_ERROR_REPOSITORY_OFFLINE");
                break;
            case E_BOOK_ERROR_NO_SUCH_BOOK :
                state_string = g_strdup ("E_BOOK_ERROR_NO_SUCH_BOOK");
                break;
            case E_BOOK_ERROR_NO_SELF_CONTACT :
                state_string = g_strdup ("E_BOOK_ERROR_NO_SELF_CONTACT");
                break;
            case E_BOOK_ERROR_SOURCE_NOT_LOADED:
                state_string = g_strdup ("E_BOOK_ERROR_SOURCE_NOT_LOADED");
                break;
            case E_BOOK_ERROR_SOURCE_ALREADY_LOADED :
                state_string = g_strdup ("E_BOOK_ERROR_SOURCE_ALREADY_LOADED");
                break;
            case E_BOOK_ERROR_PERMISSION_DENIED :
                state_string = g_strdup ("E_BOOK_ERROR_PERMISSION_DENIED");
                break;
            case E_BOOK_ERROR_CONTACT_NOT_FOUND :
                state_string = g_strdup ("E_BOOK_ERROR_CONTACT_NOT_FOUND");
                break;
            case E_BOOK_ERROR_CONTACT_ID_ALREADY_EXISTS :
                state_string = g_strdup ("E_BOOK_ERROR_CONTACT_ID_ALREADY_EXISTS");
                break;
            case E_BOOK_ERROR_PROTOCOL_NOT_SUPPORTED :
                state_string = g_strdup ("E_BOOK_ERROR_PROTOCOL_NOT_SUPPORTED");
                break;
            case E_BOOK_ERROR_CANCELLED :
                state_string = g_strdup ("E_BOOK_ERROR_CANCELLED");
                break;
            case E_BOOK_ERROR_COULD_NOT_CANCEL :
                state_string = g_strdup ("E_BOOK_ERROR_COULD_NOT_CANCEL");
                break;
            case E_BOOK_ERROR_AUTHENTICATION_FAILED :
                state_string = g_strdup ("E_BOOK_ERROR_AUTHENTICATION_FAILED");
                break;
            case E_BOOK_ERROR_AUTHENTICATION_REQUIRED :
                state_string = g_strdup ("E_BOOK_ERROR_AUTHENTICATION_REQUIRED");
                break;
            case E_BOOK_ERROR_TLS_NOT_AVAILABLE :
                state_string = g_strdup ("E_BOOK_ERROR_TLS_NOT_AVAILABLE");
                break;
            case E_BOOK_ERROR_CORBA_EXCEPTION :
                state_string = g_strdup ("E_BOOK_ERROR_CORBA_EXCEPTION");
                break;
            case E_BOOK_ERROR_NO_SUCH_SOURCE :
                state_string = g_strdup ("E_BOOK_ERROR_NO_SUCH_SOURCE");
                break;
            case E_BOOK_ERROR_OFFLINE_UNAVAILABLE :
                state_string = g_strdup ("E_BOOK_ERROR_OFFLINE_UNAVAILABLE");
                break;
            case E_BOOK_ERROR_OTHER_ERROR :
                state_string = g_strdup ("E_BOOK_ERROR_OTHER_ERROR");
                break;
            case E_BOOK_ERROR_INVALID_SERVER_VERSION :
                state_string = g_strdup ("E_BOOK_ERROR_INVALID_SERVER_VERSION");
                break;
            default:
                break;

        }

        ERROR ("%s", state_string);

        g_free (state_string);
    }

}

/**
 * Initialize address book
 */
void
init ()
{
    GError *err = NULL;
    gchar *absuri, *reluri;

    EBook *default_addressbook = e_book_new_default_addressbook (&err);

    if (err)
        ERROR ("Addressbook: Error: Could not create new book from source: %s", err->message);

    ESource *default_source = e_book_get_source (default_addressbook);

    // DEBUG ("Addressbook: Default source relative uri %s", e_source_peek_relative_uri (default_source));
    // DEBUG ("Addressbook: Default source absolute uri %s", e_source_peek_absolute_uri (default_source));

    if (current_uri) {
        g_free (current_uri);
        current_uri = NULL;
    }

    if (current_uid) {
        g_free (current_uid);
        current_uid = NULL;
    }

    if (strcmp (current_name, "Default") != 0) {
        g_free (current_name);
        current_name = NULL;
    }

    // TODO: This should work but return a NULL pointer ...
    // if (! (group = e_source_peek_group (default_source)));

    // ERROR ("Addressbook: Error: No group found for default addressbook");

    absuri = g_strdup (e_source_peek_absolute_uri (default_source));
    // absuri = g_strdup (e_source_group_peek_base_uri (group));
    reluri = g_strdup (e_source_peek_relative_uri (default_source));

    if (!absuri) {
        absuri = g_malloc (1);
        *absuri = 0;
    }

    if (!reluri) {
        reluri = g_malloc (1);
        *reluri = 0;
    }

    // Do not overwrite current_name for default
    // current_name = g_strdup (e_source_peek_name (default_source));

    current_uid = g_strdup (e_source_peek_uid (default_source));

    if (strcmp (absuri+strlen (absuri)-1, "/") == 0)
        current_uri = g_strjoin ("", absuri, reluri, NULL);
    else
        current_uri = g_strjoin ("/", absuri, reluri, NULL);

    g_free (absuri);
    g_free (reluri);
}


/**
 * Fill book data
 */
void
fill_books_data ()
{
    GSList *list, *l;
    ESourceList *source_list = NULL;
    remaining_books_to_open = 0;
    books_data = NULL;

    source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");

    if (source_list == NULL) {
        DEBUG ("Addressbook: Error could not initialize source list for addressbook (%s:%d)", __FILE__, __LINE__);
        return;
    }

    list = e_source_list_peek_groups (source_list);

    if (!list) {
        DEBUG ("Addressbook: Address Book source groups are missing (%s:%d)! Check your GConf setup.", __FILE__, __LINE__);
        return;
    }


    for (l = list; l != NULL; l = l->next) {

        ESourceGroup *group = l->data;
        GSList *sources = NULL, *m;
        gchar *absuri;

        absuri = g_strdup (e_source_group_peek_base_uri (group));

        sources = e_source_group_peek_sources (group);

        for (m = sources; m != NULL; m = m->next) {

            ESource *source = m->data;

            book_data_t *book_data = g_new (book_data_t, 1);
            book_data->active = FALSE;
            book_data->name = g_strdup (e_source_peek_name (source));
            book_data->uid = g_strdup (e_source_peek_uid (source));

            const gchar *property_name = "default";
            const gchar *prop = e_source_get_property (source, property_name);

            if (prop)
                if (strcmp (prop, "true") == 0)
                    book_data->isdefault = TRUE;
                else
                    book_data->isdefault = FALSE;
            else
                book_data->isdefault = FALSE;

            if (strcmp (absuri+strlen (absuri)-1, "/") == 0)
                book_data->uri = g_strjoin ("", absuri, e_source_peek_relative_uri (source), NULL);
            else
                book_data->uri = g_strjoin ("/", absuri, e_source_peek_relative_uri (source), NULL);

            books_data = g_slist_prepend (books_data, book_data);

        }

        g_free (absuri);
    }

    g_object_unref (source_list);
}

void
search_async_by_contacts (const char *query, int max_results, SearchAsyncHandler handler, gpointer user_data)
{
    GError *err = NULL;
    EBook *book;

    DEBUG ("Addressbook: New search by contacts: %s, max_results %d", query, max_results);

    if (strlen (query) < 1) {
        DEBUG ("Addressbook: Query is empty");
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

    if (!current_uri)
        ERROR ("Addressbook: Error: Current addressbook uri not specified uri");


    DEBUG ("Addressbook: Opening addressbook: %s", current_uri);
    DEBUG ("Addressbook: Opening addressbook: %s", current_name);

    // TODO: This hack is necessary as we cannot access group's base_uri of the default book
    // see init()

    if (strcmp (current_name, "Default") == 0)
        book = e_book_new_default_addressbook (&err);
    else
        book = e_book_new_from_uri (current_uri, &err);

    if (err)
        ERROR ("Addressbook: Error: Could not open new book: %s", err->message);

    if (book) {
        DEBUG ("Addressbook: Created empty book successfully");

        // Asynchronous open
        e_book_async_open (book, TRUE, eds_async_open_callback, had);
    } else
        ERROR ("Addressbook: Error: No book available");


}

/**
 * Fetch information for a specific contact
 */
void
fetch_information_from_contact (EContact *contact, EContactField field, gchar **info)
{
    gchar *to_fetch;

    to_fetch = g_strdup ( (char*) e_contact_get_const (contact, field));

    if (!to_fetch) {
        to_fetch = g_strdup (EMPTY_ENTRY);
    }

    *info = g_strdup (to_fetch);
}

void
set_current_addressbook (const gchar *name)
{

    GSList *book_list_iterator;
    book_data_t *book_data;

    if(!books_data) {
        DEBUG("Addressbook: No books data (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Iterate throw the list
    for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
            = book_list_iterator->next) {
        book_data = (book_data_t *) book_list_iterator->data;

        if (strcmp (book_data->name, name) == 0) {
            current_uri = book_data->uri;
            current_uid = book_data->uid;
            current_name = book_data->name;
        }
    }
}


const gchar *
get_current_addressbook (void)
{
    return current_name;
}


void
set_current_addressbook_test (EBookQueryTest test)
{
    current_test = test;
}

EBookQueryTest
get_current_addressbook_test (void)
{
    return current_test;
}
