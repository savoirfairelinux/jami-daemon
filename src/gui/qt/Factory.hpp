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

#ifndef __FACTORY_HPP__
#define __FACTORY_HPP__

template< typename T >
struct Creator
{
  virtual ~Creator(){}

  virtual T *create() = 0;
};

template< typename T >
class Factory
{
public:
  Factory();
  ~Factory();
  
  /**
   * This function will set the creator. The 
   * Factory owns the creator instance.
   */
  void setCreator(Creator< T > *creator);

  /**
   * It ask the creator to create a SessionIO.
   * If there's no creator set, it will throw
   * a std::logic_error.
   */
  T *create();

private:
  Creator< T > *mCreator;
};

#include "Factory.inl"

#endif
