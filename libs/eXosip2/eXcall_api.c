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

static int eXosip_create_transaction (eXosip_call_t * jc,
                                      eXosip_dialog_t * jd,
                                      osip_message_t * request);
static int eXosip_create_cancel_transaction (eXosip_call_t * jc,
                                             eXosip_dialog_t * jd,
                                             osip_message_t * request);
static int _eXosip_call_transaction_find (int tid, eXosip_call_t ** jc,
                                          eXosip_dialog_t ** jd,
                                          osip_transaction_t ** tr);

static int
eXosip_create_transaction (eXosip_call_t * jc,
                           eXosip_dialog_t * jd, osip_message_t * request)
{
  osip_event_t *sipevent;
  osip_transaction_t *tr;
  int i;

  i = osip_transaction_init (&tr, NICT, eXosip.j_osip, request);
  if (i != 0)
    {
      /* TODO: release the j_call.. */

      osip_message_free (request);
      return -1;
    }

  if (jd != NULL)
    osip_list_add (jd->d_out_trs, tr, 0);

  sipevent = osip_new_outgoing_sipmessage (request);
  sipevent->transactionid = tr->transactionid;

  osip_transaction_set_your_instance (tr, __eXosip_new_jinfo (jc, jd, NULL, NULL));
  osip_transaction_add_event (tr, sipevent);
  __eXosip_wakeup ();
  return 0;
}

static int
eXosip_create_cancel_transaction (eXosip_call_t * jc,
                                  eXosip_dialog_t * jd, osip_message_t * request)
{
  osip_event_t *sipevent;
  osip_transaction_t *tr;
  int i;

  i = osip_transaction_init (&tr, NICT, eXosip.j_osip, request);
  if (i != 0)
    {
      /* TODO: release the j_call.. */

      osip_message_free (request);
      return -2;
    }

  osip_list_add (eXosip.j_transactions, tr, 0);

  sipevent = osip_new_outgoing_sipmessage (request);
  sipevent->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, sipevent);
  __eXosip_wakeup ();
  return 0;
}

static int
_eXosip_call_transaction_find (int tid, eXosip_call_t ** jc,
                               eXosip_dialog_t ** jd, osip_transaction_t ** tr)
{
  for (*jc = eXosip.j_calls; *jc != NULL; *jc = (*jc)->next)
    {
      if ((*jc)->c_inc_tr != NULL && (*jc)->c_inc_tr->transactionid == tid)
        {
          *tr = (*jc)->c_inc_tr;
          *jd = (*jc)->c_dialogs;
          return 0;
        }
      if ((*jc)->c_out_tr != NULL && (*jc)->c_out_tr->transactionid == tid)
        {
          *tr = (*jc)->c_out_tr;
          *jd = (*jc)->c_dialogs;
          return 0;
        }
      for (*jd = (*jc)->c_dialogs; *jd != NULL; *jd = (*jd)->next)
        {
          osip_transaction_t *transaction;
          int pos = 0;

          while (!osip_list_eol ((*jd)->d_inc_trs, pos))
            {
              transaction =
                (osip_transaction_t *) osip_list_get ((*jd)->d_inc_trs, pos);
              if (transaction != NULL && transaction->transactionid == tid)
                {
                  *tr = transaction;
                  return 0;
                }
              pos++;
            }

          pos = 0;
          while (!osip_list_eol ((*jd)->d_out_trs, pos))
            {
              transaction =
                (osip_transaction_t *) osip_list_get ((*jd)->d_out_trs, pos);
              if (transaction != NULL && transaction->transactionid == tid)
                {
                  *tr = transaction;
                  return 0;
                }
              pos++;
            }
        }
    }
  *jd = NULL;
  *jc = NULL;
  return -1;
}

int
eXosip_call_set_reference (int id, void *reference)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;

  if (id > 0)
    {
      eXosip_call_dialog_find (id, &jc, &jd);
      if (jc == NULL)
        {
          eXosip_call_find (id, &jc);
        }
    }
  if (jc == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return -1;
    }
  jc->external_reference = reference;
  return 0;
}

/* this method can't be called unless the previous
   INVITE transaction is over. */
int
eXosip_call_build_initial_invite (osip_message_t ** invite,
                                  const char *to,
                                  const char *from,
                                  const char *route, const char *subject)
{
  int i;
  osip_uri_param_t *uri_param = NULL;
  osip_to_t *_to=NULL;

  *invite = NULL;

  if (to != NULL && *to == '\0')
    return -1;
  if (route != NULL && *route == '\0')
    route = NULL;
  if (subject != NULL && *subject == '\0')
    subject = NULL;

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
      i = generating_request_out_of_dialog (invite, "INVITE", to, uri_param->gvalue, from, route);      
    }
  else
    {
      if (eXosip.net_interfaces[0].net_socket>0)
	i = generating_request_out_of_dialog (invite, "INVITE", to, "UDP", from, route);
      else if (eXosip.net_interfaces[1].net_socket>0)
	i = generating_request_out_of_dialog (invite, "INVITE", to, "TCP", from, route);
      else
	i = generating_request_out_of_dialog (invite, "INVITE", to, "UDP", from, route);
    }
  osip_to_free(_to);
  if (i != 0)
    return -1;

  if (subject != NULL)
    osip_message_set_subject (*invite, subject);

  /* after this delay, we should send a CANCEL */
  osip_message_set_expires (*invite, "120");
  return 0;
}

int
eXosip_call_send_initial_invite (osip_message_t * invite)
{
  eXosip_call_t *jc;
  osip_transaction_t *transaction;
  osip_event_t *sipevent;
  int i;

  eXosip_call_init (&jc);

  i = osip_transaction_init (&transaction, ICT, eXosip.j_osip, invite);
  if (i != 0)
    {
      eXosip_call_free (jc);
      osip_message_free (invite);
      return -1;
    }

  jc->c_out_tr = transaction;

  sipevent = osip_new_outgoing_sipmessage (invite);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, NULL, NULL, NULL));
  osip_transaction_add_event (transaction, sipevent);

  jc->external_reference = NULL;
  ADD_ELEMENT (eXosip.j_calls, jc);

  eXosip_update ();             /* fixed? */
  __eXosip_wakeup ();
  return jc->c_id;
}

int
eXosip_call_build_ack (int did, osip_message_t ** _ack)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *tr = NULL;

  osip_message_t *ack;
  char *transport;
  int i;

  *_ack = NULL;

  if (did > 0)
    {
      eXosip_call_dialog_find (did, &jc, &jd);
    }
  if (jc == NULL || jd == NULL || jd->d_dialog == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return -1;
    }

  tr = eXosip_find_last_invite (jc, jd);

  if (tr == NULL || tr->orig_request == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No transaction for call?\n"));
      return -1;
    }

  if (0 != osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: ACK are only sent for invite transactions\n"));
      return -1;
    }

  transport = NULL;
  transport = _eXosip_transport_protocol(tr->orig_request);
  if (transport==NULL)
    i = _eXosip_build_request_within_dialog (&ack, "ACK", jd->d_dialog, "UDP");
  else
    i = _eXosip_build_request_within_dialog (&ack, "ACK", jd->d_dialog, transport);

  if (i != 0)
    {
      return -1;
    }

  /* Fix CSeq Number when request has been exchanged during INVITE transactions */
  if (tr->orig_request->cseq!=NULL && tr->orig_request->cseq->number!=NULL)
    {
      if (ack!=NULL && ack->cseq!=NULL && ack->cseq->number!=NULL)
	{
	  osip_free(ack->cseq->number);
	  ack->cseq->number = osip_strdup(tr->orig_request->cseq->number);
	}
    }

  /* copy all credentials from INVITE! */
  {
    int pos = 0;
    int i;
    osip_proxy_authorization_t *pa = NULL;

    i = osip_message_get_proxy_authorization (tr->orig_request, pos, &pa);
    while (i == 0 && pa != NULL)
      {
        osip_proxy_authorization_t *pa2;

        i = osip_proxy_authorization_clone (pa, &pa2);
        if (i != 0)
          {
            OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                    "Error in credential from INVITE\n"));
            break;
          }
        osip_list_add (ack->proxy_authorizations, pa2, -1);
        pa = NULL;
        pos++;
        i = osip_message_get_proxy_authorization (tr->orig_request, pos, &pa);
      }
  }

  *_ack = ack;
  return 0;
}

int
eXosip_call_send_ack (int did, osip_message_t * ack)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;

  osip_route_t *route;
  char *host;
  int port;

  if (did > 0)
    {
      eXosip_call_dialog_find (did, &jc, &jd);
    }

  if (jc == NULL || jd == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      if (ack != NULL)
	osip_message_free (ack);
      return -1;
    }

  if (ack == NULL)
    {
      int i;

      i = eXosip_call_build_ack (did, &ack);
      if (i != 0)
        {
          return -1;
        }
    }

  osip_message_get_route (ack, 0, &route);
  if (route != NULL)
    {
      osip_uri_param_t *lr_param=NULL;
      osip_uri_uparam_get_byname (route->url, "lr", &lr_param);
      if (lr_param==NULL)
	route=NULL;
    }

  if (route != NULL)
    {
      port = 5060;
      if (route->url->port != NULL)
        port = osip_atoi (route->url->port);
      host = route->url->host;
  } else
    {
      port = 5060;
      if (ack->req_uri->port != NULL)
        port = osip_atoi (ack->req_uri->port);
      host = ack->req_uri->host;
    }

  cb_snd_message (NULL, ack, host, port, -1);

  if (jd->d_ack != NULL)
    osip_message_free (jd->d_ack);
  jd->d_ack = ack;
  return 0;
}

int
eXosip_call_build_request (int jid, const char *method, osip_message_t ** request)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;

  osip_transaction_t *transaction;
  char *transport;
  int i;
  
  *request = NULL;
  if (method == NULL || method[0] == '\0')
    return -1;

  if (jid > 0)
    {
      eXosip_call_dialog_find (jid, &jc, &jd);
    }
  if (jd == NULL || jd->d_dialog == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return -1;
    }

  transaction = NULL;
  if (0 == osip_strcasecmp (method, "INVITE"))
    {
      transaction = eXosip_find_last_invite (jc, jd);
  } else                        /* OPTIONS, UPDATE, INFO, REFER, ?... */
    {
      transaction = eXosip_find_last_transaction (jc, jd, method);
    }

  if (transaction != NULL)
    {
      if (0 != osip_strcasecmp (method, "INVITE"))
        {
          if (transaction->state != NICT_TERMINATED &&
              transaction->state != NIST_TERMINATED &&
              transaction->state != NICT_COMPLETED &&
              transaction->state != NIST_COMPLETED)
            return -1;
      } else
        {
          if (transaction->state != ICT_TERMINATED &&
              transaction->state != IST_TERMINATED &&
              transaction->state != IST_CONFIRMED &&
              transaction->state != ICT_COMPLETED)
            return -1;
        }
    }

  transport = NULL;
  transaction = eXosip_find_last_invite (jc, jd);
  if (transaction!=NULL && transaction->orig_request!=NULL)
    transport = _eXosip_transport_protocol(transaction->orig_request);

  transaction = NULL;

  if (transport==NULL)
    i = _eXosip_build_request_within_dialog (request, method, jd->d_dialog, "UDP");
  else
    i = _eXosip_build_request_within_dialog (request, method, jd->d_dialog, transport);
  if (i != 0)
    return -2;

  if (jc->response_auth!=NULL)
    eXosip_add_authentication_information (*request, jc->response_auth);

  return 0;
}

int
eXosip_call_send_request (int jid, osip_message_t * request)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;

  osip_transaction_t *transaction;
  osip_event_t *sipevent;

  int i;

  if (request == NULL)
    return -1;

  if (request->sip_method == NULL)
    {
      osip_message_free (request);
      return -1;
    }

  if (jid > 0)
    {
      eXosip_call_dialog_find (jid, &jc, &jd);
    }
  if (jd == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      osip_message_free (request);
      return -1;
    }

  transaction = NULL;
  if (0 == osip_strcasecmp (request->sip_method, "INVITE"))
    {
      transaction = eXosip_find_last_invite (jc, jd);
  } else                        /* OPTIONS, UPDATE, INFO, REFER, ?... */
    {
      transaction = eXosip_find_last_transaction (jc, jd, request->sip_method);
    }

  if (transaction != NULL)
    {
      if (0 != osip_strcasecmp (request->sip_method, "INVITE"))
        {
          if (transaction->state != NICT_TERMINATED &&
              transaction->state != NIST_TERMINATED &&
              transaction->state != NICT_COMPLETED &&
              transaction->state != NIST_COMPLETED)
	    {
	      osip_message_free (request);
	      return -1;
	    }
      } else
        {
          if (transaction->state != ICT_TERMINATED &&
              transaction->state != IST_TERMINATED &&
              transaction->state != IST_CONFIRMED &&
              transaction->state != ICT_COMPLETED)
	    {
	      osip_message_free (request);
	      return -1;
	    }
        }
    }

  transaction = NULL;
  if (0 != osip_strcasecmp (request->sip_method, "INVITE"))
    {
      i = osip_transaction_init (&transaction, NICT, eXosip.j_osip, request);
  } else
    {
      i = osip_transaction_init (&transaction, ICT, eXosip.j_osip, request);
    }

  if (i != 0)
    {
      osip_message_free (request);
      return -2;
    }

  osip_list_add (jd->d_out_trs, transaction, 0);

  sipevent = osip_new_outgoing_sipmessage (request);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  osip_transaction_add_event (transaction, sipevent);
  __eXosip_wakeup ();
  return 0;
}

int
eXosip_call_build_refer (int did, const char *refer_to, osip_message_t ** request)
{
  int i;

  *request = NULL;
  i = eXosip_call_build_request (did, "REFER", request);
  if (i != 0)
    return -1;

  if (refer_to == NULL || refer_to[0] == '\0')
    return 0;

  osip_message_set_header (*request, "Refer-to", refer_to);
  return 0;
}

int
eXosip_call_build_options (int did, osip_message_t ** request)
{
  int i;

  *request = NULL;
  i = eXosip_call_build_request (did, "OPTIONS", request);
  if (i != 0)
    return -1;

  return 0;
}

int
eXosip_call_build_info (int did, osip_message_t ** request)
{
  int i;

  *request = NULL;
  i = eXosip_call_build_request (did, "INFO", request);
  if (i != 0)
    return -1;

  return 0;
}

int
eXosip_call_build_update (int did, osip_message_t ** request)
{
  int i;

  *request = NULL;
  i = eXosip_call_build_request (did, "UPDATE", request);
  if (i != 0)
    return -1;

  return 0;
}

int
eXosip_call_build_notify (int did, int subscription_status,
                          osip_message_t ** request)
{
  char subscription_state[50];
  char *tmp;
  int i;

  *request = NULL;
  i = eXosip_call_build_request (did, "NOTIFY", request);
  if (i != 0)
    return -1;

  if (subscription_status == EXOSIP_SUBCRSTATE_PENDING)
    osip_strncpy (subscription_state, "pending;expires=", 16);
  else if (subscription_status == EXOSIP_SUBCRSTATE_ACTIVE)
    osip_strncpy (subscription_state, "active;expires=", 15);
  else if (subscription_status == EXOSIP_SUBCRSTATE_TERMINATED)
    {
      int reason = NORESOURCE;

      if (reason == DEACTIVATED)
        osip_strncpy (subscription_state, "terminated;reason=deactivated", 29);
      else if (reason == PROBATION)
        osip_strncpy (subscription_state, "terminated;reason=probation", 27);
      else if (reason == REJECTED)
        osip_strncpy (subscription_state, "terminated;reason=rejected", 26);
      else if (reason == TIMEOUT)
        osip_strncpy (subscription_state, "terminated;reason=timeout", 25);
      else if (reason == GIVEUP)
        osip_strncpy (subscription_state, "terminated;reason=giveup", 24);
      else if (reason == NORESOURCE)
        osip_strncpy (subscription_state, "terminated;reason=noresource", 29);
    }
  tmp = subscription_state + strlen (subscription_state);
  if (subscription_status != EXOSIP_SUBCRSTATE_TERMINATED)
    sprintf (tmp, "%i", 180);
  osip_message_set_header (*request, "Subscription-State", subscription_state);

  return 0;
}

int
eXosip_call_build_answer (int tid, int status, osip_message_t ** answer)
{
  int i = -1;
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *tr = NULL;

  *answer = NULL;

  if (tid > 0)
    {
      _eXosip_call_transaction_find (tid, &jc, &jd, &tr);
    }
  if (tr == NULL || jd == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return -1;
    }

  if (0 == osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
    {
      if (status > 100 && status < 200)
        {
          i = _eXosip_answer_invite_1xx (jc, jd, status, answer);
      } else if (status > 199 && status < 300)
        {
          i = _eXosip_answer_invite_2xx (jc, jd, status, answer);
      } else if (status > 300 && status < 699)
        {
          i = _eXosip_answer_invite_3456xx (jc, jd, status, answer);
      } else
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: wrong status code (101<status<699)\n"));
          return -1;
        }
  } else
    {
      if (jd != NULL)
        {
          i =
            _eXosip_build_response_default (answer, jd->d_dialog, status,
                                            tr->orig_request);
      } else
        {
          i =
            _eXosip_build_response_default (answer, NULL, status,
                                            tr->orig_request);
        }
      if (status>100 && status < 300 )
	i = complete_answer_that_establish_a_dialog (*answer, tr->orig_request);
    }

  if (i != 0)
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                              "ERROR: Could not create response for %s\n",
                              tr->orig_request->sip_method));
      return -1;
    }
  return 0;
}

int
eXosip_call_send_answer (int tid, int status, osip_message_t * answer)
{
  int i = -1;
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *tr = NULL;
  osip_event_t *evt_answer;

  if (tid > 0)
    {
      _eXosip_call_transaction_find (tid, &jc, &jd, &tr);
    }
  if (jd == NULL || tr == NULL || tr->orig_request == NULL
      || tr->orig_request->sip_method == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here or no transaction for call\n"));
      osip_message_free (answer);
      return -1;
    }

  if (answer == NULL)
    {
      if (0 == osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
        {
          if (status >= 100 && status <= 199)
            {
          } else if (status >= 300 && status <= 699)
            {
          } else
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: Wrong parameter?\n"));
	      osip_message_free (answer);
              return -1;
            }
        }
    }

  /* is the transaction already answered? */
  if (tr->state == IST_COMPLETED
      || tr->state == IST_CONFIRMED
      || tr->state == IST_TERMINATED
      || tr->state == NIST_COMPLETED || tr->state == NIST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      osip_message_free (answer);
      return -1;
    }

  if (answer == NULL)
    {
      if (0 == osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
        {
          if (status < 200)
            i = _eXosip_default_answer_invite_1xx (jc, jd, status);
          else
            i = _eXosip_default_answer_invite_3456xx (jc, jd, status);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: cannot send response!\n"));
              return -1;
            }
      } else
        {
          /* TODO */
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: a response must be given!\n"));
          return -1;
        }
      return 0;
  } else
    {
      i = 0;
    }

  if (0 == osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
    {
      if (MSG_IS_STATUS_1XX (answer))
        {
          if (jd == NULL)
            {
              i = eXosip_dialog_init_as_uas (&jd, tr->orig_request, answer);
              if (i != 0)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_ERROR, NULL,
                               "eXosip: cannot create dialog!\n"));
                  i = 0;
              } else
                {
                  ADD_ELEMENT (jc->c_dialogs, jd);
                }
            }
      } else if (MSG_IS_STATUS_2XX (answer))
        {
          if (jd == NULL)
            {
              i = eXosip_dialog_init_as_uas (&jd, tr->orig_request, answer);
              if (i != 0)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_ERROR, NULL,
                               "eXosip: cannot create dialog!\n"));
		  osip_message_free (answer);
                  return -1;
                }
              ADD_ELEMENT (jc->c_dialogs, jd);
          } else
            i = 0;

          eXosip_dialog_set_200ok (jd, answer);
          osip_dialog_set_state (jd->d_dialog, DIALOG_CONFIRMED);
      } else if (answer->status_code >= 300 && answer->status_code <= 699)
        {
          i = 0;
      } else
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: wrong status code (101<status<699)\n"));
	  osip_message_free (answer);
          return -1;
        }
      if (i != 0)
	{
	  osip_message_free (answer);
	  return -1;
	}
    }

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  eXosip_update ();
  __eXosip_wakeup ();
  return 0;
}

int
eXosip_call_terminate (int cid, int did)
{
  int i;
  osip_transaction_t *tr;
  osip_message_t *request = NULL;
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  char *transport=NULL;

  if (did > 0)
    {
      eXosip_call_dialog_find (did, &jc, &jd);
      if (jd == NULL)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: No call here?\n"));
          return -1;
        }
  } else
    {
      eXosip_call_find (cid, &jc);
    }

  if (jc == NULL)
    {
      return -1;
    }

  tr = eXosip_find_last_out_invite (jc, jd);
  if (tr != NULL && tr->last_response != NULL
      && MSG_IS_STATUS_1XX (tr->last_response))
    {
      i = generating_cancel (&request, tr->orig_request);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot terminate this call!\n"));
          return -2;
        }
      i = eXosip_create_cancel_transaction (jc, jd, request);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot initiate SIP transaction!\n"));
          return i;
        }
      if (jd != NULL)
        {
          osip_dialog_free (jd->d_dialog);
          jd->d_dialog = NULL;
          eXosip_update(); //AMD 30/09/05
        }
      return 0;
    }

  if (jd == NULL || jd->d_dialog == NULL)
    {
      /* Check if some dialog exists */
      if (jd != NULL && jd->d_dialog != NULL)
        {
	  transport = NULL;
	  if (tr!=NULL && tr->orig_request!=NULL)
	    transport = _eXosip_transport_protocol(tr->orig_request);
	  if (transport==NULL)
	    i = generating_bye (&request, jd->d_dialog, "UDP");
	  else
	    i = generating_bye (&request, jd->d_dialog, transport);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: cannot terminate this call!\n"));
              return -2;
            }

	  if (jc->response_auth!=NULL)
	    eXosip_add_authentication_information (request, jc->response_auth);

          i = eXosip_create_transaction (jc, jd, request);
          if (i != 0)
            {
              OSIP_TRACE (osip_trace
                          (__FILE__, __LINE__, OSIP_ERROR, NULL,
                           "eXosip: cannot initiate SIP transaction!\n"));
              return -2;
            }

          osip_dialog_free (jd->d_dialog);
          jd->d_dialog = NULL;
          eXosip_update(); //AMD 30/09/05
          return 0;
        }

      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No established dialog!\n"));
      return -1;
    }

  if (tr == NULL)
    {
      /*this may not be enough if it's a re-INVITE! */
      tr = eXosip_find_last_inc_invite (jc, jd);
      if (tr != NULL && tr->last_response != NULL &&
          MSG_IS_STATUS_1XX (tr->last_response))
        {                       /* answer with 603 */
          i = eXosip_call_send_answer (tr->transactionid, 603, NULL);
          return i;
        }
    }

  transport = NULL;
  if (tr!=NULL && tr->orig_request!=NULL)
    transport = _eXosip_transport_protocol(tr->orig_request);
  if (transport==NULL)
    i = generating_bye (&request, jd->d_dialog, "UDP");
  else
    i = generating_bye (&request, jd->d_dialog, transport);

  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot terminate this call!\n"));
      return -2;
    }
  if (jc->response_auth!=NULL)
    eXosip_add_authentication_information (request, jc->response_auth);

  i = eXosip_create_transaction (jc, jd, request);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot initiate SIP transaction!\n"));
      return -2;
    }

  osip_dialog_free (jd->d_dialog);
  jd->d_dialog = NULL;
  eXosip_update(); //AMD 30/09/05
  return 0;
}

int
eXosip_call_build_prack (int tid, osip_message_t ** prack)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *tr = NULL;

  osip_header_t *rseq;
  char *transport;
  int i;

  *prack = NULL;

  if (tid > 0)
    {
      _eXosip_call_transaction_find (tid, &jc, &jd, &tr);
    }
  if (jc == NULL || jd == NULL || jd->d_dialog == NULL
      || tr == NULL || tr->orig_request == NULL
      || tr->orig_request->sip_method == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here or no transaction for call\n"));
      return -1;
    }

  if (0 != osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
    return -1;

  /* PRACK are only send in the PROCEEDING state */
  if (tr->state != ICT_PROCEEDING)
    return -1;

  if (tr->orig_request->cseq == NULL
      || tr->orig_request->cseq->number == NULL
      || tr->orig_request->cseq->method == NULL)
    return -1;

  transport = NULL;
  if (tr!=NULL && tr->orig_request!=NULL)
    transport = _eXosip_transport_protocol(tr->orig_request);

  if (transport==NULL)
    i = _eXosip_build_request_within_dialog (prack, "PRACK", jd->d_dialog, "UDP");
  else
    i = _eXosip_build_request_within_dialog (prack, "PRACK", jd->d_dialog, transport);

  if (i != 0)
    return -2;

  osip_message_header_get_byname (tr->last_response, "RSeq", 0, &rseq);
  if (rseq != NULL && rseq->hvalue != NULL)
    {
      char tmp[128];

      memset (tmp, '\0', sizeof (tmp));
      snprintf (tmp, 127, "%s %s %s", rseq->hvalue,
                tr->orig_request->cseq->number, tr->orig_request->cseq->method);
      osip_message_set_header (*prack, "RAck", tmp);
    }

  return 0;
}

int
eXosip_call_send_prack (int tid, osip_message_t * prack)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *tr = NULL;

  osip_event_t *sipevent;
  int i;

  if (prack == NULL)
    return -1;

  if (tid > 0)
    {
      _eXosip_call_transaction_find (tid, &jc, &jd, &tr);
    }
  if (jc == NULL || jd == NULL || jd->d_dialog == NULL
      || tr == NULL || tr->orig_request == NULL
      || tr->orig_request->sip_method == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here or no transaction for call\n"));
      osip_message_free (prack);
      return -1;
    }

  if (0 != osip_strcasecmp (tr->orig_request->sip_method, "INVITE"))
    {
      osip_message_free (prack);
      return -1;
    }

  /* PRACK are only send in the PROCEEDING state */
  if (tr->state != ICT_PROCEEDING)
    {
      osip_message_free (prack);
      return -1;
    }

  tr = NULL;
  i = osip_transaction_init (&tr, NICT, eXosip.j_osip, prack);

  if (i != 0)
    {
      osip_message_free (prack);
      return -2;
    }

  osip_list_add (jd->d_out_trs, tr, 0);

  sipevent = osip_new_outgoing_sipmessage (prack);
  sipevent->transactionid = tr->transactionid;

  osip_transaction_set_your_instance (tr, __eXosip_new_jinfo (jc, jd, NULL, NULL));
  osip_transaction_add_event (tr, sipevent);
  __eXosip_wakeup ();
  return 0;
}


int
_eXosip_call_redirect_request (eXosip_call_t * jc,
                               eXosip_dialog_t * jd, osip_transaction_t * out_tr)
{
  osip_transaction_t *tr = NULL;
  osip_message_t *msg = NULL;
  osip_event_t *sipevent;

  char locip[256];
  int cseq;
  char tmp[256];
  osip_via_t *via;
  osip_contact_t *co;
  int pos;
  int i;
  int protocol = IPPROTO_UDP;

  if (jc == NULL)
    return -1;
  if (jd != NULL)
    {
      if (jd->d_out_trs == NULL)
        return -1;
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

  co = NULL;
  pos = 0;
  while (!osip_list_eol (out_tr->last_response->contacts, pos))
    {
      co = (osip_contact_t *) osip_list_get (out_tr->last_response->contacts, pos);
      if (co != NULL && co->url != NULL && co->url->url_params != NULL)
        {
          /* check tranport? Only allow UDP, right now */
          osip_uri_param_t *u_param;
          int pos2;

          u_param = NULL;
          pos2 = 0;
          while (!osip_list_eol (co->url->url_params, pos2))
            {
              u_param =
                (osip_uri_param_t *) osip_list_get (co->url->url_params, pos2);
              if (u_param == NULL || u_param->gname == NULL
                  || u_param->gvalue == NULL)
                {
                  u_param = NULL;
                  /* skip */
              } else if (0 == osip_strcasecmp (u_param->gvalue, "transport"))
                {
                  if (0 == osip_strcasecmp (u_param->gvalue, "udp"))
                    {
#if 0
                      /* remove the UDP parameter */
                      osip_list_remove (co->url->url_params, pos2);
                      osip_uri_param_free (u_param);
#endif
                      u_param = NULL;
		      protocol=IPPROTO_UDP;
                      break;    /* ok */
                    }
		  else if (0 == osip_strcasecmp (u_param->gvalue, "tcp"))
		    {
#if 0
                      osip_list_remove (co->url->url_params, pos2);
                      osip_uri_param_free (u_param);
#endif
		      protocol=IPPROTO_TCP;
                      u_param = NULL;
		    }
                  break;
                }
              pos2++;
            }

          if (u_param == NULL || u_param->gname == NULL || u_param->gvalue == NULL)
            {
              break;            /* default is udp! */
            }
        }
      pos++;
      co = NULL;
    }

  if (co == NULL || co->url == NULL)
    {
      osip_message_free (msg);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: contact header\n"));
      return -1;
    }

  /* TODO:
     remove extra parameter from new request-uri
     check usual parameter like "transport"
   */

  /* replace request-uri with NEW contact address */
  osip_uri_free (msg->req_uri);
  msg->req_uri = NULL;
  osip_uri_clone (co->url, &msg->req_uri);

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

  if (protocol==IPPROTO_UDP)
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
  else if (protocol==IPPROTO_TCP)
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

  if (0 != osip_strcasecmp (msg->sip_method, "INVITE"))
    {
      i = osip_transaction_init (&tr, NICT, eXosip.j_osip, msg);
  } else
    {
      i = osip_transaction_init (&tr, ICT, eXosip.j_osip, msg);
    }

  if (i != 0)
    {
      osip_message_free (msg);
      return -1;
    }

  if (out_tr == jc->c_out_tr)
    {
      /* replace with the new tr */
      osip_list_add (eXosip.j_transactions, jc->c_out_tr, 0);
      jc->c_out_tr = tr;

      /* fix dialog issue */
      if (jd!=NULL)
      {
        REMOVE_ELEMENT(jc->c_dialogs, jd);
        eXosip_dialog_free(jd);
        jd=NULL;
      }
    } else
    {
      /* add the new tr for the current dialog */
      osip_list_add (jd->d_out_trs, tr, 0);
    }

  sipevent = osip_new_outgoing_sipmessage (msg);

  osip_transaction_set_your_instance (tr, __eXosip_new_jinfo (jc, jd, NULL, NULL));
  osip_transaction_add_event (tr, sipevent);

  eXosip_update ();             /* fixed? */
  __eXosip_wakeup ();
  return 0;
}

int
_eXosip_call_send_request_with_credential (eXosip_call_t * jc,
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
  int pos;

  if (jc == NULL)
    return -1;
  if (jd != NULL)
    {
      if (jd->d_out_trs == NULL)
        return -1;
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

  /* remove all previous authentication headers */
  pos=0;
  while (!osip_list_eol(msg->authorizations, pos))
    {
      osip_authorization_t *auth;
      auth = (osip_authorization_t*)osip_list_get(msg->authorizations, pos);
      osip_list_remove(msg->authorizations, pos);
      osip_authorization_free(auth);
      pos++;
    }

  pos=0;
  while (!osip_list_eol(msg->proxy_authorizations, pos))
    {
      osip_proxy_authorization_t *auth;
      auth = (osip_proxy_authorization_t*)osip_list_get(msg->proxy_authorizations, pos);
      osip_list_remove(msg->proxy_authorizations, pos);
      osip_authorization_free(auth);
      pos++;
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

  if (0 != osip_strcasecmp (msg->sip_method, "INVITE"))
    {
      i = osip_transaction_init (&tr, NICT, eXosip.j_osip, msg);
  } else
    {
      i = osip_transaction_init (&tr, ICT, eXosip.j_osip, msg);
    }

  if (i != 0)
    {
      osip_message_free (msg);
      return -1;
    }

  if (out_tr == jc->c_out_tr)
    {
      /* replace with the new tr */
      osip_list_add (eXosip.j_transactions, jc->c_out_tr, 0);
      jc->c_out_tr = tr;

      /* fix dialog issue */
      if (jd!=NULL)
      {
        REMOVE_ELEMENT(jc->c_dialogs, jd);
        eXosip_dialog_free(jd);
        jd=NULL;
      }
  } else
    {
      /* add the new tr for the current dialog */
      osip_list_add (jd->d_out_trs, tr, 0);
    }

  sipevent = osip_new_outgoing_sipmessage (msg);

  osip_transaction_set_your_instance (tr, __eXosip_new_jinfo (jc, jd, NULL, NULL));
  osip_transaction_add_event (tr, sipevent);

  eXosip_update ();             /* fixed? */
  __eXosip_wakeup ();
  return 0;
}
