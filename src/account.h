/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <vector>
#include "config/config.h"
#include "voiplink.h"

class VoIPLink;

/**
 * @file account.h
 * @brief Interface to protocol account (SIPAccount, IAXAccount)
 * It can be enable on loading or activate after.
 * It contains account, configuration, VoIP Link and Calls (inside the VoIPLink)
 */

typedef std::string AccountID;

#define AccountNULL ""
/** Account type: SIP / IAX2 are supported */
#define CONFIG_ACCOUNT_TYPE   "Account.type"
/** Tells if account is enable or not */
#define CONFIG_ACCOUNT_ENABLE "Account.enable"
/** Account alias */
#define CONFIG_ACCOUNT_ALIAS  "Account.alias"
/** Mail box number */
#define CONFIG_ACCOUNT_MAILBOX	"Account.mailbox"
/** IAX paramater : host name */
#define IAX_HOST              "IAX.host"
/** IAX paramater : user name */
#define IAX_USER              "IAX.user"
/** IAX paramater : password */
#define IAX_PASSWORD          "IAX.password"
/** SIP parameter : authorization name */
#define SIP_USER	      "SIP.username"
/** SIP parameter : password */
#define SIP_PASSWORD          "SIP.password"
/** SIP parameter : host name */
#define SIP_HOST	      "SIP.hostPart"
/** SIP parameter : proxy address */
#define SIP_PROXY             "SIP.proxy"
/** SIP parameter : stun server address */
#define SIP_STUN_SERVER       "STUN.server"
/** SIP parameter : tells if stun is used or not */
#define SIP_USE_STUN          "STUN.enable"
/** SIP parameter : stun port */
#define SIP_STUN_PORT         "STUN.port"

class Account{
 public:
  Account(const AccountID& accountID);
  
  /**
   * Virtual destructor
   */
  virtual ~Account();

  /**
   * Load the settings for this account.
   */
  virtual void loadConfig();
  
  /**
   * Get the account ID
   * @return constant account id
   */
  inline const AccountID& getAccountID() { return _accountID; }

  /**
   * Get the voiplink pointer
   * @return VoIPLink* the pointer or 0
   */
  inline VoIPLink* getVoIPLink() { return _link; }

  /**
   * Register the underlying VoIPLink. Launch the event listener.
   * This should update the getRegistrationState() return value.
   */
  virtual void registerVoIPLink() = 0;

  /**
   * Unregister the underlying VoIPLink. Stop the event listener.
   * This should update the getRegistrationState() return value.
   */
  virtual void unregisterVoIPLink() = 0;

  /**
   * Tell if the account is enable or not. 
   * @return true if enabled
   *	     false otherwise
   */
  bool isEnabled() { return _enabled; }

  /**
   * Get the registration state of the specified link
   * @return RegistrationState	The registration state of underlying VoIPLink
   */
  VoIPLink::RegistrationState getRegistrationState() { return _link->getRegistrationState(); }

  /**
   * Load all contacts
   */
  void loadContacts();
  
  /**
   * Suscribe presence information for selected contacts if supported
   */
  void subscribeContactsPresence();
  
  /**
   * Publish our presence information to the server
   */
  void publishPresence(std::string presenceStatus);

private:

protected:
  /**
   * Account ID are assign in constructor and shall not changed
   */
  AccountID _accountID;

  /**
   * Voice over IP Link contains a listener thread and calls
   */
  VoIPLink* _link;

  /**
   * Tells if the link is enabled, active.
   * This implies the link will be initialized on startup.
   * Modified by the configuration (key: ENABLED)
   */
  bool _enabled;
  
  /**
   * Contacts related to account that can have presence information
   */
  //std::vector<Contact*> _contacts;
};

#endif
