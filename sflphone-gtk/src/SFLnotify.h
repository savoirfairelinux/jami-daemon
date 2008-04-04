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

#include <libnotify/notify.h>

#include <accountlist.h>
#include <calllist.h>
#include <dbus.h>
#include <actions.h>
#include <statusicon.h>

#define __TIMEOUT_MODE	"default"			
#define __TIMEOUT_TIME	30000	    // 30 secondes			
#define	__POPUP_WINDOW  ( dbus_popup_mode() ) 	

/*
 * Notify an incoming call with the libnotify notification library
 * A dialog box appears near the status icon
 * @param c The incoming call
 */
void notify_incoming_call( call_t* c);

void notify_voice_mails( guint count , account_t* acc );

void notify_registered_accounts();
/*
 * Callback when answer button is pressed. 
 * Action: Pick up the incoming call 
 * @param notification	The pointer on the notification structure
 * @param data	The data associated. Here: call_t*
 */
void answer_call_cb( NotifyNotification *notification, gchar *action, gpointer data );

/*
 * Callback when refuse button is pressed
 * Action: hang up the incoming call 
 * @param notification	The pointer on the notification structure
 * @param data	The data associated. Here: call_t*
 */
void refuse_call_cb( NotifyNotification *notification, gchar *action, gpointer data );

/*
 * Callback when ignore button is pressed
 * Action: nothing - The call continues ringing 
 * @param notification	The pointer on the notification structure
 * @param data	The data associated. Here: call_t*
 */
void ignore_call_cb( NotifyNotification *notification, gchar *action, gpointer data );

#endif
