/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@savoirfairelinux.com>
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
 
#ifndef __ACCOUNTLIST_H__
#define __ACCOUNTLIST_H__

#include <gtk/gtk.h>
/** @file accountlist.h
  * @brief A list to hold accounts.
  */

/** @enum account_state_t 
  * This enum have all the states an account can take.
  */
typedef enum
{
  /** Invalid state */
   ACCOUNT_STATE_INVALID = 0,
   /** The account is registered  */
   ACCOUNT_STATE_REGISTERED,   
   /** The account is not registered */
   ACCOUNT_STATE_UNREGISTERED,   
   /** The account is trying to register */
   ACCOUNT_STATE_TRYING, 
   /** Error state. The account is not registered */
   ACCOUNT_STATE_ERROR,
   /** An authentification error occured. Wrong password or wrong username. The account is not registered */
   ACCOUNT_STATE_ERROR_AUTH,
   /** The network is unreachable. The account is not registered */
   ACCOUNT_STATE_ERROR_NETWORK,
   /** Host is unreachable. The account is not registered */
   ACCOUNT_STATE_ERROR_HOST,
   /** Stun server configuration error. The account is not registered */
   ACCOUNT_STATE_ERROR_CONF_STUN,
   /** Stun server is not existing. The account is not registered */
   ACCOUNT_STATE_ERROR_EXIST_STUN,
   /** IP profile status **/
   IP2IP_PROFILE_STATUS
} account_state_t;

/** @struct account_t
  * @brief Account information.
  * This struct holds information about an account.  All values are stored in the 
  * properties GHashTable except the accountID and state.  This match how the 
  * server internally works and the dbus API to save and retrieve the accounts details.
  * 
  * To retrieve the Alias for example, use g_hash_table_lookup(a->properties, ACCOUNT_ALIAS).  
  */

typedef struct  {
 	gchar * accountID;
  	account_state_t state;
  	gchar * protocol_state_description;
  	guint protocol_state_code;  
  	GHashTable * properties;
  	GPtrArray * credential_information;

	/* The codec list */
	GQueue *codecs;
  	guint _messages_number;
} account_t;


/** 
 * This function initialize the account list. 
 */
void account_list_init ();

/** 
 * This function empty and free the account list. 
 */
void account_list_clean ();

/** 
 * This function append an account to list. 
 * @param a The account you want to add 
 */
void account_list_add (account_t * a);

/** 
 * This function append an account to list at a given position. 
 * @param a The account you want to add
 * @param pos THe position in the list to insert the account
 */
void account_list_add_at_nth (account_t * a, guint pos);

/** 
 * This function remove an account from list. 
 * @param accountID The accountID of the account you want to remove
 */
void account_list_remove (const gchar * accountID);

/** 
 * Return the first account that corresponds to the state 
 * @param state The state
 * @return account_t* An account or NULL 
 */
account_t * account_list_get_by_state ( account_state_t state);

/**
 * @return guint The number of registered accounts in the list
 */
guint account_list_get_registered_accounts( );

/** 
 * Return the number of accounts in the list
 * @return guint The number of accounts in the list 
 */
guint account_list_get_size ( );

/** 
 * Return the account at the nth position in the list
 * @param n The position of the account you want
 * @return An account or NULL 
 */
account_t * account_list_get_nth ( guint n );

/** 
 * Return the current account struct
 *  @return The current account struct
 */
account_t * account_list_get_current( );

/** 
 * This function sets an account as the current one
 * @param current the account you want to set as current
 */
void account_list_set_current (account_t *current);

/** 
 * This function maps account_state_t enums to a description.
 * @param s The state
 * @return The full text description of the state 
 */
const gchar * account_state_name(account_state_t s);

/** 
 * This function clear the list
 */
void account_list_clear ( );

/** 
 * Return the account associated with an ID
 * @param accountID The ID of the account
 * @return An account or NULL 
 */
account_t * account_list_get_by_id(gchar * accountID); 

/** 
 * Move the account from an unit up in the account_list
 * @param index The current index in the list
 */
void account_list_move_up( guint index );

/** 
 * Move the account from an unit down in the account_list
 * @param index The current index in the list
 */
void account_list_move_down( guint index );

/**
 * Return the ID of the current default account
 * @return gchar* The id
 */
gchar* account_list_get_current_id( void );

/**
 * Returns the number of SIP accounts that have been configured
 */
int account_list_get_sip_account_number( void );

/**
 * Returns the number of IAX accounts that have been configured
 */
int account_list_get_iax_account_number( void );

gchar * account_list_get_ordered_list (void);

guint account_list_get_position (account_t *account);

gboolean account_list_current_account_has_mailbox (void);

guint current_account_get_message_number (void);

void current_account_set_message_number (guint nb);

gboolean current_account_has_new_message (void);

#endif 
