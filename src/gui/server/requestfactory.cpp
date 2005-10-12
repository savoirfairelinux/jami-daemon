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
#include "requestconfig.h"

Request *
RequestFactory::create(const std::string& requestLine)
{
  
  TokenList tList = _tokenizer.tokenize(requestLine);
  TokenList::iterator iter = tList.begin();

  // there is atleast one token (the command)
  if (iter != tList.end()) {
    std::string requestName = *iter;
    tList.pop_front();
    iter = tList.begin();

    // there is atleast a second token (the sequenceId)
    if (iter != tList.end() && iter->size() != 0 ) {
      std::string sequenceId = *iter;
      tList.pop_front();
      try {
        Request *r = create(requestName, sequenceId, tList);
        return r;
      } catch (...) {
        // if the create return an exception
        // we create a syntaxerror
        return create("syntaxerror", sequenceId, tList);
      }
    }
  }
  return create("syntaxerror", "seq0", tList);
}

Request *
RequestFactory::create(
  const std::string& requestName, 
  const std::string& sequenceId, 
  const TokenList& argList)
{
  std::map< std::string, RequestCreatorBase * >::iterator pos = mRequests.find(requestName);
  if(pos == mRequests.end()) {
    pos = mRequests.find("syntaxerror");
    if(pos == mRequests.end()) {
      throw std::runtime_error("there's no request of that name");
    }
  }
  
  return pos->second->create(sequenceId, argList);
}

RequestFactory::~RequestFactory() 
{
  std::map< std::string, RequestCreatorBase * >::iterator iter = mRequests.begin();
  while ( iter != mRequests.end() ) {
    // delete RequestCreator< T >
    delete iter->second; iter->second = NULL;
    iter++;
  }
  mRequests.clear();
}

template< typename T >
void 
RequestFactory::registerRequest(const std::string &requestname)
{
  std::map< std::string, RequestCreatorBase * >::iterator pos = 
    mRequests.find(requestname);
  if(pos != mRequests.end()) {
    delete pos->second; pos->second = NULL;
    mRequests.erase(pos);
  }
  
  mRequests.insert(std::make_pair(requestname, new RequestCreator< T >()));
}

void 
RequestFactory::registerAll() {
  registerRequest<RequestSyntaxError> ("syntaxerror");
  registerRequest<RequestCall>        ("call");
  registerRequest<RequestAnswer>      ("answer");
  registerRequest<RequestRefuse>      ("refuse");
  registerRequest<RequestHold>        ("hold");
  registerRequest<RequestUnhold>      ("unhold");
  registerRequest<RequestHangup>      ("hangup");
  registerRequest<RequestHangupAll>   ("hangupall");
  registerRequest<RequestDTMF>        ("senddtmf");
  registerRequest<RequestPlayDtmf>    ("playdtmf");
  registerRequest<RequestPlayTone>    ("playtone");
  registerRequest<RequestStopTone>    ("stoptone");
  registerRequest<RequestTransfer>    ("transfer");
  registerRequest<RequestMute>        ("mute");
  registerRequest<RequestUnmute>      ("unmute");
  registerRequest<RequestVersion>     ("version");
  registerRequest<RequestQuit>        ("quit");
  registerRequest<RequestStop>        ("stop");

  // request config
  registerRequest<RequestGetEvents>   ("getevents");
  registerRequest<RequestZeroconf>    ("getzeroconf");
  registerRequest<RequestZeroconfEvent>("getzeroconfevents");
  registerRequest<RequestCallStatus>  ("getcallstatus");
  registerRequest<RequestConfigGetAll>("configgetall");
  registerRequest<RequestConfigGet>   ("configget");
  registerRequest<RequestConfigSet>   ("configset");
  registerRequest<RequestConfigSave>  ("configsave");
  registerRequest<RequestList>        ("list");
  registerRequest<RequestVolumeSpkr>  ("setspkrvolume");
  registerRequest<RequestVolumeMic>   ("setmicvolume");
}
