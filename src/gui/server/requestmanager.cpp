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
#include "requestmanager.h"

#include <iostream>

#include "tcpsessionio.h"
#include "../../global.h"

RequestManager::RequestManager() : _sessionIO(0)
{
  _factory.registerAll();
  _quit = false;
}


RequestManager::~RequestManager()
{
  delete _sessionIO;
  flushWaitingRequest();
}

int 
RequestManager::exec() 
{
  try {
    // waiting for a new connection
    std::cout << "waiting for a new connection..." << std::endl;

    while(std::cin.good()) {

      // TCPSessionIO start a thread for the stream socket
      {
        _sessionMutex.enterMutex(); 
        _sessionIO = new TCPSessionIO();
        _sessionMutex.leaveMutex();
      }
      // wait for the first message
      std::cout << "accepting connection..." << std::endl;

      ResponseMessage outputResponse; // TCPStream output line
      std::string input;
      std::string output;
      Request *request;

      _sessionIO->init();

      // std::cin.good() is only there to close the server when
      // we do a CTRL+D
      quit = false;
      while(_sessionIO && _sessionIO->good() && std::cin.good() && !_quit) {

        if (_sessionIO->receive(input)) {
          _debug("Receive Input...: %s\n", input.c_str());
          request = _factory.create(input);
          outputResponse = request->execute();

          _sessionIO->send(outputResponse.toString());

          handleExecutedRequest(request, outputResponse);
        } // end pop
      } // end streaming

      { // session mutex block
        _sessionMutex.enterMutex(); 
        delete _sessionIO;
        _sessionIO = 0;
        _sessionMutex.leaveMutex();
      }

    } // end while
 
  } catch(ost::Socket *e) {
    std::cerr << e->getErrorString() << std::endl;
  }
  return 0;
}

/**
 * Delete the request from the list of request
 * or send it into the waitingRequest map
 */
void 
RequestManager::handleExecutedRequest(Request * const request, const ResponseMessage& response) 
{
  if (response.isFinal()) {
    delete request;
  } else {
    ost::MutexLock lock(_waitingRequestsMutex);
    if (_waitingRequests.find(request->sequenceId()) == _waitingRequests.end()) {
      // add the requests
      _waitingRequests[response.sequenceId()] = request;
    } else {
      // we don't deal with requests with a sequenceId already send...
      delete request;
    }
  }
}

/**
 * Remove waiting requests that was not handle by the server
 */
void
RequestManager::flushWaitingRequest()
{
  ost::MutexLock lock(_waitingRequestsMutex);
  // Waiting Requests cleanup
  std::map<std::string, Request*>::iterator iter = _waitingRequests.begin();
  while (iter != _waitingRequests.end()) {
    _waitingRequests.erase(iter);
    delete (iter->second);
    iter++;
  }
}

/**
 * This function is use by extern object
 * to send response
 */
void
RequestManager::sendResponse(const ResponseMessage& response) {
  _sessionMutex.enterMutex();
  if (_sessionIO) {
    _sessionIO->send(response.toString());
  } 
  _sessionMutex.leaveMutex();

  // remove the request from the waiting requests list
  if (response.isFinal()) {
    ost::MutexLock lock(_waitingRequestsMutex);
    std::map<std::string, Request*>::iterator iter = _waitingRequests.find(response.sequenceId());

    if (iter != _waitingRequests.end()) {
      _waitingRequests.erase(iter);
      delete (iter->second);
    }
  }
}
