/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#ifndef __EDS_H__
#define __EDS_H__

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libedataserver/eds-version.h>

#if EDS_CHECK_VERSION(3,5,3)
#include <libebook/libebook.h>
#else
#include <libebook/e-book.h>
#include <libebook/e-book-query.h>
#endif

#include "addressbook.h"


G_BEGIN_DECLS

/**
 * Template callback function for the asynchronous search
 */
typedef void
(* SearchAsyncHandler)(GList *hits, gpointer user_data);

/**
 * Initialize the address book.
 * Connection to evolution data server
 */
void
init_eds();

/**
 * Fill list of addressbooks
 */
void
fill_books_data(void);

/**
 * Asynchronous query to EDS using get contact method.
 */
void
search_async_by_contacts(const char *query, int max_results, SearchAsyncHandler handler, gpointer user_data);

GSList*
get_books(void);

book_data_t *
books_get_book_data_by_uid(gchar *uid);

/**
 * Public way to know if we can perform a search
 */
gboolean
books_ready();

/**
 * Public way to know if we enabled an address book
 */
gboolean
books_active();

/**
 * Good method to retrieve books_data (handle async)
 */
GSList *
addressbook_get_books_data();

/**
 * Set the current address book
 */
void
set_current_addressbook(const gchar *name);

void
set_current_addressbook_test(EBookQueryTest test);

GSList *
get_books_data();

void
determine_default_addressbook();


G_END_DECLS

#endif /* __EDS_H__ */
