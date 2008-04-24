/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 
#include <calllist.h>

#include <string.h>

/*
 * GQueue * callQueue = NULL;
 * call_t * selectedCall = NULL;
 */

/* GCompareFunc to compare a callID (gchar* and a call_t) */
gint 
is_callID_callstruct ( gconstpointer a, gconstpointer b)
{
  call_t * c = (call_t*)a;
  if(strcmp(c->callID, (const gchar*) b) == 0)
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

void 
call_list_init (calltab_t* tab)
{
  tab->callQueue = g_queue_new ();
  tab->selectedCall = NULL;
}

void 
call_list_clean (calltab_t* tab)
{
  g_queue_free (tab->callQueue);
}

void 
call_list_reset (calltab_t* tab)
{
  g_queue_free (tab->callQueue);
  tab->callQueue = g_queue_new();
}

void 
call_list_add (calltab_t* tab, call_t * c)
{
  if( tab == history )	
  {
      g_print("ADD THE  FUCKING CALL call list size = %i - max calls = %i\n", call_list_get_size(tab) , dbus_get_max_calls());
    // First case: can still add calls to the list
    if( call_list_get_size(tab) < dbus_get_max_calls() )
    {
      g_print("ADD THE  FUCKING CALL\n");
      g_queue_push_tail (tab->callQueue, (gpointer *) c);
      update_call_tree_add( history , c );
    }
    // List full -> Remove the last call from history and preprend the new call to the list
    else
    {
      update_call_tree_remove( history , (call_t*)g_queue_pop_head( tab -> callQueue ) );      
      g_queue_push_tail (tab->callQueue, (gpointer *) c);
      update_call_tree_add( history , c );
    }
  }
  else
    g_queue_push_tail (tab->callQueue, (gpointer *) c);
}

void
call_list_clean_history( void )
{
  int i;
  guint size = call_list_get_size( history );
  g_print("history list size = %i\n", call_list_get_size( history ));
  for( i = 0 ; i < size ; i++ )
  {
    g_print("Delete calls");
    call_t* c = call_list_get_nth( history , i );
    // Delete the call from the call tree
    g_print("Delete calls");
    update_call_tree_remove(history , c);
  }
  call_list_reset( history );
}

void 
call_list_remove (calltab_t* tab, const gchar * callID)
{
  call_t * c = call_list_get(tab, callID);
  if (c)
  {
    g_queue_remove(tab->callQueue, c);
  }
}


call_t * 
call_list_get_by_state (calltab_t* tab, call_state_t state )
{
  GList * c = g_queue_find_custom (tab->callQueue, &state, get_state_callstruct);
  if (c)
  {
    return (call_t *)c->data;
  }
  else 
  {
    return NULL;
  }
  
}

guint
call_list_get_size (calltab_t* tab)
{
  return g_queue_get_length (tab->callQueue);
}

call_t * 
call_list_get_nth (calltab_t* tab, guint n )
{
  return g_queue_peek_nth (tab->callQueue, n);
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


call_t * 
call_list_get (calltab_t* tab, const gchar * callID )
{
  GList * c = g_queue_find_custom (tab->callQueue, callID, is_callID_callstruct);
  if (c)
  {
    return (call_t *)c->data;
  }
  else 
  {
    return NULL;
  }
}

void
call_select (calltab_t* tab, call_t * c )
{
  tab->selectedCall = c;
}


call_t *
call_get_selected (calltab_t* tab)
{
  return tab->selectedCall;
}
