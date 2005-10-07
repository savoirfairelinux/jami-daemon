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

#ifndef __EVENTFACTORY_HPP__
#define __EVENTFACTORY_HPP__

#include <list>
#include <map>
#include <qstring.h>

#include "Event.hpp"

/**
 * This is the base class that we will use to
 * create an object from the "create" function.
 */
template< typename Base >
class EventCreatorBase
{
 public:
  virtual ~EventCreatorBase(){}
  virtual Base *create(const QString &code,
		       const std::list< QString > &args) = 0;
  
  virtual EventCreatorBase *clone() = 0;
};

/**
 * This is the actual class that will create 
 * the request. It will return a Request 
 */
template< typename Base, typename Actual >
  class EventCreator : public EventCreatorBase< Base >
{
 public:
  virtual Actual *create(const QString &code,
			 const std::list< QString > &args);
  
  virtual EventCreatorBase< Base > *clone();
};


/**
 * This class is used to create object related to
 * a string. However, thoses objects will be created
 * with the default constructor.
 */
template< typename Base >
class EventFactoryImpl
{
public:
  EventFactoryImpl();

  /**
   * Ask for a new object linked to the string.
   */
  Base *create(const QString &code,
	       const std::list< QString > &args);

  /**
   * Register the string to return a Actual type.
   */
  template< typename Actual >
  void registerEvent(const QString &code);

  template< typename Actual >
  void registerDefaultEvent();
  
 private:
  std::map< QString, EventCreatorBase< Base > * > mEventCreators;
  EventCreatorBase< Base > *mDefaultCreator;
};

#include "EventFactory.inl"

#include "utilspp/Singleton.hpp"

typedef utilspp::SingletonHolder< EventFactoryImpl< Event > > EventFactory;


#endif
