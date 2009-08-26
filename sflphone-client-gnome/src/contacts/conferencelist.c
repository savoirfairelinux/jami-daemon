/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include <conferencelist.h>


gchar* 
generate_conf_id (void)
{
    gchar *conf_id;

    conf_id = g_new0(gchar, 30);
    g_sprintf(conf_id, "%d", rand());
    return conf_id;
}


void
conferencelist_init()
{
    conferenceQueue = g_queue_new ();
}


void
conferencelist_clean()
{
    g_queue_free (conferenceQueue);
}


void
conferencelist_reset()
{
    g_queue_free (conferenceQueue);
    conferenceQueue = g_queue_new();
}


void
conferencelist_add(const conference_obj_t* conf)
{
    gchar* c = (gchar*)conferencelist_get(conf->_confID);
    if(!c)
    {
        g_queue_push_tail (conferenceQueue, (gpointer)conf);
    }
}


void
conferencelist_remove (const gchar* conf)
{
    gchar* c = (gchar*)conferencelist_get(conf);
    if (c)
    {
        g_queue_remove(conferenceQueue, c);
    }
}

conference_obj_t* 
conferencelist_get (const gchar* conf_id)
{

    GList* c = g_queue_find_custom(conferenceQueue, conf_id, is_confID_confstruct);
    if (c)
    {
	return (conference_obj_t*)c->data;
    }
    else
    {
	return NULL;
    }
}


conference_obj_t* 
conferencelist_get_nth ( guint n )
{
    GList* c = g_queue_peek_nth(conferenceQueue, n);
    if (c)
    {
	return (conference_obj_t*)c->data;
    }
    else
    {
	return NULL;
    }
}


guint
conferencelist_get_size ()
{
    return g_queue_get_length (conferenceQueue);
}
