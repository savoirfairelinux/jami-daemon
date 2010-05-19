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

#include <sflnotify.h>

GnomeNotification *_gnome_notification;

void create_new_gnome_notification (gchar *title, gchar *body, NotifyUrgency urgency, gint timeout, GnomeNotification **notif)
{
    GnomeNotification *_notif;

    if( dbus_get_notify()){

        _notif = g_new0 (GnomeNotification, 1);

        notify_init ("SFLphone");

        // Set struct fields
        _notif->notification = notify_notification_new (title, body, NULL, NULL);
        //_notif->icon = gdk_pixbuf_new_from_file_at_size (LOGO, 120, 120, NULL);
        _notif->icon = gdk_pixbuf_new_from_file (LOGO_SMALL, NULL);
#if GTK_CHECK_VERSION(2,10,0)
        notify_notification_attach_to_status_icon (_notif->notification , get_status_icon() );
#endif

        notify_notification_set_urgency (_notif->notification, urgency);
        
        if (_notif->icon != NULL)
            notify_notification_set_icon_from_pixbuf (_notif->notification, _notif->icon);
        else
            ERROR ("notify(), cannot load notification icon");

        notify_notification_set_timeout (_notif->notification, timeout);

        if (!notify_notification_show (_notif->notification, NULL)) {
            ERROR("notify(), failed to send notification");
        }

        *notif = _notif;
    }
}


    void
notify_incoming_call (callable_obj_t* c)
{

        gchar* callerid;
        gchar* title;

        if (g_strcasecmp (c->_accountID,"") == 0) {
            title = g_markup_printf_escaped ("IP-to-IP call");
        }
        else {
            title = g_markup_printf_escaped(_("%s account : %s") ,
                    (gchar*)g_hash_table_lookup(account_list_get_by_id(c->_accountID)->properties , ACCOUNT_TYPE) ,
                    (gchar*)g_hash_table_lookup(account_list_get_by_id(c->_accountID)->properties , ACCOUNT_ALIAS) ) ;
        }
        callerid = g_markup_printf_escaped(_("<i>From</i> %s"), c->_peer_number);

        create_new_gnome_notification (title,
                                        callerid, 
                                        NOTIFY_URGENCY_CRITICAL, 
                                        (g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                        &_gnome_notification); 
}

    void
notify_voice_mails (guint count, account_t* acc)
{
        // the account is different from NULL
        gchar* title;
        gchar* body;

        title = g_markup_printf_escaped(_("%s account : %s") ,
                (gchar*)g_hash_table_lookup(acc->properties , ACCOUNT_TYPE) ,
                (gchar*) g_hash_table_lookup(acc->properties , ACCOUNT_ALIAS) ) ;
        body = g_markup_printf_escaped(n_("%d voice mail", "%d voice mails", count), count);

        create_new_gnome_notification (title,
                                        body, 
                                        NOTIFY_URGENCY_LOW, 
                                        NOTIFY_EXPIRES_DEFAULT,
                                        &_gnome_notification); 
}

    void
notify_current_account (account_t* acc)
{

        // the account is different from NULL
        gchar* title;
        gchar* body="";

        body = g_markup_printf_escaped(_("Calling with %s account <i>%s</i>") ,
                (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_TYPE) ,
                (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_ALIAS));

        title = g_markup_printf_escaped(_("Current account"));

        create_new_gnome_notification (title,
                                        body, 
                                        NOTIFY_URGENCY_NORMAL, 
                                        NOTIFY_EXPIRES_DEFAULT,
                                        &_gnome_notification); 
}

    void
notify_no_accounts ()
{
    gchar* title;
    gchar* body="";

    body = g_markup_printf_escaped(_("You have no accounts set up"));
    title = g_markup_printf_escaped(_("Error"));

    create_new_gnome_notification (title,
                                    body, 
                                    NOTIFY_URGENCY_CRITICAL, 
                                    NOTIFY_EXPIRES_DEFAULT,
                                    &_gnome_notification); 
}


    void
notify_no_registered_accounts ()
{
    gchar* title;
    gchar* body="";

    body = g_markup_printf_escaped(_("You have no registered accounts"));
    title = g_markup_printf_escaped(_("Error"));

    create_new_gnome_notification (title,
                                    body, 
                                    NOTIFY_URGENCY_CRITICAL, 
                                    NOTIFY_EXPIRES_DEFAULT,
                                    &_gnome_notification); 
}

    void
stop_notification( void )
{
    /*
    if( _gnome_notification != NULL )
    {
        if(_gnome_notification->notification != NULL)
        {
            notify_notification_close (_gnome_notification->notification, NULL);
            g_object_unref(_gnome_notification->notification );
            _gnome_notification->notification = NULL;
        }
    free_notification (_gnome_notification);
    }*/
}

/**
 * Freeing a notification instance
 */
void free_notification (GnomeNotification *g)
{
  g_free(g->title);
  g_free(g->body);
  g_free(g);
}

    void
notify_secure_on (callable_obj_t* c)
{

        gchar* callerid;
        gchar* title;
        title = g_markup_printf_escaped ("Secure mode on.");
        callerid = g_markup_printf_escaped(_("<i>With:</i> %s \nusing %s") , c->_peer_number, c->_srtp_cipher);
        create_new_gnome_notification (title,
                                        callerid, 
                                        NOTIFY_URGENCY_CRITICAL, 
                                        (g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                        &_gnome_notification); 
}

    void
notify_zrtp_not_supported (callable_obj_t* c)
{

        gchar* callerid;
        gchar* title;
        title = g_markup_printf_escaped ("ZRTP Error.");
        callerid = g_markup_printf_escaped(_("%s does not support ZRTP.") , c->_peer_number);
        create_new_gnome_notification (title,
                                        callerid, 
                                        NOTIFY_URGENCY_CRITICAL, 
                                        (g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                        &_gnome_notification); 
}

    void
notify_zrtp_negotiation_failed (callable_obj_t* c)
{

        gchar* callerid;
        gchar* title;
        title = g_markup_printf_escaped ("ZRTP Error.");
        callerid = g_markup_printf_escaped(_("ZRTP negotiation failed with %s") , c->_peer_number);
        create_new_gnome_notification (title,
                                        callerid, 
                                        NOTIFY_URGENCY_CRITICAL, 
                                        (g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                        &_gnome_notification); 
}

    void
notify_secure_off (callable_obj_t* c)
{

        gchar* callerid;
        gchar* title;
        title = g_markup_printf_escaped ("Secure mode is off.");
        callerid = g_markup_printf_escaped(_("<i>With:</i> %s") , c->_peer_number);
        create_new_gnome_notification (title,
                                        callerid, 
                                        NOTIFY_URGENCY_CRITICAL, 
                                        (g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                        &_gnome_notification); 
}
