/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.net>
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

#include <codeclist.h>

#include <string.h>
#include <stdlib.h>

#include "dbus.h"

GQueue * codecsCapabilities = NULL;

	gint
is_name_codecstruct (gconstpointer a, gconstpointer b)
{
	codec_t * c = (codec_t *)a;
	if(strcmp(c->name, (const gchar *)b)==0)
		return 0;
	else
		return 1;
}

	gint
is_payload_codecstruct (gconstpointer a, gconstpointer b)
{
	codec_t * c = (codec_t *)a;
	if(c->_payload == GPOINTER_TO_INT(b))
		return 0;
	else
		return 1;
}

void codec_list_init (GQueue **queue) {

	// Create the queue object that will contain the audio codecs
	*queue = g_queue_new();
}

void codec_capabilities_load (void) {

	gchar **codecs = NULL, **pl = NULL;

	// Create the queue object that will contain the global list of audio codecs
	g_queue_free (codecsCapabilities);
	codecsCapabilities = g_queue_new();

	// This is a global list inherited by all accounts
    codecs = (gchar**) dbus_codec_list ();
    
	// Add the codecs in the list
	for (pl=codecs; *codecs; codecs++) {

		codec_t *c;
		codec_create_new (atoi (*codecs), TRUE, &c);
		g_queue_push_tail (codecsCapabilities, (gpointer*) c);
    }

	// If we didn't load any codecs, problem ...
	if (g_queue_get_length (codecsCapabilities) == 0) {

		// Error message
		ERROR ("No audio codecs found");
        dbus_unregister(getpid());
        exit(0);
    }
}

void account_create_codec_list (account_t **acc) {

	gchar **order = NULL;
	GQueue *_codecs;

	_codecs = (*acc)->codecs;
	if (_codecs != NULL)
		g_queue_free (_codecs);

	_codecs = g_queue_new ();
	_codecs = g_queue_copy (codecsCapabilities);

	(*acc)->codecs = _codecs;
	// order = (gchar**) dbus_get_active_codec_list (acc->accountID);
}

void codec_create_new (gint payload, gboolean active, codec_t **c) {

	codec_t *codec;
	gchar **specs;

	codec = g_new0 (codec_t, 1);
	codec->_payload = payload;
    specs = (gchar **) dbus_codec_details (payload);
	codec->name = specs[0];
	codec->sample_rate = atoi (specs[1]);
	codec->_bitrate = atoi (specs[2]);
	codec->_bandwidth = atoi (specs[3]);
	codec->is_active = active;

	*c = codec;
}

void codec_create_new_with_specs (gint payload, gchar **specs, gboolean active, codec_t **c) {

	codec_t *codec;

	codec = g_new0 (codec_t, 1);
	codec->_payload = payload;
	codec->name = specs[0];
	codec->sample_rate = atoi (specs[1]);
	codec->_bitrate = atoi (specs[2]);
	codec->_bandwidth = atoi (specs[3]);
	codec->is_active = active;

	*c = codec;
}


void codec_list_clear (GQueue **queue) {

	g_queue_free (*queue);
	*queue = g_queue_new();
}

/*void codec_list_clear (void) {

	g_queue_free (codecsCapabilities);
	codecsCapabilities = g_queue_new();
}*/

void codec_list_add(codec_t * c, GQueue **queue) {

	// Add a codec to a specific list
	g_queue_push_tail (*queue, (gpointer *) c);
}

void codec_set_active (codec_t * c) {

	if(c)
	{
		DEBUG("%s set active", c->name);
		c->is_active = TRUE;
	}
}

void codec_set_inactive (codec_t * c) {

	if(c)
		c->is_active = FALSE;
}

guint codec_list_get_size () {

	// The system wide codec list and the one per account have exactly the same size
	// The only difference may be the order and the enabled codecs
	return g_queue_get_length (codecsCapabilities);
}

codec_t* codec_list_get_by_name (const gchar* name) {

	GList * c = g_queue_find_custom (codecsCapabilities, name, is_name_codecstruct);
	if(c)
		return (codec_t *)c->data;
	else
		return NULL;
}

codec_t* codec_list_get_by_payload (gconstpointer payload) {

	GList * c = g_queue_find_custom(codecsCapabilities, payload, is_payload_codecstruct);
	if(c)
		return (codec_t *)c->data;
	else
		return NULL;
}

codec_t* codec_list_get_nth (guint index, GQueue *q) {
	return g_queue_peek_nth (q, index);
}

void codec_set_prefered_order (guint index, GQueue *q) {

	codec_t * prefered = codec_list_get_nth (index, q);
	g_queue_pop_nth (q, index);
	g_queue_push_head (q, prefered);
}

void codec_list_move_codec_up (guint index) {

	DEBUG("Codec list Size: %i \n", codec_list_get_size ());

	if (index != 0)
	{
		gpointer codec = g_queue_pop_nth (codecsCapabilities, index);
		g_queue_push_nth (codecsCapabilities, codec, index-1);
	}
}

void codec_list_move_codec_down (guint index) {

	DEBUG("Codec list Size: %i \n",codec_list_get_size());

	if (index != codecsCapabilities->length)
	{
		gpointer codec = g_queue_pop_nth (codecsCapabilities, index);
		g_queue_push_nth (codecsCapabilities, codec, index+1);
	}
}

void codec_list_update_to_daemon (account_t *acc) {

	/*
	// String listing of all codecs payloads
	const gchar** codecList;

	// Length of the codec list
	int length = codecsCapabilities->length;

	// Initiate double array char list for one string
	codecList = (void*)malloc(sizeof(void*));

	// Get all codecs in queue
	int i, c = 0;
	DEBUG("List of active codecs :");
	for(i = 0; i < length; i++)
	{
		codec_t* currentCodec = codec_list_get_nth (i, acc->codecs);
		// Assert not null
		if(currentCodec)
		{
			// Save only if active
			if(currentCodec->is_active)
			{
				// Reallocate memory each time more than one active codec is found
				if(c!=0)
					codecList = (void*)realloc(codecList, (c+1)*sizeof(void*));
				// Allocate memory for the payload
				*(codecList+c) = (gchar*)malloc(sizeof(gchar*));
				char payload[10];
				// Put payload string in char array
				sprintf(payload, "%d", currentCodec->_payload);
				strcpy((char*)*(codecList+c), payload);
				DEBUG(" %s", *(codecList+c));
				c++;
			}
		}
	}

	// Allocate NULL array at the end for Dbus
	codecList = (void*)realloc(codecList, (c+1)*sizeof(void*));
	*(codecList+c) = NULL;

	// call dbus function with array of strings
	dbus_set_active_codec_list (codecList);

	// Delete memory
	for(i = 0; i < c; i++) {
		free((gchar*)*(codecList+i));
	}
	free(codecList);
	*/
}

GQueue* get_system_codec_list (void) {
	return  codecsCapabilities;
}
