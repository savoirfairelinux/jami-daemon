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
#include "requestfactory.h"

#include <stdexcept>

#include "request.h"

Request *
RequestFactory::create(const std::string &requestLine)
{
  std::string requestName;
  std::string sequenceId="seq0";
  std::string arguments;
  
  unsigned int spacePos = requestLine.find(' ');
  // we find a spacePos
  if (spacePos != std::string::npos) {
    /*
    012345678901234
    call seq1 cdddd
    spacePos  = 4
    spacePos2 = 9
    0 for 4  = 0 for spacePos
    5 for 4  = (spacePos+1 for spacePos2-spacePos-1)
    10 for 5 = (spacePos2+1 for size - spacePos2+1)
    */
    requestName = requestLine.substr(0, spacePos);
    
    unsigned int spacePos2 = requestLine.find(' ', spacePos+1);
    if (spacePos2 == std::string::npos) {
      // command that end with a sequence number
      sequenceId = requestLine.substr(spacePos+1, requestLine.size()-spacePos+1);
    } else {
      sequenceId = requestLine.substr(spacePos+1, spacePos2-spacePos-1);
      arguments = requestLine.substr(spacePos2+1, requestLine.size()-spacePos2+1);
    }
  } else {
    requestName = "syntaxerror";
  }
  
  return create(requestName, sequenceId, arguments);
}

Request *
RequestFactory::create(
  const std::string &requestname, 
  const std::string &sequenceId, 
  const std::string &arg)
{
  std::map< std::string, RequestCreatorBase * >::iterator pos = mRequests.find(requestname);
  if(pos == mRequests.end()) {
    pos = mRequests.find("syntaxerror");
    if(pos == mRequests.end()) {
      throw std::runtime_error("there's no request of that name");
    }
  }
  
  return pos->second->create(sequenceId, arg);
}

template< typename T >
void 
RequestFactory::registerRequest(const std::string &requestname)
{
  std::map< std::string, RequestCreatorBase * >::iterator pos = 
    mRequests.find(requestname);
  if(pos != mRequests.end()) {
    delete pos->second;
    mRequests.erase(pos);
  }
  
  mRequests.insert(std::make_pair(requestname, new RequestCreator< T >()));
}

void 
RequestFactory::registerAll() {
  registerRequest<RequestSyntaxError> ("syntaxerror");
  registerRequest<RequestCall>     ("call");
  registerRequest<RequestQuit>     ("quit");
  registerRequest<RequestAnswer>   ("anwser");
  registerRequest<RequestRefuse>   ("refuse");
  registerRequest<RequestHold>     ("hold");
  registerRequest<RequestUnhold>   ("unhold");
  registerRequest<RequestTransfer> ("transfer");
  registerRequest<RequestMute>     ("mute");
  registerRequest<RequestUnmute>   ("unmute");
}
