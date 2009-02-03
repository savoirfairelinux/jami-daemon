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

Call::Call(const CallID& id, Call::CallType type)
           : _callMutex()
           , _codecMap()
           , _audioCodec()
           , _audioStarted(false)    
           , _localIPAddress("") 
           , _localAudioPort(0)
           , _localExternalAudioPort(0)
           , _remoteIPAddress("")
           , _remoteAudioPort(0)
           , _id(id) 
           , _type(type) 
           , _connectionState(Call::Disconnected)
           , _callState(Call::Inactive)
           , _peerName()
           , _peerNumber()
{
    time_t rawtime;
    struct tm * timeinfo;

    rawtime = std::time(NULL);
    timeinfo = localtime ( &rawtime );

    std::stringstream out;

    out << timeinfo->tm_year+1900;
    if (timeinfo->tm_mon < 9) // january is 01, not 1
      out << 0;
    out << timeinfo->tm_mon+1;
    if (timeinfo->tm_mday < 10) // 01 02 03, not 1 2 3
      out << 0;
    out << timeinfo->tm_mday;
    if (timeinfo->tm_hour < 10) // 01 02 03, not 1 2 3
      out << 0;
    out << timeinfo->tm_hour;
    if (timeinfo->tm_min < 10) // 01 02 03, not 1 2 3
      out << 0;
    out << timeinfo->tm_min;
    if (timeinfo->tm_sec < 10) // 01 02 03,  not 1 2 3
      out << 0;
    out << timeinfo->tm_sec;

    _filename = out.str();

    printf("Call::constructor filename for this call %s \n",_filename.c_str());
 
    FILE_TYPE fileType = FILE_WAV;
    SOUND_FORMAT soundFormat = INT16;
    recAudio.setRecordingOption(_filename.c_str(),fileType,soundFormat,44100);

    _debug("CALL::Constructor for this clss is called \n");    
}


Call::~Call()
{
   _debug("CALL::~Call(): Destructor for this clss is called \n");
   
   if(recAudio.isOpenFile()) {
     _debug("CALL::~Call(): A recording file is open, close it \n");
     recAudio.closeFile();
   }
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

AudioCodecType 
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

void
Call::setRecording()
{
  recAudio.setRecording();
}

bool
Call::isRecording()
{
  return recAudio.isRecording();
}

