/*
 *  Copyright (C) 2006 Savoir-Faire Linux inc.
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
#include "iaxvoiplink.h"
#include "global.h" // for _debug

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

IAXVoIPLink::IAXVoIPLink(const AccountID& accountID)
 : VoIPLink(accountID)
{
}


IAXVoIPLink::~IAXVoIPLink()
{
}

bool
IAXVoIPLink::init()
{
  bool returnValue = false;
  //_localAddress = "127.0.0.1";
  // port 0 is default
  //  iax_enable_debug(); have to enable debug when compiling iax...
  int port = iax_init(IAX_DEFAULT_PORTNO);
  if (port == IAX_FAILURE) {
    _debug("IAX Failure: Error when initializing\n");
  } else if ( port == 0 ) {
    _debug("IAX Warning: already initialize\n");
  } else {
    _debug("IAX Info: listening on port %d\n", port);
    _localPort = port;
    returnValue = true;
  }
  return returnValue;
}

void
IAXVoIPLink::terminate()
{
//  iaxc_shutdown();  
//  hangup all call
//  iax_hangup(calls[callNo].session,"Dumped Call");
}
