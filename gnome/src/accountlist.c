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

#include <glib/gi18n.h>
#include "str_utils.h"
#include "dbus.h"
#include "accountlist.h"
#include "account_schema.h"
#include "actions.h"

static GQueue * accountQueue;

static guint account_list_get_position(account_t *account)
{
    guint size = account_list_get_size();

    for (guint i = 0; i < size; i++) {
        account_t *tmp = account_list_get_nth(i);

        if (utf8_case_equal(tmp->accountID, account->accountID))
            return i;
    }

    // Not found
    return -1;
}

/* GCompareFunc to compare a accountID (gchar* and a account_t) */
static gint is_accountID_struct(gconstpointer a, gconstpointer b)
{
    if (!a || !b)
        return 1;

    account_t * c = (account_t*) a;

    /* We only want it to return 0 or 1...0 is a match, 1 is not */
    return !!(g_strcmp0(c->accountID, (gchar*) b));
}

/* GCompareFunc to compare an alias (gchar* and a account_t) */
static gint is_alias_struct(gconstpointer a, gconstpointer b)
{
    if (!a || !b)
        return 1;

    account_t * c = (account_t*) a;

    /* We only want it to return 0 or 1...0 is a match, 1 is not */
    return !!(g_strcmp0(account_lookup(c, CONFIG_ACCOUNT_ALIAS), (gchar*) b));
}

/* GCompareFunc to get current call (gchar* and a account_t) */
static gint get_state_struct(gconstpointer a, gconstpointer b)
{
    account_t * c = (account_t*) a;

    return !(c->state == *((account_state_t*) b));
}

void account_list_init()
{
    account_list_free();
    accountQueue = g_queue_new();
}

void
account_list_add(account_t * c)
{
    g_queue_push_tail(accountQueue, (gpointer) c);
}

account_t *
account_list_get_by_state(account_state_t state)
{
    GList * c = g_queue_find_custom(accountQueue, &state, get_state_struct);

    if (c)
        return (account_t *) c->data;
    else
        return NULL;
}

account_t *
account_list_get_by_id(const gchar * const accountID)
{
    if (!accountID) {
        g_debug("AccountID is NULL");
        return NULL;
    }

    GList * c = g_queue_find_custom(accountQueue, accountID, is_accountID_struct);

    if (c)
        return (account_t *) c->data;
    else
        return NULL;
}

account_t *
account_list_get_by_alias(const gchar * const alias)
{
    if (!alias) {
        g_debug("Account alias is NULL");
        return NULL;
    }

    GList * c = g_queue_find_custom(accountQueue, alias, is_alias_struct);

    if (c)
        return (account_t *) c->data;
    else
        return NULL;
}

guint account_list_get_size(void)
{
    return g_queue_get_length(accountQueue);
}

account_t * account_list_get_nth(guint n)
{
    return g_queue_peek_nth(accountQueue, n);
}

account_t*
account_list_get_current()
{
    // No account registered
    if (account_list_get_registered_accounts() == 0)
        return NULL;

    // if we are here, it means that we have at least one registered account in the list
    // So we get the first one
    return account_list_get_by_state(ACCOUNT_STATE_REGISTERED);
}

void account_list_set_current(account_t *current)
{
    // 2 steps:
    // 1 - retrieve the index of the current account in the Queue
    // 2 - then set it as first
    guint pos = account_list_get_position(current);

    if (pos > 0) {
        gpointer acc = g_queue_pop_nth(accountQueue, pos);
        g_queue_push_nth(accountQueue, acc, 0);
    }
}


const gchar * account_state_name(account_state_t s)
{
    switch (s) {
        case ACCOUNT_STATE_UNREGISTERED:
            return _("Not Registered");
        case ACCOUNT_STATE_TRYING:
            return _("Trying...");
        case ACCOUNT_STATE_REGISTERED:
            return _("Registered");
        case ACCOUNT_STATE_ERROR:
            return _("Error");
        case ACCOUNT_STATE_ERROR_AUTH:
            return _("Authentication Failed");
        case ACCOUNT_STATE_ERROR_NETWORK:
            return _("Network unreachable");
        case ACCOUNT_STATE_ERROR_HOST:
            return _("Host unreachable");
        case ACCOUNT_STATE_ERROR_SERVICE_UNAVAILABLE:
            return _("Service unavailable");
        case ACCOUNT_STATE_ERROR_EXIST_STUN:
            return _("Stun server invalid");
        case ACCOUNT_STATE_ERROR_NOT_ACCEPTABLE:
            return _("Not acceptable");
        case ACCOUNT_STATE_IP2IP_READY:
            return _("Ready");
        default:
            g_warning("Unexpected state %d", s);
            return _("Invalid");
    }
}

void account_list_free_elm(gpointer elm, G_GNUC_UNUSED gpointer data)
{
    account_t *a = elm;
    g_free(a->accountID);
    a->accountID = NULL;
    g_free(a);
}

void account_list_free()
{
    if (accountQueue) {
        g_queue_foreach(accountQueue, account_list_free_elm, NULL);
        g_queue_free(accountQueue);
        accountQueue = NULL;
    }
}

void
account_list_move_up(guint account_index)
{
    if (account_index != 0) {
        gpointer acc = g_queue_pop_nth(accountQueue, account_index);
        g_queue_push_nth(accountQueue, acc, account_index - 1);
    }
}

void
account_list_move_down(guint account_index)
{
    if (account_index < accountQueue->length-1) {
        gpointer acc = g_queue_pop_nth(accountQueue, account_index);
        g_queue_push_nth(accountQueue, acc, account_index + 1);
    }
}

guint
account_list_get_registered_accounts(void)
{
    guint res = 0;

    for (guint i = 0; i < account_list_get_size(); i++)
        if (account_list_get_nth(i)->state == (ACCOUNT_STATE_REGISTERED))
            res++;

    return res;
}

const gchar* account_list_get_current_id(void)
{
    account_t *current = account_list_get_current();

    if (current)
        return current->accountID;
    else
        return NULL;
}

void account_list_remove(const gchar *accountID)
{
    account_t *target = account_list_get_by_id(accountID);
    if (target) {
#if GLIB_CHECK_VERSION(2, 30, 0)
        if (!g_queue_remove(accountQueue, target))
            g_warning("Could not remove account with ID %s", accountID);
#else
        g_queue_remove(accountQueue, target);
#endif
    }

}

gchar * account_list_get_ordered_list(void)
{
    gchar *order = strdup("");

    for (guint i = 0; i < account_list_get_size(); i++) {
        account_t * account = account_list_get_nth(i);
        if (account) {
            gchar *new_order = g_strconcat(order, account->accountID, "/", NULL);
            g_free(order);
            order = new_order;
        }
    }

    return order;
}


gboolean current_account_has_mailbox(void)
{
    // Check if the current account has a voicemail number configured
    account_t *current = account_list_get_current();

    if (current) {
        gchar * account_mailbox = account_lookup(current, CONFIG_ACCOUNT_MAILBOX);

        if (account_mailbox && !utf8_case_equal(account_mailbox, ""))
            return TRUE;
    }

    return FALSE;
}

void current_account_set_message_number(guint nb)
{
    account_t *current = account_list_get_current();
    if (current)
        current->_messages_number = nb;
}

guint current_account_get_message_number(void)
{
    account_t *current = account_list_get_current();
    if (current)
        return current->_messages_number;
    else
        return 0;
}

gboolean current_account_has_new_message(void)
{
    account_t *current = account_list_get_current();
    return current && current->_messages_number > 0;
}

gboolean account_has_custom_user_agent(const account_t *account)
{
    return g_strcmp0(account_lookup(account, CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT), "true") == 0;
}

gboolean account_has_autoanswer_on(const account_t *account)
{
    return g_strcmp0(account_lookup(account, CONFIG_ACCOUNT_AUTOANSWER), "true") == 0;
}

gboolean account_is_IP2IP(const account_t *account)
{
    g_assert(account);
    return g_strcmp0(account->accountID, IP2IP_PROFILE) == 0;
}

static gboolean is_type(const account_t *account, const gchar *type)
{
    const gchar *account_type = account_lookup(account, CONFIG_ACCOUNT_TYPE);
    return g_strcmp0(account_type, type) == 0;
}

gboolean account_is_SIP(const account_t *account)
{
    return is_type(account, "SIP");
}

gboolean account_is_IAX(const account_t *account)
{
    return is_type(account, "IAX");
}

account_t *create_default_account()
{
    account_t *account = g_new0(account_t, 1);
    account->accountID = g_strdup("new"); // FIXME: maybe replace with NULL?
    account->properties = dbus_get_account_template();
    sflphone_fill_codec_list_per_account(account);
    initialize_credential_information(account);
    return account;
}

account_t *create_account_with_ID(const gchar *ID)
{
    account_t *account = g_new0(account_t, 1);
    account->accountID = g_strdup(ID);
    account->properties = dbus_get_account_details(ID);
    sflphone_fill_codec_list_per_account(account);
    initialize_credential_information(account);
    return account;
}

void initialize_credential_information(account_t *account)
{
    if (!account->credential_information) {
        account->credential_information = g_ptr_array_sized_new(1);
        GHashTable * new_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(new_table, g_strdup(CONFIG_ACCOUNT_REALM), g_strdup("*"));
        g_hash_table_insert(new_table, g_strdup(CONFIG_ACCOUNT_USERNAME), g_strdup(""));
        g_hash_table_insert(new_table, g_strdup(CONFIG_ACCOUNT_PASSWORD), g_strdup(""));
        g_ptr_array_add(account->credential_information, new_table);
    }
}

void account_replace(account_t *account, const gchar *key, const gchar *value)
{
    g_assert(account);
    g_assert(account->properties);
    g_hash_table_replace(account->properties, g_strdup(key), g_strdup(value));
}

void account_insert(account_t *account, const gchar *key, const gchar *value)
{
    g_assert(account && account->properties);
    g_hash_table_insert(account->properties, g_strdup(key), g_strdup(value));
}

gpointer account_lookup(const account_t *account, gconstpointer key)
{
    g_assert(account && account->properties);
    return g_hash_table_lookup(account->properties, key);
}
