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
#include "responsemessage.h"
#include "request.h"

void 
TCPSessionReader::run() 
{
  while(!testCancel() && good()) {
    std::string output;
    std::getline(*this, output);
    _gui->pushRequestMessage(output);
  }
}

void 
TCPSessionWriter::run() 
{
  while (!testCancel() && good()) {
   *this << _gui->popResponseMessage() << std::endl;
  }
}

// default constructor
GUIServer::GUIServer()
{
  _factory.registerAll();}

// destructor
GUIServer::~GUIServer()
{
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
      sessionIn = new TCPSessionReader(aServer, this);
      sessionOut = new TCPSessionWriter(aServer, this);
      
      sessionIn->start();
      sessionOut->start();
      
      // wait for the first message
      std::cout << "accepting connection..." << std::endl;
      
      while(sessionIn->good() && sessionOut->good()) {
        request = popRequest();
        output = request->execute(*this);
        pushResponseMessage(output);
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
  std::cout << "pushRequestMessage" << std::endl;
  _mutex.enterMutex();
  _requests.push_back(_factory.create(request));
  _mutex.leaveMutex();
}

Request *
GUIServer::popRequest() 
{
  Request *request = 0;
  while(!request) {
    _mutex.enterMutex();
    if ( _requests.size() ) {
      request = _requests.front();
      _requests.pop_front();
    }
    _mutex.leaveMutex();
  }
  return request;
}

void 
GUIServer::pushResponseMessage(const ResponseMessage &response) 
{
  std::cout << "pushResponseMessage" << std::endl;
  _mutex.enterMutex();
  _responses.push_back(response.toString());
  _mutex.leaveMutex();

  // remove the request from the list 
  if (response.isFinal()) {
    removeRequest(response.sequenceId());
  }
}

std::string 
GUIServer::popResponseMessage() 
{
  bool canPop = false;
  std::string message;
  while(!canPop) {
    _mutex.enterMutex();
    if ( _responses.size() ) {
      message = _responses.front();
      _responses.pop_front();
      canPop = true;
    }
    _mutex.leaveMutex();
  }
  return message;
}

/**
 * Remove a request with its sequence id
 */
void 
GUIServer::removeRequest(const std::string& sequenceId)
{
  std::list<Request*>::iterator iter;
  for(iter=_requests.begin(); iter!=_requests.end(); iter++) {
    if ( (*iter)->sequenceId() == sequenceId ) {
      delete (*iter);
    }
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
  std::string responseMessage = "s";
  responseMessage += id + " text message: " + message;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage));
}

void  
GUIServer::displayErrorText (short id, const std::string& message) 
{
  std::string responseMessage = "s";
  responseMessage += id + " error text: " + message;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage));
}

void  
GUIServer::displayError (const std::string& error) 
{
  std::string responseMessage = "error: " + error;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage));
}

void  
GUIServer::displayStatus (const std::string& status) 
{
  std::string responseMessage = "status: " + status;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage));
}

void  
GUIServer::displayContext (short id) 
{
  std::string responseMessage = "s";
  responseMessage += id;
  pushResponseMessage(ResponseMessage("700", "seq0", responseMessage));
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

int  
GUIServer::selectedCall (void) 
{
  return 0;
}

bool  
GUIServer::isCurrentId (short) 
{
  return false;
}

void  
GUIServer::startVoiceMessageNotification (void) 
{
}

void  
GUIServer::stopVoiceMessageNotification (void) 
{
}
