/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#ifndef SIPACCOUNT_H
#define SIPACCOUNT_H

#include "account.h"


/**
 * A SIP Account specify SIP specific functions and object (SIPCall/SIPVoIPLink)
 * @author Yan Morin <yan.morin@gmail.com>
*/
class SIPAccount : public Account
{
public:
  SIPAccount(const AccountID& accountID);

  virtual ~SIPAccount();

  /** Actually unuseful, since config loading is done in init() */
  void loadConfig();
  void registerVoIPLink();
  void unregisterVoIPLink();

private:
};

#endif
