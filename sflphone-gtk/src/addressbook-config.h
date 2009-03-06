/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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

#ifndef _ADDRESS_BOOK_CONFIG
#define _ADDRESS_BOOK_CONFIG

#include <gtk/gtk.h>
#include <glib/gtypes.h>

#include "actions.h"

G_BEGIN_DECLS

typedef struct _AddressBook_Config {
    guint max_results;
    guint display_contact_photo;
} AddressBook_Config;

void set_addressbook_config (AddressBook_Config);

AddressBook_Config get_addressbook_config (void);

GtkWidget* create_addressbook_settings ();

G_END_DECLS

#endif // _ADDRESS_BOOK_CONFIG

