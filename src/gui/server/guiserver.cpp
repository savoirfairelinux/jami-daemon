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
#include "protocol.tab.h" // need by TCPStreamLexer.h but can't put in it
#include "TCPStreamLexer.h"


// default constructor
GUIServer::GUIServer()
{
}

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
    
    int callid;
    std::string status;

    while (std::cin.good()) {
    
      // waiting for a new connection
      std::cout << "waiting for a new connection..." << std::endl;
      
      //I'm accepting an incomming connection
      
      aServerStream = new TCPStreamLexer(aServer, this);
      
      // wait for the first message
      std::cout << "accepting connection..." << std::endl;
      
      std::string output;
      *aServerStream << "Welcome to this serveur2" << std::endl;
      while(aServerStream->good() && output!="quit") {
        // lire
        //std::getline(*aServerStream, output);
        //_eventList.push_back(Event(output));
        
        // analyser
        //std::cout << output << ":" << output.length() << std::endl;
        
      	aServerStream->parse();
        
        /*
        if ( output.find("call ") == 0 ) {
          callid = outgoingCall(output.substr(5,output.size()-5));
          if ( callid ) {
            status = "200 OK ";
            status += callid;
            status += " Trying status...";
            displayStatus(status);
          }
          
        } else if ( output.find("hangup ") == 0 ) {
          //hangup <CSeq/Client> <Call-Id>
          int i = hangupCall(callid);
          status = "200 OK ";
          status += callid;
          status += " Hangup.";
          displayStatus(status);
        }
        */
        // repondre
        //*aServerStream << output.length() << std::endl;
      }
      
      delete aServerStream;
      std::cout << "end of connection\n";
    }
    std::cout << "end of listening" << std::endl;
  }
  catch(ost::Socket *e) {
    std::cerr << e->getErrorString() << std::endl;
  }

  return 0;
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
  if ( aServerStream ) {
    *aServerStream << "TEXTMESSAGE " << id << " " << message << std::endl;
  }
}

void  
GUIServer::displayErrorText (short id, const std::string& message) 
{
  if ( aServerStream ) {
    *aServerStream << "ERRORTEXT " << id << " " << message << std::endl;
  }
}

void  
GUIServer::displayError (const std::string& error) 
{
  if ( aServerStream ) {
    *aServerStream << "ERROR " << error << std::endl;
  }
}

void  
GUIServer::displayStatus (const std::string& status) 
{
  if ( aServerStream ) {
    *aServerStream << status << std::endl;
  }
}

void  
GUIServer::displayContext (short id) 
{
  if ( aServerStream ) {
    *aServerStream << "CONTEXT " << id << std::endl;
  }
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
GUIServer::startVoiceMessageNotification (void) {
  
}

void  
GUIServer::stopVoiceMessageNotification (void) 
{
  
}
