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
eXosip_find_last_out_subscribe (eXosip_subscribe_t * js, eXosip_dialog_t * jd)
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
          if (0 == strcmp (out_tr->cseq->method, "SUBSCRIBE"))
            break;
          else
            out_tr = NULL;
          pos++;
        }
  } else
    out_tr = NULL;

  if (out_tr == NULL)
    return js->s_out_tr;        /* can be NULL */

  return out_tr;
}

osip_transaction_t *
eXosip_find_last_inc_notify (eXosip_subscribe_t * js, eXosip_dialog_t * jd)
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
eXosip_subscribe_init (eXosip_subscribe_t ** js)
{
  *js = (eXosip_subscribe_t *) osip_malloc (sizeof (eXosip_subscribe_t));
  if (*js == NULL)
    return -1;
  memset (*js, 0, sizeof (eXosip_subscribe_t));
  return 0;
}

void
eXosip_subscribe_free (eXosip_subscribe_t * js)
{
  /* ... */

  eXosip_dialog_t *jd;

  for (jd = js->s_dialogs; jd != NULL; jd = js->s_dialogs)
    {
      REMOVE_ELEMENT (js->s_dialogs, jd);
      eXosip_dialog_free (jd);
    }

  __eXosip_delete_jinfo (js->s_inc_tr);
  __eXosip_delete_jinfo (js->s_out_tr);
  if (js->s_inc_tr != NULL)
    osip_list_add (eXosip.j_transactions, js->s_inc_tr, 0);
  if (js->s_out_tr != NULL)
    osip_list_add (eXosip.j_transactions, js->s_out_tr, 0);

  osip_free (js);
}

int
_eXosip_subscribe_set_refresh_interval (eXosip_subscribe_t * js,
                                        osip_message_t * out_subscribe)
{
  osip_header_t *exp;

  if (js == NULL || out_subscribe == NULL)
    return -1;

  osip_message_get_expires (out_subscribe, 0, &exp);
  if (exp == NULL || exp->hvalue == NULL)
    js->s_reg_period = 3600;
  else
    {
      js->s_reg_period = osip_atoi (exp->hvalue);
      if (js->s_reg_period < 0)
        js->s_reg_period = 3600;
    }

  return 0;
}

int
eXosip_subscribe_need_refresh (eXosip_subscribe_t * js, eXosip_dialog_t * jd,
                               int now)
{
  osip_transaction_t *out_tr = NULL;

  if (jd != NULL)
    out_tr = osip_list_get (jd->d_out_trs, 0);
  if (out_tr == NULL)
    out_tr = js->s_out_tr;

  if (now - out_tr->birth_time > js->s_reg_period - 60)
    return 0;
  return -1;
}
