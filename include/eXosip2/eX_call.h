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

#ifndef __EX_CALL_H__
#define __EX_CALL_H__

#include <osipparser2/osip_parser.h>
#include <osipparser2/sdp_message.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_call.h
 * @brief eXosip call API
 *
 * This file provide the API needed to control calls. You can
 * use it to:
 *
 * <ul>
 * <li>build initial invite.</li>
 * <li>send initial invite.</li>
 * <li>build request within the call.</li>
 * <li>send request within the call.</li>
 * </ul>
 *
 * This API can be used to build the following messages:
 * <pre>
 *    INVITE, INFO, OPTIONS, REFER, UPDATE, NOTIFY
 * </pre>
 */

/**
 * @defgroup eXosip2_call eXosip2 INVITE and Call Management
 * @ingroup eXosip2_msg
 * @{
 */

  struct eXosip_call_t;

/**
 * Set a new application context for an existing call
 *
 * @param id       call-id or dialog-id of call
 * @param reference New application context.
 */
  int eXosip_call_set_reference(int id, void *reference);

/**
 * Build a default INVITE message for a new call.
 * 
 * @param invite    Pointer for the SIP element to hold.
 * @param to        SIP url for callee.
 * @param from      SIP url for caller.
 * @param route     Route header for INVITE. (optionnal)
 * @param subject   Subject for the call.
 */
  int eXosip_call_build_initial_invite(osip_message_t **invite, const char *to,
				       const char *from, const char *route,
				       const char *subject);

/**
 * Initiate a call.
 * 
 * @param invite          SIP INVITE message to send.
 */
  int eXosip_call_send_initial_invite(osip_message_t *invite);

/**
 * Build a default request within a call. (INVITE, OPTIONS, INFO, REFER)
 * 
 * @param did          dialog id of call.
 * @param method       request type to build.
 * @param request      The sip request to build.
 */
  int eXosip_call_build_request(int did, const char *method,
				osip_message_t **request);

/**
 * Build a default ACK for a 200ok received.
 * 
 * @param did          dialog id of call.
 * @param ack          The sip request to build.
 */
  int eXosip_call_build_ack(int did, osip_message_t **ack);

/**
 * Send the ACK for the 200ok received..
 * 
 * @param did          dialog id of call.
 * @param ack          SIP ACK message to send.
 */
  int eXosip_call_send_ack(int did, osip_message_t *ack);

/**
 * Build a default REFER for a call transfer.
 * 
 * @param did          dialog id of call.
 * @param refer_to     url for call transfer (Refer-To header).
 * @param request      The sip request to build.
 */
  int eXosip_call_build_refer(int did, const char *refer_to, osip_message_t **request);

/**
 * Build a default INFO within a call.
 * 
 * @param did          dialog id of call.
 * @param request      The sip request to build.
 */
  int eXosip_call_build_info(int did, osip_message_t **request);

/**
 * Build a default OPTIONS within a call.
 * 
 * @param did          dialog id of call.
 * @param request      The sip request to build.
 */
  int eXosip_call_build_options(int did, osip_message_t **request);

/**
 * Build a default UPDATE within a call.
 * 
 * @param did          dialog id of call.
 * @param request      The sip request to build.
 */
  int eXosip_call_build_update(int did, osip_message_t **request);

/**
 * Build a default NOTIFY within a call.
 * 
 * @param did                   dialog id of call.
 * @param subscription_status   Subscription status of the request.
 * @param request               The sip request to build.
 */
  int eXosip_call_build_notify(int did, int subscription_status, osip_message_t **request);

/**
 * send the request within call. (INVITE, OPTIONS, INFO, REFER, UPDATE)
 * 
 * @param did          dialog id of call.
 * @param request      The sip request to send.
 */
  int eXosip_call_send_request(int did, osip_message_t *request);

/**
 * Build default Answer for request.
 * 
 * @param tid          id of transaction to answer.
 * @param status       Status code to use.
 * @param answer       The sip answer to build.
 */
  int eXosip_call_build_answer(int tid, int status, osip_message_t **answer);

/**
 * Send Answer for invite.
 * 
 * @param tid          id of transaction to answer.
 * @param status       response status if answer is NULL. (not allowed for 2XX)
 * @param answer       The sip answer to send.
 */
  int eXosip_call_send_answer(int tid, int status, osip_message_t *answer);

/**
 * Terminate a call.
 * send CANCEL, BYE or 603 Decline.
 * 
 * @param cid          call id of call.
 * @param did          dialog id of call.
 */
  int eXosip_call_terminate(int cid, int did);

/**
 * Build a PRACK for invite.
 * 
 * @param tid          id of the invite transaction.
 * @param prack        The sip prack to build.
 */
  int eXosip_call_build_prack(int tid, osip_message_t **prack);

/**
 * Send a PRACK for invite.
 * 
 * @param tid          id of the invite transaction.
 * @param prack        The sip prack to send.
 */
  int eXosip_call_send_prack(int tid, osip_message_t *prack);

/**
 * Send a NOTIFY containing the information about a call transfer.
 * 
 * THIS METHOD WILL BE REPLACED or REMOVED, please use the
 * new API to build NOTIFY.
 * 
 * @param did                  dialog id of call.
 * @param subscription_status  the subscription status.
 * @param body                 the body to attach to NOTIFY.
 */
  int eXosip_transfer_send_notify(int did, int subscription_status, char *body);

/** @} */


#ifdef __cplusplus
}
#endif

#endif
