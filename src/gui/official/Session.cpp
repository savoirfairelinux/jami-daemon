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

#include <iostream>
#include <string>

#include "Session.hpp"
#include "Requester.hpp"
#include "SessionIOFactory.hpp"


Session::Session(const std::string &id)
  : mId(id)
{}

Session::Session()
{
  mId = Requester::instance().generateSessionId();
  SessionIO *s = SessionIOFactory::instance().create();
  Requester::instance().registerSession(mId, s);
}

std::string 
Session::id() const
{
  return mId;
}

std::string
Session::playTone() const
{
  return Requester::instance().send(mId, "playtone", std::list< std::string >());
}

std::string
Session::getEvents() const
{
  return Requester::instance().send(mId, "getevents", std::list< std::string >());
}

std::string
Session::playDtmf(char c) const
{
  std::string s;
  s += c;
  std::list< std::string > args;
  args.push_back(s);
  return Requester::instance().send(mId, "playdtmf", args);
}

Account
Session::getAccount(const std::string &name) const
{
  return Account(mId, name);
}

Account
Session::getDefaultAccount() const
{
  return Account(mId, std::string("mydefaultaccount"));
}

Call
Session::createCall() const
{
  return Call(mId, Requester::instance().generateCallId());
}
