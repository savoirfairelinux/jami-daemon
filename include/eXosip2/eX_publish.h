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

#ifndef __EX_PUBLISH_H__
#define __EX_PUBLISH_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_publish.h
 * @brief eXosip publish request API
 *
 * This file provide the API needed to control PUBLISH requests. You can
 * use it to:
 *
 * <ul>
 * <li>build PUBLISH requests.</li>
 * <li>send PUBLISH requests.</li>
 * </ul>
 */

/**
 * @defgroup eXosip2_publish eXosip2 Publication Management
 * @ingroup eXosip2_msg
 * @{
 */

/**
 * build publication for a user. (PUBLISH request)
 * 
 * @param message   returned published request.
 * @param to        SIP url for callee.
 * @param from      SIP url for caller.
 * @param route     Route used for publication.
 * @param event     SIP Event header.
 * @param expires   SIP Expires header.
 * @param ctype     Content-Type of body.
 * @param body     body for publication.
 */
int eXosip_build_publish(osip_message_t **message,
			 const char *to,
			 const char *from,
			 const char *route,
			 const char *event,
			 const char *expires,
			 const char *ctype,
			 const char *body);

/**
 * Send an Publication Message (PUBLISH request).
 * 
 * @param message is a ready to be sent publish message .
 * @param sip_if_match is the SIP-If-Match header. (NULL for initial publication)
 */
int eXosip_publish (osip_message_t *message, const char *sip_if_match);


/** @} */


#ifdef __cplusplus
}
#endif

#endif
