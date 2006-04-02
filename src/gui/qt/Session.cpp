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

#include <list>
#include <iostream>
#include <qstring.h>

#include "Session.hpp"
#include "Requester.hpp"
#include "SessionIOFactory.hpp"


Session::Session(const QString &id)
  : mId(id)
{}

Session::Session()
{
  mId = Requester::instance().generateSessionId();
  SessionIO *s = SessionIOFactory::instance().create();
  Requester::instance().registerSession(mId, s);
}

QString
Session::id() const
{
  return mId;
}

Request *
Session::playTone() const
{
  return Requester::instance().send(mId, "playtone", std::list< QString >());
}

Request *
Session::stopTone() const
{
  return Requester::instance().send(mId, "stoptone", std::list< QString >());
}

void
Session::connect() const
{
  return Requester::instance().connect(mId);
}


Request *
Session::getEvents() const
{
  return Requester::instance().send(mId, "getevents", std::list< QString >());
}


Request *
Session::configSet(const QString &section,
		   const QString &name,
		   const QString &value) const
{
  std::list< QString > args;
  args.push_back(section);
  args.push_back(name);
  args.push_back(value);
  return Requester::instance().send(mId, "configset", args);
}


Request * 
Session::configGetAll() const
{
  return Requester::instance().send(mId, "configgetall", std::list< QString >());
}

Request * 
Session::configSave() const
{
  return Requester::instance().send(mId, "configsave", std::list< QString >());
}

Request *
Session::close() const
{
  return Requester::instance().send(mId, "close", std::list< QString >());
}

Request *
Session::stop() const
{
  return Requester::instance().send(mId, "stop", std::list< QString >());
}

Request *
Session::mute() const
{
  return Requester::instance().send(mId, "mute", std::list< QString >());
}

Request *
Session::unmute() const
{
  return Requester::instance().send(mId, "unmute", std::list< QString >());
}

Request *
Session::volume(unsigned int volume) const
{
  std::list< QString > args;
  args.push_back(QString("%1").arg(volume));
  return Requester::instance().send(mId, "setspkrvolume", args);
}

Request *
Session::micVolume(unsigned int volume) const
{
  std::list< QString > args;
  args.push_back(QString("%1").arg(volume));
  return Requester::instance().send(mId, "setmicvolume", args);
}

Request *
Session::getCallStatus() const
{
  return Requester::instance().send(mId, "getcallstatus", std::list< QString >());
}

Request *
Session::playDtmf(char c) const
{
  QString s;
  s += c;
  std::list< QString > args;
  args.push_back(s);
  return Requester::instance().send(mId, "playdtmf", args);
}

Request *
Session::list(const QString &category) const
{
  std::list< QString > args;
  args.push_back(category);
  return Requester::instance().send(mId, "list", args);
}

Request *
Session::registerToServer() const
{
  std::list< QString > args;
  args.push_back(getDefaultAccount().id());
  return Requester::instance().send(mId, "register", args);
}

Account
Session::getAccount(const QString &name) const
{
  return Account(mId, name);
}

Account
Session::getDefaultAccount() const
{
  return Account(mId, QString("SIP0"));
}

