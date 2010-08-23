/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "user_cfg.h"
#include "voiplink.h"
#include "manager.h"

VoIPLink::VoIPLink (const AccountID& accountID) : _accountID (accountID), _localPort (0),  _initDone (false)
{
}

VoIPLink::~VoIPLink (void)
{
    clearCallMap();
}

bool VoIPLink::addCall (Call* call)
{
    if (call) {
        if (getCall (call->getCallId()) == NULL) {
            ost::MutexLock m (_callMapMutex);
            _callMap[call->getCallId() ] = call;
        }
    }

    return false;
}

bool VoIPLink::removeCall (const CallID& id)
{
    ost::MutexLock m (_callMapMutex);

    if (_callMap.erase (id)) {
        return true;
    }

    return false;
}

Call* VoIPLink::getCall (const CallID& id)
{
    ost::MutexLock m (_callMapMutex);
    CallMap::iterator iter = _callMap.find (id);

    if (iter != _callMap.end()) {
        return iter->second;
    }

    return NULL;
}

bool VoIPLink::clearCallMap()
{
    ost::MutexLock m (_callMapMutex);
    CallMap::iterator iter = _callMap.begin();

    while (iter != _callMap.end()) {
        // if (iter) ?
        delete iter->second;
        iter->second = 0;
        iter++;
    }

    _callMap.clear();

    return true;
}

Account* VoIPLink::getAccountPtr (void)
{
    AccountID id;

    id = getAccountID();
    return Manager::instance().getAccount (id);
}
