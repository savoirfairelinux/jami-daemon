/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
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
 */
#include "call.h"

Call::Call(const CallID& id, Call::CallType type) : _id(id), _type(type), 
						    _localIPAddress(""),
						    _remoteIPAddress("")  
{
  _connectionState = Call::Disconnected;
  _callState = Call::Inactive;
  //_audioCodec = 0;
  _localAudioPort = 0;
  _localExternalAudioPort = 0;
  _remoteAudioPort = 0;
}


Call::~Call()
{
}

void 
Call::setConnectionState(ConnectionState state) 
{
  ost::MutexLock m(_callMutex);
  _connectionState = state;
}

Call::ConnectionState
Call::getConnectionState() 
{
  ost::MutexLock m(_callMutex);
  return _connectionState;
}


void 
Call::setState(CallState state) 
{
  ost::MutexLock m(_callMutex);
  _callState = state;
}

Call::CallState
Call::getState() 
{
  ost::MutexLock m(_callMutex);
  return _callState;
}

CodecDescriptor& 
Call::getCodecMap()
{
  return _codecMap;
}

const std::string& 
Call::getLocalIp()
{
  ost::MutexLock m(_callMutex);  
  return _localIPAddress;
}

unsigned int 
Call::getLocalAudioPort()
{
  ost::MutexLock m(_callMutex);  
  return _localAudioPort;
}

unsigned int 
Call::getRemoteAudioPort()
{
  ost::MutexLock m(_callMutex);  
  return _remoteAudioPort;
}

const std::string& 
Call::getRemoteIp()
{
  ost::MutexLock m(_callMutex);  
  return _remoteIPAddress;
}

CodecType 
Call::getAudioCodec()
{
  ost::MutexLock m(_callMutex);  
  return _audioCodec;  
}

void 
Call::setAudioStart(bool start)
{
  ost::MutexLock m(_callMutex);  
  _audioStarted = start;  
}

bool 
Call::isAudioStarted()
{
  ost::MutexLock m(_callMutex);  
  return _audioStarted;
}

