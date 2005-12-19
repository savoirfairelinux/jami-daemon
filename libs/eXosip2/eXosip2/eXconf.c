/*
  eXosip - This is the eXtended osip library.
  Copyright (C) 2002, 2003  Aymeric MOIZARD  - jack@atosc.org
  
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
#include <eXosip2/eXosip.h>

#include <osip2/osip_mt.h>
#include <osip2/osip_condv.h>

extern eXosip_t eXosip;

int ipv6_enable = 0;

static void *_eXosip_thread (void *arg);
static int _eXosip_execute (void);
static void _eXosip_keep_alive(void);

void
eXosip_enable_ipv6 (int _ipv6_enable)
{
  ipv6_enable = _ipv6_enable;
}

void
eXosip_masquerade_contact (const char *public_address, int port)
{
  if (public_address == NULL || public_address[0] == '\0')
    {
      memset (eXosip.net_interfaces[0].net_firewall_ip, '\0',
	      sizeof (eXosip.net_interfaces[0].net_firewall_ip));
      memset (eXosip.net_interfaces[1].net_firewall_ip, '\0',
	      sizeof (eXosip.net_interfaces[1].net_firewall_ip));
      memset (eXosip.net_interfaces[2].net_firewall_ip, '\0',
	      sizeof (eXosip.net_interfaces[2].net_firewall_ip));
      return;
    }

  snprintf (eXosip.net_interfaces[0].net_firewall_ip,
	    sizeof (eXosip.net_interfaces[0].net_firewall_ip), "%s",
            public_address);
  snprintf (eXosip.net_interfaces[1].net_firewall_ip,
	    sizeof (eXosip.net_interfaces[1].net_firewall_ip), "%s",
            public_address);
  snprintf (eXosip.net_interfaces[2].net_firewall_ip,
	    sizeof (eXosip.net_interfaces[2].net_firewall_ip), "%s",
            public_address);

  if (port>0)
    {
      snprintf (eXosip.net_interfaces[0].net_port,
		sizeof (eXosip.net_interfaces[0].net_port), "%i", port);
      snprintf (eXosip.net_interfaces[1].net_port,
		sizeof (eXosip.net_interfaces[1].net_port), "%i", port);
      snprintf (eXosip.net_interfaces[2].net_port,
		sizeof (eXosip.net_interfaces[2].net_port), "%i", port);
    }
  return;
}

int
eXosip_force_masquerade_contact (const char *public_address)
{
  if (public_address == NULL || public_address[0] == '\0')
    {
      memset (eXosip.net_interfaces[0].net_firewall_ip, '\0',
	      sizeof (eXosip.net_interfaces[0].net_firewall_ip));
      memset (eXosip.net_interfaces[1].net_firewall_ip, '\0',
	      sizeof (eXosip.net_interfaces[1].net_firewall_ip));
      memset (eXosip.net_interfaces[2].net_firewall_ip, '\0',
	      sizeof (eXosip.net_interfaces[2].net_firewall_ip));
      eXosip.forced_localip = 0;
      return 0;
    }
  eXosip.forced_localip = 1;
  snprintf (eXosip.net_interfaces[0].net_firewall_ip, 50, "%s", public_address);
  snprintf (eXosip.net_interfaces[1].net_firewall_ip, 50, "%s", public_address);
  snprintf (eXosip.net_interfaces[2].net_firewall_ip, 50, "%s", public_address);
  return 0;
}

int
eXosip_guess_localip (int family, char *address, int size)
{
  return eXosip_guess_ip_for_via (family, address, size);
}

int
eXosip_is_public_address (const char *c_address)
{
  return (0 != strncmp (c_address, "192.168", 7)
          && 0 != strncmp (c_address, "10.", 3)
          && 0 != strncmp (c_address, "172.16.", 7)
          && 0 != strncmp (c_address, "172.17.", 7)
          && 0 != strncmp (c_address, "172.18.", 7)
          && 0 != strncmp (c_address, "172.19.", 7)
          && 0 != strncmp (c_address, "172.20.", 7)
          && 0 != strncmp (c_address, "172.21.", 7)
          && 0 != strncmp (c_address, "172.22.", 7)
          && 0 != strncmp (c_address, "172.23.", 7)
          && 0 != strncmp (c_address, "172.24.", 7)
          && 0 != strncmp (c_address, "172.25.", 7)
          && 0 != strncmp (c_address, "172.26.", 7)
          && 0 != strncmp (c_address, "172.27.", 7)
          && 0 != strncmp (c_address, "172.28.", 7)
          && 0 != strncmp (c_address, "172.29.", 7)
          && 0 != strncmp (c_address, "172.30.", 7)
          && 0 != strncmp (c_address, "172.31.", 7)
          && 0 != strncmp (c_address, "169.254", 7));
}

void
eXosip_set_user_agent (const char *user_agent)
{
  osip_free (eXosip.user_agent);
  eXosip.user_agent = osip_strdup (user_agent);
}

void
eXosip_kill_transaction (osip_list_t * transactions)
{
  osip_transaction_t *transaction;

  if (!osip_list_eol (transactions, 0))
    {
      /* some transaction are still used by osip,
         transaction should be released by modules! */
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "module sfp: _osip_kill_transaction transaction should be released by modules!\n"));
    }

  while (!osip_list_eol (transactions, 0))
    {
      transaction = osip_list_get (transactions, 0);

      __eXosip_delete_jinfo (transaction);
      osip_transaction_free (transaction);
    }
}

void
eXosip_quit (void)
{
  jauthinfo_t *jauthinfo;
  eXosip_call_t *jc;
  eXosip_notify_t *jn;
  eXosip_subscribe_t *js;
  eXosip_reg_t *jreg;
  eXosip_pub_t *jpub;
  int i;
  int pos;

  eXosip.j_stop_ua = 1;         /* ask to quit the application */
  __eXosip_wakeup ();
  __eXosip_wakeup_event ();

  i = osip_thread_join ((struct osip_thread *) eXosip.j_thread);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: can't terminate thread!\n"));
    }
  osip_free ((struct osip_thread *) eXosip.j_thread);

  jpipe_close (eXosip.j_socketctl);
  jpipe_close (eXosip.j_socketctl_event);

  osip_free (eXosip.user_agent);

  for (jc = eXosip.j_calls; jc != NULL; jc = eXosip.j_calls)
    {
      REMOVE_ELEMENT (eXosip.j_calls, jc);
      eXosip_call_free (jc);
    }

  for (js = eXosip.j_subscribes; js != NULL; js = eXosip.j_subscribes)
    {
      REMOVE_ELEMENT (eXosip.j_subscribes, js);
      eXosip_subscribe_free (js);
    }

  for (jn = eXosip.j_notifies; jn != NULL; jn = eXosip.j_notifies)
    {
      REMOVE_ELEMENT (eXosip.j_notifies, jn);
      eXosip_notify_free (jn);
    }

  osip_mutex_destroy ((struct osip_mutex *) eXosip.j_mutexlock);
  osip_cond_destroy ((struct osip_cond *) eXosip.j_cond);

  if (eXosip.net_interfaces[0].net_socket)
    {
      close (eXosip.net_interfaces[0].net_socket);
      eXosip.net_interfaces[0].net_socket = -1;
    }
  if (eXosip.net_interfaces[1].net_socket)
    {
      close (eXosip.net_interfaces[1].net_socket);
      eXosip.net_interfaces[1].net_socket = -1;
    }
  if (eXosip.net_interfaces[2].net_socket)
    {
      close (eXosip.net_interfaces[2].net_socket);
      eXosip.net_interfaces[2].net_socket = -1;
    }

  for (pos=0; pos<EXOSIP_MAX_SOCKETS; pos++)
    {
      if (eXosip.net_interfaces[0].net_socket_tab[pos].socket!=0)
	close (eXosip.net_interfaces[0].net_socket_tab[pos].socket);
      if (eXosip.net_interfaces[1].net_socket_tab[pos].socket!=0)
	close (eXosip.net_interfaces[1].net_socket_tab[pos].socket);
      if (eXosip.net_interfaces[2].net_socket_tab[pos].socket!=0)
	close (eXosip.net_interfaces[2].net_socket_tab[pos].socket);
    }

  for (jreg = eXosip.j_reg; jreg != NULL; jreg = eXosip.j_reg)
    {
      REMOVE_ELEMENT (eXosip.j_reg, jreg);
      eXosip_reg_free (jreg);
    }

  for (jpub = eXosip.j_pub; jpub != NULL; jpub = eXosip.j_pub)
    {
      REMOVE_ELEMENT (eXosip.j_pub, jpub);
      _eXosip_pub_free (jpub);
    }

  while (!osip_list_eol (eXosip.j_transactions, 0))
    {
      osip_transaction_t *tr =
        (osip_transaction_t *) osip_list_get (eXosip.j_transactions, 0);
      if (tr->state == IST_TERMINATED || tr->state == ICT_TERMINATED
          || tr->state == NICT_TERMINATED || tr->state == NIST_TERMINATED)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                                  "Release a terminated transaction\n"));
          osip_list_remove (eXosip.j_transactions, 0);
          __eXosip_delete_jinfo (tr);
          osip_transaction_free (tr);
      } else
        {
          osip_list_remove (eXosip.j_transactions, 0);
          __eXosip_delete_jinfo (tr);
          osip_transaction_free (tr);
        }
    }

  osip_free (eXosip.j_transactions);

  eXosip_kill_transaction (eXosip.j_osip->osip_ict_transactions);
  eXosip_kill_transaction (eXosip.j_osip->osip_nict_transactions);
  eXosip_kill_transaction (eXosip.j_osip->osip_ist_transactions);
  eXosip_kill_transaction (eXosip.j_osip->osip_nist_transactions);
  osip_release (eXosip.j_osip);

  {
    eXosip_event_t *ev;

    for (ev = osip_fifo_tryget (eXosip.j_events); ev != NULL;
         ev = osip_fifo_tryget (eXosip.j_events))
      eXosip_event_free (ev);
  }

  osip_fifo_free (eXosip.j_events);

  for (jauthinfo = eXosip.authinfos; jauthinfo != NULL;
       jauthinfo = eXosip.authinfos)
    {
      REMOVE_ELEMENT (eXosip.authinfos, jauthinfo);
      osip_free (jauthinfo);
    }

  return;
}

int eXosip_set_socket(int transport, int socket, int port)
{
  if (eXosip.net_interfaces[0].net_socket>0)
    close(eXosip.net_interfaces[0].net_socket);

  eXosip.net_interfaces[0].net_socket = socket;
  snprintf (eXosip.net_interfaces[0].net_port,
	    sizeof (eXosip.net_interfaces[0].net_port), "%i", port);

  eXosip.j_thread = (void *) osip_thread_create (20000, _eXosip_thread, NULL);
  if (eXosip.j_thread == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: Cannot start thread!\n"));
      return -1;
    }
  return 0;
}

#ifdef IPV6_V6ONLY
int setsockopt_ipv6only (int sock)
{
  int on = 1;

  return setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
		    (char *)&on, sizeof(on));
}
#endif	/* IPV6_V6ONLY */

int
eXosip_listen_addr (int transport, const char *addr, int port, int family,
		    int secure)
{
  int res;
  struct addrinfo *addrinfo = NULL;
  struct addrinfo *curinfo;
  const char *node = addr;
  int sock = -1;
  struct eXosip_net *net_int;
  char localip[256];

  if (transport==IPPROTO_UDP)
    net_int = &eXosip.net_interfaces[0];
  else if (transport==IPPROTO_TCP)
    net_int = &eXosip.net_interfaces[1];
  else if (transport==IPPROTO_TCP && secure != 0)
    net_int = &eXosip.net_interfaces[2];
  else
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unknown protocol (use IPPROTO_UDP or IPPROTO_TCP!\n"));
      return -1;
    }

  if (eXosip.http_port)
  {
      /* USE TUNNEL CAPABILITY */
      transport=IPPROTO_TCP;
  }

  if (port < 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: port must be higher than 0!\n"));
      return -1;
    }

  net_int->net_ip_family = family;
  if (family == AF_INET6)
    {
      ipv6_enable = 1;
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "IPv6 is enabled. Pls report bugs\n"));
    }

  eXosip_guess_localip (net_int->net_ip_family, localip,
			sizeof (localip));
  if (localip[0] == '\0')
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No ethernet interface found!\n"));
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: using 127.0.0.1 (debug mode)!\n"));
      /* we should always fallback on something. The linphone user will surely
         start linphone BEFORE setting its dial up connection. */
    }

  if (!node) {
    node = ipv6_enable ? "::" : "0.0.0.0";
  }


  res = eXosip_get_addrinfo(&addrinfo, node, port, transport);
  if (res)
    return -1;
    
  for (curinfo = addrinfo; curinfo; curinfo = curinfo->ai_next)
    {
      socklen_t len;

      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_INFO2, NULL,
		   "eXosip: address for protocol %d (search %d)\n",
		   curinfo->ai_protocol, transport));
      if (curinfo->ai_protocol && curinfo->ai_protocol != transport)	
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
		      (__FILE__, __LINE__, OSIP_ERROR, NULL,
		       "eXosip: Cannot create socket!\n",
		       strerror(errno)));
	  continue;
	}

    if (eXosip.http_port)
    {
        break;
    }

      if (curinfo->ai_family == AF_INET6)
	{
#ifdef IPV6_V6ONLY
	  if (setsockopt_ipv6only(sock))
	    {
	      close(sock);
	      sock = -1;
	      OSIP_TRACE (osip_trace
			  (__FILE__, __LINE__, OSIP_ERROR, NULL,
			   "eXosip: Cannot set socket option!\n",
			   strerror(errno)));
	      continue;
	    }
#endif	/* IPV6_V6ONLY */
	}
      
      res = bind (sock, curinfo->ai_addr, curinfo->ai_addrlen);
      if (res < 0)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_ERROR, NULL,
		       "eXosip: Cannot bind socket node:%s family:%d %s\n",
		       node, curinfo->ai_family, strerror(errno)));
	  close(sock);
	  sock = -1;
	  continue;
	}
      len = sizeof(net_int->ai_addr);
      res = getsockname(sock, (struct sockaddr*)&net_int->ai_addr,
			&len );
      if (res!=0)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_ERROR, NULL,
		       "eXosip: Cannot get socket name (%s)\n",
		       strerror(errno)));
	  memcpy(&net_int->ai_addr, curinfo->ai_addr, curinfo->ai_addrlen);
	}

      if (transport!=IPPROTO_UDP)
	{
	  res = listen(sock, SOMAXCONN);
	  if (res < 0)
	    {
	      OSIP_TRACE (osip_trace
			  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		       "eXosip: Cannot bind socket node:%s family:%d %s\n",
			   node, curinfo->ai_family, strerror(errno)));
	      close(sock);
	      sock = -1;
	      continue;
	    }
        }
      
      break;
    }

  freeaddrinfo(addrinfo);

  if (sock < 0)
    {
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		   "eXosip: Cannot bind on port: %i\n",
		   port));
      return -1;
    }

  if (eXosip.http_port)
      net_int->net_protocol = IPPROTO_UDP;
  else
      net_int->net_protocol = transport;
  net_int->net_socket = sock;

  if (port==0)
    {
      /* get port number from socket */
      if (ipv6_enable == 0)
	port = ntohs (((struct sockaddr_in*)&net_int->ai_addr)->sin_port);
      else
	port = ntohs (((struct sockaddr_in6*)&net_int->ai_addr)->sin6_port);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "eXosip: Binding on port %i!\n", port));
    }

  snprintf (net_int->net_port,
	    sizeof (net_int->net_port) - 1, "%i", port);



  if (eXosip.http_port)
  {
        /* only ipv4 */
        struct sockaddr_in	_addr;
	    char http_req[2048];
	    char http_reply[2048];
        int len;
        _addr.sin_port = (unsigned short) htons(eXosip.http_port);
        _addr.sin_addr.s_addr = inet_addr(eXosip.http_proxy);
		_addr.sin_family = PF_INET;

		if (connect(net_int->net_socket, (struct sockaddr *) &_addr, sizeof(_addr)) == -1)
		{
            OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                  "eXosip: Failed to connect to http server on %s:%i!\n", eXosip.http_proxy, port));
			return -1;
		}

        sprintf(http_req, "GET / HTTP/1.1\r\nUdpHost: %s:%d\r\n\r\n", eXosip.http_outbound_proxy, 5060);

		len = send(net_int->net_socket, http_req, (int) strlen(http_req), 0);

		if (len < 0)
			return -1;

		osip_usleep(50000);

		if ((len = recv(net_int->net_socket, http_reply, sizeof(http_reply), 0)) > 0)
			http_reply[len] = '\0';
		else
			return -1;

		if (strncmp(http_reply, "HTTP/1.0 200 OK\r\n", 17) == 0 || strncmp(http_reply, "HTTP/1.1 200 OK\r\n", 17) == 0)
        {
        }
		else
			return -1;
  }

  eXosip.j_thread = (void *) osip_thread_create (20000, _eXosip_thread, NULL);
  if (eXosip.j_thread == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: Cannot start thread!\n"));
      return -1;
    }
  return 0;
}

int
eXosip_init (void)
{
  osip_t *osip;

  memset (&eXosip, 0, sizeof (eXosip));

#ifdef WIN32
  /* Initializing windows socket library */
  {
    WORD wVersionRequested;
    WSADATA wsaData;
    int i;

    wVersionRequested = MAKEWORD (1, 1);
    i = WSAStartup (wVersionRequested, &wsaData);
    if (i != 0)
      {
        OSIP_TRACE (osip_trace
                    (__FILE__, __LINE__, OSIP_WARNING, NULL,
                     "eXosip: Unable to initialize WINSOCK, reason: %d\n", i));
        /* return -1; It might be already initilized?? */
      }
  }
#endif

  eXosip.user_agent = osip_strdup ("eXosip/" EXOSIP_VERSION);

  eXosip.j_calls = NULL;
  eXosip.j_stop_ua = 0;
  eXosip.j_thread = NULL;
  eXosip.j_transactions = (osip_list_t *) osip_malloc (sizeof (osip_list_t));
  osip_list_init (eXosip.j_transactions);
  eXosip.j_reg = NULL;

  eXosip.j_cond = (struct osip_cond *) osip_cond_init ();

  eXosip.j_mutexlock = (struct osip_mutex *) osip_mutex_init ();

  if (-1 == osip_init (&osip))
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: Cannot initialize osip!\n"));
      return -1;
    }

  osip_set_application_context (osip, &eXosip);

  eXosip_set_callbacks (osip);

  eXosip.j_osip = osip;

  /* open a TCP socket to wake up the application when needed. */
  eXosip.j_socketctl = jpipe ();
  if (eXosip.j_socketctl == NULL)
    return -1;

  eXosip.j_socketctl_event = jpipe ();
  if (eXosip.j_socketctl_event == NULL)
    return -1;

  /* To be changed in osip! */
  eXosip.j_events = (osip_fifo_t *) osip_malloc (sizeof (osip_fifo_t));
  osip_fifo_init (eXosip.j_events);

  return 0;
}


static int
_eXosip_execute (void)
{
  struct timeval lower_tv;
  int i;

  osip_timers_gettimeout (eXosip.j_osip, &lower_tv);
  if (lower_tv.tv_sec > 15)
    {
      lower_tv.tv_sec = 15;
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "eXosip: Reseting timer to 15s before waking up!\n"));
  } else
    {
        /*  add a small amount of time on windows to avoid
            waking up too early. (probably a bad time precision) */
        if (lower_tv.tv_usec<900000)
            lower_tv.tv_usec = 100000; /* add 10ms */
        else 
        {
            lower_tv.tv_usec = 10000; /* add 10ms */
            lower_tv.tv_sec++;
        }
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "eXosip: timer sec:%i usec:%i!\n",
                   lower_tv.tv_sec, lower_tv.tv_usec));
    }
  i = eXosip_read_message (1, lower_tv.tv_sec, lower_tv.tv_usec);

  if (i == -2)
    {
      return -2;
    }

  eXosip_lock ();
  osip_timers_ict_execute (eXosip.j_osip);
  osip_timers_nict_execute (eXosip.j_osip);
  osip_timers_ist_execute (eXosip.j_osip);
  osip_timers_nist_execute (eXosip.j_osip);

  osip_ict_execute (eXosip.j_osip);
  osip_nict_execute (eXosip.j_osip);
  osip_ist_execute (eXosip.j_osip);
  osip_nist_execute (eXosip.j_osip);

  /* free all Calls that are in the TERMINATED STATE? */
  eXosip_release_terminated_calls ();
  eXosip_release_terminated_registrations ();

  eXosip_unlock ();


  if (eXosip.keep_alive>0)
  {
      _eXosip_keep_alive();
  }

  return 0;
}

int eXosip_set_option(eXosip_option opt, void *value)
{
    int val;
    char *tmp;
	switch (opt) {
		case EXOSIP_OPT_UDP_KEEP_ALIVE:
            val = *((int*)value);
            eXosip.keep_alive = val; /* value in ms */
            break;
		case EXOSIP_OPT_UDP_LEARN_PORT:
            val = *((int*)value);
            eXosip.learn_port = val; /* value in ms */
            break;
		case EXOSIP_OPT_SET_HTTP_TUNNEL_PORT:
            val = *((int*)value);
            eXosip.http_port = val; /* value in ms */
            break;
		case EXOSIP_OPT_SET_HTTP_TUNNEL_PROXY:
            tmp = (char*)value;
            memset(eXosip.http_proxy, '\0', sizeof(eXosip.http_proxy));
            if (tmp!=NULL && tmp[0]!='\0')
                strncpy(eXosip.http_proxy, tmp, sizeof(eXosip.http_proxy)); /* value in proxy:port */
            break;
		case EXOSIP_OPT_SET_HTTP_OUTBOUND_PROXY:
            tmp = (char*)value;
            memset(eXosip.http_outbound_proxy, '\0', sizeof(eXosip.http_outbound_proxy));
            if (tmp!=NULL && tmp[0]!='\0')
                strncpy(eXosip.http_outbound_proxy, tmp, sizeof(eXosip.http_outbound_proxy)); /* value in proxy:port */
            break;
            
    }
    return 0;
}

static void
_eXosip_keep_alive(void)
{
    static struct timeval mtimer = { 0, 0 };

    eXosip_reg_t *jr;
    struct eXosip_net *net;
    char buf[4] = "jaK";
    struct timeval now;
    osip_gettimeofday (&now, NULL);

    if (mtimer.tv_sec==0 && mtimer.tv_usec==0)
    {
        /* first init */
        osip_gettimeofday (&mtimer, NULL);
	    add_gettimeofday (&mtimer, eXosip.keep_alive);
    }

    if (osip_timercmp (&now, &mtimer, <))
    {
        return; /* not yet time */
    }

    /* reset timer */
    osip_gettimeofday (&mtimer, NULL);
	add_gettimeofday (&mtimer, eXosip.keep_alive);

    net = &eXosip.net_interfaces[0];
    if (net == NULL)
    {
        return ;
    }

    for (jr = eXosip.j_reg; jr != NULL; jr = jr->next)
    {
        if (jr->len>0)
        {
            if (sendto (net->net_socket, (const void *) buf, 4, 0,	 
                (struct sockaddr *) &(jr->addr), jr->len )>0)
            {
                OSIP_TRACE (osip_trace
                            (__FILE__, __LINE__, OSIP_INFO1, NULL,
                            "eXosip: Keep Alive sent on UDP!\n"));
            }
        }
    }
}

void *
_eXosip_thread (void *arg)
{
  int i;

  while (eXosip.j_stop_ua == 0)
    {
      i = _eXosip_execute ();
      if (i == -2)
        osip_thread_exit ();
    }
  osip_thread_exit ();
  return NULL;
}

