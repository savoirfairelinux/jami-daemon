/*
  eXosip - This is the eXtended osip library.
  Copyright (C) 2002,2003,2004,2005  Aymeric MOIZARD  - jack@atosc.org
  
  eXosip is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  eXosip is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifdef ENABLE_MPATROL
#include <mpatrol.h>
#endif


#include "eXosip2.h"

extern eXosip_t eXosip;

char *_eXosip_transport_protocol(osip_message_t *msg)
{
  osip_via_t *via;
  via = (osip_via_t *) osip_list_get (msg->vias, 0);
  if (via == NULL || via->protocol == NULL)
    return NULL;
  return via->protocol;
}

int _eXosip_find_protocol(osip_message_t *msg)
{
  osip_via_t *via;
  via = (osip_via_t *) osip_list_get (msg->vias, 0);
  if (via == NULL || via->protocol == NULL)
    return -1;
  else if (0==osip_strcasecmp(via->protocol, "UDP"))
	return IPPROTO_UDP;
  else if (0==osip_strcasecmp(via->protocol, "TCP"))
    return IPPROTO_TCP;
  return -1;;
}

int _eXosip_tcp_find_socket(char *host, int port)
{
  int pos;
  struct eXosip_net *net;

  net = &eXosip.net_interfaces[1];

  for (pos=0;pos<EXOSIP_MAX_SOCKETS;pos++)
    {
      if (net->net_socket_tab[pos].socket!=0)
	{
	  if (0==osip_strcasecmp(net->net_socket_tab[pos].remote_ip, host)
	      && port == net->net_socket_tab[pos].remote_port)
	    return net->net_socket_tab[pos].socket;
	}
    }
  return -1;
}

int _eXosip_tcp_connect_socket(char *host, int port)
{
  int pos;
  struct eXosip_net *net;

  int res;
  struct addrinfo *addrinfo = NULL;
  struct addrinfo *curinfo;
  int sock = -1;

  net = &eXosip.net_interfaces[1];

  for (pos=0;pos<EXOSIP_MAX_SOCKETS;pos++)
    {
      if (net->net_socket_tab[pos].socket==0)
	{
	  break;
	}
    }

  if (pos==EXOSIP_MAX_SOCKETS)
    return -1;

  res = eXosip_get_addrinfo(&addrinfo, host, port, IPPROTO_TCP);
  if (res)
    return -1;


  for (curinfo = addrinfo; curinfo; curinfo = curinfo->ai_next)
    {
      if (curinfo->ai_protocol && curinfo->ai_protocol != IPPROTO_TCP)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_INFO2, NULL,
		       "eXosip: Skipping protocol %d\n",
		       curinfo->ai_protocol));
	  continue;
	}

      sock = (int)socket(curinfo->ai_family, curinfo->ai_socktype,
			 curinfo->ai_protocol);
      if (sock < 0)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_INFO2, NULL,
		       "eXosip: Cannot create socket!\n",
		       strerror(errno)));
	  continue;
	}
      
      if (curinfo->ai_family == AF_INET6)
	{
#ifdef IPV6_V6ONLY
	  if (setsockopt_ipv6only(sock))
	    {
	      close(sock);
	      sock = -1;
	      OSIP_TRACE (osip_trace
			  (__FILE__, __LINE__, OSIP_INFO2, NULL,
			   "eXosip: Cannot set socket option!\n",
			   strerror(errno)));
	      continue;
	    }
#endif	/* IPV6_V6ONLY */
	}
      
#if 0
      res = bind (sock, (struct sockaddr*)&net->ai_addr, curinfo->ai_addrlen);
      if (res < 0)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_INFO2, NULL,
		       "eXosip: Cannot bind socket %s\n",
		       strerror(errno)));
	  close(sock);
	  sock = -1;
	  continue;
	}
#endif

      res = connect (sock, curinfo->ai_addr, curinfo->ai_addrlen);
      if (res < 0)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_INFO2, NULL,
		       "eXosip: Cannot bind socket node:%s family:%d %s\n",
		       host, curinfo->ai_family, strerror(errno)));
	  close(sock);
	  sock = -1;
	  continue;
	}

      break;
    }

  freeaddrinfo(addrinfo);

  if (sock>0)
    {
      net->net_socket_tab[pos].socket = sock;
      osip_strncpy(net->net_socket_tab[pos].remote_ip, host,
		   sizeof(net->net_socket_tab[pos].remote_ip)-1);
      net->net_socket_tab[pos].remote_port = port;
      return sock;
    }

  return -1;
}


int eXosip_transport_set(osip_message_t *msg, const char *transport)
{
  osip_via_t *via;

  via = (osip_via_t *) osip_list_get (msg->vias, 0);
  if (via == NULL || via->protocol == NULL)
    return -1;
  
  if (0==osip_strcasecmp(via->protocol, transport))
    return 0;

  osip_free(via->protocol);
  via->protocol = osip_strdup(transport);
  return 0;
}

int _eXosip_recvfrom(int s, char *buf, int len, unsigned int flags, struct sockaddr *from, socklen_t *fromlen)
{
    int message_size=0;
    int length_done=0;
    int real_size=0;
    int i;
    int extra_data_discarded;

    if (!eXosip.http_port)
    {
        return recvfrom(s, buf, len, flags, from, fromlen);
    }


    /* we get the size of the HTTP data */
    i = recv (eXosip.net_interfaces[0].net_socket, (char *) &(message_size), 4, 0);

    real_size = message_size;
    if (message_size<0)
    {
        return -1; /* Connection error? */
    }
    if (message_size==0)
    {
        buf[0]='\0';
        return 0;
    }
    if (message_size>len-1)
        message_size = len-1;

    length_done=0;

    i = recv (eXosip.net_interfaces[0].net_socket, buf, message_size, 0);
    length_done = i;

    if (length_done==real_size)
    {
        return length_done;
    }

    if (length_done<message_size)
    {
        /* continue reading up to message_size */
        while (length_done<message_size)
        {
            i = recv (eXosip.net_interfaces[0].net_socket, buf+length_done, message_size-length_done, 0);
            length_done = length_done+i;
        }
    }

    extra_data_discarded = length_done;
    while (extra_data_discarded<real_size)
    {
        char buf2[2048];
        /* We have to discard the end of data... up to the next message */
        i = recv (eXosip.net_interfaces[0].net_socket, buf2, 2048, 0);
        extra_data_discarded = extra_data_discarded+i;
    }
    return length_done;
}

int _eXosip_sendto(int s,  const void* buf,  size_t len,  int flags,
  const struct sockaddr* to,  socklen_t tolen)
{
    int i;
    char buf2[10000];

    if (!eXosip.http_port)
    {
       i = sendto(s, buf, len, flags, to, tolen);
       return i;
    }

    memset(buf2, '\0', sizeof(buf2));
    memcpy(buf2, &len, sizeof(int));
    memcpy(buf2+sizeof(int), buf, len);

    i = send(s, (char *) buf2, len+sizeof(int), 0); /* use TCP connection to proxy */
    if (i>0)
        i=i-sizeof(int);

    return i;
}