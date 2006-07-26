/*
 *  Copyright (C) 2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#ifndef IAXVOIPLINK_H
#define IAXVOIPLINK_H

#include "voIPLink.h"
#include <iax-client.h>

class EventThread;
class IAXCall;

/**
 * VoIPLink contains a thread that listen to external events 
 * and contains IAX Call related functions
 * @author Yan Morin <yan.morin@gmail.com>
 */
class IAXVoIPLink : public VoIPLink
{
public:
    IAXVoIPLink(const AccountID& accountID);

    ~IAXVoIPLink();

  void getEvent(void);
  bool init (void);
  bool checkNetwork (void) { return false; }
  void terminate (void);

  bool setRegister (void);
  bool setUnregister (void);

  Call* newOutgoingCall(const CallID& id, const std::string& toUrl) {return 0; }
  bool answer(const CallID& id) {return false;}

  bool hangup(const CallID& id) { return false; }
  bool cancel(const CallID& id) { return false; }
  bool onhold(const CallID& id) { return false; }
  bool offhold(const CallID& id) { return false; }
  bool transfer(const CallID& id, const std::string& to) { return false; }
  bool refuse (const CallID& id) { return false; }
  bool carryingDTMFdigits(const CallID& id, char code) { return false; }
  bool sendMessage(const std::string& to, const std::string& body) { return false; }

public: // iaxvoiplink only
  void setHost(const std::string& host) { _host = host; }
  void setUser(const std::string& user) { _user = user; }
  void setPass(const std::string& pass) { _pass = pass; }

private:
  /**
   * Find a iaxcall by iax session number
   * @param session an iax_session valid pointer
   * @return iaxcall or 0 if not found
   */
  IAXCall* iaxFindCallBySession(struct iax_session* session);

  /**
   * Handle IAX Event for a call
   * @param event An iax_event pointer
   * @param call  An IAXCall pointer 
   */
  void iaxHandleCallEvent(iax_event* event, IAXCall* call);

  /**
   * Handle IAX Registration Reply event
   * @param event An iax_event pointer
   */
  void iaxHandleRegReply(iax_event* event);

  EventThread* _evThread;
  /** registration session : 0 if not register */
  struct iax_session* _regSession;

  /** IAX Host */
  std::string _host;
  /** IAX User */
  std::string _user;
  /** IAX Password */
  std::string _pass;
  
};

#endif
