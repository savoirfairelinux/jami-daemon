/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
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
  void registerVoIPLink();

  /**
   * Send unregistration and clean all related stuff ( calls , thread )
   */
  void unregisterVoIPLink();


  void setUserName(const std::string &name) {_userName = name;}

  std::string getUserName() {return _userName;}

  void setServer(const std::string &server) {_server = server;}

  std::string getServer() {return _server;}

  void setCredInfo(pjsip_cred_info *cred) {_cred = cred;}

  pjsip_cred_info *getCredInfo() {return _cred;}

  void setContact(const std::string contact) {_contact = contact;}

  std::string getContact() {return _contact;}

  bool fullMatch(const std::string& userName, const std::string& server);

  bool userMatch(const std::string& userName);

private:
  std::string _userName;
  std::string _server;
  pjsip_cred_info *_cred;
  std::string _contact;
};

#endif
