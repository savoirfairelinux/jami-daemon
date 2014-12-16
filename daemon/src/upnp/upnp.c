/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *
 *  Some of the code below is taken from code written by Thomas BERNARD
 *  as part of MiniUPnPc and released under the BSD license:
 *
 *  MiniUPnPc
 *  Copyright (c) 2005-2014, Thomas BERNARD
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright notice,
 *        this list of conditions and the following disclaimer in the documentation
 *        and/or other materials provided with the distribution.
 *      * The name of the author may not be used to endorse or promote products
 *  	  derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "upnp.h"

#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include "logger.h"

static struct UPNPUrls urls;
static struct IGDdatas data;
static int upnp_initialized;

/**
 * Initializes UPnP by looking for a list of IGDs and using the first possible one.
 * All subsequent functions called will use that IGD, if one is found.
 * Returns 1 on success, 0 on failure.
 */
int init_upnp (void)
{
	struct UPNPDev * devlist;
	struct UPNPDev * dev;
	char * descXML;
	int descXMLsize = 0;
	int upnperror = 0;
	SFL_DBG("UPnP : initializing");
	memset(&urls, 0, sizeof(struct UPNPUrls));
	memset(&data, 0, sizeof(struct IGDdatas));
	devlist = upnpDiscover(2000, NULL/*multicast interface*/, NULL/*minissdpd socket path*/, 0/*sameport*/, 0/*ipv6*/, &upnperror);
	upnp_initialized = 0; /* not yet initialized */
	if (devlist)
	{
		dev = devlist;
		while (dev)
		{
			if (strstr (dev->st, "InternetGatewayDevice"))
				break;
			dev = dev->pNext;
		}
		if (!dev)
			dev = devlist; /* defaulting to first device */

		SFL_DBG("UPnP device :\n"
		       " desc: %s\n st: %s",
			   dev->descURL, dev->st);

		descXML = miniwget(dev->descURL, &descXMLsize);
		if (descXML)
		{
			parserootdesc (descXML, descXMLsize, &data);
			free (descXML); descXML = 0;
			GetUPNPUrls (&urls, &data, dev->descURL);
		}
		freeUPNPDevlist(devlist);
		upnp_initialized = 1;
		return 1;
	}
	else
	{
		/* error ! */
		SFL_DBG("UPnP : error initializing");
		upnp_initialized = 0;
		return 0;
	}
}

/**
 * Returns 0 if upnp has been initialzed, otherwise 1
 */
int is_upnp_initialized(void)
{
	return upnp_initialized;
}

void upnp_add_redir (const char * addr, unsigned int port_external, unsigned int port_internal)
{
	char port_ext_str[16];
	char port_int_str[16];
	int r;
	SFL_DBG("UPnP : adding port mapping : %s, %u -> %u", addr, port_external, port_internal);
	if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("UPnP : UPnP was not initialzed");
		return;
	}
	sprintf(port_ext_str, "%u", port_external);
	sprintf(port_int_str, "%u", port_internal);
	r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
	                        port_ext_str, port_int_str, addr, "ring", "UDP", NULL, NULL);
	if(r!=0)
		SFL_DBG("UPnP : AddPortMapping(%s, %s, %s) failed with error: %d", port_ext_str, port_int_str, addr, r);
}

void upnp_rem_redir (unsigned int port)
{
	char port_str[16];
	int t;
	SFL_DBG("UPnP : removing entry with port = %u", port);
	if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("UPnP : UPnP was not initialzed");
		return;
	}
	sprintf(port_str, "%u", port);
	UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port_str, "UDP", NULL);
}

int upnp_get_entry (unsigned int port)
{
    int err;
    char int_client[16];
    char int_port[16];
    char port_str[16];
    char enabled[4];

    *int_client = '\0';
    *int_port = '\0';
    *enabled = '\0';

    SFL_DBG("UPnP : finding entry with external port = %u", port);

    if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("UPnP : UPnP was not initialzed");
		return -1;
	}

    sprintf(port_str, "%u", port);

    err = UPNP_GetSpecificPortMappingEntry (urls.controlURL,
                                            data.first.servicetype,
                                            port_str,
                                            "UDP",
                                            // NULL /*remoteHost*/,
                                            int_client,
                                            int_port,
                                            NULL /*desc*/,
                                            enabled /*enabled*/,
                                            NULL /*duration*/);
    if (err == 0) {
	SFL_DBG("UPnP : got entry, client: %s, port: %s, enabled: %s", int_client, int_port, enabled);
    }

    return err;
}

void upnp_remove_all_entries(const char * remove_desc)
{
	int r;
	int i = 0;
	char index[6];
	char intClient[40];
	char intPort[6];
	char extPort[6];
	char protocol[4];
	char desc[80];
	char enabled[6];
	char rHost[64];
	char duration[16];

	SFL_DBG("UPnP : removing all port mapping entries with description: \"%s\"", remove_desc);

	if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("UPnP : UPnP was not initialzed");
		return;
	}

	do {
		snprintf(index, 6, "%d", i);
		rHost[0] = '\0'; enabled[0] = '\0';
		duration[0] = '\0'; desc[0] = '\0';
		extPort[0] = '\0'; intPort[0] = '\0'; intClient[0] = '\0';
		r = UPNP_GetGenericPortMappingEntry(urls.controlURL,
		                               data.first.servicetype,
		                               index,
		                               extPort, intClient, intPort,
									   protocol, desc, enabled,
									   rHost, duration);
		if(r==0) {
			SFL_DBG("UPnP : %2d %s %5s->%s:%-5s '%s' '%s' %s",
			       i, protocol, extPort, intClient, intPort,
			       desc, rHost, duration);
			// remove if matches description
			// once the port mapping is deleted, there will be one less, and the rest will "move down"
			// that is, we don't need to increment the mapping index in that case
			if( strcmp(remove_desc, desc) == 0 ) {
				SFL_DBG("UPnP : deleting port mapping entry entry");
				int delete_err = 0;
				delete_err = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, extPort, protocol, NULL);
				if(delete_err != 0) {
					SFL_DBG("UPnP : UPNP_DeletePortMapping() failed with error code : %d", delete_err);
				} else {
					SFL_DBG("UPnP : deletion success");
					// decrement the mapping index since it will be incremented
					i--;
				}
			}
		} else {
			SFL_DBG("UPnP : GetGenericPortMappingEntry() failed with error code %d", r);
		}
		i++;
	} while(r==0);
}

void upnp_get_external_ip(char * ext_ip)
{
	int r;

	SFL_DBG("UPnP : getting external IP");
	if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("UPnP : UPnP was not initialzed");
		return;
	}

	r = UPNP_GetExternalIPAddress(urls.controlURL,
	                          data.first.servicetype,
							  ext_ip);
	if(r != 0) {
		SFL_DBG("UPnP : GetExternalIPAddress failed with error code: %d", r);
	} else {
		SFL_DBG("UPnP : got external IP = %s", ext_ip);
	}
}
