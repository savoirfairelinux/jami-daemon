/*
 *  Copyright (C) 2005-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <string>

#include "user_cfg.h"
#include "voiplink.h"
#include "manager.h"

VoIPLink::VoIPLink(const AccountID& accountID) : _accountID(accountID), _localIPAddress("127.0.0.1"), _localPort(0), _registrationError(""), _initDone(false)
{
}

VoIPLink::~VoIPLink (void) 
{
  clearCallMap();
}

bool
VoIPLink::addCall(Call* call)
{
  if (call) {
    if (getCall(call->getCallId()) == 0) {
      ost::MutexLock m(_callMapMutex);
      _callMap[call->getCallId()] = call;
    }
  }  
  return false;
}

bool
VoIPLink::removeCall(const CallID& id)
{
  ost::MutexLock m(_callMapMutex);
  if (_callMap.erase(id)) {
    return true;
  }  
  return false;
}

Call*
VoIPLink::getCall(const CallID& id)
{
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    return iter->second;
  }
  return 0;
}

bool
VoIPLink::clearCallMap()
{
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.begin();
  while( iter != _callMap.end() ) {
    // if (iter) ?
    delete iter->second; iter->second = 0;
    iter++;
  }
  _callMap.clear();
  return true;
}

void
VoIPLink::setRegistrationState(const enum RegistrationState state, const std::string& errorMessage)
{
  /** @todo Push to the GUI when state changes */
  _registrationState = state;
  _registrationError = errorMessage;

  switch (state) {
  case Registered:
    Manager::instance().registrationSucceed(getAccountID());
    break;
  case Trying:
    //Manager::instance(). some function to say that
    break;
  case Error:
    Manager::instance().registrationFailed(getAccountID());
    break;
  case Unregistered:
    break;
  }
}

void
VoIPLink::setRegistrationState(const enum RegistrationState state)
{
  setRegistrationState(state, "");
}

// NOW
void
VoIPLink::subscribePresenceForContact(Contact* contact)
{
	// Nothing to do if presence is not supported
	// or the function will be overidden
	_debug("Presence subscription not supported for account\n");
}

void
VoIPLink::publishPresenceStatus(std::string status)
{
	// Nothing to do if presence is not supported
	// or the function will be overidden
	_debug("Presence publication not supported for account\n");
}
