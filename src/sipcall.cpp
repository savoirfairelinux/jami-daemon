/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "sipcall.h"
#include "global.h" // for _debug
#include <sstream> // for media buffer

#define _SENDRECV 0
#define _SENDONLY 1
#define _RECVONLY 2

SIPCall::SIPCall(const CallID& id, Call::CallType type) : Call(id, type)
{
  _cid = 0;
  _did = 0;
  _tid = 0;
}

SIPCall::~SIPCall() 
{
}


bool 
SIPCall::SIPCallInvite(eXosip_event_t *event)
{
  if (event->cid < 1 && event->did < 1) {
    _debug("SIP Failure: Invalid cid and did\n");
    return false;
  }

  if (event->request == NULL) {
    _debug("SIP Failure: No request into the event\n");
    return false;
  }

  setCid(event->cid);
  setDid(event->did);
  setTid(event->tid);

  setPeerInfoFromRequest(event);
  
  sdp_message_t* remote_sdp = getRemoteSDPFromRequest(event);
  if (remote_sdp == 0) {
    return false;
  }

  sdp_media_t* remote_med = getRemoteMedia(event->tid, remote_sdp);
  if (remote_med == 0) {
    sdp_message_free (remote_sdp);
    return false;
  }

  if (!setRemoteAudioFromSDP(remote_med, remote_sdp)) {
    _debug("SIP Failure: unable to set IP address and port from SDP\n");
    sdp_message_free (remote_sdp);
    return false;
  }

  if (!setAudioCodecFromSDP(remote_med, event->tid)) {
    _debug("SIP Failure: unable to set audio codecs from the remote SDP\n");
    sdp_message_free (remote_sdp);
    return false;
  }

  osip_message_t *answer = 0;
  eXosip_lock();
  _debug("< Building Answer 183\n");
  if (0 == eXosip_call_build_answer (event->tid, 183, &answer)) {
    if ( 0 != sdp_complete_message(remote_sdp, answer)) {
      osip_message_free(answer);
      // Send 415 Unsupported media type
      _debug("< Sending Answer 415 : unsupported media type\n");
      eXosip_call_send_answer (event->tid, 415, NULL);
    } else {
      sdp_message_t *local_sdp = eXosip_get_sdp_info(answer);
      sdp_media_t *local_med = NULL;
      if (local_sdp != NULL) {
         local_med = eXosip_get_audio_media(local_sdp);
      }
      if (local_sdp != NULL && local_med != NULL) {
        /* search if stream is sendonly or recvonly */
        int _remote_sendrecv = sdp_analyse_attribute (remote_sdp, remote_med);
        int _local_sendrecv =  sdp_analyse_attribute (local_sdp, local_med);
        _debug("            Remote SendRecv: %d\n", _remote_sendrecv);
        _debug("            Local  SendRecv: %d\n", _local_sendrecv);
        if (_local_sendrecv == _SENDRECV) {
          if (_remote_sendrecv == _SENDONLY)      { _local_sendrecv = _RECVONLY; }
          else if (_remote_sendrecv == _RECVONLY) { _local_sendrecv = _SENDONLY; }
        }
        _debug("            Final Local SendRecv: %d\n", _local_sendrecv);
        sdp_message_free (local_sdp);
      }
      _debug("< Sending answer 183\n");
      if (0 != eXosip_call_send_answer (event->tid, 183, answer)) {
        _debug("SipCall::newIncomingCall: cannot send 183 progress?\n");
      }
    }
  }
  eXosip_unlock ();

  sdp_message_free (remote_sdp);
  return true;
}

bool 
SIPCall::SIPCallReinvite(eXosip_event_t *event)
{
  if (event->cid < 1 && event->did < 1) {
    _debug("SIP Failure: Invalid cid and did\n");
    return false;
  }

  if (event->request == NULL) {
    _debug("SIP Failure: No request into the event\n");
    return false;
  }

  setCid(event->cid);
  setDid(event->did);
  setTid(event->tid);

  setPeerInfoFromRequest(event);

  sdp_message_t* remote_sdp = getRemoteSDPFromRequest(event);
  if (remote_sdp == 0) {
    return false;
  }

  sdp_media_t* remote_med = getRemoteMedia(event->tid, remote_sdp);
  if (remote_med == 0) {
    sdp_message_free (remote_sdp);
    return false;
  }

  if (!setRemoteAudioFromSDP(remote_med, remote_sdp)) {
    _debug("SIP Failure: unable to set IP address and port from SDP\n");
    sdp_message_free (remote_sdp);
    return false;
  }

  if (!setAudioCodecFromSDP(remote_med, event->tid)) {
    sdp_message_free (remote_sdp);
    return false;
  }

  osip_message_t *answer = 0;
  eXosip_lock();
  _debug("< Building Answer 200\n");
  if (0 == eXosip_call_build_answer (event->tid, 200, &answer)) {
    if ( 0 != sdp_complete_message(remote_sdp, answer)) {
      osip_message_free(answer);
      // Send 415 Unsupported media type
      eXosip_call_send_answer (event->tid, 415, NULL);
      _debug("< Sending Answer 415\n");
    } else {

      sdp_message_t *local_sdp = eXosip_get_sdp_info(answer);
      sdp_media_t *local_med = NULL;
      if (local_sdp != NULL) {
         local_med = eXosip_get_audio_media(local_sdp);
      }
      if (local_sdp != NULL && local_med != NULL) {
        /* search if stream is sendonly or recvonly */
        int _remote_sendrecv = sdp_analyse_attribute (remote_sdp, remote_med);
        int _local_sendrecv =  sdp_analyse_attribute (local_sdp, local_med);
        _debug("            Remote SendRecv: %d\n", _remote_sendrecv);
        _debug("            Local  SendRecv: %d\n", _local_sendrecv);
        if (_local_sendrecv == _SENDRECV) {
          if (_remote_sendrecv == _SENDONLY)      { _local_sendrecv = _RECVONLY; }
          else if (_remote_sendrecv == _RECVONLY) { _local_sendrecv = _SENDONLY; }
        }
        _debug("            Final Local SendRecv: %d\n", _local_sendrecv);
        sdp_message_free (local_sdp);
      }
      _debug("< Sending answer 200\n");
      if (0 != eXosip_call_send_answer (event->tid, 200, answer)) {
        _debug("SipCall::newIncomingCall: cannot send 200 OK?\n");
      }
    }
  }
  eXosip_unlock ();
  sdp_message_free (remote_sdp);
  return true;
}

bool 
SIPCall::SIPCallAnswered(eXosip_event_t *event)
{
  if (event->cid < 1 && event->did < 1) {
    _debug("SIP Failure: Invalid cid and did\n");
    return false;
  }

  if (event->request == NULL) {
    _debug("SIP Failure: No request into the event\n");
    return false;
  }

  setCid(event->cid);
  setDid(event->did);

  //setPeerInfoFromResponse()

  eXosip_lock ();
  {
    osip_message_t *ack = NULL;
    int i;
    i = eXosip_call_build_ack (event->did, &ack);
    if (i != 0) {
      _debug("SipCall::answeredCall: Cannot build ACK for call!\n");
    } else {
      sdp_message_t *local_sdp = NULL;
      sdp_message_t *remote_sdp = NULL;

      if (event->request != NULL && event->response != NULL) {
        local_sdp = eXosip_get_sdp_info (event->request);
        remote_sdp = eXosip_get_sdp_info (event->response);
      }
      if (local_sdp == NULL && remote_sdp != NULL) {
        /* sdp in ACK */
        i = sdp_complete_message (remote_sdp, ack);
        if (i != 0) {
            _debug("SipCall::answeredCall: Cannot complete ACK with sdp body?!\n");
        }
      }
      sdp_message_free (local_sdp);
      sdp_message_free (remote_sdp);

      _debug("< Send ACK\n");
      eXosip_call_send_ack (event->did, ack);
    }
  }
  eXosip_unlock ();
  return true;  
}


bool 
SIPCall::SIPCallAnsweredWithoutHold(eXosip_event_t* event)
{
  if (event->response == NULL || event->request  == NULL) { return false; }
  
  eXosip_lock();
  sdp_message_t* remote_sdp = eXosip_get_sdp_info (event->response);
  eXosip_unlock();
  if (remote_sdp == NULL) {
    _debug("SIP Failure: no remote sdp\n");
    sdp_message_free(remote_sdp);
    return false;
  }

  sdp_media_t* remote_med = getRemoteMedia(event->tid, remote_sdp);
  if (remote_med==NULL) {
    sdp_message_free(remote_sdp);
    return false;
  }
  if ( ! setRemoteAudioFromSDP(remote_med, remote_sdp) ) {
    sdp_message_free(remote_sdp);
    return false;
  }

#ifdef LIBOSIP2_WITHPOINTER
  char *tmp = (char*) osip_list_get (remote_med->m_payloads, 0);
#else
  char *tmp = (char*) osip_list_get (&(remote_med->m_payloads), 0);
#endif
  setAudioCodec((CodecType)-1);
  if (tmp != NULL) {
    int payload = atoi (tmp);
    _debug("            Remote Payload: %d\n", payload);
    //setAudioCodec(_codecMap.getCodecName((CodecType)payload)); // codec builder for the mic
    setAudioCodec((CodecType)payload); // codec builder for the mic
  }

/*
    // search if stream is sendonly or recvonly
    _remote_sendrecv = sdp_analyse_attribute (remote_sdp, remote_med);
    _local_sendrecv = sdp_analyse_attribute (local_sdp, local_med);
    if (_local_sendrecv == _SENDRECV) {
      if (_remote_sendrecv == _SENDONLY)
          _local_sendrecv = _RECVONLY;
      else if (_remote_sendrecv == _RECVONLY)
          _local_sendrecv = _SENDONLY;
    }
  _debug("            Remote Sendrecv: %d\n", _remote_sendrecv);
  _debug("            Local Sendrecv: %d\n", _local_sendrecv);
*/
  sdp_message_free (remote_sdp);
  return true;
}


int 
SIPCall::sdp_complete_message(sdp_message_t * remote_sdp, osip_message_t * msg)
{
  // Format port to a char*
  if (remote_sdp == NULL) {
    _debug("SipCall::sdp_complete_message: No remote SDP body found for call\n");
    return -1;
  }
  if (msg == NULL) {
    _debug("SipCall::sdp_complete_message: No message to complete\n");
    return -1;
  }

  std::ostringstream media;

  // for each medias
  int iMedia = 0;
  char *tmp = NULL;
  #ifdef LIBOSIP2_WITHPOINTER 
  const osip_list_t* remote_sdp_m_medias = remote_sdp->m_medias; // old abi
  #else
  const osip_list_t* remote_sdp_m_medias = &(remote_sdp->m_medias);
  #endif
  osip_list_t* remote_med_m_payloads = 0;

  while (!osip_list_eol(remote_sdp_m_medias, iMedia)) {
    sdp_media_t *remote_med = (sdp_media_t *)osip_list_get(remote_sdp_m_medias, iMedia);
    if (remote_med == 0) { continue; }

    if (0 != osip_strcasecmp (remote_med->m_media, "audio")) {
      // if this is not an "audio" media, we set it to 0
      media << "m=" << remote_med->m_media << " 0 " << remote_med->m_proto << " \r\n";
    } else {
      std::ostringstream listCodec;
      std::ostringstream listRtpMap;

      // search for compatible codec: foreach payload
      int iPayload = 0;
      #ifdef LIBOSIP2_WITHPOINTER 
      remote_med_m_payloads = remote_med->m_payloads; // old abi
      #else
      remote_med_m_payloads = &(remote_med->m_payloads);
      #endif

      //while (!osip_list_eol(remote_med_m_payloads, iPayload) && iPayload < 2) {
      while (!osip_list_eol(remote_med_m_payloads, iPayload)) {
        tmp = (char *)osip_list_get(remote_med_m_payloads, iPayload);
        if (tmp!=NULL) {
          int payload = atoi(tmp);
	  _debug("remote payload = %s\n", tmp);
          CodecType audiocodec = (CodecType)payload;
          if (audiocodec != (CodecType)-1 && _codecMap.isActive(audiocodec))  { 
            listCodec << payload << " ";
            //listRtpMap << "a=rtpmap:" << payload << " " << audiocodec->getCodecName() << "/" << audiocodec->getClockRate();
            listRtpMap << "a=rtpmap:" << payload << " " << _codecMap.getCodecName(audiocodec) << "/" << _codecMap.getSampleRate(audiocodec);
        // TODO: manage a way to get the channel infos    
	/*if ( audiocodec->getChannel() != 1) {
              listRtpMap << "/" << audiocodec->getChannel();
            }*/
            listRtpMap << "\r\n";
          }
        }
        iPayload++;
      }
      if (listCodec.str().empty()) {
        media << "m=" << remote_med->m_media << " 0 " << remote_med->m_proto << " \r\n";
      } else {
        // we add the media line + a=rtpmap list
        media << "m=" << remote_med->m_media << " " << getLocalExternAudioPort() << " RTP/AVP " << listCodec.str() << "\r\n";
        media << listRtpMap.str();
      }
    }
    iMedia++;
  }
  char buf[4096];
  snprintf (buf, 4096,
    "v=0\r\n"
    "o=user 0 0 IN IP4 %s\r\n"
    "s=session\r\n"
    "c=IN IP4 %s\r\n"
    "t=0 0\r\n"
    "%s\n", getLocalIp().c_str(), getLocalIp().c_str(), media.str().c_str());

  osip_message_set_body (msg, buf, strlen (buf));
  osip_message_set_content_type (msg, "application/sdp");
  _debug("          sdp: %s", buf);
  return 0;
}



int 
SIPCall::sdp_analyse_attribute (sdp_message_t * sdp, sdp_media_t * med)
{
  int pos;
  int pos_media;

  /* test media attributes */
  pos = 0;
  #ifdef LIBOSIP2_WITHPOINTER 
  const osip_list_t* med_a_attributes = med->a_attributes; // old abi
  #else
  const osip_list_t* med_a_attributes = &(med->a_attributes);
  #endif
  while (!osip_list_eol (med_a_attributes, pos)) {
      sdp_attribute_t *at;

      at = (sdp_attribute_t *) osip_list_get (med_a_attributes, pos);
      if (at->a_att_field != NULL && 
      0 == strcmp (at->a_att_field, "sendonly")) {
      return _SENDONLY;
      } else if (at->a_att_field != NULL &&
                0 == strcmp (at->a_att_field, "recvonly")) {
          return _RECVONLY;
      } else if (at->a_att_field != NULL &&
                0 == strcmp (at->a_att_field, "sendrecv")) {
          return _SENDRECV;
      }
      pos++;
  }

  /* test global attributes */
  pos_media = -1;
  pos = 0;
  #ifdef LIBOSIP2_WITHPOINTER 
  const osip_list_t* sdp_a_attributes = sdp->a_attributes; // old abi
  #else
  const osip_list_t* sdp_a_attributes = &(sdp->a_attributes);
  #endif
  while (!osip_list_eol (sdp_a_attributes, pos)) {
      sdp_attribute_t *at;

      at = (sdp_attribute_t *) osip_list_get (sdp_a_attributes, pos);
      if (at->a_att_field != NULL && 
      0 == strcmp (at->a_att_field, "sendonly")) {
          return _SENDONLY;
      } else if (at->a_att_field != NULL &&
                0 == strcmp (at->a_att_field, "recvonly")) {
          return _RECVONLY;
      } else if (at->a_att_field != NULL &&
                0 == strcmp (at->a_att_field, "sendrecv")) {
          return _SENDRECV;
      }
      pos++;
  }

  return _SENDRECV;
}

bool 
SIPCall::setPeerInfoFromRequest(eXosip_event_t *event)
{
  // event->request should not be NULL!
  char remote_uri[256] = "";
  std::string name("");
  std::string number("");

  char *tmp = NULL;
  osip_from_to_str(event->request->from, &tmp);
  if (tmp != NULL) {
    snprintf (remote_uri, 255, "%s", tmp);
    remote_uri[255] = '\0';
    osip_free (tmp);

    // Get the name/number
    osip_from_t *from;
    osip_from_init(&from);
    osip_from_parse(from, remote_uri);
    char *tmpname = osip_from_get_displayname(from);
    if ( tmpname != NULL ) {
      name = tmpname;
    }
    osip_uri_t* url = osip_from_get_url(from); 
    if ( url != NULL && url->username != NULL) {
      number = url->username;
    }
    osip_from_free(from);
  }

  _debug("            Name: %s\n", name.c_str());
  _debug("            Number: %s\n", number.c_str());
  _debug("            Remote URI: %s\n", remote_uri);

  setPeerName(name);
  setPeerNumber(number);  
  return true;
}

sdp_message_t* 
SIPCall::getRemoteSDPFromRequest(eXosip_event_t *event)
{
  // event->request should not be null!
  /* negotiate payloads */
  sdp_message_t *remote_sdp = NULL;
  if (event->request != NULL) {
    eXosip_lock();
    remote_sdp = eXosip_get_sdp_info (event->request);
    eXosip_unlock();
  }
  if (remote_sdp == NULL) {
    _debug("SIP Failure: No SDP into the request\n");
    _debug("< Sending 400 Bad Request (no SDP)\n");
    eXosip_lock();
    eXosip_call_send_answer (event->tid, 400, NULL);
    eXosip_unlock();
    return 0;
  }
  return remote_sdp;
}

sdp_media_t* 
SIPCall::getRemoteMedia(int tid, sdp_message_t* remote_sdp)
{
  // Remote Media Port
  eXosip_lock();
  sdp_media_t *remote_med = eXosip_get_audio_media(remote_sdp);
  eXosip_unlock();

  if (remote_med == NULL || remote_med->m_port == NULL) {
    // no audio media proposed
    _debug("SIP Failure: unsupported media\n");
    _debug("< Sending 415 Unsupported media type\n");
    eXosip_lock();
    eXosip_call_send_answer (tid, 415, NULL);
    eXosip_unlock();
    sdp_message_free (remote_sdp);
    return 0;
  }
  return remote_med;
}

bool 
SIPCall::setRemoteAudioFromSDP(sdp_media_t* remote_med, sdp_message_t* remote_sdp)
{
  // Remote Media IP
  eXosip_lock();
  sdp_connection_t *conn = eXosip_get_audio_connection(remote_sdp);
  eXosip_unlock();
  if (conn != NULL && conn->c_addr != NULL) {
    char _remote_sdp_audio_ip[50] = "";
    snprintf (_remote_sdp_audio_ip, 49, "%s", conn->c_addr);
    _remote_sdp_audio_ip[49] = '\0';
    _debug("            Remote Audio IP: %s\n", _remote_sdp_audio_ip);
    setRemoteIP(_remote_sdp_audio_ip);
    if (_remote_sdp_audio_ip[0] == '\0') {
      setRemoteAudioPort(0);
      return false;
    }
  }

  // Remote port
  int _remote_sdp_audio_port = atoi(remote_med->m_port);
  _debug("            Remote Audio Port: %d\n", _remote_sdp_audio_port);
  setRemoteAudioPort(_remote_sdp_audio_port);

  if (_remote_sdp_audio_port == 0) {
    return false;
  }
  return true;
}

bool 
SIPCall::setAudioCodecFromSDP(sdp_media_t* remote_med, int tid)
{
  // Remote Payload
  char *tmp = NULL;
  int pos = 0;
  #ifdef LIBOSIP2_WITHPOINTER 
  const osip_list_t* remote_med_m_payloads = remote_med->m_payloads; // old abi
  #else
  const osip_list_t* remote_med_m_payloads = &(remote_med->m_payloads);
  #endif
  while (!osip_list_eol (remote_med_m_payloads, pos)) {
    tmp = (char *) osip_list_get (remote_med_m_payloads, pos);
    if (tmp != NULL ) {
      int payload = atoi(tmp);
      // stop if we find a correct codec
      if (_codecMap.isActive((CodecType)payload)){
          break;
      }
    }
    tmp = NULL;
    pos++;
  }

  setAudioCodec((CodecType)-1);
  if (tmp != NULL) {
    int payload = atoi (tmp);
    _debug("            Payload: %d\n", payload);
    setAudioCodec((CodecType)payload); // codec builder for the mic
  }
  if (getAudioCodec() == (CodecType) -1) {
    _debug("SIPCall Failure: Unable to set codec\n");
    _debug("< Sending 415 Unsupported media type\n");
    eXosip_lock();
    eXosip_call_send_answer(tid, 415, NULL);
    eXosip_unlock();
    return false;
  }
  return true;
}
