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

#ifndef SFLPHONEGUI_OBJECTFACTORY_H
#define SFLPHONEGUI_OBJECTFACTORY_H

#include <list>
#include <map>
#include <qstring.h>

/**
 * This is the base class that we will use to
 * create an object from the "create" function.
 */
template< typename Base >
class ObjectCreatorBase
{
 public:
  virtual ~ObjectCreatorBase(){}
  virtual Base *create(const QString &command,
		       const QString &sequenceId,
		       const std::list< QString > &args) = 0;
  
  virtual ObjectCreatorBase *clone() = 0;
};

/**
 * This is the actual class that will create 
 * the request. It will return a Request 
 */
template< typename Base, typename Actual >
  class ObjectCreator : public ObjectCreatorBase< Base >
{
 public:
  virtual Actual *create(const QString &command,
			 const QString &sequenceId,
			 const std::list< QString > &args);
  
  virtual ObjectCreatorBase< Base > *clone();
};


/**
 * This class is used to create object related to
 * a string. However, thoses objects will be created
 * with the default constructor.
 */
template< typename Base >
class ObjectFactory
{
public:
  ObjectFactory();

  /**
   * Ask for a new object linked to the string.
   */
  Base *create(const QString &requestname,
	       const QString &sequenceId,
	       const std::list< QString > &args);

  /**
   * Register the string to return a Actual type.
   */
  template< typename Actual >
    void registerObject(const QString &name);

  /**
   * Register the default object to be created,
   * when the object wanted isn't registered.
   */
  template< typename Actual >
    void registerDefaultObject();
  
 private:
  std::map< QString, ObjectCreatorBase< Base > * > mObjectCreators;
  ObjectCreatorBase< Base > *mDefaultObjectCreator;
};

#include "ObjectFactory.inl"

#endif
