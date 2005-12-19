
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

#ifndef __EX_REFER_H__
#define __EX_REFER_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_refer.h
 * @brief eXosip transfer outside of calls API
 *
 * This file provide the API needed to request a blind transfer
 * outside of any call.
 *
 * <ul>
 * <li>build initial refer.</li>
 * <li>send initial refer.</li>
 * </ul>
 *
 */

/**
 * @defgroup eXosip2_refer eXosip2 REFER and blind tranfer Management outside of calls
 * @ingroup eXosip2_msg
 * @{
 */

/**
 * Build a default REFER message for a blind transfer outside of any calls.
 * 
 * @param refer     Pointer for the SIP element to hold.
 * @param refer_to  SIP url for transfer.
 * @param from      SIP url for caller.
 * @param to        SIP url for callee.
 * @param route     Route header for REFER. (optionnal)
 */
int eXosip_refer_build_request(osip_message_t **refer, const char *refer_to,
			       const char *from, const char *to,
			       const char *route);

/**
 * Initiate a blind tranfer outside of any call.
 * 
 * @param refer     SIP REFER message to send.
 */
int eXosip_refer_send_request(osip_message_t *refer);

/** @} */


#ifdef __cplusplus
}
#endif

#endif
