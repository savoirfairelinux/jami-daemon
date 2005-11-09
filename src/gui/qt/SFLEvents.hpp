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

#ifndef __SFLEVENTS_HPP__
#define __SFLEVENTS_HPP__

#include "Event.hpp"

class DefaultEvent : public Event
{
public:
  DefaultEvent(const QString &code,
	       const std::list< QString > &args);
  
  virtual void execute();
};

class HangupEvent : public CallRelatedEvent
{
public:
  HangupEvent(const QString &code,
	      const std::list< QString > &args);
  
  virtual void execute();
};

class IncommingEvent : public CallRelatedEvent
{
public:
  IncommingEvent(const QString &code,
		 const std::list< QString > &args);
  
  virtual void execute();

private:
  QString mAccountId;
  QString mOrigin;
};


class VolumeEvent : public Event
{
public:
  VolumeEvent(const QString &code,
	       const std::list< QString > &args);
  
  virtual void execute();

protected:
  unsigned int mVolume;
};

class MicVolumeEvent : public VolumeEvent
{
public:
  MicVolumeEvent(const QString &code,
		 const std::list< QString > &args);
  
  virtual void execute();

};

class MessageTextEvent : public Event
{
public:
  MessageTextEvent(const QString &code,
	       const std::list< QString > &args);
  
  virtual void execute();

protected:
  QString mMessage;
};

class LoadSetupEvent : public Event
{
public:
  LoadSetupEvent(const QString &code,
		 const std::list< QString > &args);
  
  virtual void execute();
};

#endif
