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

#ifndef __EX_SUBSCRIBE_H__
#define __EX_SUBSCRIBE_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_subscribe.h
 * @brief eXosip subscribe request API
 *
 * This file provide the API needed to control SUBSCRIBE requests. You can
 * use it to:
 *
 * <ul>
 * <li>build SUBSCRIBE requests.</li>
 * <li>send SUBSCRIBE requests.</li>
 * <li>build SUBSCRIBE answers.</li>
 * <li>send SUBSCRIBE answers.</li>
 * </ul>
 */

/**
 * @defgroup eXosip2_subscribe eXosip2 SUBSCRIBE and outgoing subscriptions
 * @ingroup eXosip2_msg
 * @{
 */

typedef enum eXosip_ss {
  EXOSIP_SUBCRSTATE_UNKNOWN,      /**< unknown subscription-state */
  EXOSIP_SUBCRSTATE_PENDING,      /**< pending subscription-state */
  EXOSIP_SUBCRSTATE_ACTIVE,       /**< active subscription-state */
  EXOSIP_SUBCRSTATE_TERMINATED    /**< terminated subscription-state */
} eXosip_ss_t;

typedef enum eXosip_ss_reason {
  DEACTIVATED,                   /**< deactivated for subscription-state */
  PROBATION,                     /**< probation for subscription-state */
  REJECTED,                      /**< rejected for subscription-state */
  TIMEOUT,                       /**< timeout for subscription-state */
  GIVEUP,                        /**< giveup for subscription-state */
  NORESOURCE                     /**< noresource for subscription-state */
} eXosip_ss_reason_t;


typedef enum eXosip_ss_status {
  EXOSIP_NOTIFY_UNKNOWN,     /**< unknown state for subscription */
  EXOSIP_NOTIFY_PENDING,     /**< subscription not yet accepted */
  EXOSIP_NOTIFY_ONLINE,      /**< online status */
  EXOSIP_NOTIFY_BUSY,        /**< busy status */
  EXOSIP_NOTIFY_BERIGHTBACK, /**< be right back status */
  EXOSIP_NOTIFY_AWAY,        /**< away status */
  EXOSIP_NOTIFY_ONTHEPHONE,  /**< on the phone status */
  EXOSIP_NOTIFY_OUTTOLUNCH,  /**< out to lunch status */
  EXOSIP_NOTIFY_CLOSED       /**< closed status */
} eXosip_ss_status_t;

/**
 * Build a default initial SUBSCRIBE request.
 * 
 * @param subscribe Pointer for the SIP request to build.
 * @param to        SIP url for callee.
 * @param from      SIP url for caller.
 * @param route     Route header for SUBSCRIBE. (optionnal)
 * @param event     Event header for SUBSCRIBE.
 * @param expires   Expires header for SUBSCRIBE.
 */
  int eXosip_subscribe_build_initial_request(osip_message_t **subscribe,
					     const char *to, const char *from,
					     const char *route, const char *event,
					     int expires);

/**
 * Send an initial SUBSCRIBE request.
 * 
 * @param subscribe          SIP SUBSCRIBE message to send.
 */
  int eXosip_subscribe_send_initial_request(osip_message_t *subscribe);

/**
 * Build a default new SUBSCRIBE message.
 * 
 * @param did      identifier of the subscription.
 * @param sub      Pointer for the SIP request to build.
 */
  int eXosip_subscribe_build_refresh_request(int did, osip_message_t **sub);

/**
 * Send a new SUBSCRIBE request.
 * 
 * @param did          identifier of the subscription.
 * @param sub          SIP SUBSCRIBE message to send.
 */
  int eXosip_subscribe_send_refresh_request(int did, osip_message_t *sub);

/** @} */

/**
 * @defgroup eXosip2_notify eXosip2 SUBSCRIBE and incoming subscriptions
 * @ingroup eXosip2_msg
 * @{
 */

/**
 * Build answer for an SUBSCRIBE request.
 * 
 * @param tid             id of SUBSCRIBE transaction.
 * @param status          status for SIP answer to build.
 * @param answer          The SIP answer to build.
 */
  int eXosip_insubscription_build_answer(int tid, int status,
					 osip_message_t **answer);

/**
 * Send answer for an SUBSCRIBE request.
 * 
 * @param tid             id of SUBSCRIBE transaction.
 * @param status          status for SIP answer to send.
 * @param answer          The SIP answer to send. (default will be sent if NULL)
 */
  int eXosip_insubscription_send_answer(int tid, int status,
					osip_message_t *answer);

/**
 * Build a request within subscription.
 * 
 * @param did             id of incoming subscription.
 * @param method          request method to build.
 * @param request         The SIP request to build.
 */
  int eXosip_insubscription_build_request(int did, const char *method,
					  osip_message_t **request);

/**
 * Build a NOTIFY request within subscription.
 * 
 * @param did                  id of incoming subscription.
 * @param subscription_status  subscription status (pending, active, terminated)
 * @param subscription_reason  subscription reason
 * @param request              The SIP request to build.
 */
  int eXosip_insubscription_build_notify(int did, int subscription_status,
					 int subscription_reason,
					 osip_message_t **request);

/**
 * Send a request within subscription.
 * 
 * @param did             id of incoming subscription.
 * @param request         The SIP request to send.
 */
  int eXosip_insubscription_send_request(int did, osip_message_t *request);
					  
/** @} */


#ifdef __cplusplus
}
#endif

#endif
