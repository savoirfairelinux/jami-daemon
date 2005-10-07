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

#include "CallManagerImpl.hpp"

void
CallManagerImpl::registerCall(const Call &call)
{
  mCallsMutex.lock();
  mCalls.insert(std::make_pair(call.id(), call));
  mCallsMutex.unlock();
}

void
CallManagerImpl::unregisterCall(const Call &call)
{
  unregisterCall(call.id());
}

void
CallManagerImpl::unregisterCall(const QString &id) 
{
  QMutexLocker guard(&mCallsMutex);
  std::map< QString, Call >::iterator pos = mCalls.find(id);
  if(pos == mCalls.end()) {
    //TODO
    //throw std::runtime_error(QString("Trying to unregister an unregistred call (%1)").arg(id).toStdString().c_str());
  }

  mCalls.erase(pos);
}

Call
CallManagerImpl::getCall(const QString &id)
{
  QMutexLocker guard(&mCallsMutex);
  std::map< QString, Call >::iterator pos = mCalls.find(id);
  if(pos == mCalls.end()) {
    //TODO
    //throw std::runtime_error(QString("Trying to retreive an unregistred call (%1)").arg(id).toStdString().c_str());
  }

  return pos->second;
}
