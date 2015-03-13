/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#ifndef __ACCOUNTCONFIGDIALOG_H__
#define __ACCOUNTCONFIGDIALOG_H__
/** @file accountconfigdialog.h
  * @brief The window to edit account details.
  */

#include "accountlist.h"

/**
 * Display the main account widget
 * @param a The account you want to display
 * @param client Our client instance
 * @param is_new TRUE if this account is being added
 * @return The dialog with the pertinent account information
 */
GtkWidget *
show_account_window(const gchar *accountID, GtkDialog *parent, SFLPhoneClient *client, gboolean is_new);

/*
 * @param dialog The dialog the account will be update from
 * @param a The account you want to display
 */
void update_account_from_dialog(GtkWidget *dialog, const gchar *accountID);

/*
 * @param position The position of the slider
 * @param size The size of the slider
 */
void update_ringtone_slider(guint position, guint size);

#endif
