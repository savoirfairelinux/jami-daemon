/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *          Yan Morin <yan.morin@savoirfairelinux.com> (cc++ mutex)
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

#ifndef SFLPHONEGUI_OBJECTPOOL_INL
#define SFLPHONEGUI_OBJECTPOOL_INL

#include <iostream>

template< typename T >
void
ObjectPool< T >::push(const T &value)
{
  ost::MutexLock guard(mMutex);
  mPool.push_back(value);
  mSemaphore.post();
  std::cerr << "push value..." << std::endl;
}

template< typename T >
bool
ObjectPool< T >::pop(T &value, unsigned long time)
{
  ost::MutexLock guard(mMutex);
  mSemaphore.wait(time);
  
  if(mPool.begin() == mPool.end()) {
    std::cerr << "empty list" << std::endl;
    return false;
  } else {
    std::cerr << "pop value..." << std::endl;
    typename std::list< T >::iterator pos = mPool.begin();
    value = (*pos);
    mPool.pop_front();
    return true;
  }
}

template< typename T >
typename std::list< T >::iterator
ObjectPool< T >::begin()
{
  
  ost::MutexLock guard(mMutex);
  std::cerr << mPool.size();
  typename std::list< T >::iterator iter = mPool.begin();
  mSemaphore.post();
  return iter;
}

template< typename T >
typename std::list< T >::iterator
ObjectPool< T >::end()
{
  ost::MutexLock guard(mMutex);
  std::cerr << mPool.size();
  typename std::list< T >::iterator iter = mPool.end();
  mSemaphore.post();
  return iter;
}


#endif
