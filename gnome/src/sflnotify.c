/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "config.h"
#include <glib.h>
#include <glib/gi18n.h>
#include "eel-gconf-extensions.h"
#include "sflnotify.h"
#include "logger.h"

#if defined(NOTIFY_CHECK_VERSION)
#define USE_NOTIFY NOTIFY_CHECK_VERSION(0,7,2)
#endif

#if USE_NOTIFY
typedef struct {
    NotifyNotification *notification;
    gchar *title;
    gchar *body;
    GdkPixbuf *icon;
} GnomeNotification;
#endif

static void
create_new_gnome_notification(gchar *title, gchar *body, NotifyUrgency urgency, gint timeout)
{
#if USE_NOTIFY
    GnomeNotification notif;

    if (eel_gconf_get_integer(NOTIFY_ALL)) {
        notify_init("SFLphone");

        // Set struct fields
        notif.notification = notify_notification_new(title, body, NULL);
        notif.icon = gdk_pixbuf_new_from_file(LOGO_SMALL, NULL);

        notify_notification_set_urgency(notif.notification, urgency);

        if (notif.icon != NULL)
            notify_notification_set_icon_from_pixbuf(notif.notification, notif.icon);
        else
            ERROR("notify(), cannot load notification icon");

        notify_notification_set_timeout(notif.notification, timeout);

        if (!notify_notification_show(notif.notification, NULL)) {
            ERROR("notify(), failed to send notification");
        }
    }

    g_free(title);
    g_free(body);
#endif
}

void
notify_incoming_message(const gchar *callID, const gchar *msg)
{
#if USE_NOTIFY
    gchar* title = g_markup_printf_escaped(_("%s says:"), callID);

    create_new_gnome_notification(title,
                                  (gchar *)msg,
                                  NOTIFY_URGENCY_CRITICAL,
                                  (g_strcasecmp(__TIMEOUT_MODE, "default") == 0) ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER);
#endif
}

void
notify_incoming_call(callable_obj_t* c)
{
#if USE_NOTIFY
    gchar* title;

    if (strlen(c->_accountID) == 0)
        title = g_markup_printf_escaped("IP-to-IP call");
    else {
        title = g_markup_printf_escaped(_("%s account : %s") ,
                                        (gchar*) g_hash_table_lookup(account_list_get_by_id(c->_accountID)->properties , ACCOUNT_TYPE) ,
                                        (gchar*) g_hash_table_lookup(account_list_get_by_id(c->_accountID)->properties , ACCOUNT_ALIAS)) ;
    }

    gchar *callerid = g_markup_printf_escaped(_("<i>From</i> %s"), c->_peer_number);

    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  (g_strcasecmp(__TIMEOUT_MODE, "default") == 0) ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER);
#endif
}

void
notify_voice_mails(guint count, account_t* acc)
{
#if USE_NOTIFY
    // the account is different from NULL
    gchar *title = g_markup_printf_escaped(_("%s account : %s") ,
                                           (gchar*) g_hash_table_lookup(acc->properties , ACCOUNT_TYPE) ,
                                           (gchar*) g_hash_table_lookup(acc->properties , ACCOUNT_ALIAS)) ;
    gchar *body = g_markup_printf_escaped(n_("%d voice mail", "%d voice mails", count), count);

    create_new_gnome_notification(title,
                                  body,
                                  NOTIFY_URGENCY_LOW,
                                  NOTIFY_EXPIRES_DEFAULT);
#endif
}

void
notify_current_account(account_t* acc)
{
#if USE_NOTIFY
    // the account is different from NULL
    gchar *body = g_markup_printf_escaped(_("Calling with %s account <i>%s</i>"),
                                          (gchar*) g_hash_table_lookup(acc->properties, ACCOUNT_TYPE) ,
                                          (gchar*) g_hash_table_lookup(acc->properties, ACCOUNT_ALIAS));

    gchar *title = g_markup_printf_escaped(_("Current account"));

    create_new_gnome_notification(title, body, NOTIFY_URGENCY_NORMAL,
                                  NOTIFY_EXPIRES_DEFAULT);
#endif
}

void
notify_no_accounts()
{
#if USE_NOTIFY
    gchar *body = g_markup_printf_escaped(_("You have no accounts set up"));
    gchar *title = g_markup_printf_escaped(_("Error"));

    create_new_gnome_notification(title, body, NOTIFY_URGENCY_CRITICAL,
                                  NOTIFY_EXPIRES_DEFAULT);
#endif
}


void
notify_no_registered_accounts()
{
#if USE_NOTIFY
    gchar *body = g_markup_printf_escaped(_("You have no registered accounts"));
    gchar *title = g_markup_printf_escaped(_("Error"));

    create_new_gnome_notification(title, body, NOTIFY_URGENCY_CRITICAL,
                                  NOTIFY_EXPIRES_DEFAULT);
#endif
}


void
notify_secure_on(callable_obj_t* c)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped("Secure mode on.");
    gchar *callerid = g_markup_printf_escaped(_("<i>With:</i> %s \nusing %s") , c->_peer_number, c->_srtp_cipher);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  (g_strcasecmp(__TIMEOUT_MODE, "default") == 0) ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER);
#endif
}

void
notify_zrtp_not_supported(callable_obj_t* c)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped("ZRTP Error.");
    gchar *callerid = g_markup_printf_escaped(_("%s does not support ZRTP.") , c->_peer_number);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  (g_strcasecmp(__TIMEOUT_MODE, "default") == 0) ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER);
#endif
}

void
notify_zrtp_negotiation_failed(callable_obj_t* c)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped("ZRTP Error.");
    gchar *callerid = g_markup_printf_escaped(_("ZRTP negotiation failed with %s"), c->_peer_number);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  (g_strcasecmp(__TIMEOUT_MODE, "default") == 0) ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER);
#endif
}

void
notify_secure_off(callable_obj_t* c)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped("Secure mode is off.");
    gchar *callerid = g_markup_printf_escaped(_("<i>With:</i> %s"), c->_peer_number);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  (g_strcasecmp(__TIMEOUT_MODE, "default") == 0) ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER);
#endif
}
