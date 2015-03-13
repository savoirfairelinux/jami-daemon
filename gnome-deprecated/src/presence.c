/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
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

#include <gtk/gtk.h>
#include "str_utils.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "presence.h"
#include "actions.h"
#include "dbus/dbus.h"
#include "account_schema.h"

#define PRESENCE_BUDDY_LIST_KEY "presence-buddy-list"
#define PRESENCE_DEFAULT_NOTE "Not found"

void presence_buddy_list_load();
void presence_buddy_list_save();

static GList * presence_buddy_list = NULL;
static GList * presence_group_list = NULL;
static GSettings * presence_setting_schema;

void
presence_buddy_list_init(SFLPhoneClient *client)
{
    // this function is called each time the buddy_list_window is created

    // should load and subscribe only once
    if (!presence_buddy_list) {
        presence_setting_schema = client->settings;
        presence_buddy_list_load();
        presence_buddy_list_print();
    }

    // send the subscriptions
    GList *b = g_list_first(presence_buddy_list);
    while (b) {
        presence_buddy_subscribe(b->data, TRUE);
        b = g_list_next(b);
    }

    presence_group_list_init();
}

void
presence_buddy_list_load()
{
    GVariant * v_list = g_settings_get_value(presence_setting_schema,
            PRESENCE_BUDDY_LIST_KEY);
    if (!g_variant_is_normal_form(v_list))
        g_print("Buddy isn't correctly formed.\n");

    GVariantIter v_iter;
    GVariant *v_buddy, *v_acc, *v_alias, *v_uri, *v_group;
    buddy_t * buddy;

    g_variant_iter_init (&v_iter, v_list);
    while ((v_buddy = g_variant_iter_next_value (&v_iter))) {
        v_acc = g_variant_lookup_value(v_buddy,"acc",G_VARIANT_TYPE_STRING);
        v_alias = g_variant_lookup_value(v_buddy,"alias",G_VARIANT_TYPE_STRING);
        v_group = g_variant_lookup_value(v_buddy,"group",G_VARIANT_TYPE_STRING);
        v_uri = g_variant_lookup_value(v_buddy,"uri",G_VARIANT_TYPE_STRING);
        // check format
        if (!v_acc || !v_alias || !v_group || !v_uri) {
            g_warning("Buddy incomplete, skipping!");
            continue;
        }

        buddy = presence_buddy_create();
        g_free(buddy->acc);
        g_free(buddy->alias);
        g_free(buddy->group);
        g_free(buddy->uri);
        buddy->acc = g_strdup(g_variant_get_data(v_acc));
        buddy->alias = g_strdup(g_variant_get_data(v_alias));
        buddy->group = g_strdup(g_variant_get_data(v_group));
        buddy->uri = g_strdup(g_variant_get_data(v_uri));

        g_debug("Presence: load buddy %s.", buddy->uri);
        presence_buddy_list = g_list_append(presence_buddy_list, (gpointer)buddy);
    }
}

void
presence_buddy_list_save()
{
    if (!presence_buddy_list) {
        g_warning("Uninitialized buddy list.");
        return;
    }

    GVariantBuilder *v_b_list = g_variant_builder_new (G_VARIANT_TYPE ("aa{ss}"));

    GList *tmp = g_list_first(presence_buddy_list);
    buddy_t * buddy;
    account_t *acc;
    while (tmp) {
        buddy = (buddy_t *)(tmp->data);
        acc = account_list_get_by_id(buddy->acc);
        // filter deleted accounts
        if (acc) {
            GVariantBuilder *v_b_buddy = g_variant_builder_new(G_VARIANT_TYPE("a{ss}"));
            g_variant_builder_add (v_b_buddy, "{ss}", "acc", buddy->acc);
            g_variant_builder_add (v_b_buddy, "{ss}", "alias", buddy->alias);
            g_variant_builder_add (v_b_buddy, "{ss}", "group", buddy->group);
            g_variant_builder_add (v_b_buddy, "{ss}", "uri", buddy->uri);
            GVariant * v_buddy = g_variant_builder_end(v_b_buddy);
            const gchar *msg = g_variant_print(v_buddy, TRUE);
            //g_debug("Presence: saved buddy: %s", msg);
            g_free((gchar*) msg);
            g_variant_builder_add_value(v_b_list, v_buddy);
        }
        tmp = g_list_next(tmp);
    }
    GVariant * v_list = g_variant_builder_end(v_b_list);

    if (g_settings_set_value(presence_setting_schema, PRESENCE_BUDDY_LIST_KEY, v_list))
        g_debug("Presence: write buddy list in gsettings.");
    presence_buddy_list_print();
}

buddy_t *
presence_buddy_list_get_buddy(buddy_t * buddy)
{
    GList *tmp = g_list_first(presence_buddy_list);
    buddy_t *b;

    if (!presence_buddy_list || !buddy) {
        g_warning("Uninitialized buddy list.");
        return FALSE;
    }

    while (tmp) {
        b = (buddy_t *) tmp->data;
        //g_print ("Compare buddy:(%s,%s) to b(%s,%s)\n", buddy->uri, buddy->acc, b->uri, b->acc);
        if ((g_strcmp0(buddy->uri, b->uri) == 0) &&
            (g_strcmp0(buddy->acc, b->acc) == 0)) {
#ifdef PRESENCE_DEBUG
            g_debug("Presence: get buddy %s.", b->uri);
#endif
            return b;
        }
        tmp = g_list_next(tmp);
    }
    return NULL;
}

GList *
presence_buddy_list_get_link(buddy_t * buddy)
{
    GList *tmp = g_list_first(presence_buddy_list);
    buddy_t *b;

    if (!presence_buddy_list || !buddy) {
        g_warning("Uninitialized buddy list.");
        return NULL;
    }

    while (tmp) {
        b = (buddy_t *) tmp->data;
        if ((g_strcmp0(buddy->uri, b->uri)==0) &&
            (g_strcmp0(buddy->acc, b->acc)==0)) {
            g_debug ("Presence: get buddy link %s.", b->uri);
            return tmp;
        }
        tmp = g_list_next (tmp);
    }
    return NULL;
}

buddy_t *
presence_buddy_list_buddy_get_by_string(const gchar *accID, const gchar *uri)
{
    GList *tmp = g_list_first(presence_buddy_list);
    buddy_t *b;

    if (!presence_buddy_list) {
        g_warning("Uninitialized buddy list.");
        return FALSE;
    }

    while (tmp) {
        b = (buddy_t *)(tmp->data);
        if ((g_strcmp0(uri, b->uri) == 0) &&
            (g_strcmp0(accID, b->acc) == 0)) {
            g_debug("Presence: get buddy:(%s,%s).", b->acc, b->uri);
            return b;
        }
        tmp = g_list_next(tmp);
    }
    return NULL;
}


buddy_t *
presence_buddy_list_buddy_get_by_uri(const gchar *uri)
{
    GList *tmp = g_list_first(presence_buddy_list);
    buddy_t *b;

    if (!presence_buddy_list) {
        g_warning("Uninitialized buddy list.");
        return FALSE;
    }

    while (tmp) {
        b = (buddy_t *) tmp->data;
        if (g_strcmp0(uri, b->uri) == 0) {
            g_debug ("Presence: get buddy by uri %s.", b->uri);
            return b;
        }
        tmp = g_list_next (tmp);
    }
    return NULL;
}

void
presence_buddy_list_edit_buddy(buddy_t * b, buddy_t * backup)
{
    if (!b || !backup)
        return;

    // send a new subscribe if the URI has changed
    if (g_strcmp0(b->uri, backup->uri) != 0) {
        g_debug("Presence: edit buddy %s with new uri", b->uri);
        //presence_buddy_subscribe(backup, FALSE); //unsubscribe the old buddy
        b->subscribed = FALSE; // subscribe to the new one
        g_free(b->note);
        b->note = g_strdup(PRESENCE_DEFAULT_NOTE);
        presence_buddy_subscribe(b, TRUE);
    } else {
        g_debug("Presence: edit buddy %s %s.", b->alias, b->uri);
    }

    presence_buddy_list_save();
}

void
presence_buddy_list_add_buddy(buddy_t * buddy)
{
    if (presence_buddy_list_get_buddy(buddy))
        return; // the buddy already exist

    g_debug("Presence: add buddy %s.", buddy->uri);
    presence_buddy_list = g_list_append(presence_buddy_list, (gpointer)buddy);
    presence_buddy_list_save();
    buddy->subscribed = FALSE;
    g_free(buddy->note);
    buddy->note = g_strdup(PRESENCE_DEFAULT_NOTE);
    presence_buddy_subscribe(buddy, TRUE);
}

void
presence_buddy_list_remove_buddy(buddy_t * buddy)
{
    GList *node = presence_buddy_list_get_link(buddy);
    if (!node)
        return; // the buddy doesn't exist

    buddy_t * b = (buddy_t*) node->data;
    g_debug("Presence: remove buddy:(%s).", b->uri);
    presence_buddy_list = g_list_remove_link(presence_buddy_list, node);
    presence_buddy_delete(b);
    presence_buddy_list_save();
}


void
presence_buddy_list_print()
{
#ifdef PRESENCE_DEBUG
    GList *tmp = g_list_first(presence_buddy_list);
    buddy_t * buddy;
    g_debug("Presence: buddy list:");
    while (tmp) {
        buddy = (buddy_t *) tmp->data;
        g_debug("  |-> (%s,%s)", buddy->alias, buddy->uri);
        tmp = g_list_next (tmp);
    }
#endif
}

GList *
presence_buddy_list_get()
{
    return presence_buddy_list;
}

buddy_t *
presence_buddy_list_get_nth(guint n)
{
    GList *tmp = g_list_nth(presence_buddy_list, n);
    return (buddy_t *) tmp->data;
}


guint
presence_buddy_list_get_size()
{
    return g_list_length(presence_buddy_list);
}

void
presence_buddy_list_flush()
{
    if (!presence_buddy_list) {
        g_warning("Uninitialized buddy list.");
        return;
    }

    // unsubscribe
    GList * b = g_list_first(presence_buddy_list);
    while (b) {
        presence_buddy_subscribe(b->data, FALSE);
        b = g_list_next(b);
    }

    g_debug("Presence: flush the buddy list.");
    g_list_foreach(presence_buddy_list, (GFunc) g_free, NULL);
    presence_buddy_list = NULL;
}

buddy_t *
presence_buddy_create()
{
    buddy_t *b = g_malloc(sizeof(buddy_t));
    b->acc = g_strdup("");
    b->uri = g_strdup("");
    b->group = g_strdup(" "); //' ' is important
    b->alias = g_strdup(" ");
    b->subscribed = FALSE;
    b->status = FALSE;
    b->note = g_strdup(PRESENCE_DEFAULT_NOTE);
    return b;
}

buddy_t *
presence_buddy_copy(buddy_t * b_src)
{
    g_assert(b_src);

    buddy_t *b_dest = g_malloc(sizeof(buddy_t));
    b_dest->alias = g_strdup(b_src->alias);
    b_dest->group = g_strdup(b_src->group);
    b_dest->uri = g_strdup(b_src->uri);
    b_dest->acc = g_strdup(b_src->acc);
    b_dest->subscribed = b_src->subscribed;
    b_dest->status = b_src->status;
    b_dest->note =  g_strdup(b_src->note);
    return b_dest;
}

void
presence_buddy_delete(buddy_t *buddy)
{
    if (!buddy)
        return;

    g_free(buddy->acc);
    g_free(buddy->alias);
    g_free(buddy->group);
    g_free(buddy->uri);
    g_free(buddy->note);
    //g_free(buddy);
}

void
presence_buddy_subscribe(buddy_t * buddy, gboolean flag)
{
    account_t * acc = account_list_get_by_id(buddy->acc);
    if (acc) {
        if (account_lookup(acc, CONFIG_PRESENCE_ENABLED) &&
            account_lookup(acc, CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED) &&
            (flag != buddy->subscribed))
            dbus_presence_subscribe(buddy->acc, buddy->uri, flag);
    }
}

void
presence_callable_to_buddy(callable_obj_t *c, buddy_t *b)
{
    if (!c || !b)
        return;

    account_t *acc =  account_list_get_current();
    if (!acc) {
        acc = account_list_get_nth(1); //0 is IP2IP
        if (!acc) {
            g_warning("At least one account must exist to able to subscribe.");
            return;
        }
    }
    gchar *uri = NULL;
    gchar *hostname;

    g_free(b->alias);
    g_free(b->uri);
    g_free(b->acc);

    b->acc = g_strdup(c->_accountID);

    hostname = account_lookup(acc, CONFIG_ACCOUNT_HOSTNAME);

    if (strlen(c->_display_name) == 0) {
        // extract a default alias from the uri
        gchar *number = g_strrstr(c->_peer_number, ":");
        gchar *end = g_strrstr(c->_peer_number, "@");
        if (end && number && number < end)
            b->alias = g_strndup(number + 1, end - number - 1);
        else
            b->alias = g_strdup(c->_peer_number);
    } else {
        b->alias = g_strdup(c->_display_name);
    }

    const gchar SIP_PREFIX[] = "<sip:";
    if (strlen(c->_peer_number) > 0) {
        if (g_str_has_prefix(c->_peer_number, SIP_PREFIX))
            b->uri = g_strdup(c->_peer_number);
        else {
            // had prefix and suffix
            if (g_strrstr(c->_peer_number, "@"))
                uri = g_strconcat("<sip:", c->_peer_number, ">", NULL);
            else
                uri = g_strconcat("<sip:", c->_peer_number, "@", hostname,
                        ">", NULL);
            b->uri = g_strdup(uri);
        }
    } else {
        g_warning("Presence: buddy has NO URI");
        uri = g_strconcat("<sip:XXXX@", hostname, ">", NULL);
        b->uri = g_strdup(uri);
    }

    if (uri)
        g_free(uri);
}

/********************************* group list functions *************************/

void
presence_group_list_init()
{
    // flush if need
    if (presence_group_list)
        presence_group_list_flush();

    GList * b = g_list_first(presence_buddy_list);
    while (b) {
        presence_group_list_add_group(((buddy_t *) b->data)->group);
        b = g_list_next(b);
    }
    presence_group_list_add_group(g_strdup(" "));
    presence_group_list_print();
}

void
presence_group_list_edit_group(gchar *new,  gchar *old)
{
    GList *node = presence_group_list_get_link(old);
    if (!node)
        return; // The group doesn't exist

    g_debug("Presence: edit group %s with new name %s.", old, new);

    // replace the group field of all the buddies
    GList *b = g_list_first(presence_buddy_list);
    while (b) {
        buddy_t *buddy = b->data;
        if (g_strcmp0(buddy->group, old) == 0) {
            g_free(buddy->group);
            buddy->group = g_strdup(new);
        }
        b = g_list_next(b);
    }

    presence_group_list_init();
    presence_group_list_print();
    presence_buddy_list_save();
}

void
presence_group_list_add_group(const gchar *group)
{
    if (presence_group_list_get_link(group))
        return; // The group already exists

    g_debug("Presence: add group %s.", group);
    gchar * copy = strdup(group);
    presence_group_list = g_list_append(presence_group_list, (gpointer)copy);
    //presence_group_list_print();
}

    void
presence_group_list_remove_group(const gchar *group)
{
    GList *node = presence_group_list_get_link(group);
    if (!node)
        return; // The group doesn't exit

    gchar *gr = (gchar*) (node->data);
    g_debug("Presence: remove group %s.", gr);
    //presence_group_list = g_list_remove(presence_group_list, (gconstpointer)gr);
    presence_group_list = g_list_delete_link(presence_group_list, node);

    // remove all associated buddies
    GList *b = g_list_first(presence_buddy_list);
    while (b) {
        buddy_t *buddy = b->data;
        if (g_strcmp0(buddy->group, gr) == 0)
            presence_buddy_list_remove_buddy(buddy);
        b = g_list_next(b);
    }
    presence_group_list_print();
}


void
presence_group_list_print()
{
#ifdef PRESENCE_DEBUG
    GList *tmp = g_list_first(presence_group_list);
    gchar *group;
    g_debug("Presence: group list:");
    while (tmp) {
        group = (gchar *)(tmp->data);
        g_debug("  |-> %s", group);
        tmp = g_list_next (tmp);
    }
#endif
}

GList *
presence_group_list_get()
{
    return presence_group_list;
}

gchar *
presence_group_list_get_nth(guint n)
{
    GList *tmp = g_list_nth(presence_group_list, n);
    return (gchar *)(tmp->data);
}


guint
presence_group_list_get_size()
{
    return g_list_length(presence_group_list);
}

void
presence_group_list_flush()
{
    if (!presence_group_list) {
        g_warning("Uninitialized group list.");
        return;
    }

    g_debug("Presence: flush the group list.");
    g_list_foreach(presence_group_list, (GFunc) g_free, NULL);
    g_list_free(presence_group_list);
    presence_group_list = NULL;
}

GList *
presence_group_list_get_link(const gchar *group)
{
    GList *tmp = g_list_first(presence_group_list);

    if (!presence_group_list || !group) {
        g_warning("Uninitialized buddy list.");
        return NULL;
    }

    while (tmp) {
        if (g_strcmp0(group, (gchar *)(tmp->data)) == 0) {
            //g_debug ("Presence: get groupe link %s.", group);
            return tmp;
        }
        tmp = g_list_next (tmp);
    }
    return NULL;
}
