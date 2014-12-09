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

void init_upnp (void)
{
	struct UPNPDev * devlist;
	struct UPNPDev * dev;
	char * descXML;
	int descXMLsize = 0;
	int upnperror = 0;
	SFL_DBG("TB : init_upnp()\n");
	memset(&urls, 0, sizeof(struct UPNPUrls));
	memset(&data, 0, sizeof(struct IGDdatas));
	devlist = upnpDiscover(2000, NULL/*multicast interface*/, NULL/*minissdpd socket path*/, 0/*sameport*/, 0/*ipv6*/, &upnperror);
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
		       " desc: %s\n st: %s\n",
			   dev->descURL, dev->st);

		descXML = miniwget(dev->descURL, &descXMLsize);
		if (descXML)
		{
			parserootdesc (descXML, descXMLsize, &data);
			free (descXML); descXML = 0;
			GetUPNPUrls (&urls, &data, dev->descURL);
		}
		freeUPNPDevlist(devlist);
	}
	else
	{
		/* error ! */
		SFL_DBG("Error initializing UPnP");
	}
}

void upnp_add_redir (const char * addr, unsigned int port_external, unsigned int port_internal)
{
	char port_ext_str[16];
	char port_int_str[16];
	int r;
	SFL_DBG("TB : upnp_add_redir (%s, %u -> %u)\n", addr, port_external, port_internal);
	if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("TB : the init was not done !\n");
		return;
	}
	sprintf(port_ext_str, "%u", port_external);
	sprintf(port_int_str, "%u", port_internal);
	r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
	                        port_ext_str, port_int_str, addr, "ring", "UDP", NULL, NULL);
	if(r!=0)
		SFL_DBG("AddPortMapping(%s, %s, %s) failed with error: %d\n", port_ext_str, port_int_str, addr, r);
}

void upnp_rem_redir (unsigned int port)
{
	char port_str[16];
	int t;
	SFL_DBG("TB : upnp_rem_redir (%d)\n", port);
	if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("TB : the init was not done !\n");
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

    SFL_DBG("TB : upnp_get_entry ( %u )\n", port);

    if(urls.controlURL[0] == '\0')
	{
		SFL_DBG("TB : the init was not done !\n");
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
	SFL_DBG("TB : got entry, client: %s, port: %s, enabled: %s", int_client, int_port, enabled);
    }

    return err;
}
