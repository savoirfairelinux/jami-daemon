/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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

#include "DNSQueryThread.h"
#include "DNSService.h"

/**
 * Construct a DNSQueryThread and initialize the cancel to deferred
 */
DNSQueryThread::DNSQueryThread(DNSService *parent, const char *regtype) : ost::Thread() 
{
  _parent = parent;
  _regtype = regtype;
  _serviceRef = NULL;
  setCancel(cancelDeferred);
}

/**
 * Destruct a DNSQueryThread
 */
DNSQueryThread::~DNSQueryThread() 
{
  if (_serviceRef) {
    DNSServiceRefDeallocate(_serviceRef);
  }
  terminate();
  _parent = NULL;
  _regtype = NULL;
  _serviceRef = NULL;
}

/**
 * Running loop
 */
void
DNSQueryThread::run() {
  DNSServiceErrorType theErr=0; // NULL;
  DNSServiceFlags     resultFlags=0;
    
  theErr = DNSServiceBrowse(&_serviceRef,
            resultFlags,
            0,  // all interfaces
            _regtype,
            NULL,
            DNSServiceAddServicesCallback,
            (void*)_parent);

  if (theErr == kDNSServiceErr_NoError) {
    while(!testCancel()) {
      DNSServiceProcessResult(_serviceRef); // blockage if none...
    }
  }
}
