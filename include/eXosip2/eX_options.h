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

#ifndef __EX_OPTIONS_H__
#define __EX_OPTIONS_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_options.h
 * @brief eXosip options request API
 *
 * This file provide the API needed to control OPTIONS requests. You can
 * use it to:
 *
 * <ul>
 * <li>build OPTIONS requests.</li>
 * <li>send OPTIONS requests.</li>
 * <li>build OPTIONS answers.</li>
 * <li>send OPTIONS answers.</li>
 * </ul>
 */

/**
 * @defgroup eXosip2_options eXosip2 OPTIONS and UA capabilities Management
 * @ingroup eXosip2_msg
 * @{
 */

/**
 * Build a default OPTIONS message.
 * 
 * @param options   Pointer for the SIP request to build.
 * @param to        SIP url for callee.
 * @param from      SIP url for caller.
 * @param route     Route header for INVITE. (optionnal)
 */
  int eXosip_options_build_request(osip_message_t **options, const char *to,
				   const char *from, const char *route);

/**
 * Send an OPTIONS request.
 * 
 * @param options          SIP OPTIONS message to send.
 */
  int eXosip_options_send_request(osip_message_t *options);

/**
 * Build answer for an OPTIONS request.
 * 
 * @param tid             id of OPTIONS transaction.
 * @param status          status for SIP answer to build.
 * @param answer          The SIP answer to build.
 */
  int eXosip_options_build_answer(int tid, int status, osip_message_t **answer);

/**
 * Send answer for an OPTIONS request.
 * 
 * @param tid             id of OPTIONS transaction.
 * @param status          status for SIP answer to send.
 * @param answer          The SIP answer to send. (default will be sent if NULL)
 */
  int eXosip_options_send_answer(int tid, int status, osip_message_t *answer);

/** @} */


#ifdef __cplusplus
}
#endif

#endif
