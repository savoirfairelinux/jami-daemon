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
eXosip_message_build_request (osip_message_t ** message, const char *method,
                              const char *to, const char *from, const char *route)
{
  int i;

  *message = NULL;
  if (method != NULL && *method == '\0')
    return -1;
  if (to != NULL && *to == '\0')
    return -1;
  if (from != NULL && *from == '\0')
    return -1;
  if (route != NULL && *route == '\0')
    route = NULL;

  i = generating_request_out_of_dialog (message, method, to, "UDP", from, route);
  if (i != 0)
    return -1;

  return 0;
}

int
eXosip_message_send_request (osip_message_t * message)
{
  osip_transaction_t *transaction;
  osip_event_t *sipevent;
  int i;

  i = osip_transaction_init (&transaction, NICT, eXosip.j_osip, message);
  if (i != 0)
    {
      osip_message_free (message);
      return -1;
    }

  osip_list_add (eXosip.j_transactions, transaction, 0);

  sipevent = osip_new_outgoing_sipmessage (message);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, NULL, NULL, NULL));
  osip_transaction_add_event (transaction, sipevent);

  __eXosip_wakeup ();
  return 0;
}

int
eXosip_message_build_answer (int tid, int status, osip_message_t ** answer)
{
  osip_transaction_t *tr = NULL;
  int i = -1;

  if (tid > 0)
    {
      eXosip_transaction_find (tid, &tr);
    }
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return -1;
    }
  if (status > 100 && status < 200)
    {
#if 0
      /* TODO: not implemented */
      i = _eXosip_build_response_default (response, NULL, code, tr->orig_request);
#endif
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: status code 1xx for message not implemented (use 200<status<699)\n"));
      return -1;
  } else if (status > 199 && status < 300)
    {
      i = _eXosip_build_response_default (answer, NULL, status, tr->orig_request);
  } else if (status > 300 && status < 699)
    {
      i = _eXosip_build_response_default (answer, NULL, status, tr->orig_request);
  } else
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: wrong status code (200<status<699)\n"));
      return -1;
    }
  if (i != 0)
    return -1;
  return 0;
}

int
eXosip_message_send_answer (int tid, int status, osip_message_t * answer)
{
  osip_transaction_t *tr = NULL;
  osip_event_t *evt_answer;
  int i = -1;

  if (tid > 0)
    {
      eXosip_transaction_find (tid, &tr);
    }
  if (tr == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No MESSAGE transaction found\n"));
      osip_message_free (answer);
      return -1;
    }

  /* is the transaction already answered? */
  if (tr->state == NIST_COMPLETED || tr->state == NIST_TERMINATED)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: transaction already answered\n"));
      osip_message_free (answer);
      return -1;
    }

  if (answer == NULL)
    {
      if (status > 100 && status < 200)
        {
#if 0
          /* TODO: not implemented */
          i =
            _eXosip_build_response_default (response, NULL, code,
                                            tr->orig_request);
#endif
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: status code 1xx for message not implemented (use 200<status<699)\n"));
          return -1;
      } else if (status > 199 && status < 300)
        {
          i =
            _eXosip_build_response_default (&answer, NULL, status,
                                            tr->orig_request);
      } else if (status > 300 && status < 699)
        {
          i =
            _eXosip_build_response_default (&answer, NULL, status,
                                            tr->orig_request);
      } else
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: wrong status code (200<status<699)\n"));
          return -1;
        }
      if (i != 0)
        return -1;
    }

  evt_answer = osip_new_outgoing_sipmessage (answer);
  evt_answer->transactionid = tr->transactionid;

  osip_transaction_add_event (tr, evt_answer);
  __eXosip_wakeup ();
  return 0;
}
