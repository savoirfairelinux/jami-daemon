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

int
eXosip_subscribe_build_initial_request (osip_message_t ** sub, const char *to,
                                        const char *from, const char *route,
                                        const char *event, int expires)
{
  char tmp[10];
  int i;
  osip_uri_param_t *uri_param = NULL;
  osip_to_t *_to=NULL;

  *sub = NULL;
  if (to != NULL && *to == '\0')
    return -1;
  if (from != NULL && *from == '\0')
    return -1;
  if (event != NULL && *event == '\0')
    return -1;
  if (route != NULL && *route == '\0')
    route = NULL;

  i = osip_to_init(&_to);
  if (i!=0)
    return -1;
  
  i = osip_to_parse(_to, to);
  if (i!=0)
    {
      osip_to_free(_to);
      return -1;
    }

  osip_uri_uparam_get_byname(_to->url, "transport", &uri_param);
  if (uri_param != NULL && uri_param->gvalue != NULL)
    {
      i = generating_request_out_of_dialog (sub, "SUBSCRIBE", to, "UDP", from, route);
    }
  else
    {
      if (eXosip.net_interfaces[0].net_socket>0)
	i = generating_request_out_of_dialog (sub, "SUBSCRIBE", to, "UDP", from, route);
      else if (eXosip.net_interfaces[1].net_socket>0)
	i = generating_request_out_of_dialog (sub, "SUBSCRIBE", to, "TCP", from, route);
      else
	i = generating_request_out_of_dialog (sub, "SUBSCRIBE", to, "UDP", from, route);
    }

  osip_to_free(_to);
  if (i != 0)
    return -1;

  snprintf (tmp, 10, "%i", expires);
  osip_message_set_expires (*sub, tmp);

  osip_message_set_header (*sub, "Event", event);

  return 0;
}

int
eXosip_subscribe_send_initial_request (osip_message_t * subscribe)
{
  eXosip_subscribe_t *js = NULL;
  osip_transaction_t *transaction;
  osip_event_t *sipevent;
  int i;

  i = eXosip_subscribe_init (&js);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot subscribe."));
      osip_message_free (subscribe);
      return -1;
    }

  i = osip_transaction_init (&transaction, NICT, eXosip.j_osip, subscribe);
  if (i != 0)
    {
      eXosip_subscribe_free (js);
      osip_message_free (subscribe);
      return -1;
    }

  _eXosip_subscribe_set_refresh_interval (js, subscribe);
  js->s_out_tr = transaction;

  sipevent = osip_new_outgoing_sipmessage (subscribe);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, NULL, js, NULL));
  osip_transaction_add_event (transaction, sipevent);

  ADD_ELEMENT (eXosip.j_subscribes, js);
  eXosip_update ();             /* fixed? */
  __eXosip_wakeup ();
  return 0;
}

int
eXosip_subscribe_build_refresh_request (int did, osip_message_t ** sub)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_subscribe_t *js = NULL;

  osip_transaction_t *transaction;
  char *transport;
  int i;

  *sub = NULL;
  if (did > 0)
    {
      eXosip_subscribe_dialog_find (did, &js, &jd);
    }
  if (jd == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No subscribe here?\n"));
      return -1;
    }

  transaction = NULL;
  transaction = eXosip_find_last_out_subscribe (js, jd);

  if (transaction != NULL)
    {
      if (transaction->state != NICT_TERMINATED &&
          transaction->state != NIST_TERMINATED &&
          transaction->state != NICT_COMPLETED &&
          transaction->state != NIST_COMPLETED)
        return -1;
    }

  transport = NULL;
  if (transaction!=NULL && transaction->orig_request!=NULL)
    transport = _eXosip_transport_protocol(transaction->orig_request);

  transaction = NULL;

  if (transport==NULL)
    i = _eXosip_build_request_within_dialog (sub, "SUBSCRIBE", jd->d_dialog, "UDP");
  else
    i = _eXosip_build_request_within_dialog (sub, "SUBSCRIBE", jd->d_dialog, transport);

  if (i != 0)
    return -2;

  return 0;
}

int
eXosip_subscribe_send_refresh_request (int did, osip_message_t * sub)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_subscribe_t *js = NULL;

  osip_transaction_t *transaction;
  osip_event_t *sipevent;
  int i;

  if (did > 0)
    {
      eXosip_subscribe_dialog_find (did, &js, &jd);
    }
  if (jd == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No subscribe here?\n"));
      osip_message_free (sub);
      return -1;
    }

  transaction = NULL;
  transaction = eXosip_find_last_out_subscribe (js, jd);

  if (transaction != NULL)
    {
      if (transaction->state != NICT_TERMINATED &&
          transaction->state != NIST_TERMINATED &&
          transaction->state != NICT_COMPLETED &&
          transaction->state != NIST_COMPLETED)
	{
	  osip_message_free (sub);
	  return -1;
	}
      transaction = NULL;
    }

  transaction = NULL;
  i = osip_transaction_init (&transaction, NICT, eXosip.j_osip, sub);

  if (i != 0)
    {
      osip_message_free (sub);
      return -2;
    }

  _eXosip_subscribe_set_refresh_interval (js, sub);
  osip_list_add (jd->d_out_trs, transaction, 0);

  sipevent = osip_new_outgoing_sipmessage (sub);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, jd, js, NULL));
  osip_transaction_add_event (transaction, sipevent);
  __eXosip_wakeup ();
  return 0;
}

int
_eXosip_subscribe_send_request_with_credential (eXosip_subscribe_t * js,
                                                eXosip_dialog_t * jd,
                                                osip_transaction_t * out_tr)
{
  osip_transaction_t *tr = NULL;
  osip_message_t *msg = NULL;
  osip_event_t *sipevent;

  char locip[256];
  int cseq;
  char tmp[256];
  osip_via_t *via;
  int i;

  if (js == NULL)
    return -1;
  if (jd != NULL)
    {
      if (jd->d_out_trs == NULL)
        return -1;
    }

  if (out_tr == NULL)
    {
      out_tr = eXosip_find_last_out_subscribe (js, jd);
    }

  if (out_tr == NULL
      || out_tr->orig_request == NULL || out_tr->last_response == NULL)
    return -1;

  osip_message_clone (out_tr->orig_request, &msg);
  if (msg == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: could not clone msg for authentication\n"));
      return -1;
    }

  via = (osip_via_t *) osip_list_get (msg->vias, 0);
  if (via == NULL || msg->cseq == NULL || msg->cseq->number == NULL)
    {
      osip_message_free (msg);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: missing via or cseq header\n"));
      return -1;
    }

  /* increment cseq */
  cseq = atoi (msg->cseq->number);
  osip_free (msg->cseq->number);
  msg->cseq->number = strdup_printf ("%i", cseq + 1);
  if (jd != NULL && jd->d_dialog != NULL)
    {
      jd->d_dialog->local_cseq++;
    }

  osip_list_remove (msg->vias, 0);
  osip_via_free (via);
  i = _eXosip_find_protocol(out_tr->orig_request);
  if (i==IPPROTO_UDP)
    {
      eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip,
			       sizeof (locip));
      if (eXosip.net_interfaces[0].net_ip_family == AF_INET6)
	snprintf (tmp, 256, "SIP/2.0/UDP [%s]:%s;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[0].net_port, via_branch_new_random ());
      else
	snprintf (tmp, 256, "SIP/2.0/UDP %s:%s;rport;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[0].net_port, via_branch_new_random ());
    }
  else if (i==IPPROTO_TCP)
    {
      eXosip_guess_ip_for_via (eXosip.net_interfaces[1].net_ip_family, locip,
			       sizeof (locip));
      if (eXosip.net_interfaces[1].net_ip_family == AF_INET6)
	snprintf (tmp, 256, "SIP/2.0/TCP [%s]:%s;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[1].net_port, via_branch_new_random ());
      else
	snprintf (tmp, 256, "SIP/2.0/TCP %s:%s;rport;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[1].net_port, via_branch_new_random ());
    }
  else
    {
      /* tls? */
      osip_message_free (msg);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unsupported protocol\n"));
      return -1;
    }

  osip_via_init (&via);
  osip_via_parse (via, tmp);
  osip_list_add (msg->vias, via, 0);

  eXosip_add_authentication_information (msg, out_tr->last_response);
  osip_message_force_update (msg);

  i = osip_transaction_init (&tr, NICT, eXosip.j_osip, msg);

  if (i != 0)
    {
      osip_message_free (msg);
      return -1;
    }

  if (out_tr == js->s_out_tr)
    {
      /* replace with the new tr */
      osip_list_add (eXosip.j_transactions, js->s_out_tr, 0);
      js->s_out_tr = tr;
  } else
    {
      /* add the new tr for the current dialog */
      osip_list_add (jd->d_out_trs, tr, 0);
    }

  sipevent = osip_new_outgoing_sipmessage (msg);

  osip_transaction_set_your_instance (tr, __eXosip_new_jinfo (NULL, jd, js, NULL));
  osip_transaction_add_event (tr, sipevent);

  eXosip_update ();             /* fixed? */
  __eXosip_wakeup ();
  return 0;
}
