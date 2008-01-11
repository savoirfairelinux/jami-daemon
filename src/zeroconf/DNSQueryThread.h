/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
#ifndef __DNSSD_DNSQUERYTHREAD_H__
#define __DNSSD_DNSQUERYTHREAD_H__

#include <cc++/thread.h>
#include <dns_sd.h>

class DNSService;
class DNSQueryThread : public ost::Thread
{
public:
  DNSQueryThread(DNSService *parent, const char *regtype);
  ~DNSQueryThread();
  virtual void run(); // looking for services
  
private:
  DNSService    *_parent;    // parent service
  DNSServiceRef _serviceRef; // service reference 
  const char    *_regtype;   // service type and socket type (_sip._udp by example)
};


#endif // __DNSSD_DNSQUERYTHREAD_H__
