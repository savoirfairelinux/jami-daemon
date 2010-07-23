/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef _ADDRESS_BOOK_CONFIG
#define _ADDRESS_BOOK_CONFIG

#include <gtk/gtk.h>
#include <glib/gtypes.h>

#include "actions.h"
#include <utils.h>

G_BEGIN_DECLS

#define ADDRESSBOOK_ENABLE                  "ADDRESSBOOK_ENABLE"
#define ADDRESSBOOK_MAX_RESULTS             "ADDRESSBOOK_MAX_RESULTS"
#define ADDRESSBOOK_DISPLAY_CONTACT_PHOTO   "ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"
#define ADDRESSBOOK_DISPLAY_PHONE_BUSINESS   "ADDRESSBOOK_DISPLAY_PHONE_BUSINESS"
#define ADDRESSBOOK_DISPLAY_PHONE_HOME       "ADDRESSBOOK_DISPLAY_PHONE_HOME"
#define ADDRESSBOOK_DISPLAY_PHONE_MOBILE     "ADDRESSBOOK_DISPLAY_PHONE_MOBILE"

typedef struct _AddressBook_Config
{
	// gint64: a signed integer guaranteed to be 64 bits on all platforms
	// To print or scan values of this type, use G_GINT64_MODIFIER and/or G_GINT64_FORMAT
    gint enable;
    gint max_results;
    gint display_contact_photo;
    gint search_phone_home;
    gint search_phone_business;
    gint search_phone_mobile;
} AddressBook_Config;

/**
 * Save the parameters through D-BUS
 */
void
addressbook_config_save_parameters(void);

/**
 * Initialize the address book structure, and retrieve the saved parameters through D-Bus
 *
 * @param settings  The addressbook structure
 */
void
addressbook_config_load_parameters(AddressBook_Config **settings);

gboolean
addressbook_display(AddressBook_Config *settings, const gchar *field);

GtkWidget*
create_addressbook_settings();

G_END_DECLS

#endif // _ADDRESS_BOOK_CONFIG
