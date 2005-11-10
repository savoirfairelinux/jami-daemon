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

#ifndef __EX_SETUP_H__
#define __EX_SETUP_H__

#include <osipparser2/osip_parser.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file eX_setup.h
 * @brief eXosip setup API
 *
 * This file provide the API needed to setup and configure
 * the SIP endpoint.
 *
 */

/**
 * @defgroup eXosip2_conf eXosip2 configuration API
 * @ingroup eXosip2_setup
 * @{
 */

/**
 * Initiate the eXtented oSIP library.
 * 
 */
  int eXosip_init(void);

/**
 * Release ressource used by the eXtented oSIP library.
 * 
 */
  void eXosip_quit(void);

typedef enum {
    EXOSIP_OPT_UDP_KEEP_ALIVE = 1,
    EXOSIP_OPT_UDP_LEARN_PORT = 2,
    EXOSIP_OPT_SET_HTTP_TUNNEL_PORT = 3,
    EXOSIP_OPT_SET_HTTP_TUNNEL_PROXY = 4,
    EXOSIP_OPT_SET_HTTP_OUTBOUND_PROXY = 5 /* used for http tunnel ONLY */
} eXosip_option;

/**
 * Set eXosip options.
 * See eXosip_option for available options.
 *
 * @param opt     option to configure.
 * @param value   value for options.
 * 
 */
int eXosip_set_option(eXosip_option opt, void *value);

/**
 * Lock the eXtented oSIP library.
 * 
 */
  int eXosip_lock(void);

/**
 * UnLock the eXtented oSIP library.
 * 
 */
  int eXosip_unlock(void);

/**
 * Listen on a specified socket.
 * 
 * @param transport IPPROTO_UDP for udp. (soon to come: TCP/TLS?)
 * @param addr      the address to bind (NULL for all interface)
 * @param port      the listening port. (0 for random port)
 * @param family    the IP family (AF_INET or AF_INET6).
 * @param secure    0 for UDP or TCP, 1 for TLS (with TCP).
 */
  int eXosip_listen_addr(int transport, const char *addr, int port, int family,
			 int secure);

/**
 * Listen on a specified socket.
 * 
 * @param transport IPPROTO_UDP for udp. (soon to come: TCP/TLS?)
 * @param socket socket to use for listening to UDP sip messages.
 * @param port the listening port for masquerading.
 */
  int eXosip_set_socket(int transport, int socket, int port);

/**
 * Set the SIP User-Agent: header string.
 *
 * @param user_agent the User-Agent header to insert in messages.
 */
  void eXosip_set_user_agent(const char *user_agent);

/**
 * Use IPv6 instead of IPv4.
 * 
 * @param ipv6_enable  This paramter should be set to 1 to enable IPv6 mode.
 */
  void eXosip_enable_ipv6(int ipv6_enable);

/**
 * This method is used to replace contact address with
 * the public address of your NAT. The ip address should
 * be retreived manually (fixed IP address) or with STUN.
 * This address will only be used when the remote
 * correspondant appears to be on an DIFFERENT LAN.
 *
 * @param public_address 	the ip address.
 * 
 * If set to NULL, then the local ip address will be guessed 
 * automatically (returns to default mode).
 */
  void eXosip_masquerade_contact(const char *public_address, int port);

#ifndef DOXYGEN

/**
 * Force eXosip to use a specific ip address in all
 * contact and Via headers in SIP message.
 * **PLEASE DO NOT USE: use eXosip_masquerade_contact instead**
 *
 * @param localip 	the ip address.
 *
 * If set to NULL, then the local ip address will be guessed 
 * automatically (returns to default mode).
 *
 * ******LINPHONE specific methods******
 *
 */
int eXosip_force_masquerade_contact(const char *localip);

/**
 * Wake Up the eXosip_event_wait method.
 * 
 */
  void __eXosip_wakeup_event(void);

#define REMOVE_ELEMENT(first_element, element)   \
       if (element->parent==NULL)                \
	{ first_element = element->next;         \
          if (first_element!=NULL)               \
          first_element->parent = NULL; }        \
       else \
        { element->parent->next = element->next; \
          if (element->next!=NULL)               \
	element->next->parent = element->parent; \
	element->next = NULL;                    \
	element->parent = NULL; }

#define ADD_ELEMENT(first_element, element) \
   if (first_element==NULL)                 \
    {                                       \
      first_element   = element;            \
      element->next   = NULL;               \
      element->parent = NULL;               \
    }                                       \
  else                                      \
    {                                       \
      element->next   = first_element;      \
      element->parent = NULL;               \
      element->next->parent = element;      \
      first_element = element;              \
    }

#define APPEND_ELEMENT(type_of_element_t, first_element, element) \
  if (first_element==NULL)                            \
    { first_element = element;                        \
      element->next   = NULL; /* useless */           \
      element->parent = NULL; /* useless */ }         \
  else                                                \
    { type_of_element_t *f;                           \
      for (f=first_element; f->next!=NULL; f=f->next) \
         { }                                          \
      f->next    = element;                           \
      element->parent = f;                            \
      element->next   = NULL;                         \
    }

#endif

/** @} */


/**
 * @defgroup eXosip2_network eXosip2 network API
 * @ingroup eXosip2_setup
 * @{
 */

/**
 * Modify the transport protocol used to send SIP message.
 * 
 * @param msg         The SIP message to modify
 * @param transport   transport protocol to use ("UDP", "TCP" or "TLS")
 */
  int eXosip_transport_set(osip_message_t *msg, const char *transport);

/**
 * Find the current localip (interface with default route).
 * 
 * @param family    AF_INET or AF_INET6
 * @param address   a string containing the local IP address.
 * @param size      The size of the string
 */
  int eXosip_guess_localip(int family, char *address, int size);

#ifndef DOXYGEN

/**
 * Find the interface to be used to reach the specified host.
 * 
 * @param ip    a string containing the local IP address.
 * @param localip	the local ip address to be used to reach host.
 *
 * You usually don't need this function at all.
 *
 * ******LINPHONE specific methods******
 *
 */
int eXosip_get_localip_for(const char *host, char *localip, int size);

#endif

/** @} */


#ifdef __cplusplus
}
#endif

#endif
