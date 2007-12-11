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

GQueue * codecQueue = NULL;

gint
is_name_codecstruct (gconstpointer a, gconstpointer b)
{
  codec_t * c = (codec_t *)a;
  if(strcmp(c->name, (const gchar *)b)==0)
    return 0;
  else
    return 1;
}

void
codec_list_init()
{
  codecQueue = g_queue_new();
}

void
codec_list_add(codec_t * c)
{
  g_queue_push_tail (codecQueue, (gpointer *) c);
}


void 
codec_set_active(gchar * codec_name)
{
  codec_t * c = codec_list_get(codec_name);
  if(c)
    c->is_active = TRUE;
}

void
codec_set_inactive(gchar * codec_name)
{
  codec_t * c = codec_list_get(codec_name);
  if(c)
    c->is_active = FALSE;
}

guint
codec_list_get_size()
{
  return g_queue_get_length(codecQueue);
}

codec_t*
codec_list_get( const gchar * name)
{
  GList * c = g_queue_find_custom(codecQueue, name, is_name_codecstruct);
  if(c)
    return (codec_t *)c->data;
  else
    return NULL;
}

codec_t*
codec_list_get_nth(guint index)
{
  return g_queue_peek_nth(codecQueue, index);
}

void
codec_set_prefered_order(guint index)
{
  codec_t * prefered = codec_list_get_nth(index);
  g_queue_pop_nth(codecQueue, index);
  g_queue_push_head(codecQueue, prefered);
}

