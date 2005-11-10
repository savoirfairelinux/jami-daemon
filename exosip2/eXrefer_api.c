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
eXosip_refer_build_request (osip_message_t ** refer, const char *refer_to,
                            const char *from, const char *to, const char *proxy)
{
  int i;

  *refer = NULL;
  i = generating_request_out_of_dialog (refer, "REFER", to, "UDP", from, proxy);
  if (i != 0)
    {
      return -1;
    }

  osip_message_set_header (*refer, "Refer-to", refer_to);
  return 0;
}

int
eXosip_refer_send_request (osip_message_t * refer)
{
  osip_transaction_t *transaction;
  osip_event_t *sipevent;
  int i;

  if (refer == NULL)
    return -1;

  i = osip_transaction_init (&transaction, NICT, eXosip.j_osip, refer);
  if (i != 0)
    {
      osip_message_free (refer);
      return -1;
    }

  osip_list_add (eXosip.j_transactions, transaction, 0);

  sipevent = osip_new_outgoing_sipmessage (refer);
  sipevent->transactionid = transaction->transactionid;

  osip_transaction_set_your_instance (transaction,
                                      __eXosip_new_jinfo (NULL, NULL, NULL, NULL));
  osip_transaction_add_event (transaction, sipevent);
  __eXosip_wakeup ();
  return 0;
}
