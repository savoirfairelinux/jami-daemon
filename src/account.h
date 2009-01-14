/*
 *  Copyright (C) 2006-2009 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

// Common account parameters
#define CONFIG_ACCOUNT_TYPE   "Account.type"  
#define CONFIG_ACCOUNT_ALIAS  "Account.alias"
#define CONFIG_ACCOUNT_MAILBOX	"Account.mailbox"
#define CONFIG_ACCOUNT_ENABLE	"Account.enable"
#define HOSTNAME        "hostname"
#define USERNAME        "username"
#define PASSWORD        "password"

// SIP specific parameters
#define SIP_PROXY             "SIP.proxy"
#define SIP_STUN_SERVER       "STUN.server"
#define SIP_USE_STUN          "STUN.enable"
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
        virtual int registerVoIPLink() = 0;

        /**
         * Unregister the underlying VoIPLink. Stop the event listener.
         * This should update the getRegistrationState() return value.
         */
        virtual int unregisterVoIPLink() = 0;

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

    private:

        // copy constructor
        Account(const Account& rh);

        // assignment operator
        Account& operator=(const Account& rh);

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

};

#endif
