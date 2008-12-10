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
#include <string>

#define _SENDRECV 0
#define _SENDONLY 1
#define _RECVONLY 2

SIPCall::SIPCall(const CallID& id, Call::CallType type) : Call(id, type)
            , _cid(0)
            , _did(0)
            , _tid(0)
            , _localSDP(NULL)
            , _negociator(NULL)
            , _ipAddr("")
            , _xferSub(NULL)
            , _invSession(NULL)
{
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
  _localSDP->name = pj_str((char*)"sflphone");
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
    _localSDP->origin.net_type = pj_str((char*)"IN"); //STR_IN;
    // The type of address
    _localSDP->origin.addr_type = pj_str((char*)"IP4"); //STR_IP4;
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
    
    med->desc.media = pj_str((char*)"audio");
    med->desc.port_count = 1;
    med->desc.port = getLocalExternAudioPort();
    med->desc.transport = pj_str((char*)"RTP/AVP");
    
    CodecOrder::iterator itr;

    itr = _codecMap.getActiveCodecs().begin();
    int count = _codecMap.getActiveCodecs().size();
    med->desc.fmt_count = count;
    
    int i = 0;
    while(itr != _codecMap.getActiveCodecs().end()) {
        std::ostringstream format;
        format << *itr;
        pj_strdup2(pool, &med->desc.fmt[i], format.str().data());
        
        pjmedia_sdp_rtpmap rtpMap;
        rtpMap.pt = med->desc.fmt[i];
        rtpMap.enc_name = pj_str((char *)_codecMap.getCodecName(*itr).data());
        rtpMap.clock_rate = _codecMap.getSampleRate(*itr);
        if(_codecMap.getChannel(*itr) > 1) {
            std::ostringstream channel;
            channel << _codecMap.getChannel(*itr);
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
  _localSDP->name = pj_str((char*)"sflphone");
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
