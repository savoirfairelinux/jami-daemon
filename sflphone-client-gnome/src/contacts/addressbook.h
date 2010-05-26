/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

/**
 * This file contains functions specific for addressbook.
 * It is used as a "mapping" between real search implementation
 * and search bar.
 */

#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <gtk/gtk.h>
#include <addressbook/eds.h>

/**
 * Return addressbook state
 */
gboolean
addressbook_is_ready();

/**
 * Return addressbook state
 */
gboolean
addressbook_is_enabled();

/**
 * Return if at least one addressbook is active
 */
gboolean
addressbook_is_active();


/**
 * Perform a search in addressbook
 */
void
addressbook_search(GtkEntry*);

/**
 * Initialize addressbook
 */
void
addressbook_init();

#endif
