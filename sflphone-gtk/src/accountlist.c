/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include <accountlist.h>
#include <actions.h>
#include <string.h>

GQueue * accountQueue;
gchar* __CURRENT_ACCOUNT_ID = NULL;

/* GCompareFunc to compare a accountID (gchar* and a account_t) */
gint 
is_accountID_struct ( gconstpointer a, gconstpointer b)
{
  account_t * c = (account_t*)a;
  if(strcmp(c->accountID, (gchar*) b) == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/* GCompareFunc to get current call (gchar* and a account_t) */
gint 
get_state_struct ( gconstpointer a, gconstpointer b)
{
  account_t * c = (account_t*)a;
  if( c->state == *((account_state_t*)b))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

void 
account_list_init ()
{
  accountQueue = g_queue_new ();
}

void 
account_list_clean ()
{
  g_queue_free (accountQueue);
}

void 
account_list_add (account_t * c)
{
  g_queue_push_tail (accountQueue, (gpointer *) c);
}


void 
account_list_remove (const gchar * accountID)
{
  GList * c = g_queue_find_custom (accountQueue, accountID, is_accountID_struct);
  if (c)
  {
    g_queue_remove(accountQueue, c->data);
  }
}


account_t * 
account_list_get_by_state (account_state_t state )
{
  GList * c = g_queue_find_custom (accountQueue, &state, get_state_struct);
  if (c)
  {
    return (account_t *)c->data;
  }
  else 
  {
    return NULL;
  }
  
}

account_t *
account_list_get_by_id(gchar * accountID)
{
	GList * c = g_queue_find_custom (accountQueue, accountID, is_accountID_struct);
	if(c)
	{
		return (account_t *)c->data;
	}
	else
	{
		return NULL;
	}
}

guint
account_list_get_size ( )
{
  return g_queue_get_length (accountQueue);
}

account_t * 
account_list_get_nth ( guint n )
{
  return g_queue_peek_nth (accountQueue, n);
}

account_t*
account_list_get_current( )
{
  if( __CURRENT_ACCOUNT_ID != NULL  )
    return account_list_get_by_id( __CURRENT_ACCOUNT_ID );
  else
    return NULL;
}

void
account_list_set_current_id(const gchar * accountID)
{
  __CURRENT_ACCOUNT_ID = g_strdup(accountID);
}

void
account_list_set_current_pos( guint n)
{
  __CURRENT_ACCOUNT_ID = account_list_get_nth(n)->accountID;
}

const gchar * account_state_name(account_state_t s)
{
  gchar * state;
  switch(s)
  {
  case ACCOUNT_STATE_REGISTERED:
    state = _("Registered");
    break;
  case ACCOUNT_STATE_UNREGISTERED:
    state = _("Not Registered");
    break;
  case ACCOUNT_STATE_TRYING:
    state = _("Trying...");
    break;
  case ACCOUNT_STATE_ERROR:
    state = _("Error");
    break;
  default:
    state = _("Invalid");
    break;
  }
  return state;
}

void
account_list_clear ( )
{
  g_queue_free (accountQueue);
  accountQueue = g_queue_new ();
}

void
account_list_move_up(guint index)
{
	if(index != 0)
	{
		gpointer acc = g_queue_pop_nth(accountQueue, index);
		g_queue_push_nth(accountQueue, acc, index-1);
	}
	account_list_set_current_pos( 0 );
}

void
account_list_move_down(guint index)
{
	if(index != accountQueue->length)
	{
		gpointer acc = g_queue_pop_nth(accountQueue, index);
		g_queue_push_nth(accountQueue, acc, index+1);
	}
	account_list_set_current_pos( 0 );
}
