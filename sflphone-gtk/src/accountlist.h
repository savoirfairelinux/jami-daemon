/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#define ACCOUNT_TYPE               "Account.type"
#define ACCOUNT_ALIAS              "Account.alias"
#define ACCOUNT_ENABLED            "Account.enable"
#define ACCOUNT_REGISTER           "Account.autoregister"

#define ACCOUNT_SIP_FULL_NAME      "SIP.fullName"
#define ACCOUNT_SIP_HOST_PART      "SIP.hostPart"
#define ACCOUNT_SIP_USER_PART      "SIP.userPart"
#define ACCOUNT_SIP_AUTH_NAME      "SIP.username"
#define ACCOUNT_SIP_PASSWORD       "SIP.password"
#define ACCOUNT_SIP_PROXY          "SIP.proxy"

#define ACCOUNT_IAX_FULL_NAME      "IAX.fullName"
#define ACCOUNT_IAX_HOST           "IAX.host"
#define ACCOUNT_IAX_USER           "IAX.user"
#define ACCOUNT_IAX_PASS           "IAX.pass"

typedef enum
{
   ACCOUNT_STATE_INVALID = 0,
   ACCOUNT_STATE_REGISTERED,   
   ACCOUNT_STATE_UNREGISTERED   
} account_state_t;


typedef struct  {
  gchar * accountID;
  account_state_t state;
  GHashTable * properties;
} account_t;

void account_list_init ();

void account_list_clean ();

void account_list_add (account_t * a);

void account_list_remove (const gchar * accountID);

/** Return the first account that corresponds to the state */
account_t * account_list_get_by_state ( account_state_t state);

guint account_list_get_size ( );

account_t * account_list_get_nth ( guint n );

const gchar * account_state_name(account_state_t s);
#endif 
