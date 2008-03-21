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
 
#ifndef __ACCOUNTLIST_H__
#define __ACCOUNTLIST_H__

#include <gtk/gtk.h>
/** @file accountlist.h
  * @brief A list to hold accounts.
  */

#define ACCOUNT_TYPE               "Account.type"
#define ACCOUNT_ALIAS              "Account.alias"
#define ACCOUNT_ENABLED            "Account.enable"
//#define ACCOUNT_REGISTER           "Account.autoregister"

#define ACCOUNT_SIP_FULL_NAME      "SIP.fullName"
#define ACCOUNT_SIP_HOST_PART      "SIP.hostPart"
#define ACCOUNT_SIP_USER_PART      "SIP.userPart"
#define ACCOUNT_SIP_AUTH_NAME      "SIP.username"
#define ACCOUNT_SIP_PASSWORD       "SIP.password"
#define ACCOUNT_SIP_PROXY          "SIP.proxy"
#define ACCOUNT_SIP_STUN_SERVER	   "STUN.server"
#define ACCOUNT_SIP_STUN_ENABLED   "STUN.enable"

#define ACCOUNT_IAX_FULL_NAME      "IAX.fullName"
#define ACCOUNT_IAX_HOST           "IAX.host"
#define ACCOUNT_IAX_USER           "IAX.user"
#define ACCOUNT_IAX_PASS           "IAX.pass"

/** @enum account_state_t 
  * This enum have all the states an account can take.
  */
typedef enum
{
   ACCOUNT_STATE_INVALID = 0,
   ACCOUNT_STATE_REGISTERED,   
   ACCOUNT_STATE_UNREGISTERED,   
   ACCOUNT_STATE_TRYING, 
   ACCOUNT_STATE_ERROR
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
  GHashTable * properties;
} account_t;



/** This function initialize the account list. */
void account_list_init ();

/** This function empty and free the account list. */
void account_list_clean ();

/** This function append an account to list. 
  * @param a The account you want to add */
void account_list_add (account_t * a);

/** This function remove an account from list. 
  * @param accountID The accountID of the account you want to remove
  */
void account_list_remove (const gchar * accountID);

/** Return the first account that corresponds to the state 
  * @param s The state
  * @return An account or NULL */
account_t * account_list_get_by_state ( account_state_t state);

/** Return the number of accounts in the list
  * @return The number of accounts in the list */
guint account_list_get_size ( );

/** Return the account at the nth position in the list
  * @param n The position of the account you want
  * @return An account or NULL */
account_t * account_list_get_nth ( guint n );

/** Return the account's id chosen as default
 *  @return The default account */
gchar * account_list_get_default( );

/** This function sets an account as default
 * @param n The position of the account you want to select
 */
void account_list_set_default(const gchar * accountID);

/** This function maps account_state_t enums to a description.
  * @param s The state
  * @return The full text description of the state */
const gchar * account_state_name(account_state_t s);

void account_list_clear ( );

/** Return the account associated with an ID
 * @param accountID The ID of the account
 * @return An account or NULL */
account_t * account_list_get_by_id(gchar * accountID); 

#endif 
