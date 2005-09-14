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
#ifndef __DNSSD_DNSSERVICE_H__
#define __DNSSD_DNSSERVICE_H__

#include <string>
#include <map>
#include <list>
#include <vector>

#include <dns_sd.h>
#include <cc++/thread.h>

class DNSQueryThread;
class DNSServiceTXTRecord;

typedef std::map<std::string, DNSServiceTXTRecord> DNSServiceMap;
class DNSService
{
public:
  DNSService();
  ~DNSService();
  
  void scanServices(); // looking for services
  void addService(const std::string &service); // adding every services
  void removeService(const std::string &service); // remove a service
  void listServices(); // listing services (call addService before)
  void stop(); // after the browsing loop stop
  
  void queryService(const std::string &service); // query the TXT record of a service
  void queryService(const char *service, const char *regtype, const char *domain);
  void addTXTRecord(const char *fullname, uint16_t rdlen, const void *rdata);
  //void removeTXTRecord(const char *fullname);

private:
  DNSServiceMap _services; //map

  std::vector<DNSQueryThread *> _queryThread;
  /**
   * Mutex to protect access to _services on add/erase
   */
  ost::Mutex _mutex;
  /**
   * RegType List contains zeroconf services to register, like sip, iax2, ...
   * It will be use to initialize the DNSQueryThread
   */
  std::list<std::string> _regtypeList;
};

void DNSServiceAddServicesCallback(DNSServiceRef sdRef,
						DNSServiceFlags flags,
						uint32_t interfaceIndex,
						DNSServiceErrorType errorCode,
						const char *serviceName,
						const char *replyType,
						const char *replyDomain,
						void *context);

void DNSServiceQueryRecordCallback(DNSServiceRef DNSServiceRef,
	DNSServiceFlags flags,
	uint32_t interfaceIndex,
	DNSServiceErrorType errorCode,
	const char *fullname,
	uint16_t rrtype,
	uint16_t rrclass,
	uint16_t rdlen,
	const void *rdata,
	uint32_t ttl,
	void *context);

#endif // __DNSSD_DNSSERVICE_H__
