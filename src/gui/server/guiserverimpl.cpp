/*
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
  _sessionPort = 3999;
}

// destructor
GUIServerImpl::~GUIServerImpl()
{
}

int
GUIServerImpl::exec() {
  return _requestManager.exec(_sessionPort);
}

/** 
 * SubCall operations
 *  insert
 *  remove
 */
void 
GUIServerImpl::insertSubCall(const CallID& id, const CallID& seq) {
  
  _callMap[id] = seq;
}

void
GUIServerImpl::removeSubCall(const CallID& id) {
  _callMap.erase(id);
}

/**
 * Retreive the sequenceId or send default sequenceId
 */
std::string 
GUIServerImpl::getSequenceIdFromId(const CallID& id) {
  CallMap::iterator iter = _callMap.find(id);
  if (iter != _callMap.end()) {
    return iter->second;
  }
  return _getEventsSequenceId;
}

bool
GUIServerImpl::getCurrentCallId(std::string& callId) {
  bool returnValue = false;
  try {
    CallID id = GuiFramework::getCurrentId();
    if (id != "") {
      callId = id;
      returnValue = true;
    }
  } catch(...) {
    // nothing, it's false
  }
  return returnValue;
}

bool 
GUIServerImpl::getEvents(const std::string& sequenceId)
{
  _getEventsSequenceId=sequenceId;

  // didn't loaded the setup?
  // 010 <CSeq> Load setup
  if ( !GuiFramework::hasLoadedSetup() ) {
    _requestManager.sendResponse(ResponseMessage("010", sequenceId, _("Load setup")));
  }

  TokenList tk;
  std::ostringstream percentSpkr;
  // 021 <CSeq> <percentage of speaker volume> Speaker volume changed.
  percentSpkr << GuiFramework::getSpkrVolume();
  tk.push_back(percentSpkr.str());
  tk.push_back(_("Speaker volume changed"));
  _requestManager.sendResponse(ResponseMessage("021", sequenceId, tk));

  // 022 <CSeq> <percentage of microphone volume> Microphone volume changed.
  tk.clear();
  std::ostringstream percentMic;
  percentMic << GuiFramework::getMicVolume();
  tk.push_back(percentMic.str());
  tk.push_back(_("Microphone volume changed"));
  _requestManager.sendResponse(ResponseMessage("022", sequenceId, tk));

  // try to register, if not done yet...
  GuiFramework::getEvents();
  return true;
}

bool
GUIServerImpl::sendGetEventsEnd()
{
  if ( _getEventsSequenceId != "seq0" ) {
    _requestManager.sendResponse(ResponseMessage("202", _getEventsSequenceId,
_("getcallstatus request stopped me")));
  }
  return true;
}

bool 
GUIServerImpl::outgoingCall(const std::string& seq, 
 const std::string& account,
 const std::string& callid, 
 const std::string& to) 
{
  insertSubCall(callid, seq);
  return GuiFramework::outgoingCall(account, callid, to);
}

bool 
GUIServerImpl::answerCall(const std::string& callId) 
{
  return GuiFramework::answerCall(callId);
}

bool
GUIServerImpl::refuseCall(const std::string& callId) 
{
  return GuiFramework::refuseCall(callId);
}
bool 
GUIServerImpl::transferCall(const std::string& callId, const std::string& to)
{
  return GuiFramework::transferCall(callId, to);
}

bool
GUIServerImpl::holdCall(const std::string& callId) 
{
  return GuiFramework::onHoldCall(callId);
}

bool
GUIServerImpl::unholdCall(const std::string& callId) 
{
  return GuiFramework::offHoldCall(callId);
}

bool
GUIServerImpl::hangupCall(const std::string& callId) 
{
  if ( GuiFramework::hangupCall(callId) ) {
    removeSubCall(callId);
    return true;
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
  CallMap::iterator iter = _callMap.begin();
  // try to hangup every call, even if one fail
  while(iter!=_callMap.end()) {
    if (!GuiFramework::hangupCall(iter->first)) {
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
  return GuiFramework::sendDtmf(callId, dtmfKey[0]);
}

/**
 * Version constant are in global.h
 * @return the version (name number)
 */
std::string 
GUIServerImpl::version() 
{
  std::ostringstream programVersion;
  programVersion << PROGNAME << " " << SFLPHONED_VERSION;
  return programVersion.str();
}


bool
GUIServerImpl::incomingCall(const AccountID& accountId, const CallID& id, const std::string& from) 
{
  TokenList arg;
  arg.push_back(id);
  arg.push_back(accountId);
  arg.push_back(from);
  arg.push_back("call");

  insertSubCall(id, _getEventsSequenceId);
  _requestManager.sendResponse(ResponseMessage("001", _getEventsSequenceId, arg));

  return 0;
}

void
GUIServerImpl::incomingMessage(const std::string& account, const std::string& message) {
  TokenList arg;
  arg.push_back(account);
  arg.push_back(message);
  _requestManager.sendResponse(ResponseMessage("030", _getEventsSequenceId, arg));
}

void  
GUIServerImpl::peerAnsweredCall (const CallID& id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    _requestManager.sendResponse(ResponseMessage("200", iter->second, _("Established")));
  }
}

void
GUIServerImpl::peerRingingCall (const CallID& id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    _requestManager.sendResponse(ResponseMessage("151", iter->second, _("Ringing")));
  } 
}

void
GUIServerImpl::peerHungupCall (const CallID& id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    TokenList tk;
    tk.push_back(id);
    tk.push_back("hangup");

    _requestManager.sendResponse(ResponseMessage("002", _getEventsSequenceId,tk));
    
    // remove this call...
    removeSubCall(id);
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
GUIServerImpl::displayTextMessage (const CallID& id, const std::string& message) 
{
  TokenList tk;
  tk.push_back(id);
  tk.push_back(message);
  tk.push_back("Text message");
  _requestManager.sendResponse(ResponseMessage("102", _getEventsSequenceId, tk));
}

void  
GUIServerImpl::displayErrorText (const CallID& id, const std::string& message) 
{
  TokenList tk;
  tk.push_back(id);
  tk.push_back(message);
  tk.push_back("Error");
  _requestManager.sendResponse(ResponseMessage("104", _getEventsSequenceId, tk));
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
GUIServerImpl::sendVoiceNbMessage(const AccountID& accountId, const std::string& nb_msg)
{
  TokenList tk;
  tk.push_back(accountId);
  tk.push_back(nb_msg);
  _requestManager.sendResponse(ResponseMessage("020", _getEventsSequenceId, tk));
}

void
GUIServerImpl::sendRegistrationState(const AccountID& accountid, bool state) 
{
  TokenList tk;
  tk.push_back(accountid);
  if (state == true) {
    tk.push_back(_("Registration succeed"));
  _requestManager.sendResponse(ResponseMessage("003", _getEventsSequenceId, tk));
  } else {
    tk.push_back(_("Registration failed"));
  _requestManager.sendResponse(ResponseMessage("004", _getEventsSequenceId, tk));
  }
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
  const CallID& id, 
  TokenList arg) 
{
  try {
    arg.push_front(id);
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
GUIServerImpl::callFailure(const CallID& id)
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    TokenList tk;
    tk.push_back(id);
    tk.push_back("Wrong number");

    _requestManager.sendResponse(ResponseMessage("504", iter->second, tk));
    removeSubCall(id);
  }
}
