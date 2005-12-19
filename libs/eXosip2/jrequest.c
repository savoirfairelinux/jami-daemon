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

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __APPLE_CC__
#include <unistd.h>
#endif
#else
#include <windows.h>
#include <Iphlpapi.h>
#endif

extern eXosip_t eXosip;

/* Private functions */
static int dialog_fill_route_set (osip_dialog_t * dialog,
                                  osip_message_t * request);

/* should use cryptographically random identifier is RECOMMENDED.... */
/* by now this should lead to identical call-id when application are
   started at the same time...   */
char *
osip_call_id_new_random ()
{
  char *tmp = (char *) osip_malloc (33);
  unsigned int number = osip_build_random_number ();

  sprintf (tmp, "%u", number);
  return tmp;
}

char *
osip_from_tag_new_random (void)
{
  return osip_call_id_new_random ();
}

char *
osip_to_tag_new_random (void)
{
  return osip_call_id_new_random ();
}

unsigned int
via_branch_new_random (void)
{
  return osip_build_random_number ();
}

/* prepare a minimal request (outside of a dialog) with required headers */
/* 
   method is the type of request. ("INVITE", "REGISTER"...)
   to is the remote target URI
   transport is either "TCP" or "UDP" (by now, only UDP is implemented!)
*/
int
generating_request_out_of_dialog (osip_message_t ** dest, const char *method,
                                  const char *to, const char *transport,
                                  const char *from, const char *proxy)
{
  /* Section 8.1:
     A valid request contains at a minimum "To, From, Call-iD, Cseq,
     Max-Forwards and Via
   */
  int i;
  osip_message_t *request;
  char locip[50];
  int doing_register;
  char *register_callid_number = NULL;
  struct eXosip_net *net;

  if (0==osip_strcasecmp(transport, "udp"))
    net = &eXosip.net_interfaces[0];
  else if (0==osip_strcasecmp(transport, "tcp"))
    net = &eXosip.net_interfaces[1];
  else
    {
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		   "eXosip: unsupported protocol (default to UDP)\n"));
    net = &eXosip.net_interfaces[0];
    }

  *dest = NULL;

  i = osip_message_init (&request);
  if (i != 0)
    return -1;

  /* prepare the request-line */
  osip_message_set_method (request, osip_strdup (method));
  osip_message_set_version (request, osip_strdup ("SIP/2.0"));
  osip_message_set_status_code (request, 0);
  osip_message_set_reason_phrase (request, NULL);

  doing_register = 0 == strcmp ("REGISTER", method);

  if (doing_register)
    {
      osip_uri_init (&(request->req_uri));
      i = osip_uri_parse (request->req_uri, proxy);
      if (i != 0)
        {
          goto brood_error_1;
        }
      osip_message_set_to (request, from);
  } else
    {
      /* in any cases except REGISTER: */
      i = osip_message_set_to (request, to);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "ERROR: callee address does not seems to be a sipurl: %s\n",
                       to));
          goto brood_error_1;
        }
      if (proxy != NULL && proxy[0] != 0)
        {                       /* equal to a pre-existing route set */
          /* if the pre-existing route set contains a "lr" (compliance
             with bis-08) then the req_uri should contains the remote target
             URI */
          osip_uri_param_t *lr_param;
          osip_route_t *o_proxy;

#ifndef __VXWORKS_OS__
          osip_route_init (&o_proxy);
#else
          osip_route_init2 (&o_proxy);
#endif
          i = osip_route_parse (o_proxy, proxy);
          if (i != 0)
            {
              osip_route_free (o_proxy);
              goto brood_error_1;
            }

          osip_uri_uparam_get_byname (o_proxy->url, "lr", &lr_param);
          if (lr_param != NULL) /* to is the remote target URI in this case! */
            {
              osip_uri_clone (request->to->url, &(request->req_uri));
              /* "[request] MUST includes a Route header field containing
                 the route set values in order." */
              osip_list_add (request->routes, o_proxy, 0);
          } else
            /* if the first URI of route set does not contain "lr", the req_uri
               is set to the first uri of route set */
            {
              request->req_uri = o_proxy->url;
              o_proxy->url = NULL;
              osip_route_free (o_proxy);
              /* add the route set */
              /* "The UAC MUST add a route header field containing
                 the remainder of the route set values in order.
                 The UAC MUST then place the remote target URI into
                 the route header field as the last value
               */
              osip_message_set_route (request, to);
            }
      } else                    /* No route set (outbound proxy) is used */
        {
          /* The UAC must put the remote target URI (to field) in the req_uri */
          i = osip_uri_clone (request->to->url, &(request->req_uri));
          if (i != 0)
            goto brood_error_1;
        }
    }
  /*guess the local ip since req uri is known */
#ifdef SM
  eXosip_get_localip_for (request->req_uri->host, locip, 49);
#else
  if (0==osip_strcasecmp(transport, "udp"))
    eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip, 49);
  else if (0==osip_strcasecmp(transport, "tcp"))
    eXosip_guess_ip_for_via (eXosip.net_interfaces[1].net_ip_family, locip, 49);
  else
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unsupported protocol (default to UDP)\n"));
      eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip, 49);
    }
#endif
  /* set To and From */
  osip_message_set_from (request, from);
  /* add a tag */
  osip_from_set_tag (request->from, osip_from_tag_new_random ());

  /* set the cseq and call_id header */
  {
    osip_call_id_t *callid;
    osip_cseq_t *cseq;
    char *num;
    char *cidrand;

    /* call-id is always the same for REGISTRATIONS */
    i = osip_call_id_init (&callid);
    if (i != 0)
      goto brood_error_1;
    cidrand = osip_call_id_new_random ();
    osip_call_id_set_number (callid, cidrand);
    if (doing_register)
      register_callid_number = cidrand;

    osip_call_id_set_host (callid, osip_strdup (locip));
    request->call_id = callid;

    i = osip_cseq_init (&cseq);
    if (i != 0)
      goto brood_error_1;
    num = osip_strdup (doing_register ? "1" : "20");
    osip_cseq_set_number (cseq, num);
    osip_cseq_set_method (cseq, osip_strdup (method));
    request->cseq = cseq;
  }

  /* always add the Max-Forward header */
  osip_message_set_max_forwards (request, "5"); /* a UA should start a request with 70 */

#define MASQUERADE_VIA
#ifdef MASQUERADE_VIA
  /* should be useless with compliant UA */
  if (eXosip.net_interfaces[0].net_firewall_ip[0] != '\0')
    {
      char *c_address = request->req_uri->host;

      struct addrinfo *addrinfo;
      struct __eXosip_sockaddr addr;

      i = eXosip_get_addrinfo (&addrinfo, request->req_uri->host, 5060, IPPROTO_UDP);
      if (i == 0)
        {
          memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
          freeaddrinfo (addrinfo);
          c_address = inet_ntoa (((struct sockaddr_in *) &addr)->sin_addr);
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_INFO1, NULL,
                       "eXosip: here is the resolved destination host=%s\n",
                       c_address));
        }

      if (eXosip_is_public_address (c_address))
        {
          char tmp[200];

          snprintf (tmp, 200, "SIP/2.0/%s %s:%s;rport;branch=z9hG4bK%u",
                    transport, eXosip.net_interfaces[0].net_firewall_ip,
		    net->net_port,
                    via_branch_new_random ());
          osip_message_set_via (request, tmp);
      } else
        {
          char tmp[200];

          if (net->net_ip_family == AF_INET6)
            snprintf (tmp, 200, "SIP/2.0/%s [%s]:%s;branch=z9hG4bK%u",
                      transport, locip, net->net_port,
                      via_branch_new_random ());
          else
            snprintf (tmp, 200, "SIP/2.0/%s %s:%s;rport;branch=z9hG4bK%u",
                      transport, locip, net->net_port,
                      via_branch_new_random ());
          osip_message_set_via (request, tmp);
        }
  } else
    {
      char tmp[200];

      if (net->net_ip_family == AF_INET6)
        snprintf (tmp, 200, "SIP/2.0/%s [%s]:%s;branch=z9hG4bK%u", transport,
                  locip, net->net_port,
		  via_branch_new_random ());
      else
        snprintf (tmp, 200, "SIP/2.0/%s %s:%s;rport;branch=z9hG4bK%u",
                  transport, locip, net->net_port,
		  via_branch_new_random ());
      osip_message_set_via (request, tmp);
    }

#else
  {
    char tmp[200];

    if (net->net_ip_family == AF_INET6)
      spnrintf (tmp, 200, "SIP/2.0/%s [%s]:%s;branch=z9hG4bK%u", transport,
                locip, net->net_port,
		via_branch_new_random ());
    else
      spnrintf (tmp, 200, "SIP/2.0/%s %s:%s;branch=z9hG4bK%u", transport,
                locip, net->net_port,
		via_branch_new_random ());

    osip_message_set_via (request, tmp);
  }
#endif

  /* add specific headers for each kind of request... */

  if (0 == strcmp ("INVITE", method) || 0 == strcmp ("SUBSCRIBE", method))
    {
      char *contact;
      osip_from_t *a_from;
      int i;

      i = osip_from_init (&a_from);
      if (i == 0)
        i = osip_from_parse (a_from, from);

      if (i == 0 && a_from != NULL
          && a_from->url != NULL && a_from->url->username != NULL)
        {
          contact = (char *) osip_malloc (50 + strlen (a_from->url->username));

          if (eXosip.net_interfaces[0].net_firewall_ip[0] != '\0')
            {
              char *c_address = request->req_uri->host;

              struct addrinfo *addrinfo;
              struct __eXosip_sockaddr addr;

	      i = eXosip_get_addrinfo (&addrinfo, request->req_uri->host, 5060, IPPROTO_TCP);
              if (i == 0)
                {
                  memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
                  freeaddrinfo (addrinfo);
                  c_address = inet_ntoa (((struct sockaddr_in *) &addr)->sin_addr);
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO1, NULL,
                               "eXosip: here is the resolved destination host=%s\n",
                               c_address));
                }

              if (eXosip_is_public_address (c_address))
                {
                  sprintf (contact, "<sip:%s@%s:%s>", a_from->url->username,
                           eXosip.net_interfaces[0].net_firewall_ip,
			   net->net_port);
              } else
                {
                  sprintf (contact, "<sip:%s@%s:%s>", a_from->url->username,
                           locip, net->net_port);
                }
          } else
            {
              sprintf (contact, "<sip:%s@%s:%s>", a_from->url->username,
                       locip, net->net_port);
            }
          osip_message_set_contact (request, contact);
          osip_free (contact);
        }
      osip_from_free (a_from);

      /* This is probably useless for other messages */
      osip_message_set_allow (request, "INVITE");
      osip_message_set_allow (request, "ACK");
      osip_message_set_allow (request, "UPDATE");
      osip_message_set_allow (request, "INFO");
      osip_message_set_allow (request, "CANCEL");
      osip_message_set_allow (request, "BYE");
      osip_message_set_allow (request, "OPTIONS");
      osip_message_set_allow (request, "REFER");
      osip_message_set_allow (request, "SUBSCRIBE");
      osip_message_set_allow (request, "NOTIFY");
      osip_message_set_allow (request, "MESSAGE");
    }

  if (0 == strcmp ("REGISTER", method))
    {
  } else if (0 == strcmp ("INFO", method))
    {
  } else if (0 == strcmp ("OPTIONS", method))
    {
      osip_message_set_accept (request, "application/sdp");
    }

  osip_message_set_user_agent (request, eXosip.user_agent);
  /*  else if ... */
  *dest = request;
  return 0;

brood_error_1:
  osip_message_free (request);
  *dest = NULL;
  return -1;
}

int
generating_register (osip_message_t ** reg, char *transport, char *from,
                     char *proxy, char *contact, int expires)
{
  osip_from_t *a_from;
  int i;
  char locip[50];
  struct eXosip_net *net;

  i = generating_request_out_of_dialog (reg, "REGISTER", NULL, transport, from, proxy);
  if (i != 0)
    return -1;

  i = _eXosip_find_protocol(*reg);
  if (i==IPPROTO_UDP)
      net = &eXosip.net_interfaces[0];
  else if (i==IPPROTO_TCP)
      net = &eXosip.net_interfaces[1];
  else
    {
      net = &eXosip.net_interfaces[0];
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unsupported protocol\n"));
      return -1;
    }

#ifdef SM
  eXosip_get_localip_for ((*reg)->req_uri->host, locip, 49);
#else
  eXosip_guess_ip_for_via (net->net_ip_family, locip, 49);
#endif

  if (contact == NULL)
    {
      i = osip_from_init (&a_from);
      if (i == 0)
        i = osip_from_parse (a_from, from);

      if (i == 0 && a_from != NULL
          && a_from->url != NULL && a_from->url->username != NULL)
        {
          contact = (char *) osip_malloc (50 + strlen (a_from->url->username));
          if (eXosip.net_interfaces[0].net_firewall_ip[0] != '\0')
            {
              char *c_address = (*reg)->req_uri->host;

              struct addrinfo *addrinfo;
              struct __eXosip_sockaddr addr;

	      i = eXosip_get_addrinfo (&addrinfo, (*reg)->req_uri->host, 5060, IPPROTO_UDP);
              if (i == 0)
                {
                  memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
                  freeaddrinfo (addrinfo);
                  c_address = inet_ntoa (((struct sockaddr_in *) &addr)->sin_addr);
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO1, NULL,
                               "eXosip: here is the resolved destination host=%s\n",
                               c_address));
                }

              if (eXosip_is_public_address (c_address))
                {
                  sprintf (contact, "<sip:%s@%s:%s>", a_from->url->username,
                           eXosip.net_interfaces[0].net_firewall_ip,
			   net->net_port);
              } else
                {
                  sprintf (contact, "<sip:%s@%s:%s>", a_from->url->username,
                           locip, net->net_port);
                }
          } else
            {
              sprintf (contact, "<sip:%s@%s:%s>", a_from->url->username,
                       locip, net->net_port);
            }

          osip_message_set_contact (*reg, contact);
          osip_free (contact);
        }
      osip_from_free (a_from);
  } else
    {
      osip_message_set_contact (*reg, contact);
    }

  {
    char exp[10];               /* MUST never be ouside 1 and 3600 */

    snprintf (exp, 9, "%i", expires);
    osip_message_set_expires (*reg, exp);
  }

  osip_message_set_content_length (*reg, "0");

  return 0;
}

int
generating_publish (osip_message_t ** message, const char *to,
                    const char *from, const char *route)
{
  int i;

  if (to != NULL && *to == '\0')
    return -1;

  if (route != NULL && *route == '\0')
    route = NULL;

  i = generating_request_out_of_dialog (message, "PUBLISH", to, "UDP", from,
					route);
  if (i != 0)
    return -1;

  /* osip_message_set_organization(*message, "Jack's Org"); */

  return 0;
}

static int
dialog_fill_route_set (osip_dialog_t * dialog, osip_message_t * request)
{
  /* if the pre-existing route set contains a "lr" (compliance
     with bis-08) then the req_uri should contains the remote target
     URI */
  int i;
  int pos = 0;
  osip_uri_param_t *lr_param;
  osip_route_t *route;
  char *last_route;

  /* AMD bug: fixed 17/06/2002 */

#ifdef OSIP_FUTURE_FIX_2_3
    route = (osip_route_t *) osip_list_get (dialog->route_set, 0);
#else
  if (dialog->type == CALLER)
    {
      pos = osip_list_size (dialog->route_set) - 1;
      route = (osip_route_t *) osip_list_get (dialog->route_set, pos);
  } else
    route = (osip_route_t *) osip_list_get (dialog->route_set, 0);
#endif

  osip_uri_uparam_get_byname (route->url, "lr", &lr_param);
  if (lr_param != NULL)         /* the remote target URI is the req_uri! */
    {
      i = osip_uri_clone (dialog->remote_contact_uri->url, &(request->req_uri));
      if (i != 0)
        return -1;
      /* "[request] MUST includes a Route header field containing
         the route set values in order." */
      /* AMD bug: fixed 17/06/2002 */
      pos = 0;                  /* first element is at index 0 */
      while (!osip_list_eol (dialog->route_set, pos))
        {
          osip_route_t *route2;

          route = osip_list_get (dialog->route_set, pos);
          i = osip_route_clone (route, &route2);
          if (i != 0)
            return -1;
#ifdef OSIP_FUTURE_FIX_2_3
            osip_list_add (request->routes, route2, -1);
#else
          if (dialog->type == CALLER)
            osip_list_add (request->routes, route2, 0);
          else
            osip_list_add (request->routes, route2, -1);
          pos++;
#endif
        }
      return 0;
    }

  /* if the first URI of route set does not contain "lr", the req_uri
     is set to the first uri of route set */


  i = osip_uri_clone (route->url, &(request->req_uri));
  if (i != 0)
    return -1;
  /* add the route set */
  /* "The UAC MUST add a route header field containing
     the remainder of the route set values in order. */
  pos = 0;                      /* yes it is */

  while (!osip_list_eol (dialog->route_set, pos))       /* not the first one in the list */
    {
      osip_route_t *route2;

      route = osip_list_get (dialog->route_set, pos);
      i = osip_route_clone (route, &route2);
      if (i != 0)
        return -1;
#ifdef OSIP_FUTURE_FIX_2_3
      if (!osip_list_eol (dialog->route_set, pos + 1))
        osip_list_add (request->routes, route2, -1);
      else
        osip_route_free (route2);
#else
      if (dialog->type == CALLER)
        {
          if (pos != osip_list_size (dialog->route_set) - 1)
            osip_list_add (request->routes, route2, 0);
          else
            osip_route_free (route2);
      } else
        {
          if (!osip_list_eol (dialog->route_set, pos + 1))
            osip_list_add (request->routes, route2, -1);
          else
            osip_route_free (route2);
        }
#endif
      pos++;
    }

  /* The UAC MUST then place the remote target URI into
     the route header field as the last value */
  i = osip_uri_to_str (dialog->remote_contact_uri->url, &last_route);
  if (i != 0)
    return -1;
  i = osip_message_set_route (request, last_route);
  osip_free (last_route);
  if (i != 0)
    {
      return -1;
    }

  /* route header and req_uri set */
  return 0;
}

int
_eXosip_build_request_within_dialog (osip_message_t ** dest,
                                     const char *method,
                                     osip_dialog_t * dialog, const char *transport)
{
  int i;
  osip_message_t *request;
  char locip[50];
  struct eXosip_net *net;

  i = osip_message_init (&request);
  if (i != 0) 
   return -1;

  if (dialog->remote_contact_uri == NULL)
    {
      /* this dialog is probably not established! or the remote UA
         is not compliant with the latest RFC
       */
      osip_message_free (request);
      return -1;
    }

  if (0==osip_strcasecmp(transport, "udp"))
    net = &eXosip.net_interfaces[0];
  else if (0==osip_strcasecmp(transport, "tcp"))
    net = &eXosip.net_interfaces[1];
  else
    {
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		   "eXosip: unsupported protocol -%s- (default to UDP)\n"));
    net = &eXosip.net_interfaces[0];
    }

#ifdef SM
  eXosip_get_localip_for (dialog->remote_contact_uri->url->host, locip, 49);
#else
  eXosip_guess_ip_for_via (net->net_ip_family, locip, 49);
#endif
  /* prepare the request-line */
  request->sip_method = osip_strdup (method);
  request->sip_version = osip_strdup ("SIP/2.0");
  request->status_code = 0;
  request->reason_phrase = NULL;

  /* and the request uri???? */
  if (osip_list_eol (dialog->route_set, 0))
    {
      /* The UAC must put the remote target URI (to field) in the req_uri */
      i = osip_uri_clone (dialog->remote_contact_uri->url, &(request->req_uri));
      if (i != 0)
        goto grwd_error_1;
  } else
    {
      /* fill the request-uri, and the route headers. */
      dialog_fill_route_set (dialog, request);
    }

  /* To and From already contains the proper tag! */
  i = osip_to_clone (dialog->remote_uri, &(request->to));
  if (i != 0)
    goto grwd_error_1;
  i = osip_from_clone (dialog->local_uri, &(request->from));
  if (i != 0)
    goto grwd_error_1;

  /* set the cseq and call_id header */
  osip_message_set_call_id (request, dialog->call_id);

  if (0 == strcmp ("ACK", method))
    {
      osip_cseq_t *cseq;
      char *tmp;

      i = osip_cseq_init (&cseq);
      if (i != 0)
        goto grwd_error_1;
      tmp = osip_malloc (20);
      sprintf (tmp, "%i", dialog->local_cseq);
      osip_cseq_set_number (cseq, tmp);
      osip_cseq_set_method (cseq, osip_strdup (method));
      request->cseq = cseq;
  } else
    {
      osip_cseq_t *cseq;
      char *tmp;

      i = osip_cseq_init (&cseq);
      if (i != 0)
        goto grwd_error_1;
      dialog->local_cseq++;     /* we should we do that?? */
      tmp = osip_malloc (20);
      sprintf (tmp, "%i", dialog->local_cseq);
      osip_cseq_set_number (cseq, tmp);
      osip_cseq_set_method (cseq, osip_strdup (method));
      request->cseq = cseq;
    }

  /* always add the Max-Forward header */
  osip_message_set_max_forwards (request, "5"); /* a UA should start a request with 70 */


  /* even for ACK for 2xx (ACK within a dialog), the branch ID MUST
     be a new ONE! */
#ifdef MASQUERADE_VIA
  /* should be useless with compliant UA */
  if (eXosip.net_interfaces[0].net_firewall_ip[0] != '\0')
    {
      char *c_address = request->req_uri->host;

      struct addrinfo *addrinfo;
      struct __eXosip_sockaddr addr;

      i = eXosip_get_addrinfo (&addrinfo, request->req_uri->host, 5060, IPPROTO_UDP);
      if (i == 0)
        {
          memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
          freeaddrinfo (addrinfo);
          c_address = inet_ntoa (((struct sockaddr_in *) &addr)->sin_addr);
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_INFO1, NULL,
                       "eXosip: here is the resolved destination host=%s\n",
                       c_address));
        }

      if (eXosip_is_public_address (c_address))
        {
          char tmp[200];

          sprintf (tmp, "SIP/2.0/%s %s:%s;rport;branch=z9hG4bK%u", transport,
                   eXosip.net_interfaces[0].net_firewall_ip,
                   net->net_port,
		   via_branch_new_random ());
          osip_message_set_via (request, tmp);
      } else
        {
          char tmp[200];

          if (eXosip.net_interfaces[0].net_ip_family == AF_INET6)
            snprintf (tmp, 200, "SIP/2.0/%s [%s]:%s;branch=z9hG4bK%u",
                      transport, locip, net->net_port,
                      via_branch_new_random ());
          else
            snprintf (tmp, 200, "SIP/2.0/%s %s:%s;rport;branch=z9hG4bK%u",
                      transport, locip, net->net_port,
                      via_branch_new_random ());

          osip_message_set_via (request, tmp);
        }
  } else
    {
      char tmp[200];

      if (net->net_ip_family == AF_INET6)
        snprintf (tmp, 200, "SIP/2.0/%s [%s]:%s;branch=z9hG4bK%u", transport,
                  locip, net->net_port,
		  via_branch_new_random ());
      else
        snprintf (tmp, 200, "SIP/2.0/%s %s:%s;rport;branch=z9hG4bK%u",
                  transport, locip, net->net_port,
		  via_branch_new_random ());

      osip_message_set_via (request, tmp);
    }

#else
  {
    char tmp[200];

    if (net->net_ip_family == AF_INET6)
      sprintf (tmp, "SIP/2.0/%s [%s]:%s;branch=z9hG4bK%u", transport,
               locip, net->net_port,
	       via_branch_new_random ());
    else
      sprintf (tmp, "SIP/2.0/%s %s:%s;branch=z9hG4bK%u", transport,
               locip, net->net_port,
	       via_branch_new_random ());

    osip_message_set_via (request, tmp);
  }
#endif

  /* add specific headers for each kind of request... */

  {
    char contact[200];

    if (net->net_firewall_ip[0] != '\0')
      {
        char *c_address = request->req_uri->host;

        struct addrinfo *addrinfo;
        struct __eXosip_sockaddr addr;

        i = eXosip_get_addrinfo (&addrinfo, request->req_uri->host, 5060, IPPROTO_UDP);
        if (i == 0)
          {
            memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
            freeaddrinfo (addrinfo);
            c_address = inet_ntoa (((struct sockaddr_in *) &addr)->sin_addr);
            OSIP_TRACE (osip_trace
                        (__FILE__, __LINE__, OSIP_INFO1, NULL,
                         "eXosip: here is the resolved destination host=%s\n",
                         c_address));
          }

        if (eXosip_is_public_address (c_address))
          {
            sprintf (contact, "<sip:%s@%s:%s>",
                     dialog->local_uri->url->username,
		     eXosip.net_interfaces[0].net_firewall_ip,
                     net->net_port);
        } else
          {
            sprintf (contact, "<sip:%s@%s:%s>",
                     dialog->local_uri->url->username, locip,
		     net->net_port);
          }
    } else
      {
        sprintf (contact, "<sip:%s@%s:%s>", dialog->local_uri->url->username,
                 locip, net->net_port);
      }
    osip_message_set_contact (request, contact);
    /* Here we'll add the supported header if it's needed! */
    /* the require header must be added by the upper layer if needed */
  }

  if (0 == strcmp ("NOTIFY", method))
    {
  } else if (0 == strcmp ("INFO", method))
    {

  } else if (0 == strcmp ("OPTIONS", method))
    {
      osip_message_set_accept (request, "application/sdp");
  } else if (0 == strcmp ("ACK", method))
    {
      /* The ACK MUST contains the same credential than the INVITE!! */
      /* TODO... */
    }

  osip_message_set_user_agent (request, eXosip.user_agent);
  /*  else if ... */
  *dest = request;
  return 0;

  /* grwd_error_2: */
  dialog->local_cseq--;
grwd_error_1:
  osip_message_free (request);
  *dest = NULL;
  return -1;
}

/* this request is only build within a dialog!! */
int
generating_bye (osip_message_t ** bye, osip_dialog_t * dialog, char *transport)
{
  int i;

  i = _eXosip_build_request_within_dialog (bye, "BYE", dialog, transport);
  if (i != 0)
    return -1;

  return 0;
}

/* It is RECOMMENDED to only cancel INVITE request */
int
generating_cancel (osip_message_t ** dest, osip_message_t * request_cancelled)
{
  int i;
  osip_message_t *request;

  i = osip_message_init (&request);
  if (i != 0)
    return -1;

  /* prepare the request-line */
  osip_message_set_method (request, osip_strdup ("CANCEL"));
  osip_message_set_version (request, osip_strdup ("SIP/2.0"));
  osip_message_set_status_code (request, 0);
  osip_message_set_reason_phrase (request, NULL);

  i = osip_uri_clone (request_cancelled->req_uri, &(request->req_uri));
  if (i != 0)
    goto gc_error_1;

  i = osip_to_clone (request_cancelled->to, &(request->to));
  if (i != 0)
    goto gc_error_1;
  i = osip_from_clone (request_cancelled->from, &(request->from));
  if (i != 0)
    goto gc_error_1;

  /* set the cseq and call_id header */
  i = osip_call_id_clone (request_cancelled->call_id, &(request->call_id));
  if (i != 0)
    goto gc_error_1;
  i = osip_cseq_clone (request_cancelled->cseq, &(request->cseq));
  if (i != 0)
    goto gc_error_1;
  osip_free (request->cseq->method);
  request->cseq->method = osip_strdup ("CANCEL");

  /* copy ONLY the top most Via Field (this method is also used by proxy) */
  {
    osip_via_t *via;
    osip_via_t *via2;

    i = osip_message_get_via (request_cancelled, 0, &via);
    if (i != 0)
      goto gc_error_1;
    i = osip_via_clone (via, &via2);
    if (i != 0)
      goto gc_error_1;
    osip_list_add (request->vias, via2, -1);
  }

  /* add the same route-set than in the previous request */
  {
    int pos = 0;
    osip_route_t *route;
    osip_route_t *route2;

    while (!osip_list_eol (request_cancelled->routes, pos))
      {
        route = (osip_route_t *) osip_list_get (request_cancelled->routes, pos);
        i = osip_route_clone (route, &route2);
        if (i != 0)
          goto gc_error_1;
        osip_list_add (request->routes, route2, -1);
        pos++;
      }
  }

  osip_message_set_max_forwards (request, "70");        /* a UA should start a request with 70 */
  osip_message_set_user_agent (request, eXosip.user_agent);

  *dest = request;
  return 0;

gc_error_1:
  osip_message_free (request);
  *dest = NULL;
  return -1;
}

