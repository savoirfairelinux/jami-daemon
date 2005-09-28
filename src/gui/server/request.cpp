/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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

#include "request.h"
#include "guiserver.h"
#include "subcall.h"

ResponseMessage
RequestCall::execute()
{
  if ( GUIServer::instance().outgoingCall(_sequenceId, _callId, _destination) ) {
    return message("150", "Trying");
  }
  return message("500","Server Error");
}

ResponseMessage
RequestAnswer::execute()
{
  if ( GUIServer::instance().answerCall(_callId) ) {
    return message("200", "OK");
  }
  return message("500","Server Error");
}

ResponseMessage
RequestRefuse::execute()
{
  if ( GUIServer::instance().refuseCall(_callId) ) {
    return message("200", "OK");
  }
  return message("500","Server Error");
}

ResponseMessage
RequestHold::execute()
{
  if ( GUIServer::instance().holdCall(_callId) ) {
    return message("200", "OK");
  }
  return message("500","Server Error");
}

ResponseMessage
RequestUnhold::execute()
{
  if ( GUIServer::instance().unholdCall(_callId) ) {
    return message("200", "OK");
  }
  return message("500","Server Error");
}

ResponseMessage
RequestTransfer::execute()
{
  return message("200","TODO");
}

ResponseMessage
RequestHangup::execute()
{
  if ( GUIServer::instance().hangupCall(_callId) ) {
    return message("200", "OK");
  }
  return message("500", "Hangup Error");
}

ResponseMessage
RequestHangupAll::execute()
{
  if ( GUIServer::instance().hangupAll() ) {
    return message("200", "OK");
  }
  return message("500", "Hangup Error");
}

RequestDTMF::RequestDTMF(const std::string &sequenceId, 
    const TokenList& argList) : RequestGlobalCall(sequenceId, argList)
{
  TokenList::iterator iter = _argList.begin();

  // check for the dtmf key
  bool argsAreValid = false;
  if (iter != _argList.end() && (*iter).length()==1) {
    _dtmfKey = *iter;
    _argList.pop_front();
    argsAreValid = true;
  }
  if (!argsAreValid) {
    throw RequestConstructorException();
  }
}

ResponseMessage
RequestDTMF::execute()
{
  if ( GUIServer::instance().dtmfCall(_callId, _dtmfKey) ) {
    return message("200", "OK");
  }
  return message("500", "DTMF Error");
}

RequestPlayDtmf::RequestPlayDtmf(const std::string &sequenceId, 
    const TokenList& argList) : RequestGlobal(sequenceId, argList)
{

  TokenList::iterator iter = _argList.begin();

  // check for the dtmf key
  bool argsAreValid = false;
  if (iter != _argList.end() && (*iter).length()==1) {
    _dtmfKey = *iter;
    _argList.pop_front();
    argsAreValid = true;
  }
  if (!argsAreValid) {
    throw RequestConstructorException();
  }
}

ResponseMessage
RequestPlayDtmf::execute()
{
  if ( GUIServer::instance().playDtmf(_dtmfKey[0]) ) {
    return message("200", "OK");
  }
  return message("500", "DTMF Error");
}

ResponseMessage
RequestPlayTone::execute()
{
  if ( GUIServer::instance().playTone() ) {
    return message("200", "OK");
  }
  return message("500", "DTMF Error");
}

ResponseMessage
RequestMute::execute()
{
  GUIServer::instance().mute();
  return message("200","OK");
}

ResponseMessage
RequestUnmute::execute()
{
  GUIServer::instance().unmute();
  return message("200","OK");
}

ResponseMessage
RequestVersion::execute()
{
  return message("200",GUIServer::instance().version());
}

ResponseMessage
RequestQuit::execute()
{
  GUIServer::instance().quit();
  return message("200", "Quitting");
}

