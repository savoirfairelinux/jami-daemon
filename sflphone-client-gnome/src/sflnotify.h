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

#ifndef __GNOME_NOTIFICATION_H__
#define __GNOME_NOTIFICATION_H__

/** @file sflnotify.h
  * @brief Implements desktop notification for incoming events.
  */

#include <libnotify/notify.h>
#include <accountlist.h>
#include <calllist.h>
#include <dbus/dbus.h>
#include <statusicon.h>
#include <sflphone_const.h>

G_BEGIN_DECLS

typedef struct {
    NotifyNotification *notification;
    gchar *title;
    gchar *body;
    GdkPixbuf *icon;
} GnomeNotification;

void create_new_gnome_notification (gchar *title, gchar *body, NotifyUrgency urgency, gint timeout, GnomeNotification **notif);

void free_notification (GnomeNotification *g);

/**
 * Notify an incoming call
 * A dialog box is attached to the status icon
 * @param c The incoming call
 */
void notify_incoming_call( callable_obj_t* c);

/**
 * Notify voice mails count
 * An info box is attached to the status icon
 * @param count The number of voice mails
 * @param acc The account that received the notification
 */
void notify_voice_mails( guint count , account_t* acc );

/**
 * Notify the current account used to make calls with
 * @param acc The current account
 */
void notify_current_account( account_t* acc );

/**
 * Notify that no accounts have been setup
 */
void notify_no_accounts( );

/**
 * Notify that there is no registered account
 */
void notify_no_registered_accounts(  );

/**
 * Stop and close the current notification if an action occured before the timeout
 */
void stop_notification( void );

/**
 * Notify that the RTP session is secured
 */
void notify_secure_on (callable_obj_t* c);

/**
 * Notify that the RTP session is now more secured
 */
void notify_secure_off (callable_obj_t* c);

/**
 * Notify that the ZRTP negotiation failed
 */
 
void notify_zrtp_negotiation_failed (callable_obj_t* c);

/**
 * Notify that the RTP session is now more secured
 */
void notify_zrtp_not_supported (callable_obj_t* c);

G_END_DECLS

#endif
