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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SFLPHONEGUI_ACCOUNT_H
#define SFLPHONEGUI_ACCOUNT_H

#include <qstring.h>

#include "Call.hpp"

class Request;

class Account {
 public:
  Account(const QString &sessionId,
	  const QString &name);

  /**
   * This will generate a call ready to be used.
   */
  Request *registerAccount() const;
  Request *unregisterAccount() const;

  /**
   * This function will create a call. The call pointer will
   * point to a newly allocated memory. You're responsible for
   * deleting this memory.
   */
  Request *createCall(Call * &call, const QString &to) const;
  

  QString id() const
  {return mId;}

private:  
  Account();

  /**
   * This is the session id that we are related to.
   */
  QString mSessionId;

  /**
   * This is the account id that we are related to.
   */
  QString mId;
};


#endif
