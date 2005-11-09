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


#ifndef SFLPHONEGUI_OBJECTFACTORY_INL
#define SFLPHONEGUI_OBJECTFACTORY_INL

#include <qobject.h>
#include <stdexcept>

#include "DebugOutput.hpp"

template< typename Base, typename Actual >
Actual *
ObjectCreator< Base, Actual >::create(const QString &command,
				      const QString &sequenceId,
				      const std::list< QString > &args)
{
  return new Actual(sequenceId, command, args);
}

template< typename Base, typename Actual >
ObjectCreatorBase< Base > *
ObjectCreator< Base, Actual >::clone()
{
  return new ObjectCreator< Base, Actual >();
}

template< typename Base >
ObjectFactory< Base >::ObjectFactory()
  : mDefaultObjectCreator(NULL)
{}

template< typename Base >
Base *
ObjectFactory< Base >::create(const QString &command, 
			      const QString &sequenceId,
			      const std::list< QString > &args)
{
  typename std::map< QString, ObjectCreatorBase< Base > * >::iterator pos = mObjectCreators.find(command);
  if(pos == mObjectCreators.end()) {
    if(!mDefaultObjectCreator) {
      throw std::logic_error("ObjectFactory: You need to specify a default creator.\n");
    }

    return mDefaultObjectCreator->create(command, sequenceId, args);
  }
  
  return pos->second->create(command, sequenceId, args);
}

template< typename Base >
template< typename Actual >
void 
ObjectFactory< Base >::registerObject(const QString &name)
{
  if(mObjectCreators.find(name) != mObjectCreators.end()) {
    delete mObjectCreators[name];
  }
  
  mObjectCreators[name] = new ObjectCreator< Base, Actual >();
}

template< typename Base >
template< typename Actual >
void 
ObjectFactory< Base >::registerDefaultObject()
{
  if(mDefaultObjectCreator) {
    delete mDefaultObjectCreator;
  }
  
  mDefaultObjectCreator = new ObjectCreator< Base, Actual >();
}


#endif

