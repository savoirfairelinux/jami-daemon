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

void presence_list_test();
void presence_list_load();
void presence_list_save();

static GList * presence_buddy_list = NULL;
static GSettings * presence_setting_schema;

void
presence_list_init(SFLPhoneClient *client)
{
    if(!presence_buddy_list) // should load and subscribe only once
    // but this function is called each time the buddy_list_window is created
    {
        presence_setting_schema = client->settings;
        presence_buddy_list = g_list_alloc();
        presence_list_load();

        for (guint i = 0; i < account_list_get_size(); i++)
        {
            account_t * acc = account_list_get_nth(i);
            presence_list_send_subscribes(acc, TRUE);
        }
    }
}

void
presence_list_test()
{
    presence_buddy_list = g_list_alloc();
    presence_list_load();
    presence_list_print();
    buddy_t b1 = {g_strdup("acc1"), g_strdup("alias1"), g_strdup("uri1"), FALSE, FALSE, g_strdup("")};
    buddy_t b2 = {g_strdup("acc2"), g_strdup("alias2"), g_strdup("uri2"), FALSE, FALSE, g_strdup("")};
    buddy_t b3 = {g_strdup("acc3"), g_strdup("alias3"), g_strdup("uri3"), FALSE, FALSE, g_strdup("")};

    presence_list_add_buddy(&b1);
    presence_list_add_buddy(&b3);
    presence_list_remove_buddy(&b2);
    presence_list_print();

    presence_list_save();

    presence_list_flush();
    presence_list_print();
}

void
presence_list_load()
{

    GVariant * v_list = g_settings_get_value(presence_setting_schema, PRESENCE_BUDDY_LIST_KEY);
    if(!(g_variant_is_normal_form(v_list)))
        g_print("Buddy isn't correctly formed.\n");

    GVariantIter v_iter;
    GVariant *v_buddy, *v_acc, *v_alias, * v_uri;

    g_variant_iter_init (&v_iter, v_list);
    while ((v_buddy = g_variant_iter_next_value (&v_iter)))
    {
        v_acc = g_variant_lookup_value(v_buddy,"acc",G_VARIANT_TYPE_STRING);
        v_alias = g_variant_lookup_value(v_buddy,"alias",G_VARIANT_TYPE_STRING);
        v_uri = g_variant_lookup_value(v_buddy,"uri",G_VARIANT_TYPE_STRING);

        buddy_t * buddy = g_malloc(sizeof(buddy_t));
        buddy->acc = g_strdup(g_variant_get_data(v_acc));
        buddy->alias = g_strdup(g_variant_get_data(v_alias));
        buddy->uri = g_strdup(g_variant_get_data(v_uri));
        buddy->status = FALSE;
        buddy->note = g_strdup("");
        buddy->subscribed = FALSE;

        g_debug("Presence : found buddy:(acc:%s, bud: %s).", buddy->acc, buddy->uri);
        presence_buddy_list = g_list_append(presence_buddy_list, (gpointer)buddy);
    }

    GList *tmp =  g_list_nth(presence_buddy_list,1);
    buddy_t * element;
    g_print("-------- Loaded buddy list:\n");
    while (tmp)
    {
        element = (buddy_t *)(tmp->data);
        g_print ("buddy:(%s,%s).\n",
           element->acc, element->uri);
        tmp = g_list_next (tmp);
    }
}

void
presence_list_save()
{
    if(presence_buddy_list == NULL){
        g_warning("Uninitialized buddy list.");
        return;
    }

    GVariantBuilder *v_b_list = g_variant_builder_new (G_VARIANT_TYPE ("aa{ss}"));

    GList *tmp = g_list_nth(presence_buddy_list,1);
    buddy_t * buddy;
    while (tmp) {
        GVariantBuilder *v_b_buddy = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
        buddy = (buddy_t *)(tmp->data);
        g_variant_builder_add (v_b_buddy, "{ss}", "acc", buddy->acc);
        g_variant_builder_add (v_b_buddy, "{ss}", "alias", buddy->alias);
        g_variant_builder_add (v_b_buddy, "{ss}", "uri", buddy->uri);
        GVariant * v_buddy = g_variant_builder_end(v_b_buddy);
        const gchar *msg = g_variant_print(v_buddy,TRUE);
        g_print("Presence : saved buddy: %s \n", msg);
        g_free((gchar*)msg);
        g_variant_builder_add_value(v_b_list, v_buddy);
        tmp = g_list_next (tmp);
    }
    GVariant * v_list = g_variant_builder_end(v_b_list);

    if(g_settings_set_value(presence_setting_schema, PRESENCE_BUDDY_LIST_KEY, v_list))
        g_debug("Presence : write buddy list in gsettings.");
}

buddy_t *
presence_list_get_buddy(buddy_t * buddy)
{
    GList *tmp = g_list_nth(presence_buddy_list,1);
    buddy_t *b;

    if((presence_buddy_list == NULL) || (buddy==NULL)){
        g_warning("Uninitialized buddy list.");
        return FALSE;
    }

    while (tmp)
    {
        b = (buddy_t *)(tmp->data);
        //g_print ("Compare buddy:(%s,%s) to b(%s,%s)\n", buddy->uri, buddy->acc, b->uri, b->acc);
        if((g_strcmp0(buddy->uri, b->uri)==0) &&
                    (g_strcmp0(buddy->acc, b->acc)==0))
        {
            g_debug ("Found buddy:(%s,%s).", b->uri, b->acc);
            return b;
        }
        tmp = g_list_next (tmp);
    }
    return NULL;
}

buddy_t *
presence_list_buddy_get_by_string(const gchar *accID, const gchar *uri){
    GList *tmp = g_list_nth(presence_buddy_list,1);
    buddy_t *b;

    if(presence_buddy_list == NULL){
        g_warning("Uninitialized buddy list.");
        return FALSE;
    }

    while (tmp)
    {
        b = (buddy_t *)(tmp->data);
        //g_print ("Compare buddy:(%s,%s) to b(%s,%s)\n", accID, uri, b->uri, b->acc);
        if((g_strcmp0(uri, b->uri)==0) &&
                    (g_strcmp0(accID, b->acc)==0))
        {
            g_debug ("Found buddy:(%s,%s).", b->acc, b->uri);
            return b;
        }
        tmp = g_list_next (tmp);
    }
    return NULL;
}

void
presence_list_update_buddy(buddy_t * buddy, buddy_t * backup)
{
    buddy_t * b = presence_list_get_buddy(buddy);

    if(b != NULL)
    {
        g_debug("Presence : udate buddy:(%s).", b->uri);

        if(!(presence_list_get_buddy(backup))){
            g_debug("Presence : update buddy with new uri/acc");
            dbus_presence_subscribe(backup->acc, backup->uri, FALSE);
            b->subscribed = FALSE;
            dbus_presence_subscribe(b->acc, b->uri, TRUE);
        }

        presence_list_save();
    }
}


void
presence_list_add_buddy(buddy_t * buddy)
{
    if(presence_list_get_buddy(buddy)==NULL)
    {
        g_debug("Presence : add buddy:(%s, %s).", buddy->acc, buddy->uri);
        presence_buddy_list = g_list_append(presence_buddy_list, (gpointer)buddy);
        presence_list_save();
        buddy->subscribed = FALSE;
        dbus_presence_subscribe(buddy->acc, buddy->uri, TRUE);
    }
    else
        g_debug("Presence : don't add  existing buddy.");
}

void
presence_list_remove_buddy(buddy_t * buddy)
{
    buddy_t * b = presence_list_get_buddy(buddy);
    if(b != NULL)
    {
        g_debug("Presence : remove buddy:(%s).", b->uri);
        dbus_presence_subscribe(b->acc, b->uri, FALSE);
        presence_buddy_list = g_list_remove(presence_buddy_list, (gconstpointer)b);
        presence_buddy_delete(b);
        presence_list_save();
    }
    else
        g_debug("Presence : don't remove non existing buddy.");
}


void
presence_list_print()
{
    GList *tmp = g_list_nth(presence_buddy_list,1);
    buddy_t * buddy;
    g_print("-------- Print buddy list:\n");
    while (tmp)
    {
        buddy = (buddy_t *)(tmp->data);
        g_print ("buddy:(%s,%s,%s).\n", buddy->acc, buddy->alias, buddy->uri);
        tmp = g_list_next (tmp);
    }
}

GList *
presence_list_get()
{
    return presence_buddy_list;
}

buddy_t *
presence_list_get_nth(guint n)
{
    GList *tmp = g_list_nth(presence_buddy_list,n);
    return (buddy_t *)(tmp->data);
}


guint
presence_list_get_size()
{
    return g_list_length(presence_buddy_list);
}

void
presence_list_flush()
{
    if(presence_buddy_list == NULL){
        g_warning("Uninitialized buddy list.");
        return;
    }
    g_debug("Presence : flush the buddy list.");
    //g_list_foreach(presence_buddy_list, (GFunc) g_free, NULL);
    g_list_free_full(presence_buddy_list, (GDestroyNotify) presence_buddy_list);
}

void
presence_list_send_subscribes(account_t *acc, gboolean flag)
{
    buddy_t * b;
    for (guint i =  1; i < presence_list_get_size(presence_buddy_list); i++)
    {
        if(acc)
        {
            b = presence_list_get_nth(i);
            if((acc->state == (ACCOUNT_STATE_REGISTERED)) &&
                        account_lookup(acc, CONFIG_PRESENCE_ENABLED))
                dbus_presence_subscribe(b->acc, b->uri, flag);
        }
    }
}

buddy_t *
presence_buddy_create()
{
    buddy_t *b = g_malloc(sizeof(buddy_t));
    b->acc = g_strdup("");
    b->uri = g_strdup("<sip:XXXX@server>");
    b->alias = g_strdup("");
    b->subscribed = FALSE;
    b->status = FALSE;
    b->note = g_strdup("");
    return b;
}

void
presence_buddy_delete(buddy_t *b)
{
    if(!b)
    {
        g_debug("Presence : can't delete buddy==NULL");
        return;
    }

    g_free(b->acc);
    g_free(b->uri);
    g_free(b->note);
    g_free(b->alias);
    g_free(b);
}
