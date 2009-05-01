/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <accountlist.h>
#include <actions.h>
#include <string.h>

GQueue * accountQueue;

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
    account_t *current;
    
    // No account registered
    if (account_list_get_registered_accounts () == 0)
        return NULL;

    // if we are here, it means that we have at least one registered account in the list
    // So we get the first one
    current = account_list_get_by_state (ACCOUNT_STATE_REGISTERED);
    if (!current)
        return NULL;

    return current;
}

void account_list_set_current (account_t *current)
{
    gpointer acc;
    guint pos;

    // 2 steps:
    // 1 - retrieve the index of the current account in the Queue
    // 2 - then set it as first
    pos = account_list_get_position (current);
    if (pos > 0)
    {
		acc = g_queue_pop_nth(accountQueue, pos);
		g_queue_push_nth(accountQueue, acc, 0);
    }
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
  case ACCOUNT_STATE_ERROR_AUTH:
    state = _("Bad authentification");
    break;
  case ACCOUNT_STATE_ERROR_NETWORK:
    state = _("Network unreachable");
    break;
  case ACCOUNT_STATE_ERROR_HOST:
    state = _("Host unreachable");
    break;
  case ACCOUNT_STATE_ERROR_CONF_STUN:
    state = _("Stun configuration error");
    break;
  case ACCOUNT_STATE_ERROR_EXIST_STUN:
    state = _("Stun server invalid");
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
    DEBUG ("index  = %i\n", index);

	if(index != 0)
	{
		gpointer acc = g_queue_pop_nth(accountQueue, index);
		g_queue_push_nth(accountQueue, acc, index-1);
	}
}

void
account_list_move_down(guint index)
{
	if(index != accountQueue->length)
	{
		gpointer acc = g_queue_pop_nth(accountQueue, index);
		g_queue_push_nth(accountQueue, acc, index+1);
	}
}

guint
account_list_get_registered_accounts( void )
{
  guint res = 0;
  unsigned int i;
  for(i=0;i<account_list_get_size();i++)
  {
    if( account_list_get_nth( i ) -> state == ( ACCOUNT_STATE_REGISTERED ))
      res ++;
  }
  DEBUG(" %d registered accounts" , res );
  return res;
}

gchar* account_list_get_current_id( void ){

    account_t *current;

    current = account_list_get_current ();
    if (current)
        return current->accountID;
    else
        return "";
}

int account_list_get_sip_account_number( void ){

    int n;
    guint size, i;
    account_t *current;

    size = account_list_get_size();
    n = 0;
    for( i=0; i<size ;i++ ){
        current = account_list_get_nth( i );
        if( strcmp(g_hash_table_lookup(current->properties, ACCOUNT_TYPE), "SIP" ) == 0 )
            n++;
    }

    return n;
}

int account_list_get_iax_account_number( void ){

    int n;
    guint size, i;
    account_t *current;

    size = account_list_get_size();
    n = 0;
    for( i=0; i<size ;i++ ){
        current = account_list_get_nth( i );
        if( strcmp(g_hash_table_lookup(current->properties, ACCOUNT_TYPE), "IAX" ) == 0 )
            n++;
    }

    return n;
}

gchar * account_list_get_ordered_list (void) {

    gchar *order="";
    guint i;

    for( i=0; i<account_list_get_size(); i++ )
    {
        order = g_strconcat (order, account_list_get_nth (i)->accountID, "/", NULL);
    }
    return order;
}


guint account_list_get_position (account_t *account) 
{
    guint size, i;
    account_t *tmp;

    size = account_list_get_size ();
    for (i=0; i<size; i++)
    {
        tmp = account_list_get_nth (i);
        if (g_strcasecmp (tmp->accountID, account->accountID) == 0)
        {
            return i;
        }
    }
    // Not found
    return -1;
}
