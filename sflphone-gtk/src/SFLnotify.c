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

#include <SFLnotify.h>

static NotifyNotification *notification;

void
notify_incoming_call( call_t* c  )
{
  GdkPixbuf *pixbuf;
  gchar* callerid;
  gchar* title;
  notify_init("sflphone");

  title = g_markup_printf_escaped(_("%s account: %s") , 
				  g_hash_table_lookup(account_list_get_by_id(c->accountID)->properties , ACCOUNT_TYPE) , 
				  g_hash_table_lookup(account_list_get_by_id(c->accountID)->properties , ACCOUNT_ALIAS) ) ;
  callerid = g_markup_printf_escaped(_("<i>From:</i> %s") , c->from);
  

  //pixbuf = gdk_pixbuf_new_from_file(ICON_DIR "/sflphone.png", NULL);
  pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);

  notification = notify_notification_new( title, 
					  callerid,
					  NULL,
					  NULL);
  notify_notification_set_urgency( notification , NOTIFY_URGENCY_NORMAL );
  notify_notification_set_icon_from_pixbuf (notification, pixbuf);
  notify_notification_attach_to_status_icon( notification , get_status_icon() );
  notify_notification_set_timeout( notification , (( g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER ));
  g_object_set_data( G_OBJECT( notification ) , "call" , c );
  notify_notification_add_action( notification , "accept" , _("Accept") , (NotifyActionCallback) answer_call_cb , NULL,  NULL );
  notify_notification_add_action( notification , "refuse" , _("Refuse") , (NotifyActionCallback) refuse_call_cb , NULL , NULL );
  notify_notification_add_action( notification , "ignore" , _("Ignore") , (NotifyActionCallback) ignore_call_cb , NULL , NULL );

  if (!notify_notification_show (notification, NULL)) {
	g_print("notify(), failed to send notification\n");
  }
}

void
answer_call_cb( NotifyNotification *notification, gchar *action, gpointer data  )
{
  call_t* c = (call_t*)g_object_get_data( G_OBJECT( notification ) , "call" );
  dbus_accept(c);
#ifdef __POPUP_WINDOW
  status_icon_unminimize();
#endif
  g_object_unref( notification );
}

void
refuse_call_cb( NotifyNotification *notification, gchar *action, gpointer data )
{
  call_t* c = (call_t*)g_object_get_data( G_OBJECT( notification ) , "call" );
  dbus_refuse(c);
  g_object_unref( notification );
}

void
ignore_call_cb( NotifyNotification *notification, gchar *action, gpointer data )
{
  call_t* c = (call_t*)g_object_get_data( G_OBJECT( notification ) , "call" );
  g_object_unref( notification );
}

