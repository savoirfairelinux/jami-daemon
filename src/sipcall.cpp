/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
#include "sipmanager.h"
#include <string>

#define _SENDRECV 0
#define _SENDONLY 1
#define _RECVONLY 2

SIPCall::SIPCall(const CallID& id, Call::CallType type) : Call(id, type)
{
  _cid = 0;
  _did = 0;
  _tid = 0;
  _xferSub = NULL;
  _invSession = NULL;
}

SIPCall::~SIPCall() 
{
}


bool 
SIPCall::SIPCallInvite(pjsip_rx_data *rdata, pj_pool_t *pool)
{
  pj_status_t status;
  
  pjmedia_sdp_session* remote_sdp = getRemoteSDPFromRequest(rdata);
  if (remote_sdp == 0) {
    return false;
  }

  // Have to do some stuff here with the SDP
  // We retrieve the remote sdp offer in the rdata struct to begin the negociation
  _localSDP = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_session);
  _localSDP->conn =  PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
  
  _localSDP->origin.version = 0;
  sdpAddOrigin();
  _localSDP->name = pj_str("sflphone");
  sdpAddConnectionInfo();
  _localSDP->time.start = _localSDP->time.stop = 0;
  sdpAddMediaDescription(pool);
  
  _debug("Before validate SDP!\n");
  status = pjmedia_sdp_validate( _localSDP );
  if (status != PJ_SUCCESS) {
    _debug("Can not generate valid local sdp\n");
    return false;
  }
  
  _debug("Before create negociator!\n");
  status = pjmedia_sdp_neg_create_w_remote_offer(pool, _localSDP, remote_sdp, &_negociator);
  if (status != PJ_SUCCESS) {
      _debug("Can not create negociator\n");
      return false;
  }
  _debug("After create negociator!\n");
  
  pjmedia_sdp_media* remote_med = getRemoteMedia(remote_sdp);
  if (remote_med == 0) {
    _debug("SIP Failure: unable to get remote media\n");
    return false;
  }

  _debug("Before set audio!\n");
  if (!setRemoteAudioFromSDP(remote_sdp, remote_med)) {
    _debug("SIP Failure: unable to set IP address and port from SDP\n");
    return false;
  }

  _debug("Before set codec!\n");
  if (!setAudioCodecFromSDP(remote_med)) {
    _debug("SIP Failure: unable to set audio codecs from the remote SDP\n");
    return false;
  }

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

  /*if (!setRemoteAudioFromSDP(remote_med, remote_sdp)) {
    _debug("SIP Failure: unable to set IP address and port from SDP\n");
    sdp_message_free (remote_sdp);
    return false;
  }

  if (!setAudioCodecFromSDP(remote_med, event->tid)) {
    sdp_message_free (remote_sdp);
    return false;
  }*/

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
    return true;
}

bool
SIPCall::SIPCallAnsweredWithoutHold(pjsip_rx_data *rdata)
{
  pjmedia_sdp_session* remote_sdp = getRemoteSDPFromRequest(rdata);
  if (remote_sdp == NULL) {
    _debug("SIP Failure: no remote sdp\n");
    return false;
  }

  pjmedia_sdp_media* remote_med = getRemoteMedia(remote_sdp);
  if (remote_med==NULL) {
    return false;
  }
  
  _debug("Before set audio!\n");
  if (!setRemoteAudioFromSDP(remote_sdp, remote_med)) {
    _debug("SIP Failure: unable to set IP address and port from SDP\n");
    return false;
  }

  _debug("Before set codec!\n");
  if (!setAudioCodecFromSDP(remote_med)) {
    _debug("SIP Failure: unable to set audio codecs from the remote SDP\n");
    return false;
  }

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
          AudioCodecType audiocodec = (AudioCodecType)payload;
          if (audiocodec != (AudioCodecType)-1 && _codecMap.isActive(audiocodec))  { 
            listCodec << payload << " ";
            //listRtpMap << "a=rtpmap:" << payload << " " << audiocodec->getCodecName() << "/" << audiocodec->getClockRate();
            listRtpMap << "a=rtpmap:" << payload << " " << _codecMap.getCodecName(audiocodec) << "/" << _codecMap.getSampleRate(audiocodec);
	    if (_codecMap.getChannel(audiocodec) != 1) {
              listRtpMap << "/" << _codecMap.getChannel(audiocodec);
            }
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

pjmedia_sdp_session* 
SIPCall::getRemoteSDPFromRequest(pjsip_rx_data *rdata)
{
    pjmedia_sdp_session *sdp;
    pjsip_msg *msg;
    pjsip_msg_body *body;

    msg = rdata->msg_info.msg;
    body = msg->body;

    pjmedia_sdp_parse( rdata->tp_info.pool, (char*)body->data, body->len, &sdp );

    return sdp;
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
SIPCall::setRemoteAudioFromSDP(pjmedia_sdp_session* remote_sdp, pjmedia_sdp_media *remote_med)
{
  std::string remoteIP(remote_sdp->conn->addr.ptr, remote_sdp->conn->addr.slen);
  _debug("            Remote Audio IP: %s\n", remoteIP.data());
  setRemoteIP(remoteIP);
  int remotePort = remote_med->desc.port; 
  _debug("            Remote Audio Port: %d\n", remotePort);
  setRemoteAudioPort(remotePort);
  
  return true;
}

bool 
SIPCall::setAudioCodecFromSDP(pjmedia_sdp_media* remote_med)
{
  // Remote Payload
  int payLoad = -1;
  int codecCount = remote_med->desc.fmt_count;
  for(int i = 0; i < codecCount; i++) {
      payLoad = atoi(remote_med->desc.fmt[i].ptr);
      if (_codecMap.isActive((AudioCodecType)payLoad))
          break;
          
      payLoad = -1;
  }
  
  if(payLoad != -1) {
    _debug("            Payload: %d\n", payLoad);
    setAudioCodec((AudioCodecType)payLoad);
  } else
    return false;
  
  return true;
}

void SIPCall::sdpAddOrigin( void )
{
    pj_time_val tv;
    pj_gettimeofday(&tv);

    _localSDP->origin.user = pj_str(pj_gethostname()->ptr);
    // Use Network Time Protocol format timestamp to ensure uniqueness.
    _localSDP->origin.id = tv.sec + 2208988800UL;
    // The type of network ( IN for INternet )
    _localSDP->origin.net_type = pj_str("IN"); //STR_IN;
    // The type of address
    _localSDP->origin.addr_type = pj_str("IP4"); //STR_IP4;
    // The address of the machine from which the session was created
    _localSDP->origin.addr = pj_str( (char*)_ipAddr.c_str() );    
}

void SIPCall::sdpAddConnectionInfo( void )
{
    _localSDP->conn->net_type = _localSDP->origin.net_type;
    _localSDP->conn->addr_type = _localSDP->origin.addr_type;
    _localSDP->conn->addr = _localSDP->origin.addr;
}

void SIPCall::sdpAddMediaDescription(pj_pool_t* pool)
{
    pjmedia_sdp_media* med;
    pjmedia_sdp_attr *attr;
    //int nbMedia, i;

    med = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    //nbMedia = getSDPMediaList().size();
    _localSDP->media_count = 1;
    
    med->desc.media = pj_str("audio");
    med->desc.port_count = 1;
    med->desc.port = getLocalExternAudioPort();
    med->desc.transport = pj_str("RTP/AVP");
    
    CodecsMap::iterator itr;
    itr = _codecMap.getCodecsMap().begin();
    int count = _codecMap.getCodecsNumber();
    med->desc.fmt_count = count;
    
    int i = 0;
    while(itr != _codecMap.getCodecsMap().end()) {
        std::ostringstream format;
        format << (*itr).first;
        pj_strdup2(pool, &med->desc.fmt[i], format.str().data());
        
        AudioCodec *codec = (*itr).second;
        pjmedia_sdp_rtpmap rtpMap;
        rtpMap.pt = med->desc.fmt[i];
        rtpMap.enc_name = pj_str((char *)codec->getCodecName().data());
        rtpMap.clock_rate = codec->getClockRate();
        if(codec->getChannel() > 1) {
            std::ostringstream channel;
            channel << codec->getChannel();
            rtpMap.param = pj_str((char *)channel.str().data());
        } else
            rtpMap.param.slen = 0;
        
        pjmedia_sdp_rtpmap_to_attr( pool, &rtpMap, &attr );
        med->attr[i] = attr;
        i++;
        itr++;
    }
    
    //FIXME! Add the direction stream
    attr = (pjmedia_sdp_attr*)pj_pool_zalloc( pool, sizeof(pjmedia_sdp_attr) );
    pj_strdup2( pool, &attr->name, "sendrecv");
    med->attr[ i++] = attr;
    med->attr_count = i;

    _localSDP->media[0] = med;
    /*for( i=0; i<nbMedia; i++ ){
        getMediaDescriptorLine( getSDPMediaList()[i], pool, &med );
        this->_local_offer->media[i] = med;
    } */
    
}

pjmedia_sdp_media* SIPCall::getRemoteMedia(pjmedia_sdp_session *remote_sdp)
{
    int count, i;
    
    count = remote_sdp->media_count;
    for(i = 0; i < count; ++i) {
        if(pj_stricmp2(&remote_sdp->media[i]->desc.media, "audio") == 0)
            return remote_sdp->media[i];
    }
    
    return NULL;
}

bool SIPCall::startNegociation(pj_pool_t *pool)
{
    pj_status_t status;
    _debug("Before negotiate!\n");
    status = pjmedia_sdp_neg_negotiate(pool, _negociator, 0);
    
    return (status == PJ_SUCCESS);
}

bool SIPCall::createInitialOffer(pj_pool_t *pool)
{
  pj_status_t status;

  // Have to do some stuff here with the SDP
  _localSDP = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_session);
  _localSDP->conn =  PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
  
  _localSDP->origin.version = 0;
  sdpAddOrigin();
  _localSDP->name = pj_str("sflphone");
  sdpAddConnectionInfo();
  _localSDP->time.start = _localSDP->time.stop = 0;
  sdpAddMediaDescription(pool);
  
  _debug("Before validate SDP!\n");
  status = pjmedia_sdp_validate( _localSDP );
  if (status != PJ_SUCCESS) {
    _debug("Can not generate valid local sdp %d\n", status);
    return false;
  }
  
  _debug("Before create negociator!\n");
  // Create the SDP negociator instance with local offer
  status = pjmedia_sdp_neg_create_w_local_offer( pool, _localSDP, &_negociator);
  //state = pjmedia_sdp_neg_get_state( _negociator );

  PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

  return true;
    
}
