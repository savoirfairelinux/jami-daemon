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
#include "iax-client.h"

class AudioCodec;

/**
	@author Yan Morin <yan.morin@gmail.com>
  VoIPLink contains a thread that listen to external events and 
  contains IAX Call related functions
*/
class IAXVoIPLink : public VoIPLink
{
public:
    IAXVoIPLink(const AccountID& accountID);

    ~IAXVoIPLink();

  void getEvent (void) { }
  bool init (void);
  bool checkNetwork (void) { return false; }
  void terminate (void);

  bool setRegister (void) { return false; }
  bool setUnregister (void) { return false; }

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
};

#endif
