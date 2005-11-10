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
#include <eXosip2/eXosip.h>

extern eXosip_t eXosip;

int
eXosip_build_publish (osip_message_t ** message,
                      const char *to,
                      const char *from,
                      const char *route,
                      const char *event,
                      const char *expires, const char *ctype, const char *body)
{
  int i;

  *message = NULL;

  if (to == NULL || to[0] == '\0')
    return -1;
  if (from == NULL || from[0] == '\0')
    return -1;
  if (event == NULL || event[0] == '\0')
    return -1;
  if (ctype == NULL || ctype[0] == '\0')
    {
      if (body != NULL && body[0] != '\0')
        return -1;
  } else
    {
      if (body == NULL || body[0] == '\0')
        return -1;
    }

  i = generating_publish (message, to, from, route);
  if (i != 0)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: cannot send message (cannot build PUBLISH)! "));
      return -1;
    }

  if (body != NULL && body[0] != '\0' && ctype != NULL && ctype[0] != '\0')
    {
      osip_message_set_content_type (*message, ctype);
      osip_message_set_body (*message, body, strlen (body));
      osip_message_set_header (*message, "Content-Disposition",
                               "render;handling=required");
    }
  if (expires != NULL && expires[0] != '\0')
    osip_message_set_expires (*message, expires);
  else
    osip_message_set_expires (*message, "3600");

  osip_message_set_header (*message, "Event", event);
  return 0;
}

int
eXosip_publish (osip_message_t * message, const char *to)
{
  osip_transaction_t *transaction;
  osip_event_t *sipevent;
  int i;
  eXosip_pub_t *pub = NULL;

  if (message == NULL)
    return -1;
  if (message->cseq == NULL || message->cseq->number == NULL)
    {
      osip_message_free (message);
      return -1;
    }
  if (to == NULL)
    {
      osip_message_free (message);
      return -1;
    }

  i = _eXosip_pub_find_by_aor (&pub, to);
  if (i != 0 || pub == NULL)
    {
      osip_header_t *expires;

      osip_message_get_expires (message, 0, &expires);
      if (expires == NULL || expires->hvalue == NULL)
        {
          OSIP_TRACE (osip_trace
                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                       "eXosip: missing expires header in PUBLISH!"));
          osip_message_free (message);
          return -1;
      } else
        {
          /* start a new publication context */
          _eXosip_pub_init (&pub, to, expires->hvalue);
          if (pub == NULL)
	    {
	      osip_message_free (message);
	      return -1;
	    }
          ADD_ELEMENT (eXosip.j_pub, pub);
        }
  } else
    {
      if (pub->p_sip_etag != NULL && pub->p_sip_etag[0] != '\0')
        {
          /* increase cseq */
          osip_message_set_header (message, "SIP-If-Match", pub->p_sip_etag);
        }

      if (pub->p_last_tr != NULL && pub->p_last_tr->cseq != NULL
          && pub->p_last_tr->cseq->number != NULL)
        {
          int osip_cseq_num = osip_atoi (pub->p_last_tr->cseq->number);
          int length = strlen (pub->p_last_tr->cseq->number);

          osip_cseq_num++;
          osip_free (message->cseq->number);
          message->cseq->number = (char *) osip_malloc (length + 2);    /* +2 like for 9 to 10 */
          sprintf (message->cseq->number, "%i", osip_cseq_num);
        }
    }

  i = osip_transaction_init (&transaction, NICT, eXosip.j_osip, message);
  if (i != 0)
    {
      osip_message_free (message);
      return -1;
    }

  if (pub->p_last_tr != NULL)
    osip_list_add (eXosip.j_transactions, pub->p_last_tr, 0);
  pub->p_last_tr = transaction;

  sipevent = osip_new_outgoing_sipmessage (message);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, NULL, NULL, NULL));
  osip_transaction_add_event (transaction, sipevent);
  __eXosip_wakeup ();
  return 0;
}
