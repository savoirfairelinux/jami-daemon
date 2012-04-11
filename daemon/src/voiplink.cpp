/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "call.h"
#include "logger.h"
#include "voiplink.h"

VoIPLink::VoIPLink() : callMap_(), callMapMutex_(), handlingEvents_(false) {}

VoIPLink::~VoIPLink()
{
    ost::MutexLock m(callMapMutex_);

    for (CallMap::const_iterator iter = callMap_.begin();
            iter != callMap_.end(); ++iter)
        delete iter->second;

    callMap_.clear();

}

void VoIPLink::addCall(Call* call)
{
    if (call and getCall(call->getCallId()) == NULL) {
        ost::MutexLock m(callMapMutex_);
        callMap_[call->getCallId()] = call;
    }
}

void VoIPLink::removeCall(const std::string& id)
{
    ost::MutexLock m(callMapMutex_);

    DEBUG("VoipLink: removing call %s from list", id.c_str());

    delete callMap_[id];
    callMap_.erase(id);
}

Call* VoIPLink::getCall(const std::string& id)
{
    ost::MutexLock m(callMapMutex_);
    CallMap::iterator iter = callMap_.find(id);

    if (iter != callMap_.end())
        return iter->second;
    else
        return NULL;
}
