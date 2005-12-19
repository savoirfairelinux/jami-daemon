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

#include <stdlib.h>

#ifdef WIN32
#include <windowsx.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include "inet_ntop.h"

#else
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <eXosip2/eXosip.h>
#include "eXosip2.h"

extern eXosip_t eXosip;


/* Private functions */
static void rcvregister_failure (int type, osip_transaction_t * tr,
                                 osip_message_t * sip);
static void cb_ict_kill_transaction (int type, osip_transaction_t * tr);
static void cb_ist_kill_transaction (int type, osip_transaction_t * tr);
static void cb_nict_kill_transaction (int type, osip_transaction_t * tr);
static void cb_nist_kill_transaction (int type, osip_transaction_t * tr);
static void cb_rcvinvite (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcvack (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcvack2 (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcvregister (int type, osip_transaction_t * tr,
                            osip_message_t * sip);
static void cb_rcvcancel (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcvrequest (int type, osip_transaction_t * tr,
                              osip_message_t * sip);
static void cb_sndinvite (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_sndack (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_sndregister (int type, osip_transaction_t * tr,
                            osip_message_t * sip);
static void cb_sndbye (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_sndcancel (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_sndinfo (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_sndoptions (int type, osip_transaction_t * tr,
                           osip_message_t * sip);
static void cb_sndnotify (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_sndsubscribe (int type, osip_transaction_t * tr,
                             osip_message_t * sip);
static void cb_sndunkrequest (int type, osip_transaction_t * tr,
                              osip_message_t * sip);
static void cb_rcv1xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv2xx_4invite (osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv2xx_4subscribe (osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv2xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv3xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv4xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv5xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcv6xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_snd1xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_snd2xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_snd3xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_snd4xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_snd5xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_snd6xx (int type, osip_transaction_t * tr, osip_message_t * sip);
static void cb_rcvresp_retransmission (int type, osip_transaction_t * tr,
                                       osip_message_t * sip);
static void cb_sndreq_retransmission (int type, osip_transaction_t * tr,
                                      osip_message_t * sip);
static void cb_sndresp_retransmission (int type, osip_transaction_t * tr,
                                       osip_message_t * sip);
static void cb_rcvreq_retransmission (int type, osip_transaction_t * tr,
                                      osip_message_t * sip);
static void cb_transport_error (int type, osip_transaction_t * tr, int error);


int
cb_snd_message (osip_transaction_t * tr, osip_message_t * sip, char *host,
		int port, int out_socket)
{
  int i;
  osip_via_t *via;
  if (eXosip.net_interfaces[0].net_socket == 0
      && eXosip.net_interfaces[1].net_socket == 0)
    return -1;

  if (host == NULL)
    {
      host = sip->req_uri->host;
      if (sip->req_uri->port != NULL)
        port = osip_atoi (sip->req_uri->port);
      else
        port = 5060;
    }

  via = (osip_via_t *) osip_list_get(sip->vias, 0);
  if (via==NULL || via->protocol==NULL)
    return -1;

  i = -1;
  if (osip_strcasecmp(via->protocol, "udp")==0)
    {
      i = cb_udp_snd_message (tr, sip, host, port, out_socket);
    }
  else
    {
      i = cb_tcp_snd_message (tr, sip, host, port, out_socket);
    }
  if (i != 0)
    {
      return -1;
    }

  return 0;

}

int
cb_udp_snd_message (osip_transaction_t * tr, osip_message_t * sip, char *host,
                    int port, int out_socket)
{
  int len = 0;
  size_t length = 0;
  struct addrinfo *addrinfo;
  struct __eXosip_sockaddr addr;
  char *message;
#ifdef INET6_ADDRSTRLEN
  char ipbuf[INET6_ADDRSTRLEN];
#else
  char ipbuf[46];
#endif
  int i;
  struct eXosip_net *net;

  if (eXosip.net_interfaces[0].net_socket == 0)
    return -1;

  net = &eXosip.net_interfaces[0];

  if (eXosip.http_port)
  {
    i = osip_message_to_str (sip, &message, &length);

    if (i != 0 || length <= 0)
        {
        return -1;
        }
    if (0 >
        _eXosip_sendto (net->net_socket, (const void *) message, length, 0,
	        (struct sockaddr *) &addr, len ))
    {
        /* should reopen connection! */
        osip_free (message);
        return -1;
    }
    return 0;
  }

  if (host == NULL)
    {
      host = sip->req_uri->host;
      if (sip->req_uri->port != NULL)
        port = osip_atoi (sip->req_uri->port);
      else
        port = 5060;
    }

  i = eXosip_get_addrinfo (&addrinfo, host, port, IPPROTO_UDP);
  if (i != 0)
    {
      return -1;
    }

  memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
  len = addrinfo->ai_addrlen;

  freeaddrinfo (addrinfo);

  i = osip_message_to_str (sip, &message, &length);

  if (i != 0 || length <= 0)
    {
      return -1;
    }

  switch (addr.ss_family)
    {
    case AF_INET:
      inet_ntop(addr.ss_family, &(((struct sockaddr_in*)&addr)->sin_addr), ipbuf, sizeof(ipbuf));
      break;
    case AF_INET6:
      inet_ntop(addr.ss_family, &(((struct sockaddr_in6*)&addr)->sin6_addr), ipbuf, sizeof(ipbuf));
      break;
    default:
      strncpy(ipbuf, "(unknown)", sizeof(ipbuf));
      break;
    }
  
  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
			  "Message sent: \n%s (to dest=%s:%i)\n",
			  message, ipbuf, port));
  if (0 >
      _eXosip_sendto (net->net_socket, (const void *) message, length, 0,
	      (struct sockaddr *) &addr, len ))
    {
#ifdef WIN32
      if (WSAECONNREFUSED == WSAGetLastError ())
#else
      if (ECONNREFUSED == errno)
#endif
        {
          /* This can be considered as an error, but for the moment,
             I prefer that the application continue to try sending
             message again and again... so we are not in a error case.
             Nevertheless, this error should be announced!
             ALSO, UAS may not have any other options than retry always
             on the same port.
           */
          osip_free (message);
          return 1;
      } else
        {
          /* SIP_NETWORK_ERROR; */
          osip_free (message);
          return -1;
        }
    }


    if (eXosip.keep_alive>0)
    {
        if (MSG_IS_REGISTER(sip))
        {
            eXosip_reg_t *reg = NULL;
            if (_eXosip_reg_find(&reg, tr)==0)
            {
                memcpy (&(reg->addr), &addr, len);
                reg->len = len;
            }
        }
    }

  osip_free (message);
  return 0;

}

int
cb_tcp_snd_message (osip_transaction_t * tr, osip_message_t * sip, char *host,
                    int port, int out_socket)
{
  size_t length = 0;
  char *message;
  int i;
  struct eXosip_net *net;
  if (eXosip.net_interfaces[1].net_socket == 0)
    return -1;

  if (host == NULL)
    {
      host = sip->req_uri->host;
      if (sip->req_uri->port != NULL)
        port = osip_atoi (sip->req_uri->port);
      else
        port = 5060;
    }

  net = &eXosip.net_interfaces[1];

  i = osip_message_to_str (sip, &message, &length);
  
  if (i != 0 || length <= 0)
    {
      return -1;
    }

  /* Step 1: find existing socket to send message */
  if (out_socket<=0)
    {
      out_socket = _eXosip_tcp_find_socket(host, port);
      
      /* Step 2: create new socket with host:port */
      if (out_socket<=0)
	{
	  out_socket = _eXosip_tcp_connect_socket(host, port);
	}
      
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
			      "Message sent: \n%s (to dest=%s:%i)\n",
			      message, host, port));
    }
  else
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
			      "Message sent: \n%s (reusing REQUEST connection)\n",
			      message, host, port));
    }

  if (out_socket<=0)
    {
      return -1;
    }


  if (0 > send (out_socket, (const void *) message, length, 0))
    {
#ifdef WIN32
      if (WSAECONNREFUSED == WSAGetLastError ())
#else
      if (ECONNREFUSED == errno)
#endif
        {
          /* This can be considered as an error, but for the moment,
             I prefer that the application continue to try sending
             message again and again... so we are not in a error case.
             Nevertheless, this error should be announced!
             ALSO, UAS may not have any other options than retry always
             on the same port.
           */
          osip_free (message);
          return 1;
      } else
        {
          /* SIP_NETWORK_ERROR; */
	  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
				  "TCP error: \n%s\n",
				  strerror(errno)));
          osip_free (message);
          return -1;
        }
    }

  osip_free (message);
  return 0;

}

static void
cb_ict_kill_transaction (int type, osip_transaction_t * tr)
{
  int i;

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_ict_kill_transaction (id=%i)\r\n", tr->transactionid));

  i = osip_remove_transaction (eXosip.j_osip, tr);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_BUG, NULL,
                   "cb_ict_kill_transaction Error: Could not remove transaction from the oSIP stack? (id=%i)\r\n",
                   tr->transactionid));
    }
}

static void
cb_ist_kill_transaction (int type, osip_transaction_t * tr)
{
  int i;

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_ist_kill_transaction (id=%i)\r\n", tr->transactionid));
  i = osip_remove_transaction (eXosip.j_osip, tr);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_BUG, NULL,
                   "cb_ist_kill_transaction Error: Could not remove transaction from the oSIP stack? (id=%i)\r\n",
                   tr->transactionid));
    }
}

static void
cb_nict_kill_transaction (int type, osip_transaction_t * tr)
{
  int i;
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_nict_kill_transaction (id=%i)\r\n", tr->transactionid));
  i = osip_remove_transaction (eXosip.j_osip, tr);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_BUG, NULL,
                   "cb_nict_kill_transaction Error: Could not remove transaction from the oSIP stack? (id=%i)\r\n",
                   tr->transactionid));
    }

  if (MSG_IS_REGISTER (tr->orig_request)
      && type == OSIP_NICT_KILL_TRANSACTION && tr->last_response == NULL)
    {
      eXosip_event_t *je;
      eXosip_reg_t *jreg = NULL;

      /* find matching j_reg */
      _eXosip_reg_find (&jreg, tr);
      if (jreg != NULL)
        {
          je = eXosip_event_init_for_reg (EXOSIP_REGISTRATION_FAILURE, jreg, tr);
          report_event (je, NULL);
        }
      return;
    }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (jn == NULL && js == NULL)
    return;

  /* no answer to a NOTIFY request! */
  if (MSG_IS_NOTIFY (tr->orig_request)
      && type == OSIP_NICT_KILL_TRANSACTION && tr->last_response == NULL)
    {
      /* delete the dialog! */
      REMOVE_ELEMENT (eXosip.j_notifies, jn);
      eXosip_notify_free (jn);
      return;
    }

  if (MSG_IS_NOTIFY (tr->orig_request)
      && type == OSIP_NICT_KILL_TRANSACTION
      && tr->last_response != NULL && tr->last_response->status_code > 299)
    {
      /* delete the dialog! */
      if (tr->last_response->status_code != 407
          && tr->last_response->status_code != 401)
        {
          REMOVE_ELEMENT (eXosip.j_notifies, jn);
          eXosip_notify_free (jn);
          return;
        }
    }

  if (MSG_IS_NOTIFY (tr->orig_request)
      && type == OSIP_NICT_KILL_TRANSACTION
      && tr->last_response != NULL
      && tr->last_response->status_code > 199
      && tr->last_response->status_code < 300)
    {
      if (jn->n_ss_status == EXOSIP_SUBCRSTATE_TERMINATED)
        {
          /* delete the dialog! */
          REMOVE_ELEMENT (eXosip.j_notifies, jn);
          eXosip_notify_free (jn);
          return;
        }
    }

  /* no answer to a SUBSCRIBE request! */
  if (MSG_IS_SUBSCRIBE (tr->orig_request)
      && type == OSIP_NICT_KILL_TRANSACTION && tr->last_response == NULL)
    {
      /* delete the dialog! */
      REMOVE_ELEMENT (eXosip.j_subscribes, js);
      eXosip_subscribe_free (js);
      return;
    }

  /* detect SUBSCRIBE request that close the dialogs! */
  /* expires=0 with MSN */
  if (MSG_IS_SUBSCRIBE (tr->orig_request) && type == OSIP_NICT_KILL_TRANSACTION)
    {
      osip_header_t *expires;

      osip_message_get_expires (tr->orig_request, 0, &expires);
      if (expires == NULL || expires->hvalue == NULL)
        {
      } else if (0 == strcmp (expires->hvalue, "0"))
        {
          /* delete the dialog! */
          REMOVE_ELEMENT (eXosip.j_subscribes, js);
          eXosip_subscribe_free (js);
          return;
        }
    }
}

static void
cb_nist_kill_transaction (int type, osip_transaction_t * tr)
{
  int i;

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_nist_kill_transaction (id=%i)\r\n", tr->transactionid));
  i = osip_remove_transaction (eXosip.j_osip, tr);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_BUG, NULL,
                   "cb_nist_kill_transaction Error: Could not remove transaction from the oSIP stack? (id=%i)\r\n",
                   tr->transactionid));
    }

}

static void
cb_rcvinvite (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcvinvite (id=%i)\n",
               tr->transactionid));
}

static void
cb_rcvack (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcvack (id=%i)\n",
               tr->transactionid));
}

static void
cb_rcvack2 (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcvack2 (id=%i)\r\n",
               tr->transactionid));
}

static void
cb_rcvregister (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_event_t *je;
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_rcvregister (id=%i)\r\n", tr->transactionid));
  
  je = eXosip_event_init_for_message (EXOSIP_MESSAGE_NEW, tr);
  eXosip_event_add (je);
  return;
}

static void
cb_rcvcancel (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_rcvcancel (id=%i)\r\n", tr->transactionid));
}

static void
cb_rcvrequest (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_notify_t *jn;
  eXosip_subscribe_t *js;

  eXosip_event_t *je;

  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_rcvunkrequest (id=%i)\r\n", tr->transactionid));

  if (jinfo == NULL)
    {
      eXosip_event_t *je;
      
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_NEW, tr);
      eXosip_event_add (je);
      return;
    }

  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;
  if (jc == NULL && jn == NULL && js == NULL)
    {
      eXosip_event_t *je;
      
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_NEW, tr);
      eXosip_event_add (je);
      return;
    }
  else if (jc!=NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "cb_rcv? (id=%i)\r\n", tr->transactionid));

      report_call_event (EXOSIP_CALL_MESSAGE_NEW, jc, jd, tr);
      return;
    }
  else if (jn!=NULL)
    {
      if (MSG_IS_SUBSCRIBE (sip))
	{
	  je = eXosip_event_init_for_notify (EXOSIP_IN_SUBSCRIPTION_NEW, jn, jd, tr);
	  report_event (je, NULL);
	  return;
	}
      return;
    }
  else if (js!=NULL)
    {
      if (MSG_IS_NOTIFY (sip))
	{
	  je = eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_NOTIFY, js, jd, tr);
	  report_event (je, NULL);      
	  return;
	}
      return;
    }
}

static void
cb_sndinvite (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndinvite (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndack (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_sndack (id=%i)\r\n",
               tr->transactionid));
}

static void
cb_sndregister (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndregister (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndbye (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_sndbye (id=%i)\r\n",
               tr->transactionid));
}

static void
cb_sndcancel (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndcancel (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndinfo (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_sndinfo (id=%i)\r\n",
               tr->transactionid));
}

static void
cb_sndoptions (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndoptions (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndnotify (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndnotify (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndsubscribe (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndsubscibe (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndunkrequest (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndunkrequest (id=%i)\r\n", tr->transactionid));
}

void
__eXosip_delete_jinfo (osip_transaction_t * transaction)
{
  jinfo_t *ji;

  if (transaction == NULL)
    return;
  ji = osip_transaction_get_your_instance (transaction);
  osip_free (ji);
  osip_transaction_set_your_instance (transaction, NULL);
}

jinfo_t *
__eXosip_new_jinfo (eXosip_call_t * jc, eXosip_dialog_t * jd,
                    eXosip_subscribe_t * js, eXosip_notify_t * jn)
{
  jinfo_t *ji = (jinfo_t *) osip_malloc (sizeof (jinfo_t));

  if (ji == NULL)
    return NULL;
  ji->jd = jd;
  ji->jc = jc;
  ji->js = js;
  ji->jn = jn;
  return ji;
}

static void
cb_rcv1xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcv1xx (id=%i)\r\n",
               tr->transactionid));

  if (eXosip.learn_port>0)
  {
      struct eXosip_net *net;
      net = &eXosip.net_interfaces[0];
      /* EXOSIP_OPT_UDP_LEARN_PORT option set */
#if 1
      /* learn through rport */
      if (net->net_firewall_ip[0]!='\0')
      {
        osip_via_t *via=NULL;
        osip_generic_param_t *br;
        int i = osip_message_get_via (sip, 0, &via);
        if (via!=NULL && via->protocol!=NULL
            && osip_strcasecmp(via->protocol, "udp")==0)
        {
            osip_via_param_get_byname (via, "rport", &br);
            if (br!=NULL && br->gvalue!=NULL)
            {
                snprintf(net->net_port, 20, "%s", br->gvalue);
                OSIP_TRACE (osip_trace
                            (__FILE__, __LINE__, OSIP_INFO1, NULL,
                            "cb_rcv1xx (id=%i) SIP port modified from rport in REGISTER answer\r\n",
                            tr->transactionid));
            }
        }
      }
#else
      /* learn through REGISTER? */
      if (net->net_firewall_ip[0]!='\0')
	      {
		int pos=0;
		while (!osip_list_eol(reg->contacts,pos))
		  {
		    osip_contact_t *co;
		    co = (osip_contact_t *)osip_list_get(reg->contacts,pos);
		    pos++;
		    if (co!=NULL && co->url!=NULL && co->url->host!=NULL
			&& 0==osip_strcasecmp(co->url->host,
					      net->net_firewall_ip))
		      {
			if (co->url->port==NULL &&
			    0!=osip_strcasecmp(net->net_port, "5060"))
			  {
			    co->url->port=osip_strdup(net->net_port);
			  }
			else if (co->url->port!=NULL &&
				 0!=osip_strcasecmp(net->net_port, co->url->port))
			  {
			    osip_free(co->url->port);
			    co->url->port=osip_strdup(net->net_port);
			  }
		      }
		  }
	      }
#endif
  }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (MSG_IS_RESPONSE_FOR (sip, "OPTIONS"))
    {
      if (jc == NULL)
        {
	  eXosip_event_t *je;
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_INFO1, NULL,
                       "cb_rcv1xx (id=%i) OPTIONS outside of any call\r\n",
                       tr->transactionid));
      
	  je = eXosip_event_init_for_message (EXOSIP_MESSAGE_PROCEEDING, tr);
	  eXosip_event_add (je);
          return;
      }
      report_call_event (EXOSIP_CALL_MESSAGE_PROCEEDING, jc, jd, tr);
      return;
    }

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE") && MSG_TEST_CODE (sip, 100))
    {
      report_call_event (EXOSIP_CALL_PROCEEDING, jc, jd, tr);
    }

  if ((MSG_IS_RESPONSE_FOR (sip, "INVITE")
       || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE")) && !MSG_TEST_CODE (sip, 100))
    {
      int i;

      /* for SUBSCRIBE, test if the dialog has been already created
         with a previous NOTIFY */
      if (jd == NULL && js != NULL && js->s_dialogs != NULL
          && MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
        {
          /* find if existing dialog match the to tag */
          osip_generic_param_t *tag;
          int i;

          i = osip_to_get_tag (sip->to, &tag);
          if (i == 0 && tag != NULL && tag->gvalue != NULL)
            {
              for (jd = js->s_dialogs; jd != NULL; jd = jd->next)
                {
                  if (0 == strcmp (jd->d_dialog->remote_tag, tag->gvalue))
                    {
                      OSIP_TRACE (osip_trace
                                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                   "eXosip: found established early dialog for this subscribe\n"));
                      jinfo->jd = jd;
                      break;
                    }
                }
            }
        }

      if (jd == NULL)           /* This transaction initiate a dialog in the case of
                                   INVITE (else it would be attached to a "jd" element. */
        {
          /* allocate a jd */

          i = eXosip_dialog_init_as_uac (&jd, sip);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: cannot establish a dialog\n"));
              return;
            }
          if (jc != NULL)
            {
              ADD_ELEMENT (jc->c_dialogs, jd);
              jinfo->jd = jd;
              eXosip_update ();
          } else if (js != NULL)
            {
              ADD_ELEMENT (js->s_dialogs, jd);
              jinfo->jd = jd;
              eXosip_update ();
          } else if (jn != NULL)
            {
              ADD_ELEMENT (jn->n_dialogs, jd);
              jinfo->jd = jd;
              eXosip_update ();
          } else
            {
#ifndef WIN32
              assert (0 == 0);
#else
              exit (0);
#endif
            }
          osip_transaction_set_your_instance (tr, jinfo);
      } else
        {
          osip_dialog_update_route_set_as_uac (jd->d_dialog, sip);
        }

      if (jd != NULL)
        jd->d_STATE = JD_TRYING;
      if (jd != NULL && MSG_IS_RESPONSE_FOR (sip, "INVITE")
          && sip->status_code < 180)
        {
          report_call_event (EXOSIP_CALL_PROCEEDING, jc, jd, tr);
      } else if (jd != NULL && MSG_IS_RESPONSE_FOR (sip, "INVITE")
                 && sip->status_code >= 180)
        {
          report_call_event (EXOSIP_CALL_RINGING, jc, jd, tr);
      } else if (jd != NULL && MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
        {
          eXosip_event_t *je;

          je =
            eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_PROCEEDING,
                                             js, jd, tr);
          report_event (je, sip);
        }
      if (MSG_TEST_CODE (sip, 180) && jd != NULL)
        {
          jd->d_STATE = JD_RINGING;
      } else if (MSG_TEST_CODE (sip, 183) && jd != NULL)
        {
          jd->d_STATE = JD_QUEUED;
        }

    }
}

static void
cb_rcv2xx_4invite (osip_transaction_t * tr, osip_message_t * sip)
{
  int i;
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)               /* This transaction initiate a dialog in the case of
                                   INVITE (else it would be attached to a "jd" element. */
    {
      /* allocate a jd */
      i = eXosip_dialog_init_as_uac (&jd, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot establish a dialog\n"));
          return;
        }
      ADD_ELEMENT (jc->c_dialogs, jd);
      jinfo->jd = jd;
      eXosip_update ();
      osip_transaction_set_your_instance (tr, jinfo);
  } else
    {
      /* Here is a special case:
         We have initiated a dialog and we have received informationnal
         answers from 2 or more remote SIP UA. Those answer can be
         differentiated with the "To" header's tag.

         We have used the first informationnal answer to create a
         dialog, but we now want to be sure the 200ok received is
         for the dialog this dialog.

         We have to check the To tag and if it does not match, we
         just have to modify the existing dialog and replace it. */
      osip_generic_param_t *tag;
      int i;

      i = osip_to_get_tag (sip->to, &tag);
      i = 1;                    /* default is the same dialog */

      if (jd->d_dialog == NULL || jd->d_dialog->remote_tag == NULL)
        {
          /* There are real use-case where a BYE is received/processed before
             the 200ok of the previous INVITE. In this case, jd->d_dialog is
             empty and the transaction should be silently discarded. */
          /* a ACK should still be sent... -but there is no dialog built- */
          return;
        }

      if (jd->d_dialog->remote_tag == NULL && tag == NULL)
        {
        } /* non compliant remote UA -> assume it is the same dialog */
      else if (jd->d_dialog->remote_tag != NULL && tag == NULL)
        {
          i = 0;
        } /* different dialog! */
      else if (jd->d_dialog->remote_tag == NULL && tag != NULL)
        {
          i = 0;
        } /* different dialog! */
      else if (jd->d_dialog->remote_tag != NULL && tag != NULL
               && tag->gvalue != NULL
               && 0 != strcmp (jd->d_dialog->remote_tag, tag->gvalue))
        {
          i = 0;
        }
      /* different dialog! */
      if (i == 1)               /* just update the dialog */
        {
          osip_dialog_update_route_set_as_uac (jd->d_dialog, sip);
          osip_dialog_set_state (jd->d_dialog, DIALOG_CONFIRMED);
      } else
        {
          /* the best thing is to update the repace the current dialog
             information... Much easier than creating a useless dialog! */
          osip_dialog_free (jd->d_dialog);
          i = osip_dialog_init_as_uac (&(jd->d_dialog), sip);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "Cannot replace the dialog.\r\n"));
          } else
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_WARNING, NULL,
                           "The dialog has been replaced with the new one fro 200ok.\r\n"));
            }
        }
    }

  jd->d_STATE = JD_ESTABLISHED;

  eXosip_dialog_set_200ok (jd, sip);

  report_call_event (EXOSIP_CALL_ANSWERED, jc, jd, tr);

  /* look for the SDP information and decide if this answer was for
     an initial INVITE, an HoldCall, or a RetreiveCall */

  /* don't handle hold/unhold by now... */
  /* eXosip_update_audio_session(tr); */

}

static void
cb_rcv2xx_4subscribe (osip_transaction_t * tr, osip_message_t * sip)
{
  int i;
  eXosip_dialog_t *jd;
  eXosip_subscribe_t *js;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  js = jinfo->js;
  _eXosip_subscribe_set_refresh_interval (js, sip);


  /* for SUBSCRIBE, test if the dialog has been already created
     with a previous NOTIFY */
  if (jd == NULL && js != NULL && js->s_dialogs != NULL
      && MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      /* find if existing dialog match the to tag */
      osip_generic_param_t *tag;
      int i;

      i = osip_to_get_tag (sip->to, &tag);
      if (i == 0 && tag != NULL && tag->gvalue != NULL)
        {
          for (jd = js->s_dialogs; jd != NULL; jd = jd->next)
            {
              if (0 == strcmp (jd->d_dialog->remote_tag, tag->gvalue))
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO2, NULL,
                               "eXosip: found established early dialog for this subscribe\n"));
                  jinfo->jd = jd;
                  break;
                }
            }
        }
    }

  if (jd == NULL)               /* This transaction initiate a dialog in the case of
                                   SUBSCRIBE (else it would be attached to a "jd" element. */
    {
      /* allocate a jd */
      i = eXosip_dialog_init_as_uac (&jd, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot establish a dialog\n"));
          return;
        }
      ADD_ELEMENT (js->s_dialogs, jd);
      jinfo->jd = jd;
      eXosip_update ();
      osip_transaction_set_your_instance (tr, jinfo);
  } else
    {
      osip_dialog_update_route_set_as_uac (jd->d_dialog, sip);
      osip_dialog_set_state (jd->d_dialog, DIALOG_CONFIRMED);
    }

  jd->d_STATE = JD_ESTABLISHED;
  /* look for the body information */

  {
    eXosip_event_t *je;

    je =
      eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_ANSWERED, js, jd, tr);
    report_event (je, sip);
  }

}

static void
cb_rcv2xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcv2xx (id=%i)\r\n",
               tr->transactionid));

  if (MSG_IS_RESPONSE_FOR (sip, "PUBLISH"))
    {
      eXosip_pub_t *pub;
      eXosip_event_t *je;
      int i;

      i = _eXosip_pub_update (&pub, tr, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "cb_rcv2xx (id=%i) No publication to update\r\n",
                       tr->transactionid));
        }
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_ANSWERED, tr);
      report_event (je, sip);
      return;
  } else if (MSG_IS_RESPONSE_FOR (sip, "REGISTER"))
    {
      eXosip_event_t *je;
      eXosip_reg_t *jreg = NULL;

      /* find matching j_reg */
      _eXosip_reg_find (&jreg, tr);
      if (jreg != NULL)
        {
          je = eXosip_event_init_for_reg (EXOSIP_REGISTRATION_SUCCESS, jreg, tr);
          report_event (je, sip);
          jreg->r_retry = 0;    /* reset value */
        }


        if (eXosip.learn_port>0)
        {
            struct eXosip_net *net;
            net = &eXosip.net_interfaces[0];
            /* EXOSIP_OPT_UDP_LEARN_PORT option set */
#if 1
            /* learn through rport */
           if (net->net_firewall_ip[0]!='\0')
           { 
                osip_via_t *via=NULL;
                osip_generic_param_t *br;
                int i = osip_message_get_via (sip, 0, &via);
                if (via!=NULL && via->protocol!=NULL
                    && osip_strcasecmp(via->protocol, "udp")==0)
                {
                    osip_via_param_get_byname (via, "rport", &br);
                    if (br!=NULL && br->gvalue!=NULL)
                    {
                        snprintf(net->net_port, 20, "%s", br->gvalue);
                        OSIP_TRACE (osip_trace
                            (__FILE__, __LINE__, OSIP_INFO1, NULL,
                            "cb_rcv1xx (id=%i) SIP port modified from rport in REGISTER answer\r\n",
                            tr->transactionid));
                    }
                }
            }
#else
            /* learn through REGISTER? */
            if (net->net_firewall_ip[0]!='\0')
            {
                int pos=0;
                while (!osip_list_eol(reg->contacts,pos))
                {
                    osip_contact_t *co;
                    co = (osip_contact_t *)osip_list_get(reg->contacts,pos);
                    pos++;
                    if (co!=NULL && co->url!=NULL && co->url->host!=NULL
                        && 0==osip_strcasecmp(co->url->host,
                        net->net_firewall_ip))
                    {
                        if (co->url->port==NULL &&
                            0!=osip_strcasecmp(net->net_port, "5060"))
                        {
                            co->url->port=osip_strdup(net->net_port);
                        }
                        else if (co->url->port!=NULL &&
                            0!=osip_strcasecmp(net->net_port, co->url->port))
                        {
                            osip_free(co->url->port);
                            co->url->port=osip_strdup(net->net_port);
                        }
                    }
                }
            }
#endif
        }

        return;
  }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (jd != NULL)
    jd->d_retry = 0;            /* reset marker for authentication */
  if (jc != NULL)
    jc->c_retry = 0;            /* reset marker for authentication */
  if (js != NULL)
    js->s_retry = 0;            /* reset marker for authentication */

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      cb_rcv2xx_4invite (tr, sip);
  } else if (MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      cb_rcv2xx_4subscribe (tr, sip);
  } else if (MSG_IS_RESPONSE_FOR (sip, "BYE"))
    {
      if (jd != NULL)
        jd->d_STATE = JD_TERMINATED;
  } else if (MSG_IS_RESPONSE_FOR (sip, "NOTIFY"))
    {
#ifdef SUPPORT_MSN
      osip_header_t *expires;

      osip_message_header_get_byname (tr->orig_request, "expires", 0, &expires);
      if (expires == NULL || expires->hvalue == NULL)
        {
          /* UNCOMPLIANT UA without a subscription-state header */
      } else if (0 == osip_strcasecmp (expires->hvalue, "0"))
        {
          /* delete the dialog! */
          if (jn != NULL)
            {
              REMOVE_ELEMENT (eXosip.j_notifies, jn);
              eXosip_notify_free (jn);
            }
        }
#else
      osip_header_t *sub_state;

      osip_message_header_get_byname (tr->orig_request, "subscription-state",
                                      0, &sub_state);
      if (sub_state == NULL || sub_state->hvalue == NULL)
        {
          /* UNCOMPLIANT UA without a subscription-state header */
      } else if (0 == osip_strncasecmp (sub_state->hvalue, "terminated", 10))
        {
          /* delete the dialog! */
          if (jn != NULL)
            {
              REMOVE_ELEMENT (eXosip.j_notifies, jn);
              eXosip_notify_free (jn);
            }
        }
#endif
  } else if (jc!=NULL)
    {
      report_call_event (EXOSIP_CALL_MESSAGE_ANSWERED, jc, jd, tr);
      return;
  } else if (jc==NULL && js==NULL && jn==NULL)
    {
      eXosip_event_t *je;
      /* For all requests outside of calls */
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_ANSWERED, tr);
      report_event (je, sip);
      return;
    }
}

void
eXosip_delete_early_dialog (eXosip_dialog_t * jd)
{
  if (jd == NULL)               /* bug? */
    return;

  /* an early dialog was created, but the call is not established */
  if (jd->d_dialog != NULL && jd->d_dialog->state == DIALOG_EARLY)
    {
      osip_dialog_free (jd->d_dialog);
      jd->d_dialog = NULL;
      eXosip_update(); //AMD 30/09/05
      eXosip_dialog_set_state (jd, JD_TERMINATED);
    }
}

static void
rcvregister_failure (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_event_t *je;
  eXosip_reg_t *jreg = NULL;

  /* find matching j_reg */
  _eXosip_reg_find (&jreg, tr);
  if (jreg != NULL)
    {
      je = eXosip_event_init_for_reg (EXOSIP_REGISTRATION_FAILURE, jreg, tr);
      report_event (je, sip);
    }
}

static void
cb_rcv3xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcv3xx (id=%i)\r\n",
               tr->transactionid));

  if (MSG_IS_RESPONSE_FOR (sip, "PUBLISH"))
    {
      eXosip_event_t *je;
      eXosip_pub_t *pub;
      int i;

      i = _eXosip_pub_update (&pub, tr, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "cb_rcv3xx (id=%i) No publication to update\r\n",
                       tr->transactionid));
        }
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_REDIRECTED, tr);
      report_event (je, sip);
      return;
  } else if (MSG_IS_RESPONSE_FOR (sip, "REGISTER"))
    {
      rcvregister_failure (type, tr, sip);
      return;
    }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      report_call_event (EXOSIP_CALL_REDIRECTED, jc, jd, tr);
  } else if (MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_event_t *je;

      je =
        eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_REDIRECTED, js,
                                         jd, tr);
      report_event (je, sip);
  } else if (jc!=NULL)
    {
      report_call_event (EXOSIP_CALL_MESSAGE_REDIRECTED, jc, jd, tr);
      return;
  } else if (jc==NULL && js==NULL && jn==NULL)
    {
      eXosip_event_t *je;
      /* For all requests outside of calls */
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_REDIRECTED, tr);
      report_event (je, sip);
      return;
    }

  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
      if (jd->d_dialog == NULL)
        jd->d_STATE = JD_REDIRECTED;
    }

}

static void
cb_rcv4xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcv4xx (id=%i)\r\n",
               tr->transactionid));

  if (MSG_IS_RESPONSE_FOR (sip, "PUBLISH"))
    {
      eXosip_pub_t *pub;
      eXosip_event_t *je; 
      int i;

      i = _eXosip_pub_update (&pub, tr, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "cb_rcv4xx (id=%i) No publication to update\r\n",
                       tr->transactionid));
        }
      /* For all requests outside of calls */
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_REQUESTFAILURE, tr);
      report_event (je, sip);
      return;
  } else if (MSG_IS_RESPONSE_FOR (sip, "REGISTER"))
    {
      rcvregister_failure (type, tr, sip);
      return;
    }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      report_call_event (EXOSIP_CALL_REQUESTFAILURE, jc, jd, tr);
  } else if (MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_event_t *je;

      je =
        eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_REQUESTFAILURE,
                                         js, jd, tr);
      report_event (je, sip);
  } else if (jc!=NULL)
    {
      report_call_event (EXOSIP_CALL_MESSAGE_REQUESTFAILURE, jc, jd, tr);
      return;
  } else if (jc==NULL && js==NULL && jn==NULL)
    {
      eXosip_event_t *je;
      /* For all requests outside of calls */
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_REQUESTFAILURE, tr);
      report_event (je, sip);
      return;
    }

  if (jc!=NULL)
    {
      if (MSG_TEST_CODE (sip, 401) || MSG_TEST_CODE (sip, 407))
	{
	  if (jc->response_auth!=NULL)
	    osip_message_free(jc->response_auth);
	  
	  osip_message_clone(sip, &jc->response_auth);
	}
    }
  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
      if (MSG_TEST_CODE (sip, 401) || MSG_TEST_CODE (sip, 407))
        jd->d_STATE = JD_AUTH_REQUIRED;
      else
        jd->d_STATE = JD_CLIENTERROR;
    }

}

static void
cb_rcv5xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcv5xx (id=%i)\r\n",
               tr->transactionid));

  if (MSG_IS_RESPONSE_FOR (sip, "PUBLISH"))
    {
      eXosip_pub_t *pub;
      eXosip_event_t *je;
      int i;

      i = _eXosip_pub_update (&pub, tr, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "cb_rcv3xx (id=%i) No publication to update\r\n",
                       tr->transactionid));
        }
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_SERVERFAILURE, tr);
      report_event (je, sip);
      return;
  } else if (MSG_IS_RESPONSE_FOR (sip, "REGISTER"))
    {
      rcvregister_failure (type, tr, sip);
      return;
    }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      report_call_event (EXOSIP_CALL_SERVERFAILURE, jc, jd, tr);
  } else if (MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_event_t *je;

      je =
        eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_SERVERFAILURE,
                                         js, jd, tr);
      report_event (je, sip);
  } else if (jc!=NULL)
    {
      report_call_event (EXOSIP_CALL_MESSAGE_SERVERFAILURE, jc, jd, tr);
      return;
  } else if (jc==NULL && js==NULL && jn==NULL)
    {
      eXosip_event_t *je;
      /* For all requests outside of calls */
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_SERVERFAILURE, tr);
      report_event (je, sip);
      return;
    }

  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
      jd->d_STATE = JD_SERVERERROR;
    }

}

static void
cb_rcv6xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_rcv6xx (id=%i)\r\n",
               tr->transactionid));

  if (MSG_IS_RESPONSE_FOR (sip, "PUBLISH"))
    {
      eXosip_pub_t *pub;
      eXosip_event_t *je;
      int i;

      i = _eXosip_pub_update (&pub, tr, sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "cb_rcv6xx (id=%i) No publication to update\r\n",
                       tr->transactionid));
        }
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_GLOBALFAILURE, tr);
      report_event (je, sip);
      return;
  } else if (MSG_IS_RESPONSE_FOR (sip, "REGISTER"))
    {
      rcvregister_failure (type, tr, sip);
      return;
    }

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      report_call_event (EXOSIP_CALL_GLOBALFAILURE, jc, jd, tr);
  } else if (MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_event_t *je;

      je =
        eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_GLOBALFAILURE,
                                         js, jd, tr);
      report_event (je, sip);
  } else if (jc!=NULL)
    {
      report_call_event (EXOSIP_CALL_MESSAGE_GLOBALFAILURE, jc, jd, tr);
      return;
  } else if (jc==NULL && js==NULL && jn==NULL)
    {
      eXosip_event_t *je;
      /* For all requests outside of calls */
      je = eXosip_event_init_for_message (EXOSIP_MESSAGE_GLOBALFAILURE, tr);
      report_event (je, sip);
      return;
    }

  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
      jd->d_STATE = JD_GLOBALFAILURE;
    }

}

static void
cb_snd1xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_snd1xx (id=%i)\r\n",
               tr->transactionid));

  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)
    return;
  jd->d_STATE = JD_TRYING;
}

static void
cb_snd2xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_snd2xx (id=%i)\r\n",
               tr->transactionid));
  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      jd->d_STATE = JD_ESTABLISHED;
      return;
    }
  jd->d_STATE = JD_ESTABLISHED;
}

static void
cb_snd3xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_snd3xx (id=%i)\r\n",
               tr->transactionid));
  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
    }
  jd->d_STATE = JD_REDIRECTED;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      /* only close calls if this is the initial INVITE */
      if (jc!=NULL && tr == jc->c_inc_tr)
	{
	  report_call_event (EXOSIP_CALL_CLOSED, jc, jd, tr);
	}
    }
}

static void
cb_snd4xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_snd4xx (id=%i)\r\n",
               tr->transactionid));
  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
    }
  jd->d_STATE = JD_CLIENTERROR;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      /* only close calls if this is the initial INVITE */
      if (jc!=NULL && tr == jc->c_inc_tr)
	{
	  report_call_event (EXOSIP_CALL_CLOSED, jc, jd, tr);
	}
    }

}

static void
cb_snd5xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_snd5xx (id=%i)\r\n",
               tr->transactionid));
  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
    }
  jd->d_STATE = JD_SERVERERROR;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      /* only close calls if this is the initial INVITE */
      if (jc!=NULL && tr == jc->c_inc_tr)
	{
	  report_call_event (EXOSIP_CALL_CLOSED, jc, jd, tr);
	}
    }

}

static void
cb_snd6xx (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL, "cb_snd6xx (id=%i)\r\n",
               tr->transactionid));
  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  if (jd == NULL)
    return;
  if (MSG_IS_RESPONSE_FOR (sip, "INVITE")
      || MSG_IS_RESPONSE_FOR (sip, "SUBSCRIBE"))
    {
      eXosip_delete_early_dialog (jd);
    }
  jd->d_STATE = JD_GLOBALFAILURE;

  if (MSG_IS_RESPONSE_FOR (sip, "INVITE"))
    {
      /* only close calls if this is the initial INVITE */
      if (jc!=NULL && tr == jc->c_inc_tr)
	{
	  report_call_event (EXOSIP_CALL_CLOSED, jc, jd, tr);
	}
    }

}

static void
cb_rcvresp_retransmission (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_rcvresp_retransmission (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndreq_retransmission (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndreq_retransmission (id=%i)\r\n", tr->transactionid));
}

static void
cb_sndresp_retransmission (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_sndresp_retransmission (id=%i)\r\n", tr->transactionid));
}

static void
cb_rcvreq_retransmission (int type, osip_transaction_t * tr, osip_message_t * sip)
{
  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_rcvreq_retransmission (id=%i)\r\n", tr->transactionid));
}

static void
cb_transport_error (int type, osip_transaction_t * tr, int error)
{
  eXosip_dialog_t *jd;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  jinfo_t *jinfo = (jinfo_t *) osip_transaction_get_your_instance (tr);

  OSIP_TRACE (osip_trace
              (__FILE__, __LINE__, OSIP_INFO1, NULL,
               "cb_transport_error (id=%i)\r\n", tr->transactionid));
  if (jinfo == NULL)
    return;
  jd = jinfo->jd;
  jc = jinfo->jc;
  jn = jinfo->jn;
  js = jinfo->js;

  if (jn == NULL && js == NULL)
    return;

  if (MSG_IS_NOTIFY (tr->orig_request) && type == OSIP_NICT_TRANSPORT_ERROR)
    {
      /* delete the dialog! */
      REMOVE_ELEMENT (eXosip.j_notifies, jn);
      eXosip_notify_free (jn);
    }

  if (MSG_IS_SUBSCRIBE (tr->orig_request) && type == OSIP_NICT_TRANSPORT_ERROR)
    {
      /* delete the dialog! */
      REMOVE_ELEMENT (eXosip.j_subscribes, js);
      eXosip_subscribe_free (js);
    }

  if (MSG_IS_OPTIONS (tr->orig_request) && jc->c_dialogs == NULL
      && type == OSIP_NICT_TRANSPORT_ERROR)
    {
      /* delete the dialog! */
      REMOVE_ELEMENT (eXosip.j_calls, jc);
      eXosip_call_free (jc);
    }
}



int
eXosip_set_callbacks (osip_t * osip)
{
  /* register all callbacks */

  osip_set_cb_send_message (osip, &cb_snd_message);

  osip_set_kill_transaction_callback (osip, OSIP_ICT_KILL_TRANSACTION,
                                      &cb_ict_kill_transaction);
  osip_set_kill_transaction_callback (osip, OSIP_IST_KILL_TRANSACTION,
                                      &cb_ist_kill_transaction);
  osip_set_kill_transaction_callback (osip, OSIP_NICT_KILL_TRANSACTION,
                                      &cb_nict_kill_transaction);
  osip_set_kill_transaction_callback (osip, OSIP_NIST_KILL_TRANSACTION,
                                      &cb_nist_kill_transaction);

  osip_set_message_callback (osip, OSIP_ICT_STATUS_2XX_RECEIVED_AGAIN,
                             &cb_rcvresp_retransmission);
  osip_set_message_callback (osip, OSIP_ICT_STATUS_3456XX_RECEIVED_AGAIN,
                             &cb_rcvresp_retransmission);
  osip_set_message_callback (osip, OSIP_ICT_INVITE_SENT_AGAIN,
                             &cb_sndreq_retransmission);
  osip_set_message_callback (osip, OSIP_IST_STATUS_2XX_SENT_AGAIN,
                             &cb_sndresp_retransmission);
  osip_set_message_callback (osip, OSIP_IST_STATUS_3456XX_SENT_AGAIN,
                             &cb_sndresp_retransmission);
  osip_set_message_callback (osip, OSIP_IST_INVITE_RECEIVED_AGAIN,
                             &cb_rcvreq_retransmission);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_2XX_RECEIVED_AGAIN,
                             &cb_rcvresp_retransmission);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_3456XX_RECEIVED_AGAIN,
                             &cb_rcvresp_retransmission);
  osip_set_message_callback (osip, OSIP_NICT_REQUEST_SENT_AGAIN,
                             &cb_sndreq_retransmission);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_2XX_SENT_AGAIN,
                             &cb_sndresp_retransmission);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_3456XX_SENT_AGAIN,
                             &cb_sndresp_retransmission);
  osip_set_message_callback (osip, OSIP_NIST_REQUEST_RECEIVED_AGAIN,
                             &cb_rcvreq_retransmission);

  osip_set_transport_error_callback (osip, OSIP_ICT_TRANSPORT_ERROR,
                                     &cb_transport_error);
  osip_set_transport_error_callback (osip, OSIP_IST_TRANSPORT_ERROR,
                                     &cb_transport_error);
  osip_set_transport_error_callback (osip, OSIP_NICT_TRANSPORT_ERROR,
                                     &cb_transport_error);
  osip_set_transport_error_callback (osip, OSIP_NIST_TRANSPORT_ERROR,
                                     &cb_transport_error);

  osip_set_message_callback (osip, OSIP_ICT_INVITE_SENT, &cb_sndinvite);
  osip_set_message_callback (osip, OSIP_ICT_ACK_SENT, &cb_sndack);
  osip_set_message_callback (osip, OSIP_NICT_REGISTER_SENT, &cb_sndregister);
  osip_set_message_callback (osip, OSIP_NICT_BYE_SENT, &cb_sndbye);
  osip_set_message_callback (osip, OSIP_NICT_CANCEL_SENT, &cb_sndcancel);
  osip_set_message_callback (osip, OSIP_NICT_INFO_SENT, &cb_sndinfo);
  osip_set_message_callback (osip, OSIP_NICT_OPTIONS_SENT, &cb_sndoptions);
  osip_set_message_callback (osip, OSIP_NICT_SUBSCRIBE_SENT, &cb_sndsubscribe);
  osip_set_message_callback (osip, OSIP_NICT_NOTIFY_SENT, &cb_sndnotify);
  /*  osip_set_cb_nict_sndprack   (osip,&cb_sndprack); */
  osip_set_message_callback (osip, OSIP_NICT_UNKNOWN_REQUEST_SENT,
                             &cb_sndunkrequest);

  osip_set_message_callback (osip, OSIP_ICT_STATUS_1XX_RECEIVED, &cb_rcv1xx);
  osip_set_message_callback (osip, OSIP_ICT_STATUS_2XX_RECEIVED, &cb_rcv2xx);
  osip_set_message_callback (osip, OSIP_ICT_STATUS_3XX_RECEIVED, &cb_rcv3xx);
  osip_set_message_callback (osip, OSIP_ICT_STATUS_4XX_RECEIVED, &cb_rcv4xx);
  osip_set_message_callback (osip, OSIP_ICT_STATUS_5XX_RECEIVED, &cb_rcv5xx);
  osip_set_message_callback (osip, OSIP_ICT_STATUS_6XX_RECEIVED, &cb_rcv6xx);

  osip_set_message_callback (osip, OSIP_IST_STATUS_1XX_SENT, &cb_snd1xx);
  osip_set_message_callback (osip, OSIP_IST_STATUS_2XX_SENT, &cb_snd2xx);
  osip_set_message_callback (osip, OSIP_IST_STATUS_3XX_SENT, &cb_snd3xx);
  osip_set_message_callback (osip, OSIP_IST_STATUS_4XX_SENT, &cb_snd4xx);
  osip_set_message_callback (osip, OSIP_IST_STATUS_5XX_SENT, &cb_snd5xx);
  osip_set_message_callback (osip, OSIP_IST_STATUS_6XX_SENT, &cb_snd6xx);

  osip_set_message_callback (osip, OSIP_NICT_STATUS_1XX_RECEIVED, &cb_rcv1xx);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_2XX_RECEIVED, &cb_rcv2xx);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_3XX_RECEIVED, &cb_rcv3xx);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_4XX_RECEIVED, &cb_rcv4xx);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_5XX_RECEIVED, &cb_rcv5xx);
  osip_set_message_callback (osip, OSIP_NICT_STATUS_6XX_RECEIVED, &cb_rcv6xx);

  osip_set_message_callback (osip, OSIP_NIST_STATUS_1XX_SENT, &cb_snd1xx);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_2XX_SENT, &cb_snd2xx);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_3XX_SENT, &cb_snd3xx);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_4XX_SENT, &cb_snd4xx);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_5XX_SENT, &cb_snd5xx);
  osip_set_message_callback (osip, OSIP_NIST_STATUS_6XX_SENT, &cb_snd6xx);

  osip_set_message_callback (osip, OSIP_IST_INVITE_RECEIVED, &cb_rcvinvite);
  osip_set_message_callback (osip, OSIP_IST_ACK_RECEIVED, &cb_rcvack);
  osip_set_message_callback (osip, OSIP_IST_ACK_RECEIVED_AGAIN, &cb_rcvack2);
  osip_set_message_callback (osip, OSIP_NIST_REGISTER_RECEIVED, &cb_rcvregister);
  osip_set_message_callback (osip, OSIP_NIST_CANCEL_RECEIVED, &cb_rcvcancel);
  osip_set_message_callback (osip, OSIP_NIST_BYE_RECEIVED, &cb_rcvrequest);
  osip_set_message_callback (osip, OSIP_NIST_INFO_RECEIVED, &cb_rcvrequest);
  osip_set_message_callback (osip, OSIP_NIST_OPTIONS_RECEIVED, &cb_rcvrequest);
  osip_set_message_callback (osip, OSIP_NIST_SUBSCRIBE_RECEIVED, &cb_rcvrequest);
  osip_set_message_callback (osip, OSIP_NIST_NOTIFY_RECEIVED, &cb_rcvrequest);
  osip_set_message_callback (osip, OSIP_NIST_UNKNOWN_REQUEST_RECEIVED,
                             &cb_rcvrequest);

  return 0;
}
