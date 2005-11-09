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

#include <qobject.h>
#include <stdexcept>

#include "CallManagerImpl.hpp"
#include "DebugOutput.hpp"

void
CallManagerImpl::registerCall(const Call &call)
{
  mCalls.insert(std::make_pair(call.id(), call));
}

void
CallManagerImpl::unregisterCall(const Call &call)
{
  unregisterCall(call.id());
}

void
CallManagerImpl::unregisterCall(const QString &id) 
{
  std::map< QString, Call >::iterator pos = mCalls.find(id);
  if(pos != mCalls.end()) {
    mCalls.erase(pos);
  }
}

bool
CallManagerImpl::exist(const QString &id)
{
  std::map< QString, Call >::iterator pos = mCalls.find(id);
  if(pos == mCalls.end()) {
    return false;
  }

  return true;
}

Call
CallManagerImpl::getCall(const QString &id)
{
  std::map< QString, Call >::iterator pos = mCalls.find(id);
  if(pos == mCalls.end()) {
    throw std::runtime_error("Trying to retreive an unregistred call\n");
  }

  return pos->second;
}
