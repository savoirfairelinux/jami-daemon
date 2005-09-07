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
 
 /**
  * Inspired by http://braden.machacking.net/zerobrowse.cpp and
  * http://developer.kde.org/documentation/library/3.4-api/dnssd/html/remoteservice_8cpp-source.html
  */
#include "DNSService.h"
#include "DNSServiceTXTRecord.h"
#include "DNSQueryThread.h"
#include "../global.h" // for _debug()

#include <cc++/thread.h>

/**
 * Simple Empty Constructor
 */
DNSService::DNSService()
{
  _regtypeList.push_back("_sip._udp");
#ifdef USE_IAX2
  _regtypeList.push_back("_iax._udp");
#endif
  
  // for the thread, the ifdef add a dynamic _regtypeList problem
  for (std::list<std::string>::iterator iterThread=_regtypeList.begin();
       iterThread!=_regtypeList.end();
       iterThread++) {
    _queryThread.push_back(new DNSQueryThread(this, (*iterThread).c_str()));
  }
}

/**
 * Simple Empty Destructor
 */
DNSService::~DNSService()
{
  int cntThread = _queryThread.size();
  for (int iThread=0;iThread<cntThread;iThread++) {
    delete _queryThread[iThread];
    _queryThread[iThread] = NULL;
  }
}

/**
 * Look for zeroconf services and add them to _services
 */
void
DNSService::scanServices() 
{
  for (std::vector<DNSQueryThread *>::iterator iter = _queryThread.begin();iter!=_queryThread.end();iter++) {
    (*iter)->start();
  }
}

/**
 * Add one service to the list of actual services
 * @param service Service to add to the list
 */
void DNSService::addService(const std::string &service) 
{
  // does push_back do a copy and I can use a reference & instead as service argument
  DNSServiceTXTRecord txtRecord;
  _mutex.enterMutex();
  _services[service] = txtRecord;
  // we leave before the queryService since, each 
  // thread will modify a DNSServiceTXTRecord of a difference services
  _mutex.leaveMutex();
  queryService(service);
}

/**
 * Remove one service to the list of actual services
 * @param service Service to remove to the list
 */
void DNSService::removeService(const std::string &service) 
{
  _mutex.enterMutex();
  _services.erase(service);
  _mutex.leaveMutex();
}

/**
 * Display the list of available services
 * run() method should be call before
 */
void 
DNSService::listServices() 
{
  _debug("Number of services detected: %d\n", _services.size());
  std::map<std::string, DNSServiceTXTRecord>::iterator iterTR;
  for (iterTR = _services.begin(); iterTR != _services.end(); iterTR++) {
    _debug("name: %s\n", iterTR->first.c_str());
    _debug("size: %d\n", iterTR->second.size());
    iterTR->second.listValue();
  }
}

/**
 * Query a service and wait for the anwser
 * the queryCallback will show the result
 * @param service The service full adress
 */
void 
DNSService::queryService(const std::string &service) 
{
  DNSServiceErrorType theErr=0;
  DNSServiceRef       myServRef=0;
  DNSServiceFlags     resultFlags=0;
  
  theErr = DNSServiceQueryRecord(&myServRef, 
				 resultFlags, 
				 0, 
				 service.c_str(), 
				 kDNSServiceType_TXT, 
				 kDNSServiceClass_IN, 
				 DNSServiceQueryRecordCallback, 
				 (void*)this);
  if (theErr == kDNSServiceErr_NoError) {
    DNSServiceProcessResult(myServRef); // blockage...
    DNSServiceRefDeallocate(myServRef);
  }
}

/**
 * Overloadding queryService
 * @param service service name
 * @param regtype registred type of service
 * @param domain  domain (habitually local.)
 */
void 
DNSService::queryService(const char *service, const char *regtype, const char *domain) 
{
  char serviceName[kDNSServiceMaxDomainName+1];
  DNSServiceConstructFullName(serviceName, service, regtype, domain);
  queryService(std::string(serviceName));
}

/**
 * Add a txt record with the queryService callback answser data
 * @param rdlen the length of the txt record data
 * @param rdata txt record data
 */
void 
DNSService::addTXTRecord(const char *fullname, uint16_t rdlen, const void *rdata) 
{
  char key[256];
  
  const char *value;
  uint8_t valueLen; // 0 to 256 by type restriction
  char valueTab[256];
  
  
  uint16_t keyCount = TXTRecordGetCount(rdlen, rdata);
  for (int iKey=0; iKey<keyCount; iKey++) {
    TXTRecordGetItemAtIndex (rdlen, rdata, iKey, 256, key, &valueLen, (const void **)(&value));
    if (value) {
      bcopy(value, valueTab, valueLen);
      valueTab[valueLen]='\0';
      _services[std::string(fullname)].addKeyValue(std::string(key), std::string(valueTab));
    } else {
      _services[std::string(fullname)].addKeyValue(std::string(key), std::string(""));
    }
  }

  // TODO: remove this call, when we do not debug..
  // addTXTRecord is a good function to know changes... 
  listServices();
}


void 
DNSServiceAddServicesCallback(DNSServiceRef sdRef,
						DNSServiceFlags flags,
						uint32_t interfaceIndex,
						DNSServiceErrorType errorCode,
						const char *serviceName,
						const char *replyType,
						const char *replyDomain,
						void *context)
{
  if (errorCode==kDNSServiceErr_NoError) {
  
    if (flags) {
      DNSService *service = (DNSService*)context;
      std::string tempService;
      tempService = std::string(serviceName) + "." + std::string(replyType) + std::string(replyDomain);
      if (flags&kDNSServiceFlagsAdd) {
        service->addService(tempService);
      } else {
        service->removeService(tempService);
      }
    }
  } else {
     // TODO: error handling
  }
}

void 
DNSServiceQueryRecordCallback(
	DNSServiceRef DNSServiceRef,
	DNSServiceFlags flags,
	uint32_t interfaceIndex,
	DNSServiceErrorType errorCode,
	const char *fullname,
	uint16_t rrtype,
	uint16_t rrclass,
	uint16_t rdlen,
	const void *rdata,
	uint32_t ttl,
	void *context)
{
  if (errorCode==kDNSServiceErr_NoError) {
    if (flags) {
        if (flags&kDNSServiceFlagsAdd) {
          ((DNSService *)context)->addTXTRecord(fullname, rdlen, rdata);
        }
    }
    if (!(flags&kDNSServiceFlagsMoreComing)) {
      // TODO: stoping, if no blocking process here
    } 
  }
}
