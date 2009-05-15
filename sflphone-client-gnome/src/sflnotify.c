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

#include <sflnotify.h>

static NotifyNotification *notification;

    void
notify_incoming_call( call_t* c  )
{
    if( dbus_get_notify()){

        GdkPixbuf *pixbuf;
        gchar* callerid;
        gchar* title;
        notify_init("sflphone");

        if (g_strcasecmp (c->accountID,"") == 0) {
            title = g_markup_printf_escaped ("IP-to-IP call");
        }
        else {
            title = g_markup_printf_escaped(_("%s account: %s") ,
                    (gchar*)g_hash_table_lookup(account_list_get_by_id(c->accountID)->properties , ACCOUNT_TYPE) ,
                    (gchar*)g_hash_table_lookup(account_list_get_by_id(c->accountID)->properties , ACCOUNT_ALIAS) ) ;
        }
        callerid = g_markup_printf_escaped(_("<i>From:</i> %s") , c->from);

        pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);

        notification = notify_notification_new( title,
                callerid,
                NULL,
                NULL);
        notify_notification_set_urgency( notification , NOTIFY_URGENCY_CRITICAL );
        notify_notification_set_icon_from_pixbuf (notification, pixbuf);
#if GTK_CHECK_VERSION(2,10,0)
        notify_notification_attach_to_status_icon( notification , get_status_icon() );
#endif
        notify_notification_set_timeout( notification , (( g_strcasecmp(__TIMEOUT_MODE, "default") == 0 )? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER ));
        g_object_set_data( G_OBJECT( notification ) , "call" , c );
        notify_notification_add_action( notification , "accept" , _("Accept") , (NotifyActionCallback) answer_call_cb , NULL,  NULL );
        notify_notification_add_action( notification , "refuse" , _("Refuse") , (NotifyActionCallback) refuse_call_cb , NULL , NULL );
        notify_notification_add_action( notification , "ignore" , _("Ignore") , (NotifyActionCallback) ignore_call_cb , NULL , NULL );

        if (!notify_notification_show (notification, NULL)) {
            ERROR("notify(), failed to send notification");
        }
    }
}
    void
answer_call_cb( NotifyNotification *notification, gpointer data  UNUSED )
{
    call_t* c = (call_t*)g_object_get_data( G_OBJECT( notification ) , "call" );
    c->history_state = INCOMING;
    calltree_update_call( history , c );
    dbus_accept(c);
#if GTK_CHECK_VERSION(2,10,0)
    if( __POPUP_WINDOW )
        status_icon_unminimize();
#endif
    g_object_unref( notification );
}

    void
refuse_call_cb( NotifyNotification *notification, gpointer data  UNUSED)
{
    call_t* c = (call_t*)g_object_get_data( G_OBJECT( notification ) , "call" );
    dbus_refuse(c);
    g_object_unref( notification );
}

    void
ignore_call_cb( NotifyNotification *notification, gpointer data  UNUSED)
{
    g_object_unref( notification );
}

    void
notify_voice_mails( guint count , account_t* acc )
{

    if( dbus_get_mail_notify()) {
        // the account is different from NULL
        GdkPixbuf *pixbuf;
        gchar* title;
        gchar* body;
        notify_init("sflphone");

        title = g_markup_printf_escaped(_("%s account: %s") ,
                (gchar*)g_hash_table_lookup(acc->properties , ACCOUNT_TYPE) ,
                (gchar*) g_hash_table_lookup(acc->properties , ACCOUNT_ALIAS) ) ;
        body = g_markup_printf_escaped(_("%d voice mails"), count);

        pixbuf = gdk_pixbuf_new_from_file(LOGO, NULL);

        notification = notify_notification_new( title,
                body,
                NULL,
                NULL);
        notify_notification_set_urgency( notification , NOTIFY_URGENCY_LOW );
        notify_notification_set_icon_from_pixbuf (notification, pixbuf);
#if GTK_CHECK_VERSION(2,10,0)
        notify_notification_attach_to_status_icon( notification , get_status_icon() );
#endif
        notify_notification_set_timeout( notification , NOTIFY_EXPIRES_DEFAULT );
        notify_notification_add_action( notification , "ignore" , _("Ignore") , (NotifyActionCallback) ignore_call_cb , NULL , NULL );

        if (!notify_notification_show (notification, NULL)) {
            ERROR("notify(), failed to send notification");
        }
    }
}

    void
notify_current_account( account_t* acc )
{

    if( dbus_get_notify()){
        // the account is different from NULL
        GdkPixbuf *pixbuf;
        gchar* title;
        gchar* body="";
        notify_init("sflphone");

        body = g_markup_printf_escaped(_("Calling with %s account <i>%s</i>") ,
                (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_TYPE) ,
                (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_ALIAS));

        title = g_markup_printf_escaped(_("Current account"));

        pixbuf = gdk_pixbuf_new_from_file(LOGO, NULL);

        notification = notify_notification_new( title,
                body,
                NULL,
                NULL);
        notify_notification_set_urgency( notification , NOTIFY_URGENCY_NORMAL );
        notify_notification_set_icon_from_pixbuf (notification, pixbuf);
#if GTK_CHECK_VERSION(2,10,0)
        notify_notification_attach_to_status_icon( notification , get_status_icon() );
#endif
        notify_notification_set_timeout( notification , NOTIFY_EXPIRES_DEFAULT );
        notify_notification_add_action( notification , "ignore" , _("Ignore") , (NotifyActionCallback) ignore_call_cb , NULL , NULL );

        if (!notify_notification_show (notification, NULL)) {
            ERROR("notify(), failed to send notification");
        }
    }
}
    void
notify_no_accounts(  )
{
    GdkPixbuf *pixbuf;
    gchar* title;
    gchar* body="";
    notify_init("sflphone");

    body = g_markup_printf_escaped(_("You haven't setup any accounts"));

    title = g_markup_printf_escaped(_("Error"));

    pixbuf = gdk_pixbuf_new_from_file(LOGO, NULL);

    notification = notify_notification_new( title,
            body,
            NULL,
            NULL);
    notify_notification_set_urgency( notification , NOTIFY_URGENCY_CRITICAL );
    notify_notification_set_icon_from_pixbuf (notification, pixbuf);
#if GTK_CHECK_VERSION(2,10,0)
    notify_notification_attach_to_status_icon( notification , get_status_icon() );
#endif
    notify_notification_set_timeout( notification , NOTIFY_EXPIRES_DEFAULT );
    //notify_notification_add_action( notification , "setup" , _("Setup Accounts") , (NotifyActionCallback) setup_accounts_cb , NULL , NULL );

    if (!notify_notification_show (notification, NULL)) {
        ERROR("notify(), failed to send notification");
    }
}

    void
setup_accounts_cb( NotifyNotification *notification, gpointer data  UNUSED)
{
    //gtk_widget_set_parent( show_accounts_window() , GTK_WIDGET(get_main_window()));
    show_accounts_window();
    g_object_unref( notification );
}

    void
notify_no_registered_accounts(  )
{
    GdkPixbuf *pixbuf;
    gchar* title;
    gchar* body="";
    notify_init("sflphone");


    body = g_markup_printf_escaped(_("You have no registered accounts"));


    title = g_markup_printf_escaped(_("Error"));

    pixbuf = gdk_pixbuf_new_from_file(LOGO, NULL);

    notification = notify_notification_new( title,
            body,
            NULL,
            NULL);
    notify_notification_set_urgency( notification , NOTIFY_URGENCY_CRITICAL );
    notify_notification_set_icon_from_pixbuf (notification, pixbuf);
#if GTK_CHECK_VERSION(2,10,0)
    notify_notification_attach_to_status_icon( notification , get_status_icon() );
#endif
    notify_notification_set_timeout( notification , NOTIFY_EXPIRES_DEFAULT );
    //notify_notification_add_action( notification , "setup" , _("Setup Accounts") , (NotifyActionCallback) setup_accounts_cb , NULL , NULL );

  if (!notify_notification_show (notification, NULL)) {
    ERROR("notify(), failed to send notification");
  }

}

    void
stop_notification( void )
{
    if( notification != NULL )
    {
        if(notify_notification_show( notification , NULL))
        {
            notify_notification_close( notification , NULL);
            g_object_unref( notification );
            notification = NULL;
        }
        notification = NULL;
    }
}
