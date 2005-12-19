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

#ifndef __EX_MESSAGE_H__
#define __EX_MESSAGE_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_message.h
 * @brief eXosip message request API
 *
 * This file provide the API needed to control MESSAGE requests. You can
 * use it to:
 *
 * <ul>
 * <li>build MESSAGE requests.</li>
 * <li>send MESSAGE requests.</li>
 * <li>build MESSAGE answers.</li>
 * <li>send MESSAGE answers.</li>
 * </ul>
 */

/**
 * @defgroup eXosip2_message eXosip2 MESSAGE and messaging services outside of dialog
 * @ingroup eXosip2_msg
 * @{
 */

/**
 * Build a default MESSAGE message.
 * 
 * This method will be updated to send any message outside of dialog
 * In this later case, you'll specify the method to use in the second
 * argument.
 *
 * By now, use this method only for the "MESSAGE" request.
 *
 * @param message   Pointer for the SIP request to build.
 * @param method    request method. ("MESSAGE")
 * @param to        SIP url for callee.
 * @param from      SIP url for caller.
 * @param route     Route header for INVITE. (optionnal)
 */
  int eXosip_message_build_request(osip_message_t **message, const char *method,
				   const char *to, const char *from,
				   const char *route);

/**
 * Send an MESSAGE request.
 * 
 * @param message          SIP MESSAGE message to send.
 */
  int eXosip_message_send_request(osip_message_t *message);

/**
 * Build answer for an MESSAGE request.
 * 
 * @param tid             id of MESSAGE transaction.
 * @param status          status for SIP answer to build.
 * @param answer          The SIP answer to build.
 */
  int eXosip_message_build_answer(int tid, int status, osip_message_t **answer);

/**
 * Send answer for an MESSAGE request.
 * 
 * @param tid             id of MESSAGE transaction.
 * @param status          status for SIP answer to send.
 * @param answer          The SIP answer to send. (default will be sent if NULL)
 */
  int eXosip_message_send_answer(int tid, int status, osip_message_t *answer);

/** @} */


#ifdef __cplusplus
}
#endif

#endif
