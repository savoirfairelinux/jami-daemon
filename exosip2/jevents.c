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

#include "eXosip2.h"
#include <eXosip2/eXosip.h>
#include <osip2/osip_condv.h>

extern eXosip_t eXosip;

static int _eXosip_event_fill_messages (eXosip_event_t * je,
                                        osip_transaction_t * tr);

static int
_eXosip_event_fill_messages (eXosip_event_t * je, osip_transaction_t * tr)
{
  int i;

  if (tr != NULL && tr->orig_request != NULL)
    {
      i = osip_message_clone (tr->orig_request, &je->request);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                  "failed to clone request for event\n"));
        }
    }
  if (tr != NULL && tr->last_response != NULL)
    {
      i = osip_message_clone (tr->last_response, &je->response);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                  "failed to clone response for event\n"));
        }
    }
  if (tr != NULL && tr->ack != NULL)
    {
      i = osip_message_clone (tr->ack, &je->ack);
      if (i != 0)
        {
          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
                                  "failed to clone ACK for event\n"));
        }
    }
  return 0;
}

eXosip_event_t *
eXosip_event_init_for_call (int type, eXosip_call_t * jc,
                            eXosip_dialog_t * jd, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  eXosip_event_init (&je, type);
  if (je == NULL)
    return NULL;
  if (jc == NULL)
    return NULL;

  je->cid = jc->c_id;
  if (jd != NULL)
    je->did = jd->d_id;
  if (tr != NULL)
    je->tid = tr->transactionid;

  je->external_reference = jc->external_reference;

  _eXosip_event_fill_messages (je, tr);
  return je;
}

eXosip_event_t *
eXosip_event_init_for_subscribe (int type, eXosip_subscribe_t * js,
                                 eXosip_dialog_t * jd, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  eXosip_event_init (&je, type);
  if (je == NULL)
    return NULL;
  if (js == NULL)
    return NULL;

  je->sid = js->s_id;
  if (jd != NULL)
    je->did = jd->d_id;
  if (tr != NULL)
    je->tid = tr->transactionid;

  je->ss_status = js->s_ss_status;
  je->ss_reason = js->s_ss_reason;

  /* je->external_reference = js->external_reference; */

  _eXosip_event_fill_messages (je, tr);

  return je;
}

eXosip_event_t *
eXosip_event_init_for_notify (int type, eXosip_notify_t * jn,
                              eXosip_dialog_t * jd, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  eXosip_event_init (&je, type);
  if (je == NULL)
    return NULL;
  if (jn == NULL)
    return NULL;

  je->nid = jn->n_id;
  if (jd != NULL)
    je->did = jd->d_id;
  if (tr != NULL)
    je->tid = tr->transactionid;

  je->ss_status = jn->n_ss_status;
  je->ss_reason = jn->n_ss_reason;

  /*je->external_reference = jc->external_reference; */

  _eXosip_event_fill_messages (je, tr);

  return je;
}

eXosip_event_t *
eXosip_event_init_for_reg (int type, eXosip_reg_t * jr, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  eXosip_event_init (&je, type);
  if (je == NULL)
    return NULL;
  if (jr == NULL)
    return NULL;
  je->rid = jr->r_id;

  _eXosip_event_fill_messages (je, tr);
  return je;
}

eXosip_event_t *
eXosip_event_init_for_message (int type, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  eXosip_event_init (&je, type);
  if (je == NULL)
    return NULL;

  if (tr != NULL)
    je->tid = tr->transactionid;

  _eXosip_event_fill_messages (je, tr);

  return je;
}

int
eXosip_event_init (eXosip_event_t ** je, int type)
{
  *je = (eXosip_event_t *) osip_malloc (sizeof (eXosip_event_t));
  if (*je == NULL)
    return -1;

  memset (*je, 0, sizeof (eXosip_event_t));
  (*je)->type = type;

  if (type == EXOSIP_CALL_NOANSWER)
    {
      sprintf ((*je)->textinfo, "No answer for this Call!");
  } else if (type == EXOSIP_CALL_PROCEEDING)
    {
      sprintf ((*je)->textinfo, "Call is being processed!");
  } else if (type == EXOSIP_CALL_RINGING)
    {
      sprintf ((*je)->textinfo, "Remote phone is ringing!");
  } else if (type == EXOSIP_CALL_ANSWERED)
    {
      sprintf ((*je)->textinfo, "Remote phone has answered!");
  } else if (type == EXOSIP_CALL_REDIRECTED)
    {
      sprintf ((*je)->textinfo, "Call is redirected!");
  } else if (type == EXOSIP_CALL_REQUESTFAILURE)
    {
      sprintf ((*je)->textinfo, "4xx received for Call!");
  } else if (type == EXOSIP_CALL_SERVERFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for Call!");
  } else if (type == EXOSIP_CALL_GLOBALFAILURE)
    {
      sprintf ((*je)->textinfo, "6xx received for Call!");
  } else if (type == EXOSIP_CALL_INVITE)
    {
      sprintf ((*je)->textinfo, "New call received!");
  } else if (type == EXOSIP_CALL_ACK)
    {
      sprintf ((*je)->textinfo, "ACK received!");
  } else if (type == EXOSIP_CALL_CANCELLED)
    {
      sprintf ((*je)->textinfo, "Call has been cancelled!");
  } else if (type == EXOSIP_CALL_TIMEOUT)
    {
      sprintf ((*je)->textinfo, "Timeout. Gave up!");
  } else if (type == EXOSIP_CALL_REINVITE)
    {
      sprintf ((*je)->textinfo, "INVITE within call received!");
  } else if (type == EXOSIP_CALL_CLOSED)
    {
      sprintf ((*je)->textinfo, "Bye Received!");
  } else if (type == EXOSIP_CALL_RELEASED)
    {
      sprintf ((*je)->textinfo, "Call Context is released!");
  } else if (type == EXOSIP_REGISTRATION_SUCCESS)
    {
      sprintf ((*je)->textinfo, "User is successfully registred!");
  } else if (type == EXOSIP_REGISTRATION_FAILURE)
    {
      sprintf ((*je)->textinfo, "Registration failed!");
  } else if (type == EXOSIP_CALL_MESSAGE_NEW)
    {
      sprintf ((*je)->textinfo, "New request received!");
  } else if (type == EXOSIP_CALL_MESSAGE_PROCEEDING)
    {
      sprintf ((*je)->textinfo, "request is being processed!");
  } else if (type == EXOSIP_CALL_MESSAGE_ANSWERED)
    {
      sprintf ((*je)->textinfo, "2xx received for request!");
  } else if (type == EXOSIP_CALL_MESSAGE_REDIRECTED)
    {
      sprintf ((*je)->textinfo, "3xx received for request!");
  } else if (type == EXOSIP_CALL_MESSAGE_REQUESTFAILURE)
    {
      sprintf ((*je)->textinfo, "4xx received for request!");
  } else if (type == EXOSIP_CALL_MESSAGE_SERVERFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for request!");
  } else if (type == EXOSIP_CALL_MESSAGE_GLOBALFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for request!");
  } else if (type == EXOSIP_MESSAGE_NEW)
    {
      sprintf ((*je)->textinfo, "New request outside call received!");
  } else if (type == EXOSIP_MESSAGE_PROCEEDING)
    {
      sprintf ((*je)->textinfo, "request outside call is being processed!");
  } else if (type == EXOSIP_MESSAGE_ANSWERED)
    {
      sprintf ((*je)->textinfo, "2xx received for request outside call!");
  } else if (type == EXOSIP_MESSAGE_REDIRECTED)
    {
      sprintf ((*je)->textinfo, "3xx received for request outside call!");
  } else if (type == EXOSIP_MESSAGE_REQUESTFAILURE)
    {
      sprintf ((*je)->textinfo, "4xx received for request outside call!");
  } else if (type == EXOSIP_MESSAGE_SERVERFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for request outside call!");
  } else if (type == EXOSIP_MESSAGE_GLOBALFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for request outside call!");
  } else if (type == EXOSIP_SUBSCRIPTION_NOANSWER)
    {
      sprintf ((*je)->textinfo, "No answer for this SUBSCRIBE!");
  } else if (type == EXOSIP_SUBSCRIPTION_PROCEEDING)
    {
      sprintf ((*je)->textinfo, "SUBSCRIBE is being processed!");
  } else if (type == EXOSIP_SUBSCRIPTION_ANSWERED)
    {
      sprintf ((*je)->textinfo, "2xx received for SUBSCRIBE!");
  } else if (type == EXOSIP_SUBSCRIPTION_REDIRECTED)
    {
      sprintf ((*je)->textinfo, "3xx received for SUBSCRIBE!");
  } else if (type == EXOSIP_SUBSCRIPTION_REQUESTFAILURE)
    {
      sprintf ((*je)->textinfo, "4xx received for SUBSCRIBE!");
  } else if (type == EXOSIP_SUBSCRIPTION_SERVERFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for SUBSCRIBE!");
  } else if (type == EXOSIP_SUBSCRIPTION_GLOBALFAILURE)
    {
      sprintf ((*je)->textinfo, "5xx received for SUBSCRIBE!");
  } else if (type == EXOSIP_SUBSCRIPTION_NOTIFY)
    {
      sprintf ((*je)->textinfo, "NOTIFY request for subscription!");
  } else if (type == EXOSIP_SUBSCRIPTION_RELEASED)
    {
      sprintf ((*je)->textinfo, "Subscription has terminate!");
  } else if (type == EXOSIP_IN_SUBSCRIPTION_NEW)
    {
      sprintf ((*je)->textinfo, "New incoming SUBSCRIBE!");
  } else if (type == EXOSIP_IN_SUBSCRIPTION_RELEASED)
    {
      sprintf ((*je)->textinfo, "Incoming Subscription has terminate!");
  } else
    {
      (*je)->textinfo[0] = '\0';
    }
  return 0;
}

void
eXosip_event_free (eXosip_event_t * je)
{
  if (je == NULL)
    return;
  if (je->request != NULL)
    osip_message_free (je->request);
  if (je->response != NULL)
    osip_message_free (je->response);
  if (je->ack != NULL)
    osip_message_free (je->ack);
  osip_free (je);
}

void
report_event (eXosip_event_t * je, osip_message_t * sip)
{
  if (je != NULL)
    {
      eXosip_event_add (je);
    }
}

void
report_call_event (int evt, eXosip_call_t * jc,
                   eXosip_dialog_t * jd, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  je = eXosip_event_init_for_call (evt, jc, jd, tr);
  report_event (je, NULL);
}

void
report_options_event (int evt, osip_transaction_t * tr)
{
  eXosip_event_t *je;

  eXosip_event_init (&je, evt);
  if (je == NULL)
    return;

  if (tr != NULL)
    je->tid = tr->transactionid;

  _eXosip_event_fill_messages (je, tr);
  report_event (je, NULL);
}

int
eXosip_event_add (eXosip_event_t * je)
{
  int i = osip_fifo_add (eXosip.j_events, (void *) je);

  osip_cond_signal ((struct osip_cond *) eXosip.j_cond);
  __eXosip_wakeup_event ();
  return i;
}

#if 0
#ifdef CLOCK_REALTIME
/* if CLOCK_REALTIME exist, then clock_gettime should be defined */

#define OSIP_CLOCK_REALTIME CLOCK_REALTIME

void
__eXosip_clock_gettime (clockid_t cid, struct timespec *time)
{
  clock_gettime (cid, time);
}

#elif defined (WIN32) || defined (_WIN32_WCE)

#include <sys/types.h>
#include <sys/timeb.h>

#define OSIP_CLOCK_REALTIME 4002

void
__eXosip_clock_gettime (unsigned int clock_id, struct timespec *time)
{
  struct _timeb time_val;

  if (clock_id != OSIP_CLOCK_REALTIME)
    return;

  _ftime (&time_val);
  time->tv_sec = time_val.time;
  time->tv_nsec = time_val.millitm * 1000000;
  return;
}
#endif
#endif

eXosip_event_t *
eXosip_event_wait (int tv_s, int tv_ms)
{
  eXosip_event_t *je = NULL;

#if 0                           /* this does not seems to work. by now */
#if defined (CLOCK_REALTIME) || defined (WIN32) || defined (_WIN32_WCE)
  int i;

  struct timespec deadline;
  struct timespec interval;
  long tot_ms = (tv_s * 1000) + tv_ms;

  static struct osip_mutex *mlock = NULL;

  if (mlock == NULL)
    mlock = osip_mutex_init ();

  je = (eXosip_event_t *) osip_fifo_tryget (eXosip.j_events);
  if (je)
    return je;

  interval.tv_sec = tot_ms / 1000;
  interval.tv_nsec = (tot_ms % 1000) * 1000000L;

  __eXosip_clock_gettime (OSIP_CLOCK_REALTIME, &deadline);

  if ((deadline.tv_nsec += interval.tv_nsec) >= 1000000000L)
    {
      deadline.tv_nsec -= 1000000000L;
      deadline.tv_sec += 1;
  } else
    deadline.tv_nsec += interval.tv_nsec;

  deadline.tv_sec += interval.tv_sec;

  i = osip_cond_timedwait ((struct osip_cond *) eXosip.j_cond,
                           (struct osip_mutex *) mlock, &deadline);

#endif
#else
  /* basic replacement */
  {
    fd_set fdset;
    struct timeval tv;
    int max, i;

    FD_ZERO (&fdset);
#if defined (WIN32) || defined (_WIN32_WCE)
    FD_SET ((unsigned int) jpipe_get_read_descr (eXosip.j_socketctl_event),
            &fdset);
#else
    FD_SET (jpipe_get_read_descr (eXosip.j_socketctl_event), &fdset);
#endif
    max = jpipe_get_read_descr (eXosip.j_socketctl_event);
    tv.tv_sec = tv_s;
    tv.tv_usec = tv_ms * 1000;

    je = (eXosip_event_t *) osip_fifo_tryget (eXosip.j_events);
    if (je != NULL)
      return je;

    if (tv_s == 0 && tv_ms == 0)
      return NULL;

    i = select (max + 1, &fdset, NULL, NULL, &tv);
    if (i <= 0)
      return 0;

    if (FD_ISSET (jpipe_get_read_descr (eXosip.j_socketctl_event), &fdset))
      {
        char buf[500];

        jpipe_read (eXosip.j_socketctl_event, buf, 499);
      }

    je = (eXosip_event_t *) osip_fifo_tryget (eXosip.j_events);
    if (je != NULL)
      return je;
  }
#endif

  return je;
}

eXosip_event_t *
eXosip_event_get ()
{
  eXosip_event_t *je;

  je = (eXosip_event_t *) osip_fifo_get (eXosip.j_events);
  return je;
}
