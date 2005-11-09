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


#ifndef __EVENTFACTORY_INL__
#define __EVENTFACTORY_INL__

#include <qobject.h>
#include <stdexcept>

#include "DebugOutput.hpp"

template< typename Base, typename Actual >
Actual *
EventCreator< Base, Actual >::create(const QString &code,
				     const std::list< QString > &args)
{
  return new Actual(code, args);
}

template< typename Base, typename Actual >
EventCreatorBase< Base > *
EventCreator< Base, Actual >::clone()
{
  return new EventCreator< Base, Actual >();
}

template< typename Base >
EventFactoryImpl< Base >::EventFactoryImpl()
  : mDefaultCreator(NULL)
{}

template< typename Base >
Base *
EventFactoryImpl< Base >::create(const QString &code, 
			     const std::list< QString > &args)
{
  typename std::map< QString, EventCreatorBase< Base > * >::iterator pos = mEventCreators.find(code);
  if(pos == mEventCreators.end()) {
    if(mDefaultCreator) {
      return mDefaultCreator->create(code, args);
    }
    else{
      DebugOutput::instance() <<  QObject::tr("The code %1 has no creator registered.\n"
					      "and there's no default creator").arg(code);
    }
  }
  
  return pos->second->create(code, args);
}

template< typename Base >
template< typename Actual >
void 
EventFactoryImpl< Base >::registerEvent(const QString &code)
{
  if(mEventCreators.find(code) != mEventCreators.end()) {
    delete mEventCreators[code];
  }
  
  mEventCreators[code] = new EventCreator< Base, Actual >();
}

template< typename Base >
template< typename Actual >
void 
EventFactoryImpl< Base >::registerDefaultEvent()
{
  if(mDefaultCreator) {
    delete mDefaultCreator;
  }
  
  mDefaultCreator = new EventCreator< Base, Actual >();
}


#endif

