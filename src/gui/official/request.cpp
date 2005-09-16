/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sstream>

#include "request.h"
#include "requester.h"

Request::Request(const std::string &sequenceId,
		 const std::string &command,
		 const std::list< std::string > &args)
  : mSequenceId(sequenceId)
  , mCommand(command)
  , mArgs(args)
{}

void
Request::onError(const std::string &, const std::string &)
{}

void
Request::onEntry(const std::string &, const std::string &)
{}

void
Request::onSuccess(const std::string &, const std::string &)
{}

std::string
Request::toString()
{
  std::ostringstream id;
  id << mCommand << mSequenceId;
  for(std::list< std::string >::const_iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    id << " " << (*pos);
  }

  return id.str();
}


CallRequest::CallRequest(const std::string &sequenceId,
			 const std::string &command,
			 const std::list< std::string > &args)
  : Request(sequenceId, command, args)
  , mCallId(*args.begin())
{}

void
CallRequest::onError(const std::string &code, const std::string &message)
{
  onError(Call(Requester::instance().getSessionIdFromSequenceId(getSequenceId()), 
	       mCallId), 
	  code, 
	  message);
}

void
CallRequest::onError(Call, const std::string &, const std::string &)
{}

void
CallRequest::onEntry(const std::string &code, const std::string &message)
{
  onEntry(Call(Requester::instance().getSessionIdFromSequenceId(getSequenceId()), 
	       mCallId), 
	  code, 
	  message);
}

void
CallRequest::onEntry(Call, const std::string &, const std::string &)
{}

void
CallRequest::onSuccess(const std::string &code, const std::string &message)
{
  onSuccess(Call(Requester::instance().getSessionIdFromSequenceId(getSequenceId()), 
		 mCallId), 
	    code, 
	    message);
}

void
CallRequest::onSuccess(Call, const std::string &, const std::string &)
{}

