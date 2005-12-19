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

extern eXosip_t eXosip;


int
_eXosip_build_response_default (osip_message_t ** dest,
                                osip_dialog_t * dialog, int status,
                                osip_message_t * request)
{
  osip_generic_param_t *tag;
  osip_message_t *response;
  int pos;
  int i;

  *dest = NULL;
  if (request == NULL)
    return -1;

  i = osip_message_init (&response);
  if (i != 0)
    return -1;
  /* initialise osip_message_t structure */
  /* yet done... */

  response->sip_version = (char *) osip_malloc (8 * sizeof (char));
  sprintf (response->sip_version, "SIP/2.0");
  osip_message_set_status_code (response, status);

  /* handle some internal reason definitions. */
  if (MSG_IS_NOTIFY (request) && status == 481)
    {
      response->reason_phrase = osip_strdup ("Subcription Does Not Exist");
  } else if (MSG_IS_SUBSCRIBE (request) && status == 202)
    {
      response->reason_phrase = osip_strdup ("Accepted subscription");
  } else
    {
      response->reason_phrase = osip_strdup (osip_message_get_reason (status));
      if (response->reason_phrase == NULL)
        {
          if (response->status_code == 101)
            response->reason_phrase = osip_strdup ("Dialog Establishement");
          else
            response->reason_phrase = osip_strdup ("Unknown code");
        }
      response->req_uri = NULL;
      response->sip_method = NULL;
    }

  i = osip_to_clone (request->to, &(response->to));
  if (i != 0)
    goto grd_error_1;

  i = osip_to_get_tag (response->to, &tag);
  if (i != 0)
    {                           /* we only add a tag if it does not already contains one! */
      if ((dialog != NULL) && (dialog->local_tag != NULL))
        /* it should contain the local TAG we created */
        {
          osip_to_set_tag (response->to, osip_strdup (dialog->local_tag));
      } else
        {
          if (status != 100)
            osip_to_set_tag (response->to, osip_to_tag_new_random ());
        }
    }

  i = osip_from_clone (request->from, &(response->from));
  if (i != 0)
    goto grd_error_1;

  pos = 0;
  while (!osip_list_eol (request->vias, pos))
    {
      osip_via_t *via;
      osip_via_t *via2;

      via = (osip_via_t *) osip_list_get (request->vias, pos);
      i = osip_via_clone (via, &via2);
      if (i != -0)
        goto grd_error_1;
      osip_list_add (response->vias, via2, -1);
      pos++;
    }

  i = osip_call_id_clone (request->call_id, &(response->call_id));
  if (i != 0)
    goto grd_error_1;
  i = osip_cseq_clone (request->cseq, &(response->cseq));
  if (i != 0)
    goto grd_error_1;

  if (MSG_IS_SUBSCRIBE (request))
    {
      osip_header_t *exp;
      osip_header_t *evt_hdr;

      osip_message_header_get_byname (request, "event", 0, &evt_hdr);
      if (evt_hdr != NULL && evt_hdr->hvalue != NULL)
	osip_message_set_header (response, "Event", evt_hdr->hvalue);
      else
	osip_message_set_header (response, "Event", "presence");
      i = osip_message_get_expires (request, 0, &exp);
      if (exp == NULL)
        {
          osip_header_t *cp;

          i = osip_header_clone (exp, &cp);
          if (cp != NULL)
            osip_list_add (response->headers, cp, 0);
        }
    }

  osip_message_set_allow (response, "INVITE");
  osip_message_set_allow (response, "ACK");
  osip_message_set_allow (response, "OPTIONS");
  osip_message_set_allow (response, "CANCEL");
  osip_message_set_allow (response, "BYE");
  osip_message_set_allow (response, "SUBSCRIBE");
  osip_message_set_allow (response, "NOTIFY");
  osip_message_set_allow (response, "MESSAGE");
  osip_message_set_allow (response, "INFO");
  osip_message_set_allow (response, "REFER");
  osip_message_set_allow (response, "UPDATE");

  *dest = response;
  return 0;

grd_error_1:
  osip_message_free (response);
  return -1;
}

int
complete_answer_that_establish_a_dialog (osip_message_t * response,
                                         osip_message_t * request)
{
  int i;
  int pos = 0;
  char contact[1000];
  char locip[50];
  struct eXosip_net *net;
  /* 12.1.1:
     copy all record-route in response
     add a contact with global scope
   */
  while (!osip_list_eol (request->record_routes, pos))
    {
      osip_record_route_t *rr;
      osip_record_route_t *rr2;

      rr = osip_list_get (request->record_routes, pos);
      i = osip_record_route_clone (rr, &rr2);
      if (i != 0)
        return -1;
      osip_list_add (response->record_routes, rr2, -1);
      pos++;
    }

  i = _eXosip_find_protocol(response);
  if (i==IPPROTO_UDP)
    {
      net = &eXosip.net_interfaces[0];
    }
  else if (i==IPPROTO_TCP)
    {
      net = &eXosip.net_interfaces[1];
    }
  else
    {
      net = &eXosip.net_interfaces[0];
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unsupported protocol (default to UDP)\n"));
      return -1;
    }

#ifdef SM
  eXosip_get_localip_from_via (response, locip, 49);
#else
  eXosip_guess_ip_for_via (net->net_ip_family, locip, 49);
#endif

  if (request->to->url->username == NULL)
    snprintf (contact, 1000, "<sip:%s:%s>", locip, net->net_port);
  else
    snprintf (contact, 1000, "<sip:%s@%s:%s>", request->to->url->username,
              locip, net->net_port);

  if (eXosip.net_interfaces[0].net_firewall_ip[0] != '\0')
    {
      osip_contact_t *con =
        (osip_contact_t *) osip_list_get (request->contacts, 0);
      if (con != NULL && con->url != NULL && con->url->host != NULL)
        {
          char *c_address = con->url->host;

          struct addrinfo *addrinfo;
          struct __eXosip_sockaddr addr;
	  i = eXosip_get_addrinfo (&addrinfo, con->url->host, 5060, IPPROTO_UDP);
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

          /* If c_address is a PUBLIC address, the request was
             coming from the PUBLIC network. */
          if (eXosip_is_public_address (c_address))
            {
              if (request->to->url->username == NULL)
                snprintf (contact, 1000, "<sip:%s:%s>",
			  eXosip.net_interfaces[0].net_firewall_ip,
                          net->net_port);
              else
                snprintf (contact, 1000, "<sip:%s@%s:%s>",
                          request->to->url->username,
			  eXosip.net_interfaces[0].net_firewall_ip,
                          net->net_port);
            }
        }
    }

  osip_message_set_contact (response, contact);

  return 0;
}

int
_eXosip_answer_invite_1xx (eXosip_call_t * jc, eXosip_dialog_t * jd, int code,
                           osip_message_t ** answer)
{
  int i;
  osip_transaction_t *tr;

  *answer = NULL;
  tr = eXosip_find_last_inc_invite (jc, jd);
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer"));
      return -1;
    }
  /* is the transaction already answered? */
  if (tr->state == IST_COMPLETED
      || tr->state == IST_CONFIRMED || tr->state == IST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      return -1;
    }

  if (jd == NULL)
    i = _eXosip_build_response_default (answer, NULL, code, tr->orig_request);
  else
    i =
      _eXosip_build_response_default (answer, jd->d_dialog, code,
                                      tr->orig_request);

  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "ERROR: Could not create response for invite\n"));
      return -2;
    }

  osip_message_set_content_length (*answer, "0");
  /*  send message to transaction layer */

  if (code > 100)
    {
      i = complete_answer_that_establish_a_dialog (*answer, tr->orig_request);
    }

  return 0;
}

int
_eXosip_answer_invite_2xx (eXosip_call_t * jc, eXosip_dialog_t * jd, int code,
                           osip_message_t ** answer)
{
  int i;
  osip_transaction_t *tr;

  *answer = NULL;
  tr = eXosip_find_last_inc_invite (jc, jd);

  if (tr == NULL || tr->orig_request == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer\n"));
      return -1;
    }

  if (jd != NULL && jd->d_dialog == NULL)
    {                           /* element previously removed */
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot answer this closed transaction\n"));
      return -1;
    }

  /* is the transaction already answered? */
  if (tr->state == IST_COMPLETED
      || tr->state == IST_CONFIRMED || tr->state == IST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      return -1;
    }

  if (jd == NULL)
    i = _eXosip_build_response_default (answer, NULL, code, tr->orig_request);
  else
    i =
      _eXosip_build_response_default (answer, jd->d_dialog, code,
                                      tr->orig_request);

  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "ERROR: Could not create response for invite\n"));
      return -1;
    }

  /* request that estabish a dialog: */
  /* 12.1.1 UAS Behavior */
  {
    i = complete_answer_that_establish_a_dialog (*answer, tr->orig_request);
    if (i != 0)
      goto g2atii_error_1;;     /* ?? */
  }

  return 0;

g2atii_error_1:
  osip_message_free (*answer);
  return -1;
}

int
_eXosip_answer_invite_3456xx (eXosip_call_t * jc, eXosip_dialog_t * jd,
                              int code, osip_message_t ** answer)
{
  int i;
  osip_transaction_t *tr;

  *answer = NULL;
  tr = eXosip_find_last_inc_invite (jc, jd);
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer"));
      return -1;
    }
  /* is the transaction already answered? */
  if (tr->state == IST_COMPLETED
      || tr->state == IST_CONFIRMED || tr->state == IST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      return -1;
    }

  i =
    _eXosip_build_response_default (answer, jd->d_dialog, code, tr->orig_request);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "ERROR: Could not create response for invite\n"));
      return -1;
    }

  if ((300 <= code) && (code <= 399))
    {
      /* Should add contact fields */
      /* ... */
    }

  osip_message_set_content_length (*answer, "0");
  /*  send message to transaction layer */

  return 0;
}

int
_eXosip_default_answer_invite_1xx (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                   int code)
{
  osip_event_t *evt_answer;
  osip_message_t *response;
  int i;
  osip_transaction_t *tr;

  tr = eXosip_find_last_inc_invite (jc, jd);
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer"));
      return -1;
    }
  /* is the transaction already answered? */
  if (tr->state == IST_COMPLETED
      || tr->state == IST_CONFIRMED || tr->state == IST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      return -1;
    }

  if (jd == NULL)
    i = _eXosip_build_response_default (&response, NULL, code, tr->orig_request);
  else
    i =
      _eXosip_build_response_default (&response, jd->d_dialog, code,
                                      tr->orig_request);

  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "ERROR: Could not create response for invite\n"));
      return -2;
    }

  osip_message_set_content_length (response, "0");
  /*  send message to transaction layer */

  if (code > 100)
    {
      /* request that estabish a dialog: */
      /* 12.1.1 UAS Behavior */
      i = complete_answer_that_establish_a_dialog (response, tr->orig_request);

      if (jd == NULL)
        {
          i = eXosip_dialog_init_as_uas (&jd, tr->orig_request, response);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: cannot create dialog!\n"));
          } else
            {
              ADD_ELEMENT (jc->c_dialogs, jd);
            }
        }
    }

  evt_answer = osip_new_outgoing_sipmessage (response);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  __eXosip_wakeup ();

  return 0;
}

int
_eXosip_default_answer_invite_3456xx (eXosip_call_t * jc,
                                      eXosip_dialog_t * jd, int code)
{
  osip_event_t *evt_answer;
  osip_message_t *response;
  int i;
  osip_transaction_t *tr;

  tr = eXosip_find_last_inc_invite (jc, jd);
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer"));
      return -1;
    }
  /* is the transaction already answered? */
  if (tr->state == IST_COMPLETED
      || tr->state == IST_CONFIRMED || tr->state == IST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      return -1;
    }

  i =
    _eXosip_build_response_default (&response, jd->d_dialog, code,
                                    tr->orig_request);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "ERROR: Could not create response for invite\n"));
      return -1;
    }

  osip_message_set_content_length (response, "0");
  /*  send message to transaction layer */

  evt_answer = osip_new_outgoing_sipmessage (response);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  __eXosip_wakeup ();
  return 0;
}


int
_eXosip_insubscription_answer_1xx (eXosip_notify_t * jn, eXosip_dialog_t * jd,
                                   int code)
{
  osip_event_t *evt_answer;
  osip_message_t *response;
  int i;
  osip_transaction_t *tr;

  tr = eXosip_find_last_inc_subscribe (jn, jd);
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer"));
      return -1;
    }

  if (jd == NULL)
    i = _eXosip_build_response_default (&response, NULL, code, tr->orig_request);
  else
    i =
      _eXosip_build_response_default (&response, jd->d_dialog, code,
                                      tr->orig_request);

  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "ERROR: Could not create response for subscribe\n"));
      return -1;
    }

  if (code > 100)
    {
      /* request that estabish a dialog: */
      /* 12.1.1 UAS Behavior */
      i = complete_answer_that_establish_a_dialog (response, tr->orig_request);

      if (jd == NULL)
        {
          i = eXosip_dialog_init_as_uas (&jd, tr->orig_request, response);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: cannot create dialog!\n"));
            }
          ADD_ELEMENT (jn->n_dialogs, jd);
        }
    }

  evt_answer = osip_new_outgoing_sipmessage (response);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  __eXosip_wakeup ();
  return 0;
}

int
_eXosip_insubscription_answer_2xx (eXosip_notify_t * jn, eXosip_dialog_t * jd,
                                   int code)
{
  osip_event_t *evt_answer;
  osip_message_t *response;
  int i;
  osip_transaction_t *tr;

  tr = eXosip_find_last_inc_subscribe (jn, jd);

  if (tr == NULL || tr->orig_request == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer\n"));
      return -1;
    }

  if (jd != NULL && jd->d_dialog == NULL)
    {                           /* element previously removed, this is a no hop! */
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot answer this closed transaction\n"));
      return -1;
    }

  if (jd == NULL)
    i = _eXosip_build_response_default (&response, NULL, code, tr->orig_request);
  else
    i =
      _eXosip_build_response_default (&response, jd->d_dialog, code,
                                      tr->orig_request);

  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "ERROR: Could not create response for subscribe\n"));
      code = 500;               /* ? which code to use? */
      return -1;
    }

  /* request that estabish a dialog: */
  /* 12.1.1 UAS Behavior */
  {
    i = complete_answer_that_establish_a_dialog (response, tr->orig_request);
    if (i != 0)
      goto g2atii_error_1;;     /* ?? */
  }

  /* THIS RESPONSE MUST BE SENT RELIABILY until the final ACK is received !! */
  /* this response must be stored at the upper layer!!! (it will be destroyed */
  /* right after being sent! */

  if (jd == NULL)
    {
      i = eXosip_dialog_init_as_uas (&jd, tr->orig_request, response);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot create dialog!\n"));
          return -1;
        }
      ADD_ELEMENT (jn->n_dialogs, jd);
    }

  eXosip_dialog_set_200ok (jd, response);
  evt_answer = osip_new_outgoing_sipmessage (response);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  __eXosip_wakeup ();

  osip_dialog_set_state (jd->d_dialog, DIALOG_CONFIRMED);
  return 0;

g2atii_error_1:
  osip_message_free (response);
  return -1;
}

int
_eXosip_insubscription_answer_3456xx (eXosip_notify_t * jn,
                                      eXosip_dialog_t * jd, int code)
{
  osip_event_t *evt_answer;
  osip_message_t *response;
  int i;
  osip_transaction_t *tr;

  tr = eXosip_find_last_inc_subscribe (jn, jd);
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot find transaction to answer"));
      return -1;
    }
  if (jd == NULL)
    i = _eXosip_build_response_default (&response, NULL, code, tr->orig_request);
  else
    i =
      _eXosip_build_response_default (&response, jd->d_dialog, code,
                                      tr->orig_request);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "ERROR: Could not create response for subscribe\n"));
      return -1;
    }

  if ((300 <= code) && (code <= 399))
    {
      /* Should add contact fields */
      /* ... */
    }

  evt_answer = osip_new_outgoing_sipmessage (response);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  __eXosip_wakeup ();
  return 0;
}
