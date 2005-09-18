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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string>
#include <list>

#include "Call.hpp"
#include "Requester.hpp"


Call::Call(const std::string &sessionId,
	   const std::string &callId)
  : mSessionId(sessionId)
  , mId(callId)
{}

std::string
Call::answer() 
{
  std::list< std::string> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "answer", args);
}

std::string
Call::hangup() 
{
  std::list< std::string> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "hangup", args);
}

std::string
Call::cancel() 
{
  std::list< std::string> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "cancel", args);
}

std::string
Call::hold() 
{
  std::list< std::string> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "hold", args);
}

std::string
Call::unhold() 
{
  std::list< std::string> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "unhold", args);
}

std::string
Call::refuse() 
{
  std::list< std::string> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "refuse", args);
}

std::string
Call::sendDtmf(char c) 
{
  std::list< std::string> args;
  args.push_back(mId);
  std::string s;
  s += c;
  args.push_back(s);
  return Requester::instance().send(mSessionId, "senddtmf", args);
}

