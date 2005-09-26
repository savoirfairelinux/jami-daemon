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

#include "guiserver.h"
#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "responsemessage.h"

// default constructor
GUIServerImpl::GUIServerImpl()
{
  _shouldQuit = false;
}

// destructor
GUIServerImpl::~GUIServerImpl()
{
}

int
GUIServerImpl::exec() {
  return _requestManager.exec();
}


/** 
 * SubCall operations
 *  insert
 *  remove
 */
void 
GUIServerImpl::insertSubCall(short id, SubCall& subCall) {
  _callMap[id] = subCall;
}

void
GUIServerImpl::removeSubCall(short id) {
  _callMap.erase(id);
}

/**
 * Retreive the subcall or send 0
 */
std::string 
GUIServerImpl::getSequenceIdFromId(short id) {
  CallMap::iterator iter = _callMap.find(id);
  if (iter != _callMap.end()) {
    return iter->second.sequenceId();
  }
  return "seq0";
}

short
GUIServerImpl::getIdFromCallId(const std::string& callId) 
{
  CallMap::iterator iter = _callMap.begin();
  while (iter != _callMap.end()) {
    if (iter->second.callId()==callId) {
      return iter->first;
    }
    iter++;
  }
  throw std::runtime_error("No match for this CallId");
}

bool 
GUIServerImpl::outgoingCall (const std::string& seq, const std::string& callid, const std::string& to) 
{
  short serverCallId = GuiFramework::outgoingCall(to);
  if ( serverCallId ) {
    SubCall subcall(seq, callid);
    insertSubCall(serverCallId, subcall);
    return true;
  } else {
    return false;
  }
}

bool 
GUIServerImpl::answerCall(const std::string& callId) 
{
  try {
    short id = getIdFromCallId(callId);
    if (GuiFramework::answerCall(id)) {
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

bool
GUIServerImpl::refuseCall(const std::string& callId) 
{
  try {
    short id = getIdFromCallId(callId);
    if (GuiFramework::refuseCall(id)) {
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

bool
GUIServerImpl::holdCall(const std::string& callId) 
{
  try {
    short id = getIdFromCallId(callId);
    if (GuiFramework::onHoldCall(id)) {
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

bool
GUIServerImpl::unholdCall(const std::string& callId) 
{
  try {
    short id = getIdFromCallId(callId);
    if (GuiFramework::offHoldCall(id)) {
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

bool
GUIServerImpl::hangupCall(const std::string& callId) 
{
  try {
    short id = getIdFromCallId(callId);
    if (GuiFramework::hangupCall(id)) {
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

bool 
GUIServerImpl::dtmfCall(const std::string& callId, const std::string& dtmfKey) 
{
  try {
    short id = getIdFromCallId(callId);
    char code = dtmfKey[0];
    return GuiFramework::sendDtmf(id, code);
  } catch(...) {
    return false;
  }
  return false;
}

int 
GUIServerImpl::incomingCall (short id) 
{
  std::ostringstream responseMessage, callId;
  callId << "s" << id;
  responseMessage << "acc1 " << callId.str() << " call";

  SubCall subcall("seq0", callId.str());

  insertSubCall(id, subcall);

  _requestManager.sendResponse(ResponseMessage("001", "seq0", responseMessage.str()));

  return 0;
}

void  
GUIServerImpl::peerAnsweredCall (short id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    _requestManager.sendResponse(ResponseMessage("200", iter->second.sequenceId(), "OK"));
  } else {
    std::ostringstream responseMessage;
    responseMessage << "Peer Answered Call: " << id;
    _requestManager.sendResponse(ResponseMessage("500", "seq0", responseMessage.str()));
  }
}

int  
GUIServerImpl::peerRingingCall (short id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    _requestManager.sendResponse(ResponseMessage("151", iter->second.sequenceId(), "Ringing"));
  } 
  return 0;
}

int  
GUIServerImpl::peerHungupCall (short id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    std::ostringstream responseMessage;
    responseMessage << iter->second.callId() << " hangup";

    _requestManager.sendResponse(ResponseMessage("250", "seq0", responseMessage.str()));
    
    // remove this call...
    _callMap.erase(id);
  }
  return 0;
}

void  
GUIServerImpl::displayTextMessage (short id, const std::string& message) 
{
  std::ostringstream responseMessage;
  std::string seq = getSequenceIdFromId(id);
  responseMessage << "s" << id << " text message: " + message;
  _requestManager.sendResponse(ResponseMessage("700", seq, responseMessage.str()));
}

void  
GUIServerImpl::displayErrorText (short id, const std::string& message) 
{
  std::ostringstream responseMessage;
  std::string seq = getSequenceIdFromId(id);
  responseMessage << "s" << id << " error text: " << message;
  _requestManager.sendResponse(ResponseMessage("700", seq, responseMessage.str()));
}

void  
GUIServerImpl::displayError (const std::string& error) 
{
  std::ostringstream responseMessage;
  responseMessage << "error: " << error;
  _requestManager.sendResponse(ResponseMessage("700", "seq0", responseMessage.str()));
}

void  
GUIServerImpl::displayStatus (const std::string& status) 
{
  std::ostringstream responseMessage;
  responseMessage << "status: " + status;
  _requestManager.sendResponse(ResponseMessage("700", "seq0", responseMessage.str()));
}

void  
GUIServerImpl::displayContext (short id) 
{
  std::ostringstream responseMessage;
  responseMessage << "s" << id;
  std::string seq = getSequenceIdFromId(id);
  _requestManager.sendResponse(ResponseMessage("700", seq, responseMessage.str()));
}

std::string  
GUIServerImpl::getRingtoneFile (void) 
{
  return std::string("");
}

void  
GUIServerImpl::setup (void) 
{
}

void  
GUIServerImpl::startVoiceMessageNotification (void) 
{
}

void  
GUIServerImpl::stopVoiceMessageNotification (void) 
{
}
