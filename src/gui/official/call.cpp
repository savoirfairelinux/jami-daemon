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


#include "call.h"

std::string
Call::call(const std::string &destination) 
{
  std::list< std::string> args;
  args.push_back(mAccountId);
  args.push_back(to);
  return Requester::instance().sendCallCommand(mSession, mId, "answer", args)
}

std::string
Call::answer() 
{
  return Requester::instance().sendCallCommand(mSession, mId, "answer")
}

std::string
Call::hangup() 
{
  return Requester::instance().sendCallCommand(mSession, mId, "hangup")
}

std::string
Call::cancel() 
{
  return Requester::instance().sendCallCommand(mSession, mId, "cancel")
}

std::string
Call::hold() 
{
  return Requester::instance().sendCallCommand(mSession, mId, "hold")
}

std::string
Call::unhold() 
{
  return Requester::instance().sendCallCommand(mSession, mId, "unhold")
}

std::string
Call::refuse() 
{
  return Requester::instance().sendCallCommand(mSession, mId, "refuse")
}

std::string
Call::sendDtmf(char c) 
{
  std::list< std::string > args;
  args.push_back(std::string(c));
  return Requester::instance().sendCallCommand(mSession, mId, "senddtmf", args)
}

