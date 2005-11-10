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

#ifndef __EXOSIP2_H__
#define __EXOSIP2_H__

#ifdef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define snprintf _snprintf
#define close(s) closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <stdio.h>

#include <osip2/osip.h>
#include <osip2/osip_dialog.h>

#include <eXosip2/eXosip.h>

#include "jpipe.h"

#ifndef JD_EMPTY

#define JD_EMPTY          0
#define JD_INITIALIZED    1
#define JD_TRYING         2
#define JD_QUEUED         3
#define JD_RINGING        4
#define JD_ESTABLISHED    5
#define JD_REDIRECTED     6
#define JD_AUTH_REQUIRED  7
#define JD_CLIENTERROR    8
#define JD_SERVERERROR    9
#define JD_GLOBALFAILURE  10
#define JD_TERMINATED     11

#define JD_MAX            11

#endif

#define EXOSIP_VERSION	"0.1"

#ifdef __cplusplus
extern "C"
{
#endif

  void eXosip_update (void);
  void __eXosip_wakeup (void);

  struct __eXosip_sockaddr
  {
    u_char ss_len;
    u_char ss_family;
    u_char padding[128 - 2];
  };

  typedef struct eXosip_dialog_t eXosip_dialog_t;

  struct eXosip_dialog_t
  {

    int d_id;
    int d_STATE;
    osip_dialog_t *d_dialog;    /* active dialog */

    int d_timer;
    osip_message_t *d_200Ok;
    osip_message_t *d_ack;

    osip_list_t *d_inc_trs;
    osip_list_t *d_out_trs;
    int d_retry;                /* avoid too many unsuccessfull retry */

    eXosip_dialog_t *next;
    eXosip_dialog_t *parent;
  };

  typedef struct eXosip_subscribe_t eXosip_subscribe_t;

  struct eXosip_subscribe_t
  {

    int s_id;
    int s_ss_status;
    int s_ss_reason;
    int s_reg_period;
    eXosip_dialog_t *s_dialogs;

    int s_retry;                /* avoid too many unsuccessfull retry */
    osip_transaction_t *s_inc_tr;
    osip_transaction_t *s_out_tr;

    eXosip_subscribe_t *next;
    eXosip_subscribe_t *parent;
  };

  typedef struct eXosip_notify_t eXosip_notify_t;

  struct eXosip_notify_t
  {

    int n_id;
    char n_uri[255];
    int n_online_status;

    int n_ss_status;
    int n_ss_reason;
    int n_ss_expires;
    eXosip_dialog_t *n_dialogs;

    osip_transaction_t *n_inc_tr;
    osip_transaction_t *n_out_tr;

    eXosip_notify_t *next;
    eXosip_notify_t *parent;
  };

  typedef struct eXosip_call_t eXosip_call_t;

  struct eXosip_call_t
  {

    int c_id;
    eXosip_dialog_t *c_dialogs;
    osip_transaction_t *c_inc_tr;
    osip_transaction_t *c_out_tr;
    int c_retry;                /* avoid too many unsuccessfull retry */
    osip_transaction_t *c_inc_options_tr;
    osip_transaction_t *c_out_options_tr;
    void *external_reference;

    /* cache old credentials for reuse in next request */
    osip_message_t *response_auth;

    eXosip_call_t *next;
    eXosip_call_t *parent;
  };


  typedef struct eXosip_reg_t eXosip_reg_t;

  struct eXosip_reg_t
  {

    int r_id;

    int r_reg_period;           /* delay between registration */
    char *r_aor;                /* sip identity */
    char *r_registrar;          /* registrar */
    char *r_contact;            /* list of contacts string */

    osip_transaction_t *r_last_tr;
    int r_retry;                /* avoid too many unsuccessfull retry */

    char transport[10];         /* transport used for registration */

    struct __eXosip_sockaddr addr;
    int len;

    eXosip_reg_t *next;
    eXosip_reg_t *parent;
  };


  typedef struct eXosip_pub_t eXosip_pub_t;

  struct eXosip_pub_t
  {
    int p_id;

    time_t p_expires;           /* expiration date (started+period) */
    int p_period;               /* delay between registration */
    char p_aor[256];            /* sip identity */
    char p_sip_etag[64];        /* sip_etag from 200ok */

    osip_transaction_t *p_last_tr;
    eXosip_pub_t *next;
    eXosip_pub_t *parent;
  };

  int _eXosip_pub_update (eXosip_pub_t ** pub, osip_transaction_t * tr,
                          osip_message_t * answer);
  int _eXosip_pub_find_by_aor (eXosip_pub_t ** pub, const char *aor);
  int _eXosip_pub_init (eXosip_pub_t ** pub, const char *aor, const char *exp);
  void _eXosip_pub_free (eXosip_pub_t * pub);

  typedef struct jauthinfo_t jauthinfo_t;

  struct jauthinfo_t
  {
    char username[50];
    char userid[50];
    char passwd[50];
    char ha1[50];
    char realm[50];
    jauthinfo_t *parent;
    jauthinfo_t *next;
  };

  int
    __eXosip_create_authorization_header (osip_message_t * previous_answer,
                                          const char *rquri,
                                          const char *username,
                                          const char *passwd,
                                          const char *ha1,
                                          osip_authorization_t ** auth,
					  const char *method);
  int __eXosip_create_proxy_authorization_header (osip_message_t *
                                                  previous_answer,
                                                  const char *rquri,
                                                  const char *username,
                                                  const char *passwd,
                                                  const char *ha1,
                                                  osip_proxy_authorization_t
                                                  ** auth,
						  const char *method);

  eXosip_event_t *eXosip_event_init_for_call (int type, eXosip_call_t * jc,
                                              eXosip_dialog_t * jd,
                                              osip_transaction_t * tr);

  eXosip_event_t *eXosip_event_init_for_subscribe (int type,
                                                   eXosip_subscribe_t * js,
                                                   eXosip_dialog_t * jd,
                                                   osip_transaction_t * tr);
  eXosip_event_t *eXosip_event_init_for_notify (int type,
                                                eXosip_notify_t * jn,
                                                eXosip_dialog_t * jd,
                                                osip_transaction_t * tr);
  eXosip_event_t *eXosip_event_init_for_reg (int type, eXosip_reg_t * jr,
                                             osip_transaction_t * tr);
  eXosip_event_t *eXosip_event_init_for_message (int type,
                                                 osip_transaction_t * tr);

  int eXosip_event_init (eXosip_event_t ** je, int type);
  void report_call_event (int evt, eXosip_call_t * jc, eXosip_dialog_t * jd,
                          osip_transaction_t * tr);
  void report_options_event (int evt, osip_transaction_t * tr);
  void report_event (eXosip_event_t * je, osip_message_t * sip);
  int eXosip_event_add (eXosip_event_t * je);
  eXosip_event_t *eXosip_event_wait (int tv_s, int tv_ms);
  eXosip_event_t *eXosip_event_get (void);

  typedef void (*eXosip_callback_t) (int type, eXosip_event_t *);

  char *strdup_printf (const char *fmt, ...);

#define eXosip_trace(loglevel,args)  do        \
{                       \
	char *__strmsg;  \
	__strmsg=strdup_printf args ;    \
	OSIP_TRACE(osip_trace(__FILE__,__LINE__,(loglevel),NULL,"%s\n",__strmsg)); \
	osip_free (__strmsg);        \
}while (0);

#ifndef EXOSIP_MAX_SOCKETS
#define EXOSIP_MAX_SOCKETS 200
#endif

  /* structure used for keepalive management with connected protocols (TCP or TLS) */
  struct eXosip_socket {
    int  socket;
    char remote_ip[50];
    int  remote_port;
  };

  struct eXosip_net {
    char net_firewall_ip[50];     /* ip address to use for masquerading contacts */
    int  net_ip_family;           /* AF_INET6 or AF_INET */
    struct sockaddr_storage ai_addr;
    char net_port[20];            /* port for receiving message/connection */
    int  net_socket;              /* initial socket for receiving message/connection */
    int  net_protocol;            /* initial socket for receiving message/connection */
    struct eXosip_socket net_socket_tab[EXOSIP_MAX_SOCKETS];
  };

  char *_eXosip_transport_protocol(osip_message_t *msg);
  int _eXosip_find_protocol(osip_message_t *msg);
  int _eXosip_tcp_find_socket(char *host, int port);
  int _eXosip_tcp_connect_socket(char *host, int port);
  int setsockopt_ipv6only (int sock);

  int _eXosip_recvfrom(int s, char *buf, int len, unsigned int flags, struct sockaddr *from, socklen_t *fromlen);
  int _eXosip_sendto(int s,  const void* buf,  size_t len,  int flags,
    const struct sockaddr* to,  socklen_t tolen);

  typedef struct eXosip_t eXosip_t;

  struct eXosip_t
  {
    int forced_localip;         /* set to 1 for masquerading contact headers */
    struct eXosip_net net_interfaces[3];
    char *user_agent;

    eXosip_call_t *j_calls;             /* my calls        */
    eXosip_subscribe_t *j_subscribes;   /* my friends      */
    eXosip_notify_t *j_notifies;        /* my susbscribers */
    osip_list_t *j_transactions;

    eXosip_reg_t *j_reg;        /* my registrations */
    eXosip_pub_t *j_pub;        /* my publications  */

    void *j_cond;
    void *j_mutexlock;

    osip_t *j_osip;
    int j_stop_ua;
    void *j_thread;
    jpipe_t *j_socketctl;
    jpipe_t *j_socketctl_event;

    osip_fifo_t *j_events;

    jauthinfo_t *authinfos;

    int   keep_alive;
    int   learn_port;
    int   http_port;
    char  http_proxy[256];
    char  http_outbound_proxy[256];
  };

  typedef struct jinfo_t jinfo_t;

  struct jinfo_t
  {
    eXosip_dialog_t *jd;
    eXosip_call_t *jc;
    eXosip_subscribe_t *js;
    eXosip_notify_t *jn;
  };

  int eXosip_guess_ip_for_via (int family, char *address, int size);

/**
 * Prepare addrinfo for socket binding and resolv hostname
 * 
 * @param addrinfo  informations about the connections
 * @param hostname  hostname to resolv.
 * @param service   port number or "sip" SRV record if service=0
 */
  int eXosip_get_addrinfo (struct addrinfo **addrinfo, const char *hostname,
                           int service, int protocol);

  int eXosip_set_callbacks (osip_t * osip);
  int cb_snd_message (osip_transaction_t * tr, osip_message_t * sip,
			   char *host, int port, int out_socket);
  int cb_udp_snd_message (osip_transaction_t * tr, osip_message_t * sip,
                          char *host, int port, int out_socket);
  int cb_tcp_snd_message (osip_transaction_t * tr, osip_message_t * sip,
                          char *host, int port, int out_socket);
  char *osip_call_id_new_random (void);
  char *osip_to_tag_new_random (void);
  char *osip_from_tag_new_random (void);
  unsigned int via_branch_new_random (void);
  void __eXosip_delete_jinfo (osip_transaction_t * transaction);
  jinfo_t *__eXosip_new_jinfo (eXosip_call_t * jc, eXosip_dialog_t * jd,
                               eXosip_subscribe_t * js, eXosip_notify_t * jn);

  int eXosip_dialog_init_as_uac (eXosip_dialog_t ** jd, osip_message_t * _200Ok);
  int eXosip_dialog_init_as_uas (eXosip_dialog_t ** jd,
                                 osip_message_t * _invite,
                                 osip_message_t * _200Ok);
  void eXosip_dialog_free (eXosip_dialog_t * jd);
  void eXosip_dialog_set_state (eXosip_dialog_t * jd, int state);
  void eXosip_delete_early_dialog (eXosip_dialog_t * jd);


  int isrfc1918 (char *ipaddr);
  void eXosip_get_localip_from_via (osip_message_t *, char *localip, int size);
  int generating_request_out_of_dialog (osip_message_t ** dest,
                                        const char *method, const char *to,
                                        const char *transport,
                                        const char *from, const char *proxy);
  int generating_publish (osip_message_t ** message, const char *to,
                          const char *from, const char *route);
  int generating_cancel (osip_message_t ** dest,
                         osip_message_t * request_cancelled);
  int generating_bye (osip_message_t ** bye, osip_dialog_t * dialog, char *transport);
  int generating_ack_for_2xx (osip_message_t ** ack, osip_dialog_t * dialog);

  int eXosip_update_top_via (osip_message_t * sip);
  int eXosip_add_authentication_information (osip_message_t * req,
                                             osip_message_t * last_response);
  int _eXosip_reg_find (eXosip_reg_t ** reg, osip_transaction_t * tr);
  int eXosip_reg_init (eXosip_reg_t ** jr, const char *from,
                       const char *proxy, const char *contact);
  void eXosip_reg_free (eXosip_reg_t * jreg);
  int generating_register (osip_message_t ** reg, char *transport, char *from,
                           char *proxy, char *contact, int expires);

  int _eXosip_call_redirect_request (eXosip_call_t * jc,
                                     eXosip_dialog_t * jd,
                                     osip_transaction_t * out_tr);
  int _eXosip_call_send_request_with_credential (eXosip_call_t * jc,
                                                 eXosip_dialog_t * jd,
                                                 osip_transaction_t * out_tr);
  int eXosip_transaction_find (int tid, osip_transaction_t ** transaction);
  int eXosip_call_dialog_find (int jid, eXosip_call_t ** jc,
                               eXosip_dialog_t ** jd);
  int eXosip_notify_dialog_find (int nid, eXosip_notify_t ** jn,
                                 eXosip_dialog_t ** jd);
  int eXosip_subscribe_dialog_find (int nid, eXosip_subscribe_t ** js,
                                    eXosip_dialog_t ** jd);
  int eXosip_call_find (int cid, eXosip_call_t ** jc);
  int eXosip_dialog_set_200ok (eXosip_dialog_t * _jd, osip_message_t * _200Ok);

  int _eXosip_answer_invite_3456xx (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                    int code, osip_message_t ** answer);
  int _eXosip_answer_invite_2xx (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                 int code, osip_message_t ** answer);
  int _eXosip_answer_invite_1xx (eXosip_call_t * jc, eXosip_dialog_t * jd,
                                 int code, osip_message_t ** answer);
  int _eXosip_default_answer_invite_1xx (eXosip_call_t * jc,
                                         eXosip_dialog_t * jd, int code);
  int _eXosip_default_answer_invite_3456xx (eXosip_call_t * jc,
                                            eXosip_dialog_t * jd, int code);
  int _eXosip_insubscription_answer_1xx (eXosip_notify_t * jc,
                                         eXosip_dialog_t * jd, int code);
  int _eXosip_insubscription_answer_2xx (eXosip_notify_t * jn,
                                         eXosip_dialog_t * jd, int code);
  int _eXosip_insubscription_answer_3456xx (eXosip_notify_t * jn,
                                            eXosip_dialog_t * jd, int code);

  int eXosip_build_response_default (int jid, int status);
  int _eXosip_build_response_default (osip_message_t ** dest,
                                      osip_dialog_t * dialog, int status,
                                      osip_message_t * request);
  int complete_answer_that_establish_a_dialog (osip_message_t * response,
                                               osip_message_t * request);
  int _eXosip_build_request_within_dialog (osip_message_t ** dest,
                                           const char *method,
                                           osip_dialog_t * dialog,
                                           const char *transport);
  void eXosip_kill_transaction (osip_list_t * transactions);
  int eXosip_remove_transaction_from_call (osip_transaction_t * tr,
                                           eXosip_call_t * jc);
  osip_transaction_t *eXosip_find_last_inc_notify (eXosip_subscribe_t * jn,
                                                   eXosip_dialog_t * jd);
  osip_transaction_t *eXosip_find_last_out_notify (eXosip_notify_t * jn,
                                                   eXosip_dialog_t * jd);
  osip_transaction_t *eXosip_find_last_inc_subscribe (eXosip_notify_t * jn,
                                                      eXosip_dialog_t * jd);
  osip_transaction_t *eXosip_find_last_out_subscribe (eXosip_subscribe_t * js,
                                                      eXosip_dialog_t * jd);

  osip_transaction_t *eXosip_find_last_transaction (eXosip_call_t * jc,
                                                    eXosip_dialog_t * jd,
                                                    const char *method);
  osip_transaction_t *eXosip_find_last_inc_transaction (eXosip_call_t * jc,
                                                        eXosip_dialog_t * jd,
                                                        const char *method);
  osip_transaction_t *eXosip_find_last_out_transaction (eXosip_call_t * jc,
                                                        eXosip_dialog_t * jd,
                                                        const char *method);
  osip_transaction_t *eXosip_find_last_invite (eXosip_call_t * jc,
                                               eXosip_dialog_t * jd);
  osip_transaction_t *eXosip_find_last_inc_invite (eXosip_call_t * jc,
                                                   eXosip_dialog_t * jd);
  osip_transaction_t *eXosip_find_last_out_invite (eXosip_call_t * jc,
                                                   eXosip_dialog_t * jd);


  int eXosip_call_init (eXosip_call_t ** jc);
  void eXosip_call_free (eXosip_call_t * jc);
  void __eXosip_call_remove_dialog_reference_in_call (eXosip_call_t * jc,
                                                      eXosip_dialog_t * jd);
  int eXosip_read_message (int max_message_nb, int sec_max, int usec_max);
  void eXosip_release_terminated_calls (void);
  void eXosip_release_terminated_registrations (void);


  int eXosip_subscribe_init (eXosip_subscribe_t ** js);
  void eXosip_subscribe_free (eXosip_subscribe_t * js);
  int _eXosip_subscribe_set_refresh_interval (eXosip_subscribe_t * js,
                                              osip_message_t * inc_subscribe);
  int eXosip_subscribe_need_refresh (eXosip_subscribe_t * js,
                                     eXosip_dialog_t * jd, int now);
  int _eXosip_subscribe_send_request_with_credential (eXosip_subscribe_t * js,
                                                      eXosip_dialog_t * jd,
                                                      osip_transaction_t * out_tr);
  int eXosip_notify_init (eXosip_notify_t ** jn, osip_message_t * inc_subscribe);
  void eXosip_notify_free (eXosip_notify_t * jn);
  int _eXosip_notify_set_contact_info (eXosip_notify_t * jn, char *uri);
  int _eXosip_notify_set_refresh_interval (eXosip_notify_t * jn,
                                           osip_message_t * inc_subscribe);
  void _eXosip_notify_add_expires_in_2XX_for_subscribe (eXosip_notify_t * jn,
                                                        osip_message_t * answer);
  int _eXosip_insubscription_send_request_with_credential (eXosip_notify_t *
                                                           jn,
                                                           eXosip_dialog_t *
                                                           jd,
                                                           osip_transaction_t
                                                           * out_tr);

  int eXosip_is_public_address (const char *addr);

#ifdef __cplusplus
}
#endif

#endif
