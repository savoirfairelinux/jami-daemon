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

osip_transaction_t *
eXosip_find_last_inc_subscribe (eXosip_notify_t * jn, eXosip_dialog_t * jd)
{
  osip_transaction_t *inc_tr;
  int pos;

  inc_tr = NULL;
  pos = 0;
  if (jd != NULL)
    {
      while (!osip_list_eol (jd->d_inc_trs, pos))
        {
          inc_tr = osip_list_get (jd->d_inc_trs, pos);
          if (0 == strcmp (inc_tr->cseq->method, "SUBSCRIBE"))
            break;
          else
            inc_tr = NULL;
          pos++;
        }
  } else
    inc_tr = NULL;

  if (inc_tr == NULL)
    return jn->n_inc_tr;        /* can be NULL */

  return inc_tr;
}


osip_transaction_t *
eXosip_find_last_out_notify (eXosip_notify_t * jn, eXosip_dialog_t * jd)
{
  osip_transaction_t *out_tr;
  int pos;

  out_tr = NULL;
  pos = 0;
  if (jd != NULL)
    {
      while (!osip_list_eol (jd->d_out_trs, pos))
        {
          out_tr = osip_list_get (jd->d_out_trs, pos);
          if (0 == strcmp (out_tr->cseq->method, "NOTIFY"))
            return out_tr;
          pos++;
        }
    }

  return NULL;
}

int
eXosip_notify_init (eXosip_notify_t ** jn, osip_message_t * inc_subscribe)
{
  osip_contact_t *co;
  char *uri;
  int i;
  char locip[50];

#ifdef SM
  eXosip_get_localip_from_via (inc_subscribe, locip, 49);
#else
  i = _eXosip_find_protocol(inc_subscribe);
  if (i==IPPROTO_UDP)
    {
      eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip, 49);
    }
  else if (i==IPPROTO_TCP)
    {
      eXosip_guess_ip_for_via (eXosip.net_interfaces[1].net_ip_family, locip, 49);
    }
  else
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unsupported protocol (default to UDP)\n"));
      eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip, 49);
      return -1;
    }
    
#endif
  if (inc_subscribe == NULL
      || inc_subscribe->to == NULL || inc_subscribe->to->url == NULL)
    return -1;
  co = (osip_contact_t *) osip_list_get (inc_subscribe->contacts, 0);
  if (co == NULL || co->url == NULL)
    return -1;

  *jn = (eXosip_notify_t *) osip_malloc (sizeof (eXosip_notify_t));
  if (*jn == NULL)
    return -1;
  memset (*jn, 0, sizeof (eXosip_notify_t));

  i = osip_uri_to_str (co->url, &uri);
  if (i != 0)
    {
      osip_free (*jn);
      *jn = NULL;
      return -1;
    }
  osip_strncpy ((*jn)->n_uri, uri, 254);
  osip_free (uri);

  return 0;
}

void
eXosip_notify_free (eXosip_notify_t * jn)
{
  /* ... */

  eXosip_dialog_t *jd;

  for (jd = jn->n_dialogs; jd != NULL; jd = jn->n_dialogs)
    {
      REMOVE_ELEMENT (jn->n_dialogs, jd);
      eXosip_dialog_free (jd);
    }

  __eXosip_delete_jinfo (jn->n_inc_tr);
  __eXosip_delete_jinfo (jn->n_out_tr);
  if (jn->n_inc_tr != NULL)
    osip_list_add (eXosip.j_transactions, jn->n_inc_tr, 0);
  if (jn->n_out_tr != NULL)
    osip_list_add (eXosip.j_transactions, jn->n_out_tr, 0);
  osip_free (jn);
}

int
_eXosip_notify_set_refresh_interval (eXosip_notify_t * jn,
                                     osip_message_t * inc_subscribe)
{
  osip_header_t *exp;
  int now;

  now = time (NULL);
  if (jn == NULL || inc_subscribe == NULL)
    return -1;

  osip_message_get_expires (inc_subscribe, 0, &exp);
  if (exp == NULL || exp->hvalue == NULL)
    jn->n_ss_expires = now + 600;
  else
    {
      jn->n_ss_expires = osip_atoi (exp->hvalue);
      if (jn->n_ss_expires != -1)
        jn->n_ss_expires = now + jn->n_ss_expires;
      else                      /* on error, set it to default */
        jn->n_ss_expires = now + 600;
    }

  return 0;
}

void
_eXosip_notify_add_expires_in_2XX_for_subscribe (eXosip_notify_t * jn,
                                                 osip_message_t * answer)
{
  char tmp[20];
  int now;

  now = time (NULL);

  if (jn->n_ss_expires - now < 0)
    {
      tmp[0] = '0';
      tmp[1] = '\0';
  } else
    {
      sprintf (tmp, "%i", jn->n_ss_expires - now);
    }
  osip_message_set_expires (answer, tmp);
}
