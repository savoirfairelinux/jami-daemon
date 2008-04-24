/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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


#ifndef __SFL_NOTIFY_H__
#define __SFL_NOTIFY_H__

/** @file sflnotify.h
  * @brief Implements desktop notification for incoming events.
  */

#include <libnotify/notify.h>
#include <accountlist.h>
#include <calllist.h>
#include <dbus.h>
#include <actions.h>
#include <statusicon.h>
#include <configwindow.h>
#include <sflphone_const.h>

/**
 * Notify an incoming call 
 * A dialog box is attached to the status icon
 * @param c The incoming call
 */
void notify_incoming_call( call_t* c);

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
 * Callback when answer button is pressed. 
 * Action associated: Pick up the incoming call 
 * @param notification	The pointer on the notification structure
 * @param data	The data associated. Here: call_t*
 */
void answer_call_cb( NotifyNotification *notification, gpointer data );

/**
 * Callback when refuse button is pressed
 * Action associated: Hang up the incoming call 
 * @param notification	The pointer on the notification structure
 * @param data	The data associated. Here: call_t*
 */
void refuse_call_cb( NotifyNotification *notification, gpointer data );

/**
 * Callback when ignore button is pressed
 * Action associated: Nothing - The call continues ringing 
 * @param notification	The pointer on the notification structure
 * @param data	The data associated. Here: call_t*
 */
void ignore_call_cb( NotifyNotification *notification, gpointer data );

/**
 * Callback when you try to make a call without accounts setup and 'setup account' button is clicked. 
 * Action associated: Open the account window
 * @param notification The pointer on the notification structure
 * @param data The data associated. Here: account_t*
 */
void setup_accounts_cb(NotifyNotification *notification, gpointer data);

/**
 * Stop and close the current notification if an action occured before the timeout
 */
void stop_notification( void );

#endif
