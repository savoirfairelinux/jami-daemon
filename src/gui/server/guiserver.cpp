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
#include "request.h"

void 
TCPSessionIO::run() {
  std::string response;
  while(!testCancel() && good()) {
    if (isPending(ost::TCPSocket::pendingInput, 10)) {
      std::string input;
      std::getline(*this, input);
      _gui->pushRequestMessage(input);
    }
    if (_outputPool.pop(response, 10)) {
      *this << response << std::endl;
    }
  }
}

// default constructor
GUIServer::GUIServer()
{
  _factory.registerAll();
  _sessionIO = 0;
  _shouldQuit = false;
}

// destructor
GUIServer::~GUIServer()
{
  // Waiting Requests cleanup
  std::map<std::string, Request*>::iterator iter = _waitingRequests.begin();
  while (iter != _waitingRequests.end()) {
    delete iter->second; iter->second = NULL;
    iter++;
  }
 _waitingRequests.clear();
}

int
GUIServer::exec() {
  try {
    ost::InetAddress addr("127.0.0.1");
    
    //Creating a listening socket.
    ost::TCPSocket aServer(addr, 3999);
    
    std::cout << "listening on " << aServer.getLocal() << ":" << 3999 << std::endl;
    
    ResponseMessage output; // TCPStream output line
    Request *request;
    
    while (std::cin.good()) {
      
      // waiting for a new connection
      std::cout << "waiting for a new connection..." << std::endl;

      //I'm accepting an incomming connection
      _sessionIO = new TCPSessionIO(aServer, this);
      _sessionIO->start();

      // wait for the first message
      std::cout << "accepting connection..." << std::endl;

      _shouldQuit = false;
      while(_sessionIO->good() && !_shouldQuit) {
        if ( _requests.pop(request, 1000)) {
          output = request->execute(*this);
          handleExecutedRequest(request, output);
        }
      }
    }
  }
  catch(ost::Socket *e) {
    std::cerr << e->getErrorString() << std::endl;
  }

  return 0;
}

void 
GUIServer::pushRequestMessage(const std::string &request)
{
  Request *tempRequest = _factory.create(request);
  _requests.push(tempRequest);
}

void 
GUIServer::pushResponseMessage(const ResponseMessage &response) 
{
  if (_sessionIO) {
    _sessionIO->push(response.toString());
  } else {
    std::cerr << "PushResponseMessage: " << response.toString() << std::endl;
  }

  // remove the request from the list 
  if (response.isFinal()) {
    std::map<std::string, Request*>::iterator iter = _waitingRequests.find(response.sequenceId());
    if (iter != _waitingRequests.end()) {
      delete iter->second; iter->second = NULL;
      _waitingRequests.erase(iter);
    }
  }
}

/**
 * Delete the request from the list of request
 * or send it into the waitingRequest map
 */
void 
GUIServer::handleExecutedRequest(Request * const request, const ResponseMessage& response) 
{
  if (response.isFinal()) {
    delete request; request = NULL;
  } else {
    if (_waitingRequests.find(request->sequenceId()) == _waitingRequests.end()) {
      _waitingRequests[response.sequenceId()] = request;
    } else {
      // we don't deal with requests with a sequenceId already send...
      delete request; request = NULL;
    }
  }
  if (_sessionIO) {
    _sessionIO->push(response.toString());
  }
}

/** 
 * SubCall operations
 *  insert
 *  remove
 */
void 
GUIServer::insertSubCall(short id, SubCall& subCall) {
  _callMap[id] = subCall;
}

void
GUIServer::removeSubCall(short id) {
  _callMap.erase(id);
}

/**
 * Retreive the subcall or send 0
 */
std::string 
GUIServer::getSequenceIdFromId(short id) {
  CallMap::iterator iter = _callMap.find(id);
  if (iter != _callMap.end()) {
    return iter->second.sequenceId();
  }
  return "seq0";
}

short
GUIServer::getIdFromCallId(const std::string& callId) 
{
  CallMap::iterator iter = _callMap.begin();
  while (iter != _callMap.end()) {
    if (iter->second.callId()==callId) {
      return iter->first;
    }
  }
  throw std::runtime_error("No match for this CallId");
}

void 
GUIServer::hangup(const std::string& callId) {
  try {
    short id = getIdFromCallId(callId);
    // There was a problem when hanging up...
    if (!GuiFramework::hangupCall(id)) {
      throw std::runtime_error("Error when hangup");
    }
  } catch(...) {
    throw;
  }
}

int 
GUIServer::incomingCall (short id) 
{
  return 0;
}

void  
GUIServer::peerAnsweredCall (short id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    pushResponseMessage(ResponseMessage("200", iter->second.sequenceId(), "OK"));
  } else {
    std::ostringstream responseMessage;
    responseMessage << "Peer Answered Call: " << id;
    pushResponseMessage(ResponseMessage("500", "seq0", responseMessage.str()));
  }
}

int  
GUIServer::peerRingingCall (short id) 
{
  CallMap::iterator iter = _callMap.find(id);
  if ( iter != _callMap.end() ) {
    pushResponseMessage(ResponseMessage("151", iter->second.sequenceId(), "Ringing"));
  } 
  return 0;
}

int  
GUIServer::peerHungupCall (short id) 
{
  return 0;
}

void  
GUIServer::displayTextMessage (short id, const std::string& message) 
{
  std::ostringstream responseMessage;
  std::string seq = getSequenceIdFromId(id);
  responseMessage <<"s" << id << "text message: " + message;
  pushResponseMessage(ResponseMessage("700", seq, responseMessage.str()));
}

void  
GUIServer::displayErrorText (short id, const std::string& message) 
{
  std::ostringstream responseMessage;
  std::string seq = getSequenceIdFromId(id);
  responseMessage << "s" << id << " error text: " << message;
  pushResponseMessage(ResponseMessage("700", seq, responseMessage.str()));
}

void  
GUIServer::displayError (const std::string& error) 
{
  std::ostringstream responseMessage;
  responseMessage << "error: " << error;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage.str()));
}

void  
GUIServer::displayStatus (const std::string& status) 
{
  std::ostringstream responseMessage;
  responseMessage << "status: " + status;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage.str()));
}

void  
GUIServer::displayContext (short id) 
{
  std::ostringstream responseMessage;
  responseMessage << "s" << id;
  std::string seq = getSequenceIdFromId(id);
  pushResponseMessage(ResponseMessage("700", seq, responseMessage.str()));
}

std::string  
GUIServer::getRingtoneFile (void) 
{
  return std::string("");
}

void  
GUIServer::setup (void) 
{
}

void  
GUIServer::startVoiceMessageNotification (void) 
{
}

void  
GUIServer::stopVoiceMessageNotification (void) 
{
}
