/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#ifndef _ADDRESS_BOOK_CONFIG
#define _ADDRESS_BOOK_CONFIG

#include <gtk/gtk.h>
#include <glib.h>

#include "addressbook.h"
#include "actions.h"
#include "utils.h"

G_BEGIN_DECLS

#define ADDRESSBOOK_ENABLE                  "ADDRESSBOOK_ENABLE"
#define ADDRESSBOOK_MAX_RESULTS             "ADDRESSBOOK_MAX_RESULTS"
#define ADDRESSBOOK_DISPLAY_CONTACT_PHOTO   "ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"
#define ADDRESSBOOK_DISPLAY_PHONE_BUSINESS   "ADDRESSBOOK_DISPLAY_PHONE_BUSINESS"
#define ADDRESSBOOK_DISPLAY_PHONE_HOME       "ADDRESSBOOK_DISPLAY_PHONE_HOME"
#define ADDRESSBOOK_DISPLAY_PHONE_MOBILE     "ADDRESSBOOK_DISPLAY_PHONE_MOBILE"

/**
 * Save the parameters through D-BUS
 */
void
addressbook_config_save_parameters (void);

/**
 * Return the saved parameters through D-Bus
 */
AddressBook_Config *addressbook_config_load_parameters();

gboolean
addressbook_display (AddressBook_Config *settings, const gchar *field);

GtkWidget*
create_addressbook_settings();

G_END_DECLS

#endif // _ADDRESS_BOOK_CONFIG
