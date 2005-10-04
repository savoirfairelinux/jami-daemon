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

#include "../../global.h" // for VERSION and PROGNAME
#include "responsemessage.h"

// default constructor
GUIServerImpl::GUIServerImpl() : _getEventsSequenceId("seq0")
{
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
 * Retreive the sequenceId or send default sequenceId
 */
std::string 
GUIServerImpl::getSequenceIdFromId(short id) {
  CallMap::iterator iter = _callMap.find(id);
  if (iter != _callMap.end()) {
    return iter->second.sequenceId();
  }
  return _getEventsSequenceId;
}
/**
 * Retreive the string callid from the id
 */
std::string 
GUIServerImpl::getCallIdFromId(short id) {
  CallMap::iterator iter = _callMap.find(id);
  if (iter != _callMap.end()) {
    return iter->second.callId();
  }
  throw std::runtime_error("No match for this id");
}

bool
GUIServerImpl::getCurrentCallId(std::string& callId) {
  bool returnValue = false;
  try {
    short id = GuiFramework::getCurrentId();
    if (id!=0) {
      callId = getCallIdFromId(id);
      returnValue = true;
    }
  } catch(...) {
    // nothing, it's false
  }
  return returnValue;
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
GUIServerImpl::getEvents(const std::string& sequenceId)
{
  _getEventsSequenceId=sequenceId;
  return true;
}
bool
GUIServerImpl::sendGetEventsEnd()
{
  _requestManager.sendResponse(ResponseMessage("202", _getEventsSequenceId,
"getcallstatus request stopped me"));
  return true;
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
GUIServerImpl::transferCall(const std::string& callId, const std::string& to)
{
  try {
    short id = getIdFromCallId(callId);
    if (GuiFramework::transferCall(id, to)) {
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
      _callMap.erase(id);
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

bool
GUIServerImpl::hangupAll()
{
  bool result = true;
  short id;
  CallMap::iterator iter = _callMap.begin();
  // try to hangup every call, even if one fail
  while(iter!=_callMap.end()) {
    id = iter->first;
    if (!GuiFramework::hangupCall(id)) {
      result = false;
    } else {
      _callMap.erase(id);
    }
    iter++;
  }
  return result;
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

/**
 * Version constant are in global.h
 * @return the version (name number)
 */
std::string 
GUIServerImpl::version() 
{
  std::ostringstream version;
  version << PROGNAME << " " << VERSION;
  return version.str();
}


int 
GUIServerImpl::incomingCall (short id) 
{
  _debug("ERROR: GUIServerImpl::incomingCall(%d) should not be call\n",id);
  return 0;
}

int 
GUIServerImpl::incomingCall (short id, const std::string& accountId, const std::string& from) 
{
  TokenList arg;
  std::ostringstream callId;
  callId << "s" << id;
  arg.push_back(callId.str());
  arg.push_back(accountId);
  arg.push_back(from);
  arg.push_back("call");

  SubCall subcall(_getEventsSequenceId, callId.str());

  insertSubCall(id, subcall);

  _requestManager.sendResponse(ResponseMessage("001", _getEventsSequenceId,
arg));

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
    _requestManager.sendResponse(ResponseMessage("500", _getEventsSequenceId,
responseMessage.str()));
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

    _requestManager.sendResponse(ResponseMessage("002", _getEventsSequenceId,
responseMessage.str()));
    
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
  _requestManager.sendResponse(ResponseMessage("700", _getEventsSequenceId,
responseMessage.str()));
}

void  
GUIServerImpl::displayStatus (const std::string& status) 
{
  std::ostringstream responseMessage;
  responseMessage << "status: " + status;
  _requestManager.sendResponse(ResponseMessage("700", _getEventsSequenceId,
responseMessage.str()));
}

void  
GUIServerImpl::displayContext (short id) 
{
  std::ostringstream responseMessage;
  responseMessage << "s" << id;
  std::string seq = getSequenceIdFromId(id);
  _requestManager.sendResponse(ResponseMessage("700", seq, responseMessage.str()));
}

void  
GUIServerImpl::setup (void) 
{
}

void
GUIServerImpl::sendVoiceNbMessage(const std::string& nb_msg)
{
  _requestManager.sendResponse(ResponseMessage("020", _getEventsSequenceId, nb_msg));
}

void 
GUIServerImpl::sendMessage(const std::string& code, const std::string& seqId, TokenList& arg) 
{
  _requestManager.sendResponse(ResponseMessage(code, seqId, arg));
}

void 
GUIServerImpl::sendCallMessage(const std::string& code, 
  const std::string& sequenceId, 
  short id, 
  TokenList arg) 
{
  try {
    std::string callid = getCallIdFromId(id);
    arg.push_front(callid);
    _requestManager.sendResponse(ResponseMessage(code, sequenceId, arg));
  } catch(...) {
    // no callid found
  }
}

void 
GUIServerImpl::update()
{
  
}
