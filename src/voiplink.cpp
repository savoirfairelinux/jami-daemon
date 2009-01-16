/*
 *  Copyright (C) 2005-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#include "user_cfg.h"
#include "voiplink.h"
#include "manager.h"

VoIPLink::VoIPLink(const AccountID& accountID) : _accountID(accountID), _localIPAddress("127.0.0.1"), _localPort(0),  _initDone(false) 
{
    setRegistrationState(VoIPLink::Unregistered);
}

VoIPLink::~VoIPLink (void) 
{
    clearCallMap();
}

bool VoIPLink::addCall(Call* call)
{
    if (call) {
        if (getCall(call->getCallId()) == 0) {
            ost::MutexLock m(_callMapMutex);
            _callMap[call->getCallId()] = call;
        }
    }  
    return false;
}

bool VoIPLink::removeCall(const CallID& id)
{
    ost::MutexLock m(_callMapMutex);
    if (_callMap.erase(id)) {
        return true;
    }  
    return false;
}

Call* VoIPLink::getCall(const CallID& id)
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

void VoIPLink::setRegistrationState(const RegistrationState state)
{
    _registrationState = state;
    // Notify the client
    Manager::instance().connectionStatusNotification( );
}
