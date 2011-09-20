/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
#include "manager.h"
#include "audio/mainbuffer.h"

const char * const Call::DEFAULT_ID = "audiolayer_id";

Call::Call (const std::string& id, Call::CallType type)
    : _callMutex()
    , _localIPAddress ("")
    , _localAudioPort (0)
    , _localVideoPort (0)
    , _id (id)
    , _confID ("")
    , _type (type)
    , _connectionState (Call::Disconnected)
    , _callState (Call::Inactive)
    , _callConfig (Call::Classic)
    , _peerName()
    , _peerNumber()
{

}


Call::~Call()
{
}

void
Call::setConnectionState (ConnectionState state)
{
    ost::MutexLock m (_callMutex);
    _connectionState = state;
}

Call::ConnectionState
Call::getConnectionState()
{
    ost::MutexLock m (_callMutex);
    return _connectionState;
}


void
Call::setState (CallState state)
{
    ost::MutexLock m (_callMutex);
    _callState = state;
}

Call::CallState
Call::getState()
{
    ost::MutexLock m (_callMutex);
    return _callState;
}

std::string
Call::getStateStr ()
{
    switch (getState()) {
    case Active:
        switch (getConnectionState()) {
        case Ringing: 	return isIncoming() ? "INCOMING" : "RINGING";
        case Connected:
        default:		return isRecording() ? "RECORD" : "CURRENT";
        }
    case Hold:			return "HOLD";
    case Busy:			return "BUSY";
    case Inactive:
    	switch (getConnectionState()) {
    	case Ringing:	return isIncoming() ? "INCOMING" : "RINGING";
    	case Connected:	return "CURRENT";
    	default:		return "INACTIVE";
    	}
    case Conferencing:	return "CONFERENCING";
    case Refused:
    case Error:
    default:			return "FAILURE";
    }
}


const std::string&
Call::getLocalIp()
{
    ost::MutexLock m (_callMutex);
    return _localIPAddress;
}

unsigned int
Call::getLocalAudioPort()
{
    ost::MutexLock m (_callMutex);
    return _localAudioPort;
}

unsigned int
Call::getLocalVideoPort()
{
    ost::MutexLock m (_callMutex);
    return _localVideoPort;
}

bool
Call::setRecording()
{
    bool recordStatus = Recordable::recAudio.isRecording();

    Recordable::recAudio.setRecording();
    MainBuffer *mbuffer = Manager::instance().getMainBuffer();
    std::string process_id = Recordable::recorder.getRecorderID();

    if (!recordStatus) {
        mbuffer->bindHalfDuplexOut (process_id, _id);
        mbuffer->bindHalfDuplexOut (process_id);

        Recordable::recorder.start();
    } else {
        mbuffer->unBindHalfDuplexOut (process_id, _id);
        mbuffer->unBindHalfDuplexOut (process_id);
    }

    Manager::instance().getMainBuffer()->stateInfo();

    return recordStatus;
}
