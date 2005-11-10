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

#ifndef __EX_REGISTER_H__
#define __EX_REGISTER_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @file eX_register.h
 * @brief eXosip registration API
 *
 * This file provide the API needed to control registrations. You can
 * use it to:
 *
 * <ul>
 * <li>build initial REGISTER.</li>
 * <li>build REGISTER.</li>
 * <li>send REGISTER.</li>
 * </ul>
 */

/**
 * @defgroup eXosip2_registration eXosip2 REGISTER and Registration Management
 * @ingroup eXosip2_msg
 * @{
 */

  struct eXosip_reg_t;

/**
 * Build initial REGISTER request.
 * 
 * @param from      SIP url for caller.
 * @param proxy     Proxy used for registration.
 * @param contact   Contact address. (optional)
 * @param expires   The expires value for registration.
 * @param reg       The SIP request to build.
 */
  int eXosip_register_build_initial_register(const char *from, const char *proxy,
					     const char *contact, int expires,
					     osip_message_t **reg);

/**
 * Build a new REGISTER request for an existing registration.
 * 
 * @param rid       A unique identifier for the registration context
 * @param expires   The expires value for registration.
 * @param reg       The SIP request to build.
 */
  int eXosip_register_build_register(int rid, int expires, osip_message_t **reg);

/**
 * Send a REGISTER request for an existing registration.
 * 
 * @param rid       A unique identifier for the registration context
 * @param reg       The SIP request to build. (NULL for default REGISTER)
 */
  int eXosip_register_send_register(int rid, osip_message_t *reg);

/** @} */


#ifdef __cplusplus
}
#endif

#endif
