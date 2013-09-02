/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef GNOME_NOTIFICATION_H_
#define GNOME_NOTIFICATION_H_

/** @file sflnotify.h
  * @brief Implements desktop notification for incoming events.
  */

#include "accountlist.h"
#include "calllist.h"
#include "sflphone_client.h"

/**
 * Notify an incoming call
 * A dialog box is attached to the status icon
 * @param c The incoming call
 */
void notify_incoming_call(callable_obj_t* c, SFLPhoneClient *client);

/**
 * Notify an incoming text message
 * A dialog box is attached to the status icon
 * @param c The incoming message
 */
void notify_incoming_message(const gchar *callID, const gchar *msg, SFLPhoneClient *client);

/**
 * Notify voice mails count
 * An info box is attached to the status icon
 * @param count The number of voice mails
 * @param acc The account that received the notification
 */
void notify_voice_mails(guint count , account_t* acc, SFLPhoneClient *client);

/**
 * Notify the current account used to make calls with
 * @param acc The current account
 */
void notify_current_account(account_t* acc, SFLPhoneClient *client);

/**
 * Notify that no accounts have been setup
 */
void notify_no_accounts(SFLPhoneClient *client);

/**
 * Notify that there is no registered account
 */
void notify_no_registered_accounts(SFLPhoneClient *client);

/**
 * Notify that the RTP session is secured
 */
void notify_secure_on(callable_obj_t* c, SFLPhoneClient *client);

/**
 * Notify that the RTP session is now more secured
 */
void notify_secure_off(callable_obj_t* c, SFLPhoneClient *client);

/**
 * Notify that the ZRTP negotiation failed
 */

void notify_zrtp_negotiation_failed(callable_obj_t* c, SFLPhoneClient *client);

/**
 * Notify that the RTP session is now more secured
 */
void notify_zrtp_not_supported(callable_obj_t* c, SFLPhoneClient *client);

#endif
