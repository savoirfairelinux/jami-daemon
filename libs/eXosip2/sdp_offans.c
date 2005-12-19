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

extern eXosip_t eXosip;

sdp_message_t *_eXosip_get_remote_sdp (osip_transaction_t * invite_tr);
sdp_message_t *_eXosip_get_local_sdp (osip_transaction_t * invite_tr);


sdp_message_t *
eXosip_get_remote_sdp (int jid)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *invite_tr = NULL;

  if (jid > 0)
    {
      eXosip_call_dialog_find (jid, &jc, &jd);
    }
  if (jc == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return NULL;
    }
  invite_tr = eXosip_find_last_invite (jc, jd);
  if (invite_tr == NULL)
    return NULL;

  return _eXosip_get_remote_sdp (invite_tr);
}

sdp_message_t *
eXosip_get_local_sdp (int jid)
{
  eXosip_dialog_t *jd = NULL;
  eXosip_call_t *jc = NULL;
  osip_transaction_t *invite_tr = NULL;

  if (jid > 0)
    {
      eXosip_call_dialog_find (jid, &jc, &jd);
    }
  if (jc == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_ERROR, NULL,
                   "eXosip: No call here?\n"));
      return NULL;
    }
  invite_tr = eXosip_find_last_invite (jc, jd);
  if (invite_tr == NULL)
    return NULL;

  return _eXosip_get_local_sdp (invite_tr);
}

sdp_message_t *
_eXosip_get_remote_sdp (osip_transaction_t * invite_tr)
{
  osip_message_t *message;

  if (invite_tr == NULL)
    return NULL;
  if (invite_tr->ctx_type == IST)
    message = invite_tr->orig_request;
  else if (invite_tr->ctx_type == ICT)
    message = invite_tr->last_response;
  else
    return NULL;
  return eXosip_get_sdp_info (message);
}

sdp_message_t *
_eXosip_get_local_sdp (osip_transaction_t * invite_tr)
{
  osip_message_t *message;

  if (invite_tr == NULL)
    return NULL;
  if (invite_tr->ctx_type == IST)
    message = invite_tr->last_response;
  else if (invite_tr->ctx_type == ICT)
    message = invite_tr->orig_request;
  else
    return NULL;
  return eXosip_get_sdp_info (message);
}

sdp_message_t *
eXosip_get_sdp_info (osip_message_t * message)
{
  osip_content_type_t *ctt;
  osip_mime_version_t *mv;
  sdp_message_t *sdp;
  osip_body_t *oldbody;
  int pos;

  if (message == NULL)
    return NULL;

  /* get content-type info */
  ctt = osip_message_get_content_type (message);
  mv = osip_message_get_mime_version (message);
  if (mv == NULL && ctt == NULL)
    return NULL;                /* previous message was not correct or empty */
  if (mv != NULL)
    {
      /* look for the SDP body */
      /* ... */
  } else if (ctt != NULL)
    {
      if (ctt->type == NULL || ctt->subtype == NULL)
        /* it can be application/sdp or mime... */
        return NULL;
      if (osip_strcasecmp (ctt->type, "application") != 0 ||
          osip_strcasecmp (ctt->subtype, "sdp") != 0)
        {
          return NULL;
        }
    }

  pos = 0;
  while (!osip_list_eol (message->bodies, pos))
    {
      int i;

      oldbody = (osip_body_t *) osip_list_get (message->bodies, pos);
      pos++;
      sdp_message_init (&sdp);
      i = sdp_message_parse (sdp, oldbody->body);
      if (i == 0)
        return sdp;
      sdp_message_free (sdp);
      sdp = NULL;
    }
  return NULL;
}


sdp_connection_t *
eXosip_get_audio_connection (sdp_message_t * sdp)
{
  int pos = 0;
  sdp_media_t *med = (sdp_media_t *) osip_list_get (sdp->m_medias, 0);

  while (med != NULL)
    {
      if (med->m_media != NULL && osip_strcasecmp (med->m_media, "audio") == 0)
        break;
      pos++;
      med = (sdp_media_t *) osip_list_get (sdp->m_medias, pos);
    }
  if (med == NULL)
    return NULL;                /* no audio stream */
  if (osip_list_eol (med->c_connections, 0))
    return sdp->c_connection;

  /* just return the first one... */
  return (sdp_connection_t *) osip_list_get (med->c_connections, 0);
}


sdp_media_t *
eXosip_get_audio_media (sdp_message_t * sdp)
{
  int pos = 0;
  sdp_media_t *med = (sdp_media_t *) osip_list_get (sdp->m_medias, 0);

  while (med != NULL)
    {
      if (med->m_media != NULL && osip_strcasecmp (med->m_media, "audio") == 0)
        return med;
      pos++;
      med = (sdp_media_t *) osip_list_get (sdp->m_medias, pos);
    }

  return NULL;
}
