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



#ifndef  WIN32
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

/* Private functions */
static jauthinfo_t *eXosip_find_authentication_info (const char *username,
                                                     const char *realm);

eXosip_t eXosip;

void
__eXosip_wakeup (void)
{
  jpipe_write (eXosip.j_socketctl, "w", 1);
}

void
__eXosip_wakeup_event (void)
{
  jpipe_write (eXosip.j_socketctl_event, "w", 1);
}


int
eXosip_lock (void)
{
  return osip_mutex_lock ((struct osip_mutex *) eXosip.j_mutexlock);
}

int
eXosip_unlock (void)
{
  return osip_mutex_unlock ((struct osip_mutex *) eXosip.j_mutexlock);
}

int
eXosip_transaction_find (int tid, osip_transaction_t ** transaction)
{
  int pos = 0;

  *transaction = NULL;
  while (!osip_list_eol (eXosip.j_transactions, pos))
    {
      osip_transaction_t *tr;

      tr = (osip_transaction_t *) osip_list_get (eXosip.j_transactions, pos);
      if (tr->transactionid == tid)
        {
          *transaction = tr;
          return 0;
        }
      pos++;
    }
  return -1;
}

void
eXosip_automatic_action (void)
{
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_dialog_t *jd;
  eXosip_notify_t *jn;

  eXosip_reg_t *jr;
  int now;

  now = time (NULL);

  for (jc = eXosip.j_calls; jc != NULL; jc = jc->next)
    {
      if (jc->c_id < 1)
        {
      } else if (jc->c_dialogs == NULL || jc->c_dialogs->d_dialog == NULL)
        {
          /* an EARLY dialog may have failed with 401,407 or 3Xx */

          osip_transaction_t *out_tr = NULL;

          out_tr = jc->c_out_tr;

          if (out_tr != NULL
              && (out_tr->state == ICT_TERMINATED
                  || out_tr->state == NICT_TERMINATED
                  || out_tr->state == ICT_COMPLETED
                  || out_tr->state == NICT_COMPLETED) &&
              now - out_tr->birth_time < 120 &&
              out_tr->orig_request != NULL &&
              out_tr->last_response != NULL &&
              (out_tr->last_response->status_code == 401
               || out_tr->last_response->status_code == 407))
            {
              /* retry with credential */
              if (jc->c_retry < 3)
                {
                  int i;

                  i = _eXosip_call_send_request_with_credential (jc, NULL, out_tr);
                  if (i != 0)
                    {
                      OSIP_TRACE (osip_trace
                                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                   "eXosip: could not clone msg for authentication\n"));
                    }
                  jc->c_retry++;
                }
          } else if (out_tr != NULL
                     && (out_tr->state == ICT_TERMINATED
                         || out_tr->state == NICT_TERMINATED
                         || out_tr->state == ICT_COMPLETED
                         || out_tr->state == NICT_COMPLETED) &&
                     now - out_tr->birth_time < 120 &&
                     out_tr->orig_request != NULL &&
                     out_tr->last_response != NULL &&
                     (out_tr->last_response->status_code >= 300
                      && out_tr->last_response->status_code <= 399))
            {
              /* retry with credential */
              int i;

              i = _eXosip_call_redirect_request (jc, NULL, out_tr);
              if (i != 0)
                {
                  OSIP_TRACE (osip_trace
                              (__FILE__, __LINE__, OSIP_ERROR, NULL,
                               "eXosip: could not clone msg for redirection\n"));
                }
            }
        }

      for (jd = jc->c_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog == NULL)     /* finished call */
            {
          } else
            {
              osip_transaction_t *out_tr = NULL;

              out_tr = osip_list_get (jd->d_out_trs, 0);
              if (out_tr == NULL)
                out_tr = jc->c_out_tr;

              if (out_tr != NULL
                  && (out_tr->state == ICT_TERMINATED
                      || out_tr->state == NICT_TERMINATED
                      || out_tr->state == ICT_COMPLETED
                      || out_tr->state == NICT_COMPLETED) &&
                  now - out_tr->birth_time < 120 &&
                  out_tr->orig_request != NULL &&
                  out_tr->last_response != NULL &&
                  (out_tr->last_response->status_code == 401
                   || out_tr->last_response->status_code == 407))
                {
                  /* retry with credential */
                  if (jd->d_retry < 3)
                    {
                      int i;

                      i =
                        _eXosip_call_send_request_with_credential (jc, jd, out_tr);
                      if (i != 0)
                        {
                          OSIP_TRACE (osip_trace
                                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                       "eXosip: could not clone msg for authentication\n"));
                        }
                      jd->d_retry++;
                    }
              } else if (out_tr != NULL
                         && (out_tr->state == ICT_TERMINATED
                             || out_tr->state == NICT_TERMINATED
                             || out_tr->state == ICT_COMPLETED
                             || out_tr->state == NICT_COMPLETED) &&
                         now - out_tr->birth_time < 120 &&
                         out_tr->orig_request != NULL &&
                         out_tr->last_response != NULL &&
                         (out_tr->last_response->status_code >= 300
                          && out_tr->last_response->status_code <= 399))
                {
                  /* retry with credential */
                  int i;

                  i = _eXosip_call_redirect_request (jc, jd, out_tr);
                  if (i != 0)
                    {
                      OSIP_TRACE (osip_trace
                                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                   "eXosip: could not clone msg for redirection\n"));
                    }
                }
            }
        }
    }

  for (js = eXosip.j_subscribes; js != NULL; js = js->next)
    {
      if (js->s_id < 1)
        {
      } else if (js->s_dialogs == NULL)
        {
          osip_transaction_t *out_tr = NULL;

          out_tr = js->s_out_tr;

          if (out_tr != NULL
              && (out_tr->state == NICT_TERMINATED
                  || out_tr->state == NICT_COMPLETED) &&
              now - out_tr->birth_time < 120 &&
              out_tr->orig_request != NULL &&
              out_tr->last_response != NULL &&
              (out_tr->last_response->status_code == 401
               || out_tr->last_response->status_code == 407))
            {
              /* retry with credential */
              if (js->s_retry < 3)
                {
                  int i;

                  i =
                    _eXosip_subscribe_send_request_with_credential (js, NULL,
                                                                    out_tr);
                  if (i != 0)
                    {
                      OSIP_TRACE (osip_trace
                                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                   "eXosip: could not clone msg for authentication\n"));
                    }
                  js->s_retry++;
                }
            }
        }

      for (jd = js->s_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)     /* finished call */
            {
              if (jd->d_id >= 1)
                {
                  osip_transaction_t *out_tr = NULL;

                  out_tr = osip_list_get (jd->d_out_trs, 0);
                  if (out_tr == NULL)
                    out_tr = js->s_out_tr;

                  if (out_tr != NULL
                      && (out_tr->state == NICT_TERMINATED
                          || out_tr->state == NICT_COMPLETED) &&
                      now - out_tr->birth_time < 120 &&
                      out_tr->orig_request != NULL &&
                      out_tr->last_response != NULL &&
                      (out_tr->last_response->status_code == 401
                       || out_tr->last_response->status_code == 407))
                    {
                      /* retry with credential */
                      if (jd->d_retry < 3)
                        {
                          int i;

                          i =
                            _eXosip_subscribe_send_request_with_credential
                            (js, jd, out_tr);
                          if (i != 0)
                            {
                              OSIP_TRACE (osip_trace
                                          (__FILE__, __LINE__, OSIP_ERROR,
                                           NULL,
                                           "eXosip: could not clone suscbribe for authentication\n"));
                            }
                          jd->d_retry++;
                        }
                  } else if (js->s_reg_period == 0 || out_tr == NULL)
                    {
                  } else if (now - out_tr->birth_time > js->s_reg_period - 60)
                    {           /* will expires in 60 sec: send refresh! */
                      int i;

                      i =
                        _eXosip_subscribe_send_request_with_credential (js,
                                                                        jd,
                                                                        out_tr);
                      if (i != 0)
                        {
                          OSIP_TRACE (osip_trace
                                      (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                       "eXosip: could not clone subscribe for refresh\n"));
                        }
                    }
                }
            }
        }
    }


  for (jn = eXosip.j_notifies; jn != NULL; jn = jn->next)
    {
      for (jd = jn->n_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)     /* finished call */
            {
              if (jd->d_id >= 1)
                {
                  osip_transaction_t *out_tr = NULL;

                  out_tr = osip_list_get (jd->d_out_trs, 0);

                  if (out_tr != NULL
                      && (out_tr->state == NICT_TERMINATED
                          || out_tr->state == NICT_COMPLETED) &&
                      now - out_tr->birth_time < 120 &&
                      out_tr->orig_request != NULL &&
                      out_tr->last_response != NULL &&
                      (out_tr->last_response->status_code == 401
                       || out_tr->last_response->status_code == 407))
                    {
                      /* retry with credential */
                      if (jd->d_retry < 3)
                        {
                          int i;

                          i =
                            _eXosip_insubscription_send_request_with_credential
                            (jn, jd, out_tr);
                          if (i != 0)
                            {
                              OSIP_TRACE (osip_trace
                                          (__FILE__, __LINE__, OSIP_ERROR,
                                           NULL,
                                           "eXosip: could not clone notify for authentication\n"));
                            }
                          jd->d_retry++;
                        }
                    }
                }
            }
        }
    }


  for (jr = eXosip.j_reg; jr != NULL; jr = jr->next)
    {
      if (jr->r_id >= 1 && jr->r_last_tr != NULL)
        {
          if (jr->r_reg_period == 0)
            {
              /* skip refresh! */
          } else if (now - jr->r_last_tr->birth_time > 900)
            {
              /* automatic refresh */
              eXosip_register_send_register (jr->r_id, NULL);
          } else if (now - jr->r_last_tr->birth_time > jr->r_reg_period - 60)
            {
              /* automatic refresh */
              eXosip_register_send_register (jr->r_id, NULL);
          } else if (now - jr->r_last_tr->birth_time > 120 &&
                     (jr->r_last_tr->last_response == NULL
                      || (!MSG_IS_STATUS_2XX (jr->r_last_tr->last_response))))
            {
              /* automatic refresh */
              eXosip_register_send_register (jr->r_id, NULL);
          } else if (now - jr->r_last_tr->birth_time < 120 &&
                     jr->r_last_tr->orig_request != NULL &&
                     (jr->r_last_tr->last_response != NULL
                      && (jr->r_last_tr->last_response->status_code == 401
                          || jr->r_last_tr->last_response->status_code == 407)))
            {
              if (jr->r_retry < 3)
                {
                  /* TODO: improve support for several retries when
                     several credentials are needed */
                  eXosip_register_send_register (jr->r_id, NULL);
                  jr->r_retry++;
                }
            }
        }
    }
}

void
eXosip_update ()
{
  static int static_id = 1;
  eXosip_call_t *jc;
  eXosip_subscribe_t *js;
  eXosip_notify_t *jn;
  eXosip_dialog_t *jd;
  int now;

  if (static_id > 100000)
    static_id = 1;              /* loop */

  now = time (NULL);
  for (jc = eXosip.j_calls; jc != NULL; jc = jc->next)
    {
      if (jc->c_id < 1)
        {
          jc->c_id = static_id;
          static_id++;
        }
      for (jd = jc->c_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)     /* finished call */
            {
              if (jd->d_id < 1)
                {
                  jd->d_id = static_id;
                  static_id++;
                }
          } else
            jd->d_id = -1;
        }
    }

  for (js = eXosip.j_subscribes; js != NULL; js = js->next)
    {
      if (js->s_id < 1)
        {
          js->s_id = static_id;
          static_id++;
        }
      for (jd = js->s_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)     /* finished call */
            {
              if (jd->d_id < 1)
                {
                  jd->d_id = static_id;
                  static_id++;
                }
          } else
            jd->d_id = -1;
        }
    }

  for (jn = eXosip.j_notifies; jn != NULL; jn = jn->next)
    {
      if (jn->n_id < 1)
        {
          jn->n_id = static_id;
          static_id++;
        }
      for (jd = jn->n_dialogs; jd != NULL; jd = jd->next)
        {
          if (jd->d_dialog != NULL)     /* finished call */
            {
              if (jd->d_id < 1)
                {
                  jd->d_id = static_id;
                  static_id++;
                }
          } else
            jd->d_id = -1;
        }
    }
}

static jauthinfo_t *
eXosip_find_authentication_info (const char *username, const char *realm)
{
  jauthinfo_t *fallback = NULL;
  jauthinfo_t *authinfo;

  for (authinfo = eXosip.authinfos; authinfo != NULL; authinfo = authinfo->next)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "INFO: authinfo: %s %s\n", realm, authinfo->realm));
      if (0 == strcmp (authinfo->username, username))
        {
          if (authinfo->realm == NULL || authinfo->realm[0] == '\0')
            {
              fallback = authinfo;
          } else if (strcmp (realm, authinfo->realm) == 0
                     || 0 == strncmp (realm + 1, authinfo->realm,
                                      strlen (realm) - 2))
            {
              return authinfo;
            }
        }
    }

  /* no matching username has been found for this realm,
     try with another username... */
  for (authinfo = eXosip.authinfos; authinfo != NULL; authinfo = authinfo->next)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "INFO: authinfo: %s %s\n", realm, authinfo->realm));
      if (authinfo->realm == NULL || authinfo->realm[0] == '\0')
        {
          fallback = authinfo;
      } else if (strcmp (realm, authinfo->realm) == 0
                 || 0 == strncmp (realm + 1, authinfo->realm, strlen (realm) - 2))
        {
          return authinfo;
        }
    }

  return fallback;
}


int
eXosip_clear_authentication_info ()
{
  jauthinfo_t *jauthinfo;

  for (jauthinfo = eXosip.authinfos; jauthinfo != NULL;
       jauthinfo = eXosip.authinfos)
    {
      REMOVE_ELEMENT (eXosip.authinfos, jauthinfo);
      osip_free (jauthinfo);
    }
  return 0;
}

int
eXosip_add_authentication_info (const char *username, const char *userid,
                                const char *passwd, const char *ha1,
                                const char *realm)
{
  jauthinfo_t *authinfos;

  if (username == NULL || username[0] == '\0')
    return -1;
  if (userid == NULL || userid[0] == '\0')
    return -1;

  if (passwd != NULL && passwd[0] != '\0')
    {
  } else if (ha1 != NULL && ha1[0] != '\0')
    {
  } else
    return -1;

  authinfos = (jauthinfo_t *) osip_malloc (sizeof (jauthinfo_t));
  if (authinfos == NULL)
    return -1;
  memset (authinfos, 0, sizeof (jauthinfo_t));

  snprintf (authinfos->username, 50, "%s", username);
  snprintf (authinfos->userid, 50, "%s", userid);
  if (passwd != NULL && passwd[0] != '\0')
    snprintf (authinfos->passwd, 50, "%s", passwd);
  else if (ha1 != NULL && ha1[0] != '\0')
    snprintf (authinfos->ha1, 50, "%s", ha1);
  if (realm != NULL && realm[0] != '\0')
    snprintf (authinfos->realm, 50, "%s", realm);

  ADD_ELEMENT (eXosip.authinfos, authinfos);
  return 0;
}

int
eXosip_add_authentication_information (osip_message_t * req,
                                       osip_message_t * last_response)
{
  osip_authorization_t *aut = NULL;
  osip_www_authenticate_t *wwwauth = NULL;
  osip_proxy_authorization_t *proxy_aut = NULL;
  osip_proxy_authenticate_t *proxyauth = NULL;
  jauthinfo_t *authinfo = NULL;
  int pos;
  int i;

  if (req == NULL
      || req->from == NULL
      || req->from->url == NULL || req->from->url->username == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "authinfo: Invalid message\n"));
      return -1;
    }

  pos = 0;
  osip_message_get_www_authenticate (last_response, pos, &wwwauth);
  osip_message_get_proxy_authenticate (last_response, pos, &proxyauth);
  if (wwwauth == NULL && proxyauth == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "authinfo: No WWW-Authenticate or Proxy-Authenticate\n"));
      return -1;
    }

  while (wwwauth != NULL)
    {
      char *uri;

      authinfo = eXosip_find_authentication_info (req->from->url->username,
                                                  wwwauth->realm);
      if (authinfo == NULL)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_INFO2, NULL,
		       "authinfo: No authentication found for %s %s\n",
		       req->from->url->username, wwwauth->realm));
	  return -1;
	}
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "authinfo: %s\n", authinfo->username));
      i = osip_uri_to_str (req->req_uri, &uri);
      if (i != 0)
        return -1;

      i = __eXosip_create_authorization_header (last_response, uri,
                                                authinfo->userid,
                                                authinfo->passwd,
                                                authinfo->ha1, &aut,
						req->sip_method);
      osip_free (uri);
      if (i != 0)
        return -1;

      if (aut != NULL)
        {
          osip_list_add (req->authorizations, aut, -1);
          osip_message_force_update (req);
        }

      pos++;
      osip_message_get_www_authenticate (last_response, pos, &wwwauth);
    }

  pos = 0;
  while (proxyauth != NULL)
    {
      char *uri;

      authinfo = eXosip_find_authentication_info (req->from->url->username,
                                                  proxyauth->realm);
      if (authinfo == NULL)
	{
	  OSIP_TRACE (osip_trace
		      (__FILE__, __LINE__, OSIP_INFO2, NULL,
		       "authinfo: No authentication found for %s %s\n",
		       req->from->url->username, proxyauth->realm));
	  return -1;
	}
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO1, NULL,
                   "authinfo: %s\n", authinfo->username));
      i = osip_uri_to_str (req->req_uri, &uri);
      if (i != 0)
        return -1;

      i = __eXosip_create_proxy_authorization_header (last_response, uri,
                                                      authinfo->userid,
                                                      authinfo->passwd,
                                                      authinfo->ha1,
                                                      &proxy_aut,
						      req->sip_method);
      osip_free (uri);
      if (i != 0)
        return -1;

      if (proxy_aut != NULL)
        {
          osip_list_add (req->proxy_authorizations, proxy_aut, -1);
          osip_message_force_update (req);
        }

      pos++;
      osip_message_get_proxy_authenticate (last_response, pos, &proxyauth);
    }

  return 0;
}

int
eXosip_update_top_via (osip_message_t * sip)
{
  char locip[50];
  char *tmp = (char *) osip_malloc (256 * sizeof (char));
  osip_via_t *via = (osip_via_t *) osip_list_get (sip->vias, 0);
  int i;

  i = _eXosip_find_protocol(sip);

  osip_list_remove (sip->vias, 0);
  osip_via_free (via);
#ifdef SM
  eXosip_get_localip_for (sip->req_uri->host, locip, 49);
#else 
  if (i==IPPROTO_UDP)
    eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip, 49);
  else if (i==IPPROTO_TCP)
     eXosip_guess_ip_for_via (eXosip.net_interfaces[1].net_ip_family, locip, 49);
  else
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: unsupported protocol (using default UDP)\n"));
      eXosip_guess_ip_for_via (eXosip.net_interfaces[0].net_ip_family, locip, 49);
    }
#endif
  if (i==IPPROTO_UDP)
    {
      if (eXosip.net_interfaces[0].net_ip_family == AF_INET6)
	snprintf (tmp, 256, "SIP/2.0/UDP [%s]:%s;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[0].net_port,
		  via_branch_new_random ());
      else
	snprintf (tmp, 256, "SIP/2.0/UDP %s:%s;rport;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[0].net_port,
		  via_branch_new_random ());
    }
  else if (i==IPPROTO_TCP)
    {
      if (eXosip.net_interfaces[1].net_ip_family == AF_INET6)
	snprintf (tmp, 256, "SIP/2.0/TCP [%s]:%s;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[1].net_port,
		  via_branch_new_random ());
      else
	snprintf (tmp, 256, "SIP/2.0/TCP %s:%s;rport;branch=z9hG4bK%u",
		  locip, eXosip.net_interfaces[1].net_port,
		  via_branch_new_random ());
    }

  osip_via_init (&via);
  osip_via_parse (via, tmp);
  osip_list_add (sip->vias, via, 0);
  osip_free (tmp);

  return 0;
}
