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

#include "tcpsessionio.h"
#include "../../global.h"
#include <iostream>

RequestManager::RequestManager() : _sessionIO(0)
{
  _factory.registerAll();
  _quit = false;
}


RequestManager::~RequestManager()
{
  delete _sessionIO; _sessionIO = NULL;
  flushWaitingRequest();
}

int 
RequestManager::exec() 
{
  _debug("Request manager waiting TCP session\n");
  try {
    _stop = false;
    while(std::cin.good() && !_stop) {

      // TCPSessionIO start a thread for the stream socket
      {
        _sessionMutex.enterMutex(); 
        _sessionIO = new TCPSessionIO();
        _sessionMutex.leaveMutex();
      }

      ResponseMessage outputResponse; // TCPStream output line
      std::string input;
      std::string output;
      Request *request;

      _quit = false;
      _debug("Initiate a new TCP Session... \n");
      _sessionIO->init();

      // std::cin.good() is only there to close the server when
      // we do a CTRL+D
      while(_sessionIO && _sessionIO->good() && std::cin.good() && !_quit) {

        if (_sessionIO->receive(input)) {
          request = _factory.create(input);
          outputResponse = request->execute();

          _sessionIO->send(outputResponse.toString());

          handleExecutedRequest(request, outputResponse);
        } // end pop
      } // end streaming

      { // session mutex block
        _debug("Closing TCP Session... \n");
        _sessionMutex.enterMutex(); 
        delete _sessionIO; _sessionIO = NULL;
        _sessionMutex.leaveMutex();
        _debug("TCP Session has closed\n");
      }

    } // end while

  } catch(ost::Socket *e) {
    std::cerr << "Exception: " << e->getErrorString() << std::endl;
  }
  _debug("Request manager has closed\n");
  return 0;
}

/**
 * Delete the request from the list of request
 * or send it into the waitingRequest map
 */
void 
RequestManager::handleExecutedRequest(Request * request, const ResponseMessage& response) 
{
  if (response.isFinal()) {
    delete request; request = NULL;
  } else {
    ost::MutexLock lock(_waitingRequestsMutex);
    if (_waitingRequests.find(request->getSequenceId()) == _waitingRequests.end()) {
      // add the requests
      _waitingRequests[response.getSequenceId()] = request;
    } else {
      // we don't deal with requests with a sequenceId already send...
      delete request; request = NULL;
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
    delete iter->second; iter->second = NULL;
    iter++;
  }
  _waitingRequests.clear();
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
    std::map<std::string, Request*>::iterator iter = _waitingRequests.find(response.getSequenceId());

    if (iter != _waitingRequests.end()) {
      delete iter->second; iter->second = NULL;
      _waitingRequests.erase(iter);
    }
  }
}
