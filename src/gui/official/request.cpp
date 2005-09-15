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

#include "request.h"

Request::Request(const std::string &sequenceId,
		 const std::string &command,
		 const std::list< std::string > &args)
  : mSequenceId(sequenceId)
  , mCommand(command)
  , mArgs(args)
{}

void
Request::onError(int code, const std::string &message)
{}

void
Request::onSuccess(int code, const std::string &message)
{}

std::string
Request::toString()
{
  std::ostream id;
  id << mCommand << mSequenceId;
  for(std::list< std::string >::iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    id << " " << (*pos);
  }

  return id.str();
}


CallRequest::CallRequest(const std::string &sequenceId,
			 const std::string &callId,
			 const std::string &command,
			 const std::list< std::string > &args)
  : Request(sequenceId, command, args)
  , mCallId(callId)
{}

void
CallRequest::onError(int code, const std::string &message)
{
  onError(Call(mCallId), code, message);
}

void
CallRequest::onError(Call call, int code, const std::string &message)
{}

void
CallRequest::onSuccess(int code, const std::string &message)
{
  onSuccess(Call(mCallId), code, message);
}

void
CallRequest::onSuccess(Call call, int code, const std::string &message)
{}

