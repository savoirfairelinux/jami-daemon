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

#ifndef __CALL_MANAGER_IMPL_HPP__
#define __CALL_MANAGER_IMPL_HPP__

#include <qmutex.h>
#include <qstring.h>
#include <map>

#include "Call.hpp"

class CallManagerImpl
{
public:
  void registerCall(const Call &call);
  void unregisterCall(const Call &call);
  void unregisterCall(const QString &id);

  /**
   * Return true if the call is registered.
   */
  bool exist(const QString &id);

  /**
   * Return the call with the given id. If
   * there's no such call it will throw a
   * std::runtime_error.
   */
  Call getCall(const QString &id);

private:
  std::map< QString, Call > mCalls;
};

#endif
