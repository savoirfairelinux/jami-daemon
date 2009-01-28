/*
 *  Copyright (C) 2006-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#ifndef SIPACCOUNT_H
#define SIPACCOUNT_H

#include "account.h"
#include "sipvoiplink.h"

struct pjsip_cred_info;

class SIPVoIPLink;

/**
 * @file sipaccount.h
 * @brief A SIP Account specify SIP specific functions and object (SIPCall/SIPVoIPLink)
 */

class SIPAccount : public Account
{
    public:
        /**
         * Constructor
         * @param accountID The account identifier
         */
        SIPAccount(const AccountID& accountID);

        /* Copy Constructor */
        SIPAccount(const SIPAccount& rh);

        /* Assignment Operator */
        SIPAccount& operator=( const SIPAccount& rh);

        /**
         * Virtual destructor
         */
        virtual ~SIPAccount();

        /** 
         * Actually unuseful, since config loading is done in init() 
         */
        void loadConfig();

        /**
         * Initialize the SIP voip link with the account parameters and send registration
         */ 
        int registerVoIPLink();

        /**
         * Send unregistration and clean all related stuff ( calls , thread )
         */
        int unregisterVoIPLink();

        inline void setCredInfo(pjsip_cred_info *cred) {_cred = cred;}
        inline pjsip_cred_info *getCredInfo() {return _cred;}

        inline void setContact(const std::string &contact) {_contact = contact;}
        inline std::string getContact() {return _contact;}

        bool fullMatch(const std::string& username, const std::string& hostname);
        bool userMatch(const std::string& username);

        inline std::string getHostname( void ) { return _hostname; }
        inline void setHostname( std::string hostname) { _hostname = hostname; }

        inline std::string getPassword( void ) { return _password; }
        inline void setPassword( std::string password ) { _password = password; }

        inline std::string getAlias( void ) { return _alias; }
        inline void setAlias( std::string alias ) { _alias = alias; }

        inline std::string getType( void ) { return _type; }
        inline void setType( std::string type ) { _type = type; }


    private:
        /**
         * Credential information
         */
        pjsip_cred_info *_cred;

        /*
         * SIP address
         */
        std::string _contact;
};

#endif
