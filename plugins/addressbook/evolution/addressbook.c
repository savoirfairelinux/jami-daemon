/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "eds.h"
#include "addressbook.h"

/**
 * Perform a search on address book
 */
void
addressbook_search (void (*search_cb)(GList *, gpointer), GtkEntry* entry, AddressBook_Config *addressbook_config)
{
    const gint max_results = addressbook_config ? addressbook_config->max_results : 10;
    search_async_by_contacts (gtk_entry_get_text(entry),
        max_results,
        search_cb,
        addressbook_config);
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
 * Good method to get books_data
 */
GSList *
addressbook_get_books_data()
{
    determine_default_addressbook();

    return get_books_data();
}

book_data_t *
addressbook_get_book_data_by_uid(gchar *uid)
{
    return books_get_book_data_by_uid(uid);
}

/**
 * Initialize books.
 * Set active/inactive status depending on config.
 */
void
addressbook_init()
{
    fill_books_data();
    determine_default_addressbook();

    // Call books initialization
    init_eds();
}

void addressbook_set_search_type(AddrbookSearchType searchType) {
    if (searchType > ABOOK_QUERY_CONTAINS)
        return;

    static const EBookQueryTest map[] = {
        [ABOOK_QUERY_IS]            = E_BOOK_QUERY_IS,
        [ABOOK_QUERY_BEGINS_WITH]   = E_BOOK_QUERY_BEGINS_WITH,
        [ABOOK_QUERY_CONTAINS]      = E_BOOK_QUERY_CONTAINS
    };

    set_current_addressbook_test(map[searchType]);
}

void addressbook_set_current_book(const gchar *current) {
    set_current_addressbook(current);
}
