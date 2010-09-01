/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

Call::Call (const CallID& id, Call::CallType type)
        : _callMutex()
        , _audioStarted (false)
        , _localIPAddress ("")
        , _localAudioPort (0)
        , _localExternalAudioPort (0)
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
    CallState state = getState();
    ConnectionState connection = getConnectionState ();
    CallType type = _type;
    std::string state_str;

    switch (state) {

        case Active:

            switch (connection) {

                case Ringing:
                    (type == Incoming) ? state_str = "INCOMING":state_str = "RINGING";
                    break;

                case Connected:
                    state_str = "CURRENT";
                    break;

                default:
                    state_str = "CURRENT";
                    break;
            }

            break;

        case Hold:
            state_str = "HOLD";
            break;

        case Busy:
            state_str = "BUSY";
            break;

        case Inactive:

            switch (connection) {

                case Ringing:
                    (type == Incoming) ? state_str = "INCOMING":state_str = "RINGING";
                    break;

                case Connected:
                    state_str = "CURRENT";
                    break;

                default:
                    state_str = "INACTIVE";
                    break;
            }

            break;

        case Conferencing:
            state_str = "CONFERENCING";
            break;

        case Refused:

        case Error:

        default:
            state_str = "FAILURE";
            break;
    }

    return state_str;
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

void
Call::setAudioStart (bool start)
{
    ost::MutexLock m (_callMutex);
    _audioStarted = start;
}

bool
Call::isAudioStarted()
{
    ost::MutexLock m (_callMutex);
    return _audioStarted;
}


bool
Call::setRecording()
{
    _debug ("Call: Set recording");

    bool recordStatus = Recordable::recAudio.isRecording();

    Recordable::recAudio.setRecording();

    // Start recording
    if (!recordStatus) {

        _debug ("Call: Call not recording yet, set ringbuffers");

        MainBuffer *mbuffer = Manager::instance().getMainBuffer();
        CallID process_id = Recordable::recorder.getRecorderID();

        mbuffer->bindHalfDuplexOut (process_id, _id);
        mbuffer->bindHalfDuplexOut (process_id);

        Recordable::recorder.start();
    }
    // Stop recording
    else {

        _debug ("Call: Stop recording");

        MainBuffer *mbuffer = Manager::instance().getMainBuffer();
        CallID process_id = Recordable::recorder.getRecorderID();

        mbuffer->unBindHalfDuplexOut (process_id, _id);
        mbuffer->unBindHalfDuplexOut (process_id);

    }

    Manager::instance().getMainBuffer()->stateInfo();

    return recordStatus;
}
