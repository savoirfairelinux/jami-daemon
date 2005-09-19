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

ResponseMessage
RequestCall::execute(GUIServer& gui)
{
  int serverCallId = gui.outgoingCall(_destination); 
  if (serverCallId) {
    return message("150", "Trying...");
  } else {
    return message("500","Server Error");
  }
}

// default constructor
GUIServer::GUIServer()
{
  _factory = new RequestFactory();
  _factory->registerRequest< RequestSyntaxError > ("syntaxerror");
  _factory->registerRequest< RequestCall >     ("call");
  _factory->registerRequest< RequestQuit >     ("quit");
  _factory->registerRequest< RequestAnswer >   ("anwser");
  _factory->registerRequest< RequestRefuse >   ("refuse");
  _factory->registerRequest< RequestHold >     ("hold");
  _factory->registerRequest< RequestUnhold >   ("unhold");
  _factory->registerRequest< RequestTransfer > ("transfer");
  _factory->registerRequest< RequestMute >     ("mute");
  _factory->registerRequest< RequestUnmute >   ("unmute");
}

// destructor
GUIServer::~GUIServer()
{
  delete _factory;
}

int
GUIServer::exec() {
  try {
    ost::InetAddress addr("127.0.0.1");
    
    //Creating a listening socket.
    ost::TCPSocket aServer(addr, 3999);
    
    std::cout << "listening on " << aServer.getLocal() << ":" << 3999 << std::endl;
    
    std::string output; // TCPStream output line
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
      
      output = "";
      while(sessionIn->good() && sessionOut->good()) {
        request = popRequest();
        output = request->execute(*this);
        pushResponseMessage(output);
        delete request;
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
  _requests.push_back(_factory->create(request));
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


int 
GUIServer::incomingCall (short id) 
{
  
}

void  
GUIServer::peerAnsweredCall (short id) 
{
  
}

int  
GUIServer::peerRingingCall (short id) 
{
  
}

int  
GUIServer::peerHungupCall (short id) 
{
  
}

void  
GUIServer::displayTextMessage (short id, const std::string& message) 
{
  std::string requestMessage = "500 seq0 s";
  requestMessage += id + " text message: " + message;
  pushRequestMessage(requestMessage);
}

void  
GUIServer::displayErrorText (short id, const std::string& message) 
{
  std::string requestMessage = "500 seq0 s";
  requestMessage += id + " error text: " + message;
  pushRequestMessage(requestMessage);
}

void  
GUIServer::displayError (const std::string& error) 
{
  std::string requestMessage = "500 seq0 ";
  requestMessage += error;
  pushRequestMessage(requestMessage);
}

void  
GUIServer::displayStatus (const std::string& status) 
{
  std::string requestMessage = "500 seq0 ";
  requestMessage += status;
  pushRequestMessage(requestMessage);
}

void  
GUIServer::displayContext (short id) 
{
  std::string requestMessage = "500 seq0 s";
  requestMessage += id + " context";
  pushRequestMessage(requestMessage);
}

std::string  
GUIServer::getRingtoneFile (void) 
{
}

void  
GUIServer::setup (void) 
{
}

int  
GUIServer::selectedCall (void) 
{
 
}

bool  
GUIServer::isCurrentId (short) 
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
