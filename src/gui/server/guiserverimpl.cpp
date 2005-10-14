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
GUIServerImpl::insertSubCall(CALLID id, SubCall& subCall) {
  _callMap[id] = subCall;
}

void
GUIServerImpl::removeSubCall(CALLID id) {
  _callMap.erase(id);
}

/**
 * Retreive the sequenceId or send default sequenceId
 */
std::string 
GUIServerImpl::getSequenceIdFromId(CALLID id) {
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
GUIServerImpl::getCallIdFromId(CALLID id) {
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
    CALLID id = GuiFramework::getCurrentId();
    if (id!=0) {
      callId = getCallIdFromId(id);
      returnValue = true;
    }
  } catch(...) {
    // nothing, it's false
  }
  return returnValue;
}

CALLID
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

  // didn't loaded the setup?
  // 010 <CSeq> Load setup
  if ( !GuiFramework::hasLoadedSetup() ) {
    _requestManager.sendResponse(ResponseMessage("010", sequenceId, "Load setup"));
  }

  TokenList tk;
  std::ostringstream percentSpkr;
  // 021 <CSeq> <percentage of speaker volume> Speaker volume changed.
  percentSpkr << GuiFramework::getSpkrVolume();
  tk.push_back(percentSpkr.str());
  tk.push_back("Speaker volume changed");
  _requestManager.sendResponse(ResponseMessage("021", sequenceId, tk));

  // 022 <CSeq> <percentage of microphone volume> Microphone volume changed.
  tk.clear();
  std::ostringstream percentMic;
  percentMic << GuiFramework::getMicVolume();
  tk.push_back(percentMic.str());
  tk.push_back("Microphone volume changed");
  _requestManager.sendResponse(ResponseMessage("022", sequenceId,tk));

  return true;
}

bool
GUIServerImpl::sendGetEventsEnd()
{
  if ( _getEventsSequenceId != "seq0" ) {
    _requestManager.sendResponse(ResponseMessage("202", _getEventsSequenceId,
"getcallstatus request stopped me"));
  }
  return true;
}

bool 
GUIServerImpl::outgoingCall (const std::string& seq, const std::string& callid, const std::string& to) 
{
  CALLID serverCallId = GuiFramework::outgoingCall(to);
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
    CALLID id = getIdFromCallId(callId);
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
    CALLID id = getIdFromCallId(callId);
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
    CALLID id = getIdFromCallId(callId);
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
    CALLID id = getIdFromCallId(callId);
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
    CALLID id = getIdFromCallId(callId);
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
    CALLID id = getIdFromCallId(callId);
    if (GuiFramework::hangupCall(id)) {
      _callMap.erase(id);
      return true;
    }
  } catch(...) {
    return false;
  }
  return false;
}

/*
 * we hangup everything in callmap, and flush it
 * @return false if atleast one hangup failed
 */
bool
GUIServerImpl::hangupAll()
{
  bool result = true;
  CALLID id;
  CallMap::iterator iter = _callMap.begin();
  // try to hangup every call, even if one fail
  while(iter!=_callMap.end()) {
    id = iter->first;
    if (!GuiFramework::hangupCall(id)) {
      result = false;
    }
    iter++;
  }
  _callMap.clear();
  return result;
}

bool 
GUIServerImpl::dtmfCall(const std::string& callId, const std::string& dtmfKey) 
{
  try {
    CALLID id = getIdFromCallId(callId);
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
GUIServerImpl::incomingCall (CALLID id, const std::string& accountId, const std::string& from) 
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

  _requestManager.sendResponse(ResponseMessage("001", _getEventsSequenceId,arg));

  return 0;
}

void  
GUIServerImpl::peerAnsweredCall (CALLID id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    _requestManager.sendResponse(ResponseMessage("200", iter->second.sequenceId(), "OK"));
  }
}

void
GUIServerImpl::peerRingingCall (CALLID id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    _requestManager.sendResponse(ResponseMessage("151", iter->second.sequenceId(), "Ringing"));
  } 
}

void
GUIServerImpl::peerHungupCall (CALLID id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    TokenList tk;
    tk.push_back(iter->second.callId());
    tk.push_back("hangup");

    _requestManager.sendResponse(ResponseMessage("002", _getEventsSequenceId,tk));
    
    // remove this call...
    _callMap.erase(id);
  }
}

void  
GUIServerImpl::displayStatus (const std::string& status) 
{
  TokenList tk;
  tk.push_back(status);
  tk.push_back("Status");
  _requestManager.sendResponse(ResponseMessage("100", _getEventsSequenceId, tk));
}

void  
GUIServerImpl::displayConfigError (const std::string& error) 
{
  TokenList tk;
  tk.push_back(error);
  tk.push_back("Config Error");
  _requestManager.sendResponse(ResponseMessage("101", _getEventsSequenceId, tk));
}

void  
GUIServerImpl::displayTextMessage (CALLID id, const std::string& message) 
{
  try {
    std::string callId = getCallIdFromId(id);
    TokenList tk;
    tk.push_back(callId);
    tk.push_back(message);
    tk.push_back("Text message");
    _requestManager.sendResponse(ResponseMessage("102", _getEventsSequenceId, tk));
  } catch(...) {
    TokenList tk;
    tk.push_back(message);
    tk.push_back("Text message");
    _requestManager.sendResponse(ResponseMessage("103", _getEventsSequenceId, tk));
  }
}

void  
GUIServerImpl::displayErrorText (CALLID id, const std::string& message) 
{
  try {
    std::string callId = getCallIdFromId(id);
    TokenList tk;
    tk.push_back(callId);
    tk.push_back(message);
    tk.push_back("Error");
    _requestManager.sendResponse(ResponseMessage("104", _getEventsSequenceId, tk));
  } catch(...) {
    displayError(message);
  }
}

void  
GUIServerImpl::displayError (const std::string& error) 
{
  TokenList tk;
  tk.push_back(error);
  tk.push_back("Error");
  _requestManager.sendResponse(ResponseMessage("105", _getEventsSequenceId, tk));
}

void
GUIServerImpl::sendVoiceNbMessage(const std::string& nb_msg)
{
  _requestManager.sendResponse(ResponseMessage("020", _getEventsSequenceId, nb_msg));
}

void
GUIServerImpl::setup() 
{
}

void 
GUIServerImpl::sendMessage(const std::string& code, const std::string& seqId, TokenList& arg) 
{
  _requestManager.sendResponse(ResponseMessage(code, seqId, arg));
}

void 
GUIServerImpl::sendCallMessage(const std::string& code, 
  const std::string& sequenceId, 
  CALLID id, 
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

void
GUIServerImpl::callFailure(CALLID id)
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    TokenList tk;
    tk.push_back(iter->second.callId());
    tk.push_back("Wrong number");

    _requestManager.sendResponse(ResponseMessage("504", iter->second.sequenceId(), tk));
  }
}
