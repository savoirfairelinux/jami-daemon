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

#include <stdexcept>

template< typename T >
Factory< T >::Factory()
  : mCreator(0)
{}

template< typename T >
Factory< T >::~Factory()
{
  delete mCreator;
}

template< typename T >
void
Factory< T >::setCreator(Creator< T > *creator)
{
  mCreator = creator;
}

template< typename T >
T *
Factory< T >::create()
{
  if(!mCreator) {
    throw std::logic_error("Trying to create without a creator.");
  }
  else {
    return mCreator->create();
  }
}

