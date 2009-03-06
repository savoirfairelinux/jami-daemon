/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */

#ifndef __EDS_H__
#define __EDS_H__

#include <glib/gtypes.h>
#include <libebook/e-book.h>

#define EMPTY_ENTRY     "empty"

G_BEGIN_DECLS

typedef struct _Hit
{
  gchar *name;
  gchar *phone_business;
  gchar *phone_home;
  gchar *phone_mobile;
} Hit;

void free_hit (Hit *h);

typedef void (* SearchAsyncHandler) (GList *hits, gpointer user_data);

void init (void);

void search_async (const char         *query,
                   int                 max_results,
                   SearchAsyncHandler  handler,
                   gpointer            user_data);

GList * search_sync (const char *query, int max_results);

void fetch_information_from_contact (EContact *contact, EContactField field, gchar **info);

G_END_DECLS

#endif /* __EDS_H__ */
