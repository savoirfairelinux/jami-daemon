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
#define PRESENCE_BUDDY_LIST_TEST_KEY "presence-buddy-list-test" // temp


void presence_print_list(GList * list);
void presence_load_list(SFLPhoneClient *client, GList *list);
void presence_save_list(SFLPhoneClient *client, GList * list);
void presence_flush_list(GList * list);
void presence_add_buddy(GList * list, buddy_t * buddy);
void presence_remove_buddy(GList * list, buddy_t * buddy);
GList * presence_get_buddy(GList * list, buddy_t * buddy);
void presence_test(SFLPhoneClient *client);

void
presence_init(SFLPhoneClient *client)
{
    presence_test(client);
}

GList *
presence_new_list(){
    GList * list = g_list_alloc();
    return list;
}

void
presence_test(SFLPhoneClient *client)
{
    GList * buddy_list = presence_new_list();
    presence_load_list(client, buddy_list);
    presence_print_list(buddy_list);
    buddy_t b1 = {g_strdup("uri1"), g_strdup("acc1")};
    buddy_t b2 = {g_strdup("uri2"), g_strdup("acc2")};
    buddy_t b3 = {g_strdup("uri3"), g_strdup("acc3")};

    presence_add_buddy(buddy_list,&b1);
    presence_add_buddy(buddy_list,&b3);
    presence_remove_buddy(buddy_list,&b2);
    presence_print_list(buddy_list);

    presence_save_list(client, buddy_list);

    presence_flush_list(buddy_list);
    presence_print_list(buddy_list);
}

void
presence_load_list(SFLPhoneClient *client, GList * list)
{

    GVariant * v_list = g_settings_get_value(client->settings, PRESENCE_BUDDY_LIST_KEY);
    if(!(g_variant_is_normal_form(v_list)))
        g_print("Buddy isn't correctly formed.\n");

    GVariantIter v_iter;
    GVariant *v_buddy;
    GVariant *v_uri;
    GVariant *v_acc;
    g_variant_iter_init (&v_iter, v_list);
    while ((v_buddy = g_variant_iter_next_value (&v_iter)))
    {
        v_uri = g_variant_lookup_value(v_buddy,"uri",G_VARIANT_TYPE_STRING);
        v_acc = g_variant_lookup_value(v_buddy,"acc",G_VARIANT_TYPE_STRING);
        buddy_t * buddy = g_malloc(sizeof(buddy_t));
        buddy->uri = g_strdup(g_variant_get_data(v_uri));
        buddy->acc = g_strdup(g_variant_get_data(v_acc));
        g_debug("Presence : found buddy:(%s, %s).", buddy->uri,buddy->acc);
        list = g_list_append(list, (gpointer)buddy);
    }

    GList *tmp =  g_list_nth(list,1);
    buddy_t * element;
    g_print("-------- Loaded buddy list:\n");
    while (tmp)
    {
        element = (buddy_t *)(tmp->data);
        g_print ("buddy:(%s,%s).\n",
            element->uri,element->acc);
        tmp = g_list_next (tmp);
    }
}

void
presence_save_list(SFLPhoneClient *client, GList * list)
{
    if(list == NULL){
        g_warning("Uninitialized buddy list.");
        return;
    }
    GVariantBuilder *v_b_list = g_variant_builder_new (G_VARIANT_TYPE ("aa{ss}"));

    GList *tmp = g_list_nth(list,1);
    buddy_t * buddy;
    while (tmp) {
        GVariantBuilder *v_b_buddy = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
        buddy = (buddy_t *)(tmp->data);
        g_variant_builder_add (v_b_buddy, "{ss}", "uri", buddy->uri);
        g_variant_builder_add (v_b_buddy, "{ss}", "acc", buddy->acc);
        GVariant * v_buddy = g_variant_builder_end(v_b_buddy);
        g_print("New buddy: %s \n",g_variant_print(v_buddy,TRUE));
        g_variant_builder_add_value(v_b_list, v_buddy);
        tmp = g_list_next (tmp);
    }
    GVariant * v_list = g_variant_builder_end(v_b_list);

    if(g_settings_set_value(client->settings, PRESENCE_BUDDY_LIST_TEST_KEY, v_list))
        g_debug("Presence : saved buddy list.");
}

GList *
presence_get_buddy(GList * list, buddy_t * buddy)
{
    GList *tmp = g_list_nth(list,1);
    buddy_t *b;

    if((list == NULL) || (buddy==NULL)){
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
            return tmp;
        }
        tmp = g_list_next (tmp);
    }
    return NULL;
}


void
presence_add_buddy(GList * list, buddy_t * buddy){
    if(presence_get_buddy(list,buddy)==NULL)
    {
        buddy_t * b = g_malloc(sizeof(buddy_t));
        b->uri = g_strdup(buddy->uri);
        b->acc = g_strdup(buddy->acc);
        g_debug("Presence : add buddy:(%s, %s).", b->uri, b->acc);
        list = g_list_append(list, (gpointer)b);
    }
    else
        g_debug("Presence : don't add  existing buddy.");
}

void
presence_remove_buddy(GList * list, buddy_t * buddy)
{
    GList * ptr = presence_get_buddy(list, buddy);
    if(ptr != NULL)
    {
        buddy_t * s = (buddy_t *)(ptr->data);
        g_debug("Presence : remove buddy:(%s).", s->uri);
        list = g_list_remove(list, (gconstpointer)s);
    }
    else
        g_debug("Presence : don't remove non existing buddy.");
}


void
presence_print_list(GList *list)
{
    GList *tmp = g_list_nth(list,1);
    buddy_t * buddy;
    g_print("-------- Print buddy list:\n");
    while (tmp)
    {
        buddy = (buddy_t *)(tmp->data);
        g_print ("buddy:(%s,%s).\n",buddy->uri,buddy->acc);
        tmp = g_list_next (tmp);
    }
}

void
presence_flush_list(GList *list)
{
    if(list == NULL){
        g_warning("Uninitialized buddy list.");
        return;
    }
    g_debug("Presence : flush the buddy list.");
    g_list_foreach(list, (GFunc) g_free, NULL);
    g_list_free(list);
}
