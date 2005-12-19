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
#endif

extern eXosip_t eXosip;
extern int ipv6_enable;

/* Private functions */
static void eXosip_send_default_answer (eXosip_dialog_t * jd,
                                        osip_transaction_t * transaction,
                                        osip_event_t * evt,
                                        int status,
                                        char *reason_phrase,
                                        char *warning, int line);
static void eXosip_process_info (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                 osip_transaction_t * transaction,
                                 osip_event_t * evt);
static void eXosip_process_options (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                    osip_transaction_t * transaction,
                                    osip_event_t * evt);
static void eXosip_process_bye (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                osip_transaction_t * transaction,
                                osip_event_t * evt);
static void eXosip_process_refer (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                  osip_transaction_t * transaction,
                                  osip_event_t * evt);
static void eXosip_process_ack (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                osip_event_t * evt);
static void eXosip_process_prack (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                  osip_transaction_t * transaction,
                                  osip_event_t * evt);
static int cancel_match_invite (osip_transaction_t * invite,
                                osip_message_t * cancel);
static void eXosip_process_cancel (osip_transaction_t * transaction,
                                   osip_event_t * evt);
static osip_event_t *eXosip_process_reinvite (eXosip_call_t * jc,
                                              eXosip_dialog_t * jd,
                                              osip_transaction_t *
                                              transaction, osip_event_t * evt);
static void eXosip_process_new_options (osip_transaction_t * transaction,
                                        osip_event_t * evt);
static void eXosip_process_new_invite (osip_transaction_t * transaction,
                                       osip_event_t * evt);
static int eXosip_event_package_is_supported (osip_transaction_t *
                                              transaction, osip_event_t * evt);
static void eXosip_process_new_subscribe (osip_transaction_t * transaction,
                                          osip_event_t * evt);
static void eXosip_process_subscribe_within_call (eXosip_notify_t * jn,
                                                  eXosip_dialog_t * jd,
                                                  osip_transaction_t *
                                                  transaction, osip_event_t * evt);
static void eXosip_process_notify_within_dialog (eXosip_subscribe_t * js,
                                                 eXosip_dialog_t * jd,
                                                 osip_transaction_t *
                                                 transaction, osip_event_t * evt);
static int eXosip_match_notify_for_subscribe (eXosip_subscribe_t * js,
                                              osip_message_t * notify);
static void eXosip_process_message_outside_of_dialog (osip_transaction_t * tr,
						      osip_event_t * evt);
static void eXosip_process_refer_outside_of_dialog (osip_transaction_t * tr,
						    osip_event_t * evt);
static void eXosip_process_message_within_dialog (eXosip_call_t * jc,
						  eXosip_dialog_t * jd,
						  osip_transaction_t * transaction,
						  osip_event_t * evt);
static void eXosip_process_newrequest (osip_event_t * evt, int socket);
static void eXosip_process_response_out_of_transaction (osip_event_t * evt);
static int eXosip_pendingosip_transaction_exist (eXosip_call_t * jc,
                                                 eXosip_dialog_t * jd);
static int eXosip_release_finished_calls (eXosip_call_t * jc,
                                          eXosip_dialog_t * jd);
static int eXosip_release_aborted_calls (eXosip_call_t * jc, eXosip_dialog_t * jd);


static void
eXosip_send_default_answer (eXosip_dialog_t * jd,
                            osip_transaction_t * transaction,
                            osip_event_t * evt,
                            int status,
                            char *reason_phrase, char *warning, int line)
{
  osip_event_t *evt_answer;
  osip_message_t *answer;
  int i;

  /* osip_list_add(eXosip.j_transactions, transaction, 0); */
  osip_transaction_set_your_instance (transaction, NULL);

  /* THIS METHOD DOES NOT ACCEPT STATUS CODE BETWEEN 101 and 299 */
  if (status > 100 && status < 299 && MSG_IS_INVITE (evt->sip))
    return;

  if (jd != NULL)
    i = _eXosip_build_response_default (&answer, jd->d_dialog, status, evt->sip);
  else
    i = _eXosip_build_response_default (&answer, NULL, status, evt->sip);

  if (i != 0 || answer == NULL)
    {
      return;
    }

  if (reason_phrase != NULL)
    {
      char *_reason;

      _reason = osip_message_get_reason_phrase (answer);
      if (_reason != NULL)
        osip_free (_reason);
      _reason = osip_strdup (reason_phrase);
      osip_message_set_reason_phrase (answer, _reason);
    }

  osip_message_set_content_length (answer, "0");

  if (status == 500)
    osip_message_set_retry_after (answer, "10");

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = transaction->transactionid;
  osip_transaction_add_event (transaction, evt_answer);
  __eXosip_wakeup ();

}

static void
eXosip_process_options (eXosip_call_t * jc, eXosip_dialog_t * jd,
                        osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_list_add (jd->d_inc_trs, transaction, 0);
  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  __eXosip_wakeup ();
}

static void
eXosip_process_info (eXosip_call_t * jc, eXosip_dialog_t * jd,
                     osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_list_add (jd->d_inc_trs, transaction, 0);
  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  __eXosip_wakeup ();
}


static void
eXosip_process_bye (eXosip_call_t * jc, eXosip_dialog_t * jd,
                    osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_event_t *evt_answer;
  osip_message_t *answer;
  int i;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, NULL /*jd */ ,
                                                          NULL, NULL));

  i = _eXosip_build_response_default (&answer, jd->d_dialog, 200, evt->sip);
  if (i != 0)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      return;
    }
  osip_message_set_content_length (answer, "0");

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = transaction->transactionid;

  osip_list_add (jd->d_inc_trs, transaction, 0);

  /* Release the eXosip_dialog */
  osip_dialog_free (jd->d_dialog);
  jd->d_dialog = NULL;
  report_call_event (EXOSIP_CALL_CLOSED, jc, jd, transaction);
  eXosip_update(); /* AMD 30/09/05 */

  osip_transaction_add_event (transaction, evt_answer);
  __eXosip_wakeup ();
}

static void
eXosip_process_refer (eXosip_call_t * jc, eXosip_dialog_t * jd,
                      osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_header_t *referto_head = NULL;
  osip_contact_t *referto;
  int i;

  /* check if the refer is valid */
  osip_message_header_get_byname (evt->sip, "refer-to", 0, &referto_head);
  if (referto_head == NULL || referto_head->hvalue == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400,
                                  "Missing Refer-To header",
                                  "Missing Refer-To header", __LINE__);
      return;
    }
  /* check if refer-to is well-formed */
  osip_contact_init (&referto);
  i = osip_contact_parse (referto, referto_head->hvalue);
  if (i != 0)
    {
      osip_contact_free (referto);
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400,
                                  "Non valid Refer-To header",
                                  "Non valid Refer-To header", __LINE__);
      return;
    }

  osip_contact_free (referto);

  /* check policy so we can decline immediatly the refer */

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  osip_list_add (jd->d_inc_trs, transaction, 0);
  __eXosip_wakeup ();
}

static void
eXosip_process_notify_for_refer (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                 osip_transaction_t * transaction,
                                 osip_event_t * evt)
{
  osip_event_t *evt_answer;
  osip_message_t *answer;
  int i;
  osip_transaction_t *ref;
  osip_header_t *event_hdr;
  osip_header_t *sub_state;
  osip_content_type_t *ctype;
  osip_body_t *body = NULL;

  /* get the event type and return "489 Bad Event". */
  osip_message_header_get_byname (evt->sip, "event", 0, &event_hdr);
  if (event_hdr == NULL || event_hdr->hvalue == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400,
                                  "Missing Event header in Notify",
                                  "Missing Event header in Notify", __LINE__);
      return;
    }
  if (NULL==strstr(event_hdr->hvalue, "refer"))
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 501,
                                  "Unsupported Event header",
                                  "Unsupported Event header in Notify", __LINE__);
      return;
    }
  osip_message_header_get_byname (evt->sip, "subscription-state", 0, &sub_state);
  if (sub_state == NULL || sub_state->hvalue == NULL)
    {
#ifndef CISCO_BUG
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400, "Missing Header",
                                  "Missing subscription-state Header", __LINE__);
      return;
#else
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_WARNING, NULL,
                   "eXosip: Missing subscription-state Header (cisco 7960 bug)\n"));
#endif
    }

  ctype = osip_message_get_content_type (evt->sip);
  if (ctype == NULL || ctype->type == NULL || ctype->subtype == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400, "Missing Header",
                                  "Missing Content-Type Header", __LINE__);
      return;
    }
  if (0 != osip_strcasecmp (ctype->type, "message")
      || 0 != osip_strcasecmp (ctype->subtype, "sipfrag"))
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 501,
                                  "Unsupported body type",
                                  "Unsupported body type", __LINE__);
      return;
    }

  osip_message_get_body (evt->sip, 0, &body);
  if (body == NULL || body->body == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400, "Missing Body",
                                  "Missing Body", __LINE__);
      return;
    }

#if 0
  report_call_event (EXOSIP_CALL_REFER_STATUS, jc, jd, transaction);
#endif

  /* check if a refer was sent previously! */
  ref = eXosip_find_last_out_transaction (jc, jd, "REFER");
  if (ref == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 481, NULL,
                                  "No associated refer", __LINE__);
      return;
    }

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  /* for now, send default response of 200ok.  eventually, application should
     be deciding how to answer NOTIFY messages */
  i = _eXosip_build_response_default (&answer, jd->d_dialog, 200, evt->sip);
  if (i != 0)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      return;
    }

  i = complete_answer_that_establish_a_dialog (answer, evt->sip);

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = transaction->transactionid;

  osip_list_add (jd->d_inc_trs, transaction, 0);

  osip_transaction_add_event (transaction, evt_answer);
  __eXosip_wakeup ();
}

static void
eXosip_process_ack (eXosip_call_t * jc, eXosip_dialog_t * jd, osip_event_t * evt)
{
  /* TODO: We should find the matching transaction for this ACK
     and also add the ACK in the event. */
  eXosip_event_t *je;
  int i;

  je = eXosip_event_init_for_call (EXOSIP_CALL_ACK, jc, jd, NULL);
  if (je!=NULL)
    {
      i = osip_message_clone (evt->sip, &je->ack);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                  "failed to clone ACK for event\n"));
        }
      else
	report_event (je, NULL);
    }

  osip_event_free (evt);
}

static void
eXosip_process_prack (eXosip_call_t * jc, eXosip_dialog_t * jd,
                      osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_event_t *evt_answer;
  osip_message_t *answer;
  int i;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  i = _eXosip_build_response_default (&answer, jd->d_dialog, 200, evt->sip);
  if (i != 0)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      return;
    }

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = transaction->transactionid;

  osip_list_add (jd->d_inc_trs, transaction, 0);

  osip_transaction_add_event (transaction, evt_answer);
  __eXosip_wakeup ();
}

static int
cancel_match_invite (osip_transaction_t * invite, osip_message_t * cancel)
{
  osip_generic_param_t *br;
  osip_generic_param_t *br2;
  osip_via_t *via;

  osip_via_param_get_byname (invite->topvia, "branch", &br);
  via = osip_list_get (cancel->vias, 0);
  if (via == NULL)
    return -1;                  /* request without via??? */
  osip_via_param_get_byname (via, "branch", &br2);
  if (br != NULL && br2 == NULL)
    return -1;
  if (br2 != NULL && br == NULL)
    return -1;
  if (br2 != NULL && br != NULL)        /* compliant UA  :) */
    {
      if (br->gvalue != NULL && br2->gvalue != NULL &&
          0 == strcmp (br->gvalue, br2->gvalue))
        return 0;
      return -1;
    }
  /* old backward compatibility mechanism */
  if (0 != osip_call_id_match (invite->callid, cancel->call_id))
    return -1;
  if (0 != osip_to_tag_match (invite->to, cancel->to))
    return -1;
  if (0 != osip_from_tag_match (invite->from, cancel->from))
    return -1;
  if (0 != osip_via_match (invite->topvia, via))
    return -1;
  return 0;
}

static void
eXosip_process_cancel (osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_transaction_t *tr;
  osip_event_t *evt_answer;
  osip_message_t *answer;
  int i;

  eXosip_call_t *jc;
  eXosip_dialog_t *jd;

  tr = NULL;
  jd = NULL;
  /* first, look for a Dialog in the map of element */
  for (jc = eXosip.j_calls; jc != NULL; jc = jc->next)
    {
      if (jc->c_inc_tr != NULL)
        {
          i = cancel_match_invite (jc->c_inc_tr, evt->sip);
          if (i == 0)
            {
              tr = jc->c_inc_tr;
	      /* fixed */
	      if (jc->c_dialogs!=NULL)
		jd = jc->c_dialogs;
              break;
            }
        }
      tr = NULL;
      for (jd = jc->c_dialogs; jd != NULL; jd = jd->next)
        {
          int pos = 0;

          while (!osip_list_eol (jd->d_inc_trs, pos))
            {
              tr = osip_list_get (jd->d_inc_trs, pos);
              i = cancel_match_invite (tr, evt->sip);
              if (i == 0)
                break;
              tr = NULL;
              pos++;
            }
        }
      if (jd != NULL)
        break;                  /* tr has just been found! */
    }

  if (tr == NULL)               /* we didn't found the transaction to cancel */
    {
      i = _eXosip_build_response_default (&answer, NULL, 481, evt->sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot cancel transaction.\n"));
          osip_list_add (eXosip.j_transactions, tr, 0);
          osip_transaction_set_your_instance (tr, NULL);
          return;
        }
      osip_message_set_content_length (answer, "0");
      evt_answer = osip_new_outgoing_sipmessage (answer);
      evt_answer->transactionid = transaction->transactionid;
      osip_transaction_add_event (transaction, evt_answer);

      osip_list_add (eXosip.j_transactions, transaction, 0);
      osip_transaction_set_your_instance (transaction, NULL);
      __eXosip_wakeup ();
      return;
    }

  if (tr->state == IST_TERMINATED || tr->state == IST_CONFIRMED
      || tr->state == IST_COMPLETED)
    {
      /* I can't find the status code in the rfc?
         (I read I must answer 200? wich I found strange)
         I probably misunderstood it... and prefer to send 481
         as the transaction has been answered. */
      if (jd == NULL)
        i = _eXosip_build_response_default (&answer, NULL, 481, evt->sip);
      else
        i = _eXosip_build_response_default (&answer, jd->d_dialog, 481, evt->sip);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: cannot cancel transaction.\n"));
          osip_list_add (eXosip.j_transactions, tr, 0);
          osip_transaction_set_your_instance (tr, NULL);
          return;
        }
      osip_message_set_content_length (answer, "0");
      evt_answer = osip_new_outgoing_sipmessage (answer);
      evt_answer->transactionid = transaction->transactionid;
      osip_transaction_add_event (transaction, evt_answer);

      if (jd != NULL)
        osip_list_add (jd->d_inc_trs, transaction, 0);
      else
        osip_list_add (eXosip.j_transactions, transaction, 0);
      osip_transaction_set_your_instance (transaction, NULL);
      __eXosip_wakeup ();

      return;
    }

  {
    if (jd == NULL)
      i = _eXosip_build_response_default (&answer, NULL, 200, evt->sip);
    else
      i = _eXosip_build_response_default (&answer, jd->d_dialog, 200, evt->sip);
    if (i != 0)
      {
        OSIP_TRACE (osip_trace
                    (__FILE__, __LINE__, OSIP_ERROR, NULL,
                     "eXosip: cannot cancel transaction.\n"));
        osip_list_add (eXosip.j_transactions, tr, 0);
        osip_transaction_set_your_instance (tr, NULL);
        return;
      }
    osip_message_set_content_length (answer, "0");
    evt_answer = osip_new_outgoing_sipmessage (answer);
    evt_answer->transactionid = transaction->transactionid;
    osip_transaction_add_event (transaction, evt_answer);
    __eXosip_wakeup ();

    if (jd != NULL)
      osip_list_add (jd->d_inc_trs, transaction, 0);
    else
      osip_list_add (eXosip.j_transactions, transaction, 0);
    osip_transaction_set_your_instance (transaction, NULL);

    /* answer transaction to cancel */
    if (jd == NULL)
      i = _eXosip_build_response_default (&answer, NULL, 487, tr->orig_request);
    else
      i = _eXosip_build_response_default (&answer, jd->d_dialog, 487,
                                          tr->orig_request);
    if (i != 0)
      {
        OSIP_TRACE (osip_trace
                    (__FILE__, __LINE__, OSIP_ERROR, NULL,
                     "eXosip: cannot cancel transaction.\n"));
        osip_list_add (eXosip.j_transactions, tr, 0);
        osip_transaction_set_your_instance (tr, NULL);
        return;
      }
    osip_message_set_content_length (answer, "0");
    evt_answer = osip_new_outgoing_sipmessage (answer);
    evt_answer->transactionid = tr->transactionid;
    osip_transaction_add_event (tr, evt_answer);
    __eXosip_wakeup ();
  }
}

static osip_event_t *
eXosip_process_reinvite (eXosip_call_t * jc, eXosip_dialog_t * jd,
                         osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_message_t *answer;
  osip_event_t *sipevent;
  int i;

  i = _eXosip_build_response_default (&answer, jd->d_dialog, 100, evt->sip);
  if (i != 0)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 500,
                                  "Internal SIP Error",
                                  "Failed to build Answer for INVITE within call",
                                  __LINE__);
      return NULL;
    }

  complete_answer_that_establish_a_dialog (answer, evt->sip);

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  sipevent = osip_new_outgoing_sipmessage (answer);
  sipevent->transactionid = transaction->transactionid;

  osip_list_add (jd->d_inc_trs, transaction, 0);

  osip_ist_execute (eXosip.j_osip);

  report_call_event (EXOSIP_CALL_REINVITE, jc, jd, transaction);
  return sipevent;
}

static void
eXosip_process_new_options (osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_list_add (eXosip.j_transactions, transaction, 0);
  __eXosip_wakeup ();           /* needed? */
}

static void
eXosip_process_new_invite (osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_event_t *evt_answer;
  int i;
  eXosip_call_t *jc;
  eXosip_dialog_t *jd;
  osip_message_t *answer;

  eXosip_call_init (&jc);

  ADD_ELEMENT (eXosip.j_calls, jc);

  i = _eXosip_build_response_default (&answer, NULL, 101, evt->sip);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot create dialog."));
      osip_list_add (eXosip.j_transactions, transaction, 0);
      osip_transaction_set_your_instance (transaction, NULL);
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                              "ERROR: Could not create response for invite\n"));
      return;
    }
  osip_message_set_content_length (answer, "0");
  i = complete_answer_that_establish_a_dialog (answer, evt->sip);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot complete answer!\n"));
      osip_list_add (eXosip.j_transactions, transaction, 0);
      osip_transaction_set_your_instance (transaction, NULL);
      osip_message_free (answer);
      return;
    }

  i = eXosip_dialog_init_as_uas (&jd, evt->sip, answer);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot create dialog!\n"));
      osip_list_add (eXosip.j_transactions, transaction, 0);
      osip_transaction_set_your_instance (transaction, NULL);
      osip_message_free (answer);
      return;
    }
  ADD_ELEMENT (jc->c_dialogs, jd);

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = transaction->transactionid;

  eXosip_update ();
  jc->c_inc_tr = transaction;
  osip_transaction_add_event (transaction, evt_answer);

  /* be sure the invite will be processed
     before any API call on this dialog */
  osip_ist_execute (eXosip.j_osip);

  if (transaction->orig_request != NULL)
    {
      report_call_event (EXOSIP_CALL_INVITE, jc, jd, transaction);
    }

  __eXosip_wakeup ();

}

static int
eXosip_event_package_is_supported (osip_transaction_t * transaction,
                                   osip_event_t * evt)
{
  osip_header_t *event_hdr;
  int code;

  /* get the event type and return "489 Bad Event". */
  osip_message_header_get_byname (evt->sip, "event", 0, &event_hdr);
  if (event_hdr == NULL || event_hdr->hvalue == NULL)
    {
#ifdef SUPPORT_MSN
      /* msn don't show any event header */
      code = 200;               /* Bad Request... anyway... */
#else
      code = 400;               /* Bad Request */
#endif
  } else if (0 != osip_strcasecmp (event_hdr->hvalue, "presence"))
    code = 489;
  else
    code = 200;
  if (code != 200)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (NULL, transaction, evt, code, NULL, NULL,
                                  __LINE__);
      return 0;
    }
  return -1;
}

static void
eXosip_process_new_subscribe (osip_transaction_t * transaction, osip_event_t * evt)
{
  osip_event_t *evt_answer;
  eXosip_notify_t *jn;
  eXosip_dialog_t *jd;
  osip_message_t *answer;
  int i;

  eXosip_notify_init (&jn, evt->sip);
  _eXosip_notify_set_refresh_interval (jn, evt->sip);

  i = _eXosip_build_response_default (&answer, NULL, 101, evt->sip);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                              "ERROR: Could not create response for invite\n"));
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_notify_free (jn);
      return;
    }
  i = complete_answer_that_establish_a_dialog (answer, evt->sip);
  if (i != 0)
    {
      osip_message_free (answer);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot complete answer!\n"));
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_notify_free (jn);
      return;
    }

  i = eXosip_dialog_init_as_uas (&jd, evt->sip, answer);
  if (i != 0)
    {
      osip_message_free (answer);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot create dialog!\n"));
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_notify_free (jn);
      return;
    }
  ADD_ELEMENT (jn->n_dialogs, jd);

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, jd, NULL, jn));

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = transaction->transactionid;
  osip_transaction_add_event (transaction, evt_answer);

  ADD_ELEMENT (eXosip.j_notifies, jn);
  __eXosip_wakeup ();

  jn->n_inc_tr = transaction;

  eXosip_update ();
  __eXosip_wakeup ();
}

static void
eXosip_process_subscribe_within_call (eXosip_notify_t * jn,
                                      eXosip_dialog_t * jd,
                                      osip_transaction_t * transaction,
                                      osip_event_t * evt)
{
  _eXosip_notify_set_refresh_interval (jn, evt->sip);
  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, jd, NULL, jn));

  /* if subscribe request contains expires="0", close the subscription */
  {
    int now = time (NULL);

    if (jn->n_ss_expires - now <= 0)
      {
        jn->n_ss_status = EXOSIP_SUBCRSTATE_TERMINATED;
        jn->n_ss_reason = TIMEOUT;
      }
  }

  osip_list_add (jd->d_inc_trs, transaction, 0);
  __eXosip_wakeup ();
  return;
}

static void
eXosip_process_notify_within_dialog (eXosip_subscribe_t * js,
                                     eXosip_dialog_t * jd,
                                     osip_transaction_t * transaction,
                                     osip_event_t * evt)
{
  osip_message_t *answer;
  osip_event_t *sipevent;
  osip_header_t *sub_state;

#ifdef SUPPORT_MSN
  osip_header_t *expires;
#endif
  int i;

  if (jd == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 500,
                                  "Internal SIP Error",
                                  "No dialog for this NOTIFY", __LINE__);
      return;
    }

  /* if subscription-state has a reason state set to terminated,
     we close the dialog */
#ifndef SUPPORT_MSN
  osip_message_header_get_byname (evt->sip, "subscription-state", 0, &sub_state);
  if (sub_state == NULL || sub_state->hvalue == NULL)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 400, NULL, NULL, __LINE__);
      return;
    }
#endif

  i = _eXosip_build_response_default (&answer, jd->d_dialog, 200, evt->sip);
  if (i != 0)
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 500,
                                  "Internal SIP Error",
                                  "Failed to build Answer for NOTIFY", __LINE__);
      return;
    }
#ifdef SUPPORT_MSN
  osip_message_header_get_byname (evt->sip, "expires", 0, &expires);
  if (expires != NULL && expires->hvalue != NULL
      && 0 == osip_strcasecmp (expires->hvalue, "0"))
    {
      /* delete the dialog! */
      js->s_ss_status = EXOSIP_SUBCRSTATE_TERMINATED;
      {
        eXosip_event_t *je;

        je = eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_NOTIFY, js, jd);
        eXosip_event_add (je);
      }

      sipevent = osip_new_outgoing_sipmessage (answer);
      sipevent->transactionid = transaction->transactionid;
      osip_transaction_add_event (transaction, sipevent);

      osip_list_add (eXosip.j_transactions, transaction, 0);

      REMOVE_ELEMENT (eXosip.j_subscribes, js);
      eXosip_subscribe_free (js);
      __eXosip_wakeup ();

      return;
  } else
    {
      osip_transaction_set_your_instance (transaction,
                                          __eXosip_new_jinfo (NULL, jd, js, NULL));
      js->s_ss_status = EXOSIP_SUBCRSTATE_ACTIVE;
    }
#else
  /* modify the status of user */
  if (0 == osip_strncasecmp (sub_state->hvalue, "active", 6))
    {
      js->s_ss_status = EXOSIP_SUBCRSTATE_ACTIVE;
  } else if (0 == osip_strncasecmp (sub_state->hvalue, "pending", 7))
    {
      js->s_ss_status = EXOSIP_SUBCRSTATE_PENDING;
    }

  if (0 == osip_strncasecmp (sub_state->hvalue, "terminated", 10))
    {
      /* delete the dialog! */
      js->s_ss_status = EXOSIP_SUBCRSTATE_TERMINATED;

      {
        eXosip_event_t *je;

        je =
          eXosip_event_init_for_subscribe (EXOSIP_SUBSCRIPTION_NOTIFY, js, jd,
                                           transaction);
        eXosip_event_add (je);
      }

      sipevent = osip_new_outgoing_sipmessage (answer);
      sipevent->transactionid = transaction->transactionid;
      osip_transaction_add_event (transaction, sipevent);

      osip_list_add (eXosip.j_transactions, transaction, 0);

      REMOVE_ELEMENT (eXosip.j_subscribes, js);
      eXosip_subscribe_free (js);
      __eXosip_wakeup ();
      return;
  } else
    {
      osip_transaction_set_your_instance (transaction,
                                          __eXosip_new_jinfo (NULL, jd, js, NULL));
    }
#endif

  osip_list_add (jd->d_inc_trs, transaction, 0);

  sipevent = osip_new_outgoing_sipmessage (answer);
  sipevent->transactionid = transaction->transactionid;
  osip_transaction_add_event (transaction, sipevent);

  __eXosip_wakeup ();
  return;
}

static int
eXosip_match_notify_for_subscribe (eXosip_subscribe_t * js,
                                   osip_message_t * notify)
{
  osip_transaction_t *out_sub;

  if (js == NULL)
    return -1;
  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                          "Trying to match notify with subscribe\n"));

  out_sub = eXosip_find_last_out_subscribe (js, NULL);
  if (out_sub == NULL || out_sub->orig_request == NULL)
    return -1;
  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                          "subscribe transaction found\n"));

  /* some checks to avoid crashing on bad requests */
  if (notify == NULL || notify->cseq == NULL
      || notify->cseq->method == NULL || notify->to == NULL)
    return -1;

  if (0 != osip_call_id_match (out_sub->callid, notify->call_id))
    return -1;

  {
    /* The From tag of outgoing request must match
       the To tag of incoming notify:
     */
    osip_generic_param_t *tag_from;
    osip_generic_param_t *tag_to;

    osip_from_param_get_byname (out_sub->from, "tag", &tag_from);
    osip_from_param_get_byname (notify->to, "tag", &tag_to);
    if (tag_to == NULL || tag_to->gvalue == NULL)
      {
        OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                "Uncompliant user agent: no tag in from of outgoing request\n"));
        return -1;
      }
    if (tag_from == NULL || tag_to->gvalue == NULL)
      {
        OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                "Uncompliant user agent: no tag in to of incoming request\n"));
        return -1;
      }

    if (0 != strcmp (tag_from->gvalue, tag_to->gvalue))
      return -1;
  }

  return 0;
}

static void
eXosip_process_message_outside_of_dialog (osip_transaction_t * transaction,
                                          osip_event_t * evt)
{
  osip_list_add (eXosip.j_transactions, transaction, 0);
  __eXosip_wakeup ();           /* needed? */
  return;
}

static void
eXosip_process_refer_outside_of_dialog (osip_transaction_t * transaction,
					osip_event_t * evt)
{
  osip_list_add (eXosip.j_transactions, transaction, 0);
  __eXosip_wakeup ();           /* needed? */
  return;
}

static void
eXosip_process_message_within_dialog (eXosip_call_t * jc,
				      eXosip_dialog_t * jd,
				      osip_transaction_t * transaction,
				      osip_event_t * evt)
{
  osip_list_add (jd->d_inc_trs, transaction, 0);
  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (jc, jd, NULL, NULL));
  __eXosip_wakeup ();
  return;
}


static void
eXosip_process_newrequest (osip_event_t * evt, int socket)
{
  osip_transaction_t *transaction;
  osip_event_t *evt_answer;
  osip_message_t *answer;
  int i;
  int ctx_type;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  eXosip_dialog_t *jd;

  if (MSG_IS_INVITE (evt->sip))
    {
      ctx_type = IST;
  } else if (MSG_IS_ACK (evt->sip))
    {                           /* this should be a ACK for 2xx (but could be a late ACK!) */
      ctx_type = -1;
  } else if (MSG_IS_REQUEST (evt->sip))
    {
      ctx_type = NIST;
  } else
    {                           /* We should handle late response and 200 OK before coming here. */
      ctx_type = -1;
      osip_event_free (evt);
      return;
    }

  transaction = NULL;
  if (ctx_type != -1)
    {
      i = osip_transaction_init (&transaction,
                                 (osip_fsm_type_t) ctx_type,
                                 eXosip.j_osip, evt->sip);
      if (i != 0)
        {
          osip_event_free (evt);
          return;
        }

      osip_transaction_set_in_socket(transaction, socket);
      osip_transaction_set_out_socket(transaction, socket);

      evt->transactionid = transaction->transactionid;
      osip_transaction_set_your_instance (transaction, NULL);

      osip_transaction_add_event (transaction, evt);
      if (ctx_type == IST)
        {
          i = _eXosip_build_response_default (&answer, NULL, 100, evt->sip);
          if (i != 0)
            {
              __eXosip_delete_jinfo (transaction);
              osip_transaction_free (transaction);
              return;
            }

          osip_message_set_content_length (answer, "0");
          /*  send message to transaction layer */

          evt_answer = osip_new_outgoing_sipmessage (answer);
          evt_answer->transactionid = transaction->transactionid;

          /* add the REQUEST & the 100 Trying */
          osip_transaction_add_event (transaction, evt_answer);
          __eXosip_wakeup ();
        }
    }

  if (MSG_IS_CANCEL (evt->sip))
    {
      /* special handling for CANCEL */
      /* in the new spec, if the CANCEL has a Via branch, then it
         is the same as the one in the original INVITE */
      eXosip_process_cancel (transaction, evt);
      return;
    }

  jd = NULL;
  /* first, look for a Dialog in the map of element */
  for (jc = eXosip.j_calls; jc != NULL; jc = jc->next)
    {
      for (jd = jc->c_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)
            {
              if (osip_dialog_match_as_uas (jd->d_dialog, evt->sip) == 0)
                break;
            }
        }
      if (jd != NULL)
        break;
    }


  if (jd != NULL)
    {
      osip_transaction_t *old_trn;

      /* it can be:
         1: a new INVITE offer.
         2: a REFER request from one of the party.
         2: a BYE request from one of the party.
         3: a REQUEST with a wrong CSeq.
         4: a NOT-SUPPORTED method with a wrong CSeq.
       */

      if (!MSG_IS_BYE (evt->sip))
        {
          /* reject all requests for a closed dialog */
          old_trn = eXosip_find_last_inc_transaction (jc, jd, "BYE");
          if (old_trn == NULL)
            old_trn = eXosip_find_last_out_transaction (jc, jd, "BYE");

          if (old_trn != NULL)
            {
              osip_list_add (eXosip.j_transactions, transaction, 0);
              eXosip_send_default_answer (jd, transaction, evt, 481, NULL,
                                          NULL, __LINE__);
              return;
            }
        }

      if (MSG_IS_INVITE (evt->sip))
        {
          /* the previous transaction MUST be freed */
          old_trn = eXosip_find_last_inc_invite (jc, jd);

          if (old_trn != NULL && old_trn->state != IST_TERMINATED)
            {
              osip_list_add (eXosip.j_transactions, transaction, 0);
              eXosip_send_default_answer (jd, transaction, evt, 500,
                                          "Retry Later",
                                          "An INVITE is not terminated", __LINE__);
              return;
            }

          old_trn = eXosip_find_last_out_invite (jc, jd);
          if (old_trn != NULL && old_trn->state != ICT_TERMINATED)
            {
              osip_list_add (eXosip.j_transactions, transaction, 0);
              eXosip_send_default_answer (jd, transaction, evt, 491, NULL,
                                          NULL, __LINE__);
              return;
            }

          osip_dialog_update_osip_cseq_as_uas (jd->d_dialog, evt->sip);
          osip_dialog_update_route_set_as_uas (jd->d_dialog, evt->sip);

          eXosip_process_reinvite (jc, jd, transaction, evt);
      } else if (MSG_IS_BYE (evt->sip))
        {
          old_trn = eXosip_find_last_inc_transaction (jc, jd, "BYE");

          if (old_trn != NULL)  /* && old_trn->state!=NIST_TERMINATED) */
            {                   /* this situation should NEVER occur?? (we can't receive
                                   two different BYE for one call! */
              osip_list_add (eXosip.j_transactions, transaction, 0);
              eXosip_send_default_answer (jd, transaction, evt, 500,
                                          "Call Already Terminated",
                                          "A pending BYE has already terminate this call",
                                          __LINE__);
              return;
            }
          /* osip_transaction_free(old_trn); */
          eXosip_process_bye (jc, jd, transaction, evt);
      } else if (MSG_IS_ACK (evt->sip))
        {
          eXosip_process_ack (jc, jd, evt);
      } else if (MSG_IS_REFER (evt->sip))
        {
          eXosip_process_refer (jc, jd, transaction, evt);
      } else if (MSG_IS_OPTIONS (evt->sip))
        {
          eXosip_process_options (jc, jd, transaction, evt);
      } else if (MSG_IS_INFO (evt->sip))
        {
          eXosip_process_info (jc, jd, transaction, evt);
      } else if (MSG_IS_NOTIFY (evt->sip))
        {
          eXosip_process_notify_for_refer (jc, jd, transaction, evt);
      } else if (MSG_IS_PRACK (evt->sip))
        {
          eXosip_process_prack (jc, jd, transaction, evt);
      } else if (MSG_IS_MESSAGE (evt->sip))
        {
          eXosip_process_message_within_dialog (jc, jd, transaction, evt);
      } else if (MSG_IS_SUBSCRIBE (evt->sip))
        {
          osip_list_add (eXosip.j_transactions, transaction, 0);
          eXosip_send_default_answer (jd, transaction, evt, 489, NULL,
                                      "Bad Event", __LINE__);
      } else
        {
#if 0
          osip_list_add (eXosip.j_transactions, transaction, 0);
          eXosip_send_default_answer (jd, transaction, evt, 405, NULL,
                                      "Method Not Allowed", __LINE__);
#else
	  eXosip_process_message_within_dialog (jc, jd, transaction, evt);
#endif
        }
      return;
    }

  if (MSG_IS_ACK (evt->sip))
    {
      /* no transaction has been found for this ACK! */
      osip_event_free (evt);
      return;
    }

  if (MSG_IS_INFO (evt->sip))
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 481, NULL, NULL, __LINE__);
      return;                   /* fixed */
    }
  if (MSG_IS_OPTIONS (evt->sip))
    {
      eXosip_process_new_options (transaction, evt);
      return;
  } else if (MSG_IS_INVITE (evt->sip))
    {
      eXosip_process_new_invite (transaction, evt);
      return;
  } else if (MSG_IS_BYE (evt->sip))
    {
      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (jd, transaction, evt, 481, NULL, NULL, __LINE__);
      return;
    }

  js = NULL;
  /* first, look for a Dialog in the map of element */
  for (js = eXosip.j_subscribes; js != NULL; js = js->next)
    {
      for (jd = js->s_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)
            {
              if (osip_dialog_match_as_uas (jd->d_dialog, evt->sip) == 0)
                break;
            }
        }
      if (jd != NULL)
        break;
    }

  if (js != NULL)
    {
      /* dialog found */
      osip_transaction_t *old_trn;

      /* it can be:
         1: a new INVITE offer.
         2: a REFER request from one of the party.
         2: a BYE request from one of the party.
         3: a REQUEST with a wrong CSeq.
         4: a NOT-SUPPORTED method with a wrong CSeq.
       */
      if (MSG_IS_MESSAGE (evt->sip))
        {
          /* eXosip_process_imessage_within_subscribe_dialog(transaction, evt); */
          osip_list_add (eXosip.j_transactions, transaction, 0);
          eXosip_send_default_answer (jd, transaction, evt,
                                      SIP_NOT_IMPLEMENTED, NULL,
                                      "MESSAGEs within dialogs are not implemented.",
                                      __LINE__);
          return;
      } else if (MSG_IS_NOTIFY (evt->sip))
        {
          /* the previous transaction MUST be freed */
          old_trn = eXosip_find_last_inc_notify (js, jd);

          /* shouldn't we wait for the COMPLETED state? */
          if (old_trn != NULL && old_trn->state != NIST_TERMINATED)
            {
              /* retry later? */
              osip_list_add (eXosip.j_transactions, transaction, 0);
              eXosip_send_default_answer (jd, transaction, evt, 500,
                                          "Retry Later",
                                          "A pending NOTIFY is not terminated",
                                          __LINE__);
              return;
            }

          osip_dialog_update_osip_cseq_as_uas (jd->d_dialog, evt->sip);
          osip_dialog_update_route_set_as_uas (jd->d_dialog, evt->sip);

          eXosip_process_notify_within_dialog (js, jd, transaction, evt);
      } else
        {
          osip_list_add (eXosip.j_transactions, transaction, 0);
          eXosip_send_default_answer (jd, transaction, evt, 501, NULL,
                                      "Just Not Implemented", __LINE__);
        }
      return;
    }

  if (MSG_IS_NOTIFY (evt->sip))
    {
      /* let's try to check if the NOTIFY is related to an existing
         subscribe */
      js = NULL;
      /* first, look for a Dialog in the map of element */
      for (js = eXosip.j_subscribes; js != NULL; js = js->next)
        {
          if (eXosip_match_notify_for_subscribe (js, evt->sip) == 0)
            {
              i = eXosip_dialog_init_as_uac (&jd, evt->sip);
              if (i != 0)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_ERROR, NULL,
                               "eXosip: cannot establish a dialog\n"));
                  return;
                }

              /* update local cseq from subscribe request */
              if (js->s_out_tr != NULL && js->s_out_tr->cseq != NULL
                  && js->s_out_tr->cseq->number != NULL)
                {
                  jd->d_dialog->local_cseq = atoi (js->s_out_tr->cseq->number);
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO2, NULL,
                               "eXosip: local cseq has been updated\n"));
                }

              ADD_ELEMENT (js->s_dialogs, jd);
              eXosip_update ();

              eXosip_process_notify_within_dialog (js, jd, transaction, evt);
              return;
            }
        }

      osip_list_add (eXosip.j_transactions, transaction, 0);
      eXosip_send_default_answer (NULL, transaction, evt, 481, NULL, NULL,
                                  __LINE__);
      return;
    }

  jn = NULL;
  /* first, look for a Dialog in the map of element */
  for (jn = eXosip.j_notifies; jn != NULL; jn = jn->next)
    {
      for (jd = jn->n_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)
            {
              if (osip_dialog_match_as_uas (jd->d_dialog, evt->sip) == 0)
                break;
            }
        }
      if (jd != NULL)
        break;
    }

  if (jn != NULL)
    {
      /* dialog found */
      osip_transaction_t *old_trn;

      /* it can be:
         1: a new INVITE offer.
         2: a REFER request from one of the party.
         2: a BYE request from one of the party.
         3: a REQUEST with a wrong CSeq.
         4: a NOT-SUPPORTED method with a wrong CSeq.
       */
      if (MSG_IS_MESSAGE (evt->sip))
        {
          osip_list_add (eXosip.j_transactions, transaction, 0);
          eXosip_send_default_answer (jd, transaction, evt,
                                      SIP_NOT_IMPLEMENTED, NULL,
                                      "MESSAGEs within dialogs are not implemented.",
                                      __LINE__);
          return;
      } else if (MSG_IS_SUBSCRIBE (evt->sip))
        {
          /* the previous transaction MUST be freed */
          old_trn = eXosip_find_last_inc_subscribe (jn, jd);

          /* shouldn't we wait for the COMPLETED state? */
          if (old_trn != NULL && old_trn->state != NIST_TERMINATED
              && old_trn->state != NIST_COMPLETED)
            {
              /* retry later? */
              osip_list_add (eXosip.j_transactions, transaction, 0);
              eXosip_send_default_answer (jd, transaction, evt, 500,
                                          "Retry Later",
                                          "A SUBSCRIBE is not terminated",
                                          __LINE__);
              return;
            }

          osip_dialog_update_osip_cseq_as_uas (jd->d_dialog, evt->sip);
          osip_dialog_update_route_set_as_uas (jd->d_dialog, evt->sip);

          eXosip_process_subscribe_within_call (jn, jd, transaction, evt);
      } else
        {
          osip_list_add (eXosip.j_transactions, transaction, 0);
          eXosip_send_default_answer (jd, transaction, evt, 501, NULL, NULL,
                                      __LINE__);
        }
      return;
    }

  if (MSG_IS_MESSAGE (evt->sip))
    {
      eXosip_process_message_outside_of_dialog (transaction, evt);
      return;
    }

  if (MSG_IS_REFER (evt->sip))
    {
      eXosip_process_refer_outside_of_dialog (transaction, evt);
      return;
    }

  if (MSG_IS_SUBSCRIBE (evt->sip))
    {

      if (0 == eXosip_event_package_is_supported (transaction, evt))
        {
          return;
        }
      eXosip_process_new_subscribe (transaction, evt);
      return;
    }

  /* default answer */
  osip_list_add (eXosip.j_transactions, transaction, 0);
#if 0
  eXosip_send_default_answer (NULL, transaction, evt, 501, NULL, NULL, __LINE__);
#endif
}

static void
eXosip_process_response_out_of_transaction (osip_event_t * evt)
{
  osip_event_free (evt);
}

static int _eXosip_handle_incoming_message(char *buf, size_t len, int socket,
					   char *host, int port);

static int _eXosip_handle_incoming_message(char *buf, size_t len, int socket,
					   char *host, int port)
{
  osip_transaction_t *transaction = NULL;
  osip_event_t *sipevent;
  int i;

  sipevent = osip_parse (buf, len);
  transaction = NULL;
  if (sipevent != NULL && sipevent->sip != NULL)
    {
    }
  else
    {
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		   "Could not parse SIP message\n"));
      osip_event_free (sipevent);
      return -1;
    }

  OSIP_TRACE (osip_trace
	      (__FILE__, __LINE__, OSIP_INFO1, NULL,
	       "Message received from: %s:%i\n",
	       host, port));
  
  osip_message_fix_last_via_header (sipevent->sip,
				    host,
				    port);
  
  i = osip_find_transaction_and_add_event (eXosip.j_osip, sipevent);
  if (i != 0)
    {
      /* this event has no transaction, */
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_INFO1, NULL,
		   "This is a request\n", buf));
      eXosip_lock ();
      if (MSG_IS_REQUEST (sipevent->sip))
	eXosip_process_newrequest (sipevent, socket);
      else if (MSG_IS_RESPONSE (sipevent->sip))
	eXosip_process_response_out_of_transaction (sipevent);
      eXosip_unlock ();
    }
  else
    {
      /* handled by oSIP ! */
      return 0;
    }
  return 0;
}

#if defined (WIN32) || defined (_WIN32_WCE)
#define eXFD_SET(A, B)   FD_SET((unsigned int) A, B)
#else
#define eXFD_SET(A, B)   FD_SET(A, B)
#endif

/* if second==-1 && useconds==-1  -> wait for ever
   if max_message_nb<=0  -> infinite loop....  */
int
eXosip_read_message (int max_message_nb, int sec_max, int usec_max)
{
  fd_set osip_fdset;
  struct timeval tv;
  char *buf;

  tv.tv_sec = sec_max;
  tv.tv_usec = usec_max;

  buf = (char *) osip_malloc (SIP_MESSAGE_MAX_LENGTH * sizeof (char) + 1);
  while (max_message_nb != 0 && eXosip.j_stop_ua == 0)
    {
      int i;
      int max;
      int wakeup_socket = jpipe_get_read_descr (eXosip.j_socketctl);

      FD_ZERO (&osip_fdset);
      if (eXosip.net_interfaces[0].net_socket>0)
	{
	  eXFD_SET (eXosip.net_interfaces[0].net_socket, &osip_fdset);
	  max = eXosip.net_interfaces[0].net_socket;
	}
      if (eXosip.net_interfaces[1].net_socket>0)
	{
	  int pos;
	  struct eXosip_net *net = &eXosip.net_interfaces[1];
	  eXFD_SET (net->net_socket, &osip_fdset);
	  if (net->net_socket>max)
	    max = net->net_socket;

	  for (pos=0;pos<EXOSIP_MAX_SOCKETS;pos++)
	    {
	      if (net->net_socket_tab[pos].socket!=0)
		{
		  eXFD_SET (net->net_socket_tab[pos].socket, &osip_fdset);
		  if (net->net_socket_tab[pos].socket>max)
		    max = net->net_socket_tab[pos].socket;
		}
	    }
	}

      if (eXosip.net_interfaces[2].net_socket>0)
	{
	  eXFD_SET (eXosip.net_interfaces[2].net_socket, &osip_fdset);
	  if (eXosip.net_interfaces[2].net_socket>max)
	    max = eXosip.net_interfaces[2].net_socket;
	}


      eXFD_SET (wakeup_socket, &osip_fdset);
      if (wakeup_socket > max)
        max = wakeup_socket;

      if ((sec_max == -1) || (usec_max == -1))
        i = select (max + 1, &osip_fdset, NULL, NULL, NULL);
      else
        i = select (max + 1, &osip_fdset, NULL, NULL, &tv);

      if ((i == -1) && (errno == EINTR || errno == EAGAIN))
        continue;

      if ((i > 0) && FD_ISSET (wakeup_socket, &osip_fdset))
        {
          char buf2[500];

          jpipe_read (eXosip.j_socketctl, buf2, 499);
        }

      if (0 == i || eXosip.j_stop_ua != 0)
        {
	} else if (-1 == i)
	  {
	    osip_free (buf);
	    return -2;            /* error */
	  } else if (FD_ISSET (eXosip.net_interfaces[1].net_socket, &osip_fdset))
	    {
	      /* accept incoming connection */
	      char src6host[NI_MAXHOST];
	      int recvport = 0;
	      struct sockaddr_storage sa;
	      int sock;
	      int i;
	      int pos;
#ifdef __linux
	      socklen_t slen;
#else
	      int slen;
#endif
	      if (ipv6_enable == 0)
		slen = sizeof (struct sockaddr_in);
	      else
		slen = sizeof (struct sockaddr_in6);

	      for (pos=0; pos<EXOSIP_MAX_SOCKETS; pos++)
		{
		  if (eXosip.net_interfaces[1].net_socket_tab[pos].socket==0)
		    break;
		}
	      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
				      "creating TCP socket at index: %i\n", pos));
	      sock = accept(eXosip.net_interfaces[1].net_socket, (struct sockaddr *) &sa, 
			 &slen);
	      if (sock<0)
		{
		  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					  "Error accepting TCP socket\n"));
		  break;
		}
	      eXosip.net_interfaces[1].net_socket_tab[pos].socket = sock;
	      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
				      "New TCP connection accepted\n"));

	      memset(src6host, 0, sizeof(src6host));
	      
	      if (ipv6_enable == 0)
		recvport = ntohs (((struct sockaddr_in*)&sa)->sin_port);
	      else
		recvport = ntohs (((struct sockaddr_in6*)&sa)->sin6_port);
	      
	      i = getnameinfo((struct sockaddr*)&sa, slen,
			      src6host, NI_MAXHOST,
			      NULL, 0,
			      NI_NUMERICHOST);
	      
	      if (i!=0)
		{
		  OSIP_TRACE (osip_trace
			      (__FILE__, __LINE__, OSIP_ERROR, NULL,
			       "Message received from: %s:%i Error with getnameinfo\n",
			       src6host, recvport));
		}
	      else           
		{
		  OSIP_TRACE (osip_trace
			      (__FILE__, __LINE__, OSIP_INFO1, NULL,
			       "Message received from: %s:%i\n",
			       src6host, recvport));
		  osip_strncpy(eXosip.net_interfaces[1].net_socket_tab[pos].remote_ip,
			       src6host,
			       sizeof(eXosip.net_interfaces[1].net_socket_tab[pos].remote_ip));
		  eXosip.net_interfaces[1].net_socket_tab[pos].remote_port = recvport;
		}

	  } else if (FD_ISSET (eXosip.net_interfaces[0].net_socket, &osip_fdset))
	    {
	      /*AMDstruct sockaddr_in sa; */
	      struct sockaddr_storage sa;

#ifdef __linux
	      socklen_t slen;
#else
	      int slen;
#endif
	      if (ipv6_enable == 0)
		slen = sizeof (struct sockaddr_in);
	      else
		slen = sizeof (struct sockaddr_in6);

          i = _eXosip_recvfrom (eXosip.net_interfaces[0].net_socket, buf, SIP_MESSAGE_MAX_LENGTH, 0,
			    (struct sockaddr *) &sa, &slen);

         if (i > 5)            /* we expect at least one byte, otherwise there's no doubt that it is not a sip message ! */
		{
		  /* Message might not end with a "\0" but we know the number of */
		  /* char received! */
		  osip_transaction_t *transaction = NULL;
		  osip_event_t *sipevent;

		  osip_strncpy (buf + i, "\0", 1);
		  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					  "Received message: \n%s\n", buf));
#ifdef WIN32
		  if (strlen (buf) > 412)
		    {
		      OSIP_TRACE (osip_trace
				  (__FILE__, __LINE__, OSIP_INFO1, NULL,
				   "Message suite: \n%s\n", buf + 412));
		    }
#endif

		  sipevent = osip_parse (buf, i);
		  transaction = NULL;
		  if (sipevent != NULL && sipevent->sip != NULL)
          {
            if (!eXosip.http_port)
            {
		      char src6host[NI_MAXHOST];
		      char src6buf[NI_MAXSERV];
		      int recvport = 0;
		      memset(src6host, 0, sizeof(src6host));
		      memset(src6buf, 0, sizeof(src6buf));
		  
		      if (ipv6_enable == 0)
			recvport = ntohs (((struct sockaddr_in*)&sa)->sin_port);
		      else
			recvport = ntohs (((struct sockaddr_in6*)&sa)->sin6_port);

		      i = getnameinfo((struct sockaddr*)&sa, slen,
				      src6host, NI_MAXHOST,
				      NULL, 0,
				      NI_NUMERICHOST);

		      if (i!=0)
			{
			  OSIP_TRACE (osip_trace
				      (__FILE__, __LINE__, OSIP_ERROR, NULL,
				       "Message received from: %s:%i (serv=%s) Error with getnameinfo\n",
				       src6host, recvport, src6buf));
			}
		      else           
			{
			  OSIP_TRACE (osip_trace
				      (__FILE__, __LINE__, OSIP_INFO1, NULL,
				       "Message received from: %s:%i (serv=%s)\n",
				       src6host, recvport, src6buf));          
			}

		      OSIP_TRACE (osip_trace
				  (__FILE__, __LINE__, OSIP_INFO1, NULL,
				   "Message received from: %s:%i (serv=%s)\n",
				   src6host, recvport, src6buf));

		      osip_message_fix_last_via_header (sipevent->sip,
							src6host,
							recvport);
            }
		      i =
			osip_find_transaction_and_add_event (eXosip.j_osip, sipevent);
		      if (i != 0)
			{
			  /* this event has no transaction, */
			  OSIP_TRACE (osip_trace
				      (__FILE__, __LINE__, OSIP_INFO1, NULL,
				       "This is a request\n", buf));
			  eXosip_lock ();
			  if (MSG_IS_REQUEST (sipevent->sip))
			    eXosip_process_newrequest (sipevent, 0);
			  else if (MSG_IS_RESPONSE (sipevent->sip))
			    eXosip_process_response_out_of_transaction (sipevent);
			  eXosip_unlock ();
			} else
			  {
			    /* handled by oSIP ! */
			  }
		    } else
		      {
			OSIP_TRACE (osip_trace
				    (__FILE__, __LINE__, OSIP_ERROR, NULL,
				     "Could not parse SIP message\n"));
			osip_event_free (sipevent);
		      }
		} else if (i < 0)
		  {
		    OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
					    "Could not read socket\n"));
		  } else
		    {
		      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					      "Dummy SIP message received\n"));
		    }
	    }
      else
	{
	  /* loop over all TCP socket */
	  int pos = 0;
	  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
				  "TCP DATA ready?\n"));
	  for (pos=0; pos<EXOSIP_MAX_SOCKETS; pos++)
	    {
	      if (eXosip.net_interfaces[1].net_socket_tab[pos].socket>0
		  && FD_ISSET (eXosip.net_interfaces[1].net_socket_tab[pos].socket, &osip_fdset))
		{
		  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					  "TCP DATA ready! message received\n"));
		  i = recv (eXosip.net_interfaces[1].net_socket_tab[pos].socket,
			    buf, SIP_MESSAGE_MAX_LENGTH, 0);
		  if (i > 5)
		    {
		      osip_strncpy (buf + i, "\0", 1);
		      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					      "Received TCP message: \n%s\n", buf));
#ifdef WIN32
		      if (strlen (buf) > 412)
			{
			  OSIP_TRACE (osip_trace
				      (__FILE__, __LINE__, OSIP_INFO1, NULL,
				       "Message suite: \n%s\n", buf + 412));
			}
#endif
		      _eXosip_handle_incoming_message(buf, i,
						      eXosip.net_interfaces[1].net_socket_tab[pos].socket,
						      eXosip.net_interfaces[1].net_socket_tab[pos].remote_ip,
						      eXosip.net_interfaces[1].net_socket_tab[pos].remote_port);
		    }
		  else if (i < 0)
		    {
		      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
					      "Could not read socket - close it\n"));
		      close(eXosip.net_interfaces[1].net_socket_tab[pos].socket);
		      memset(&(eXosip.net_interfaces[1].net_socket_tab[pos]),
			     0, sizeof(struct eXosip_socket));
		    }
		  else if (i==0)
		    {
		      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					      "End of stream (read 0 byte from %s:%i)\n", eXosip.net_interfaces[1].net_socket_tab[pos].remote_ip, eXosip.net_interfaces[1].net_socket_tab[pos].remote_port));
		      close(eXosip.net_interfaces[1].net_socket_tab[pos].socket);
		      memset(&(eXosip.net_interfaces[1].net_socket_tab[pos]),
			     0, sizeof(struct eXosip_socket));
		    }
		  else
		    {
		      /* we expect at least one byte, otherwise there's no doubt that it is not a sip message ! */
		      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
					      "Dummy SIP message received (size=%i)\n", i));
		    }
		}
	    }
	}


      max_message_nb--;
    }
  osip_free (buf);
  return 0;
}


static int
eXosip_pendingosip_transaction_exist (eXosip_call_t * jc, eXosip_dialog_t * jd)
{
  osip_transaction_t *tr;
  int now = time (NULL);

  tr = eXosip_find_last_inc_transaction (jc, jd, "BYE");
  if (tr != NULL && tr->state != NIST_TERMINATED)
    {                           /* Don't want to wait forever on broken transaction!! */
      if (tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
        {
          /* remove the transaction from oSIP: */
          osip_remove_transaction (eXosip.j_osip, tr);
          eXosip_remove_transaction_from_call (tr, jc);
	  osip_list_add (eXosip.j_transactions, tr, 0);
      } else
        return 0;
    }

  tr = eXosip_find_last_out_transaction (jc, jd, "BYE");
  if (tr != NULL && tr->state != NICT_TERMINATED)
    {                           /* Don't want to wait forever on broken transaction!! */
      if (tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
        {
          /* remove the transaction from oSIP: */
          osip_remove_transaction (eXosip.j_osip, tr);
          eXosip_remove_transaction_from_call (tr, jc);
	  osip_list_add (eXosip.j_transactions, tr, 0);
      } else
        return 0;
    }

  tr = eXosip_find_last_inc_invite (jc, jd);
  if (tr != NULL && tr->state != IST_TERMINATED)
    {                           /* Don't want to wait forever on broken transaction!! */
      if (tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
        {
          /* remove the transaction from oSIP: */
          /* osip_remove_transaction(eXosip.j_osip, tr);
             eXosip_remove_transaction_from_call(tr, jc);
             osip_transaction_free(tr); */
      } else
        return 0;
    }

  tr = eXosip_find_last_out_invite (jc, jd);
  if (tr != NULL && tr->state != ICT_TERMINATED)
    {                           /* Don't want to wait forever on broken transaction!! */
      if (tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
        {
          /* remove the transaction from oSIP: */
          /* osip_remove_transaction(eXosip.j_osip, tr);
             eXosip_remove_transaction_from_call(tr, jc);
             osip_transaction_free(tr); */
      } else
        return 0;
    }

  tr = eXosip_find_last_inc_transaction (jc, jd, "REFER");
  if (tr != NULL && tr->state != IST_TERMINATED)
    {                           /* Don't want to wait forever on broken transaction!! */
      if (tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
        {
          /* remove the transaction from oSIP: */
          osip_remove_transaction (eXosip.j_osip, tr);
          eXosip_remove_transaction_from_call (tr, jc);
	  osip_list_add (eXosip.j_transactions, tr, 0);
      } else
        return 0;
    }

  tr = eXosip_find_last_out_transaction (jc, jd, "REFER");
  if (tr != NULL && tr->state != NICT_TERMINATED)
    {                           /* Don't want to wait forever on broken transaction!! */
      if (tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
        {
          /* remove the transaction from oSIP: */
          osip_remove_transaction (eXosip.j_osip, tr);
          eXosip_remove_transaction_from_call (tr, jc);
	  osip_list_add (eXosip.j_transactions, tr, 0);
      } else
        return 0;
    }

  return -1;
}

static int
eXosip_release_finished_calls (eXosip_call_t * jc, eXosip_dialog_t * jd)
{
  osip_transaction_t *tr;

  tr = eXosip_find_last_inc_transaction (jc, jd, "BYE");
  if (tr == NULL)
    tr = eXosip_find_last_out_transaction (jc, jd, "BYE");

  if (tr != NULL && (tr->state == NIST_TERMINATED || tr->state == NICT_TERMINATED))
    {
        int did = -2;
        if (jd!=NULL)
            did = jd->d_id;
        OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
            "eXosip: eXosip_release_finished_calls remove a dialog (cid=%i did=%i)\n", jc->c_id, did));
      /* Remove existing reference to the dialog from transactions! */
      __eXosip_call_remove_dialog_reference_in_call (jc, jd);
      REMOVE_ELEMENT (jc->c_dialogs, jd);
      eXosip_dialog_free (jd);
      return 0;
    }
  return -1;
}



static void
__eXosip_release_call (eXosip_call_t * jc, eXosip_dialog_t * jd)
{
  REMOVE_ELEMENT (eXosip.j_calls, jc);
  report_call_event (EXOSIP_CALL_RELEASED, jc, jd, NULL);
  eXosip_call_free (jc);
  __eXosip_wakeup ();
}


static int
eXosip_release_aborted_calls (eXosip_call_t * jc, eXosip_dialog_t * jd)
{
  int now = time (NULL);
  osip_transaction_t *tr;

#if 0
  tr = eXosip_find_last_inc_invite (jc, jd);
  if (tr == NULL)
    tr = eXosip_find_last_out_invite (jc, jd);
#else
  /* close calls only when the initial INVITE failed */
  tr = jc->c_inc_tr;
  if (tr==NULL)
    tr = jc->c_out_tr;
#endif

  if (tr == NULL)
    {
      if (jd != NULL)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                  "eXosip: eXosip_release_aborted_calls remove an empty dialog\n"));
          __eXosip_call_remove_dialog_reference_in_call (jc, jd);
          REMOVE_ELEMENT (jc->c_dialogs, jd);
          eXosip_dialog_free (jd);
          return 0;
        }
      return -1;
    }

  if (tr != NULL && tr->state != IST_TERMINATED && tr->state != ICT_TERMINATED && tr->birth_time + 180 < now)   /* Wait a max of 2 minutes */
    {
      if (jd != NULL)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                  "eXosip: eXosip_release_aborted_calls remove a dialog for an unfinished transaction\n"));
          __eXosip_call_remove_dialog_reference_in_call (jc, jd);
          REMOVE_ELEMENT (jc->c_dialogs, jd);
          report_call_event (EXOSIP_CALL_NOANSWER, jc, jd, NULL);
          eXosip_dialog_free (jd);
          __eXosip_wakeup ();
          return 0;
        }
    }

  if (tr != NULL && (tr->state == IST_TERMINATED || tr->state == ICT_TERMINATED))
    {
      if (tr == jc->c_inc_tr)
        {
          if (jc->c_inc_tr->last_response == NULL)
            {
              /* OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO2,NULL,
                 "eXosip: eXosip_release_aborted_calls transaction with no answer\n")); */
          } else if (MSG_IS_STATUS_3XX (jc->c_inc_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls answered with a 3xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_4XX (jc->c_inc_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls answered with a 4xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_5XX (jc->c_inc_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls answered with a 5xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_6XX (jc->c_inc_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls answered with a 6xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
            }
      } else if (tr == jc->c_out_tr)
        {
          if (jc->c_out_tr->last_response == NULL)
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls completed with no answer\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_3XX (jc->c_out_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls completed answered with 3xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_4XX (jc->c_out_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls completed answered with 4xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_5XX (jc->c_out_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls completed answered with 5xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
          } else if (MSG_IS_STATUS_6XX (jc->c_out_tr->last_response))
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                                      "eXosip: eXosip_release_aborted_calls completed answered with 6xx\n"));
              __eXosip_release_call (jc, jd);
              return 0;
            }
        }
    }

  return -1;
}


void
eXosip_release_terminated_calls (void)
{
  eXosip_dialog_t *jd;
  eXosip_dialog_t *jdnext;
  eXosip_call_t *jc;
  eXosip_call_t *jcnext;
  int now = time (NULL);
  int pos;


  for (jc = eXosip.j_calls; jc != NULL;)
    {
      jcnext = jc->next;
      /* free call terminated with a BYE */

      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
          "eXosip: working on (cid=%i)\n", jc->c_id));
      for (jd = jc->c_dialogs; jd != NULL;)
        {
          jdnext = jd->next;
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
              "eXosip: working on (cid=%i did=%i)\n", jc->c_id, jd->d_id));
          if (0 == eXosip_pendingosip_transaction_exist (jc, jd))
            {
          } else if (0 == eXosip_release_finished_calls (jc, jd))
            {
              jd = jc->c_dialogs;
          } else if (0 == eXosip_release_aborted_calls (jc, jd))
            {
              jdnext = NULL;
          } else if (jd->d_id==-1)
            {
                OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                    "eXosip: eXosip_release_terminated_calls delete a removed dialog (cid=%i did=%i)\n", jc->c_id, jd->d_id));
                /* Remove existing reference to the dialog from transactions! */
                __eXosip_call_remove_dialog_reference_in_call (jc, jd);
                REMOVE_ELEMENT (jc->c_dialogs, jd);
                eXosip_dialog_free (jd);

                jd = jc->c_dialogs;
            }
          jd = jdnext;
        }
      jc = jcnext;
    }

  for (jc = eXosip.j_calls; jc != NULL;)
    {
      jcnext = jc->next;
      if (jc->c_dialogs == NULL)
        {
          /* release call for options requests */
          if (jc->c_inc_options_tr != NULL)
            {
              if (jc->c_inc_options_tr->state == NIST_TERMINATED)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO1, NULL,
                               "eXosip: remove an incoming OPTIONS with no final answer\n"));
                  __eXosip_release_call (jc, NULL);
              } else if (jc->c_inc_options_tr->state != NIST_TERMINATED
                         && jc->c_inc_options_tr->birth_time + 180 < now)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO1, NULL,
                               "eXosip: remove an incoming OPTIONS with no final answer\n"));
                  __eXosip_release_call (jc, NULL);
                }
          } else if (jc->c_out_options_tr != NULL)
            {
              if (jc->c_out_options_tr->state == NICT_TERMINATED)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO1, NULL,
                               "eXosip: remove an outgoing OPTIONS with no final answer\n"));
                  __eXosip_release_call (jc, NULL);
              } else if (jc->c_out_options_tr->state != NIST_TERMINATED
                         && jc->c_out_options_tr->birth_time + 180 < now)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_INFO1, NULL,
                               "eXosip: remove an outgoing OPTIONS with no final answer\n"));
                  __eXosip_release_call (jc, NULL);
                }
          } else if (jc->c_inc_tr != NULL
                     && jc->c_inc_tr->state != IST_TERMINATED
                     && jc->c_inc_tr->birth_time + 180 < now)
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                                      "eXosip: remove an incoming call with no final answer\n"));
              __eXosip_release_call (jc, NULL);
          } else if (jc->c_out_tr != NULL
                     && jc->c_out_tr->state != ICT_TERMINATED
                     && jc->c_out_tr->birth_time + 180 < now)
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                                      "eXosip: remove an outgoing call with no final answer\n"));
              __eXosip_release_call (jc, NULL);
          } else if (jc->c_inc_tr != NULL && jc->c_inc_tr->state != IST_TERMINATED)
            {
          } else if (jc->c_out_tr != NULL && jc->c_out_tr->state != ICT_TERMINATED)
            {
          } else                /* no active pending transaction */
            {
              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                                      "eXosip: remove a call\n"));
              __eXosip_release_call (jc, NULL);
            }
        }
      jc = jcnext;
    }

  pos = 0;
  while (!osip_list_eol (eXosip.j_transactions, pos))
    {
      osip_transaction_t *tr =
        (osip_transaction_t *) osip_list_get (eXosip.j_transactions, pos);
      if (tr->state == IST_TERMINATED || tr->state == ICT_TERMINATED
          || tr->state == NICT_TERMINATED || tr->state == NIST_TERMINATED)

        {                       /* free (transaction is already removed from the oSIP stack) */
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
                                  "Release a terminated transaction\n"));
          osip_list_remove (eXosip.j_transactions, pos);
          __eXosip_delete_jinfo (tr);
          osip_transaction_free (tr);
      } else if (tr->birth_time + 180 < now)    /* Wait a max of 2 minutes */
        {
          osip_list_remove (eXosip.j_transactions, pos);
          __eXosip_delete_jinfo (tr);
          osip_transaction_free (tr);
      } else
        pos++;
    }
}

void
eXosip_release_terminated_registrations (void)
{
  eXosip_reg_t *jr;
  eXosip_reg_t *jrnext;
  int now = time (NULL);

  for (jr = eXosip.j_reg; jr != NULL;)
    {
      jrnext = jr->next;
      if (jr->r_reg_period == 0 && jr->r_last_tr!=NULL)
	{
	  if (now - jr->r_last_tr->birth_time > 60)
	    {
	      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
				      "Release a terminated registration\n"));
	      REMOVE_ELEMENT (eXosip.j_reg, jr);
	      eXosip_reg_free (jr);
	    }
	  else if (jr->r_last_tr->last_response!=NULL
		   && jr->r_last_tr->last_response->status_code>=200
		   && jr->r_last_tr->last_response->status_code<=299)
	    {
	      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO1, NULL,
				      "Release a terminated registration with 2xx\n"));
	      REMOVE_ELEMENT (eXosip.j_reg, jr);
	      eXosip_reg_free (jr);
	    }
	}

      jr = jrnext;
    }

  return;
}
