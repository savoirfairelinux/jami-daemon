/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#include <call.h>

/* GCompareFunc to compare a callID (gchar* and a call_t) */
gint
is_callID_callstruct ( gconstpointer a, gconstpointer b)
{
  call_t * c = (call_t*)a;
  if(g_strcasecmp(c->callID, (const gchar*) b) == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/* GCompareFunc to get current call (gchar* and a call_t) */
gint
get_state_callstruct ( gconstpointer a, gconstpointer b)
{
  call_t * c = (call_t*)a;
  if( c->state == *((call_state_t*)b))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

gchar *
call_get_name (const call_t * c)
{
  gchar * end = g_strrstr(c->from, "\"");
  if (!end) {
    return g_strndup(c->from, 0);
  } else {
    gchar * name = c->from +1;
    return g_strndup(name, end - name);
  }
}

gchar *
call_get_number (const call_t * c)
{
  gchar * number = g_strrstr(c->from, "<") + 1;
  gchar * end = g_strrstr(c->from, ">");
  number = g_strndup(number, end - number  );
  return number;
}

gchar *
call_get_recipient( const call_t * c )
{
  return c->to;
}

void create_new_call (gchar *to, gchar *from, call_state_t state, gchar *accountID, call_t **new_call) {

    gchar *call_id;
    call_t *call;

    call = g_new0 (call_t, 1);
    call->to = g_strdup (to);
    call->from = g_strdup (from);
    call->state = state;
    call->accountID = g_strdup (accountID);
    call->_start = 0;
    call->_stop = 0;

    call_id = g_new0(gchar, 30);
    g_sprintf(call_id, "%d", rand());
    call->callID = g_strdup (call_id);

    *new_call = call;
}

void create_new_call_from_details (const gchar *call_id, GHashTable *details, call_t **call) 
{
    gchar *from, *to, *accountID;
    call_t *new_call;
    GHashTable *call_details;
        
    accountID = g_hash_table_lookup (details, "ACCOUNTID");
    to = g_hash_table_lookup (details, "PEER_NUMBER");
    from = g_markup_printf_escaped("\"\" <%s>",  to);
        
    create_new_call (from, from, CALL_STATE_DIALING, accountID, &new_call);
    *call = new_call;
}

    
    void
free_call_t (call_t *c)
{
    g_free (c->callID);
    g_free (c->accountID);
    g_free (c->from);
    g_free (c->to);
    g_free (c);
}

void attach_thumbnail (call_t *call, GdkPixbuf *pixbuf) {
    call->contact_thumbnail = pixbuf;
}
