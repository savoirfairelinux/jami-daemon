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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <libnotify/notify.h>
#include "sflphone_const.h"

#include "account_schema.h"
#include "str_utils.h"
#include "sflnotify.h"

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

/* XXX: This function will free its arguments */
static void
create_new_gnome_notification(gchar *title, gchar *body, NotifyUrgency urgency, gint timeout, SFLPhoneClient *client)
{
#if USE_NOTIFY
    GnomeNotification notif;

    if (g_settings_get_boolean(client->settings, "notify-all")) {
        notify_init("SFLphone");

        // Set struct fields
        notif.notification = notify_notification_new(title, body, NULL);
        notif.icon = gdk_pixbuf_new_from_file(LOGO_SMALL, NULL);

        notify_notification_set_urgency(notif.notification, urgency);

        if (notif.icon != NULL)
            notify_notification_set_icon_from_pixbuf(notif.notification, notif.icon);
        else
            g_warning("notify(), cannot load notification icon");

        notify_notification_set_timeout(notif.notification, timeout);

        if (!notify_notification_show(notif.notification, NULL)) {
            g_warning("notify(), failed to send notification");
        }
    }

    g_free(title);
    g_free(body);
#endif
}

void
notify_incoming_message(const gchar *callID, const gchar *msg, SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar* title = g_markup_printf_escaped(_("%s says:"), callID);

    create_new_gnome_notification(title,
                                  (gchar *)msg,
                                  NOTIFY_URGENCY_CRITICAL,
                                  utf8_case_equal(__TIMEOUT_MODE, "default") ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                  client);
#endif
}

void
notify_incoming_call(callable_obj_t* c, SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar* title;

    if (strlen(c->_accountID) == 0)
        title = g_markup_printf_escaped(_("IP-to-IP call"));
    else {
        account_t *account = account_list_get_by_id(c->_accountID);
        g_return_if_fail(account != NULL);

        title = g_markup_printf_escaped(_("%s account : %s") ,
                (gchar*) g_hash_table_lookup(account->properties, CONFIG_ACCOUNT_TYPE),
                (gchar*) g_hash_table_lookup(account->properties, CONFIG_ACCOUNT_ALIAS)) ;
    }

    gchar *callerid = g_markup_printf_escaped(_("<i>From</i> %s"), c->_peer_number);

    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  utf8_case_equal(__TIMEOUT_MODE, "default") ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                  client);
#endif
}

void
notify_voice_mails(guint count, account_t* acc, SFLPhoneClient *client)
{
#if USE_NOTIFY
    // the account is different from NULL
    gchar *title = g_markup_printf_escaped(_("%s account : %s") ,
                                           (gchar*) g_hash_table_lookup(acc->properties, CONFIG_ACCOUNT_TYPE) ,
                                           (gchar*) g_hash_table_lookup(acc->properties, CONFIG_ACCOUNT_ALIAS)) ;
    gchar *body = g_markup_printf_escaped(n_("%d voice mail", "%d voice mails", count), count);

    create_new_gnome_notification(title,
                                  body,
                                  NOTIFY_URGENCY_LOW,
                                  NOTIFY_EXPIRES_DEFAULT,
                                  client);
#endif
}

void
notify_current_account(account_t* acc, SFLPhoneClient *client)
{
#if USE_NOTIFY
    // the account is different from NULL
    gchar *body = g_markup_printf_escaped(_("Calling with %s account <i>%s</i>"),
                                          (gchar*) g_hash_table_lookup(acc->properties, CONFIG_ACCOUNT_TYPE) ,
                                          (gchar*) g_hash_table_lookup(acc->properties, CONFIG_ACCOUNT_ALIAS));

    gchar *title = g_markup_printf_escaped(_("Current account"));

    create_new_gnome_notification(title, body, NOTIFY_URGENCY_NORMAL,
                                  NOTIFY_EXPIRES_DEFAULT,
                                  client);
#endif
}

void
notify_no_accounts(SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar *body = g_markup_printf_escaped(_("You have no accounts set up"));
    gchar *title = g_markup_printf_escaped(_("Error"));

    create_new_gnome_notification(title, body, NOTIFY_URGENCY_CRITICAL,
                                  NOTIFY_EXPIRES_DEFAULT, client);
#endif
}


void
notify_no_registered_accounts(SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar *body = g_markup_printf_escaped(_("You have no registered accounts"));
    gchar *title = g_markup_printf_escaped(_("Error"));

    create_new_gnome_notification(title, body, NOTIFY_URGENCY_CRITICAL,
                                  NOTIFY_EXPIRES_DEFAULT, client);
#endif
}


void
notify_secure_on(callable_obj_t* c, SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped(_("Secure mode on."));
    gchar *callerid = g_markup_printf_escaped(_("<i>With:</i> %s \n") , c->_peer_number);

    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  utf8_case_equal(__TIMEOUT_MODE, "default") ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                  client);
#endif
}

void
notify_zrtp_not_supported(callable_obj_t* c, SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped(_("ZRTP Error."));
    gchar *callerid = g_markup_printf_escaped(_("%s does not support ZRTP.") , c->_peer_number);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  utf8_case_equal(__TIMEOUT_MODE, "default") ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                  client);
#endif
}

void
notify_zrtp_negotiation_failed(callable_obj_t* c, SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped(_("ZRTP Error."));
    gchar *callerid = g_markup_printf_escaped(_("ZRTP negotiation failed with %s"), c->_peer_number);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  utf8_case_equal(__TIMEOUT_MODE, "default") ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                  client);
#endif
}

void
notify_secure_off(callable_obj_t* c, SFLPhoneClient *client)
{
#if USE_NOTIFY
    gchar *title = g_markup_printf_escaped(_("Secure mode is off."));
    gchar *callerid = g_markup_printf_escaped(_("<i>With:</i> %s"), c->_peer_number);
    create_new_gnome_notification(title,
                                  callerid,
                                  NOTIFY_URGENCY_CRITICAL,
                                  utf8_case_equal(__TIMEOUT_MODE, "default") ? __TIMEOUT_TIME : NOTIFY_EXPIRES_NEVER,
                                  client);
#endif
}
