/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * Portions Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <osipparser2/sdp_message.h>
#include <string.h> // strcpy
 
// For AF_INET
#include <sys/socket.h>

#include "global.h"
#include "audio/audiocodec.h"
#include "audio/codecDescriptor.h"
#include "sipcall.h"

SipCall::SipCall (CALLID id, CodecDescriptorVector* cdv) : _localIp("127.0.0.1")
{
  _id = id;	  // Same id of Call object
  _cid = 0; // call id, from the sipvoiplink
  _did = 0; // dialog id
  _tid = 0; // transaction id

  _standby = false;
  _status_code = 0;

  alloc(); // char* allocation
  _cdv = cdv;
  _audiocodec = NULL;

  _local_audio_port = 0;
  _remote_sdp_audio_port = 0;
  _local_sendrecv  = 0;           /* _SENDRECV, _SENDONLY, _RECVONLY */
  _remote_sendrecv = 0;
}


SipCall::~SipCall (void) 
{
	dealloc();
  //delete _audiocodec;  don't delete it, the Manager will do it...
  _audiocodec = NULL;
}

void
SipCall::setLocalAudioPort (int newport) 
{
	_local_audio_port = newport;
}

int
SipCall::getLocalAudioPort (void) 
{
	return _local_audio_port;
}

void
SipCall::setId (CALLID id)
{
	_id = id;
}

CALLID
SipCall::getId (void)
{
	return _id;
}

void
SipCall::setDid (int did)
{
	_did = did;
}

int
SipCall::getDid (void)
{
	return _did;
}

void
SipCall::setCid (int cid)
{
	_cid = cid;
}

int
SipCall::getCid (void)
{
	return _cid;
}

void
SipCall::setTid (int tid)
{
	_tid = tid;
}

int
SipCall::getTid (void)
{
	return _tid;
}

int
SipCall::getRemoteSdpAudioPort (void)
{
	return _remote_sdp_audio_port;
}

char*
SipCall::getRemoteSdpAudioIp (void)
{
	return _remote_sdp_audio_ip;
}

AudioCodec*
SipCall::getAudioCodec (void)
{
	return _audiocodec;
}

void
SipCall::setAudioCodec (AudioCodec* ac)
{
  // it use a new!
  // delete _audiocodec;
 _audiocodec = ac;
}

// newIncomingCall is called when the IP-Phone user receives a new call.
int 
SipCall::newIncomingCall (eXosip_event_t *event) {	

  _cid = event->cid;
  _did = event->did;
  _tid = event->tid;

  if (_did < 1 && _cid < 1) {
    return -1; /* not enough information for this event?? */
  }
  osip_strncpy (_textinfo, event->textinfo, 255);

  if (event->response != NULL) {
    _status_code = event->response->status_code;
    snprintf (_reason_phrase, 49, "%s", event->response->reason_phrase);
    _debug("  Status: %d %s\n", _status_code, _reason_phrase);
  }

  strcpy(_remote_uri, "");
  _name = "";
  _number = "";
  if (event->request != NULL) {
    char *tmp = NULL;

    osip_from_to_str(event->request->from, &tmp);
    if (tmp != NULL) {
      snprintf (_remote_uri, 255, "%s", tmp);
      osip_free (tmp);

      // Get the name/number
      osip_from_t *from;
      osip_from_init(&from);
      osip_from_parse(from, _remote_uri);
      _name = osip_from_get_displayname(from);
      osip_uri_t* url = osip_from_get_url(from); 
      if ( url != NULL ) {
        _number = url->username;
      }
      osip_from_free(from);
    }
  }
  _debug("  Name: %s\n", _name.c_str());
  _debug("  Number: %s\n", _number.c_str());
  _debug("  Remote URI: %s\n", _remote_uri);

  /* negotiate payloads */
  sdp_message_t *remote_sdp = NULL;
  if (event->request != NULL) {
    eXosip_lock();
    remote_sdp = eXosip_get_sdp_info (event->request);
    eXosip_unlock();
  }
  if (remote_sdp == NULL) {
    _debug("SipCall::newIncomingCall: No remote SDP in INVITE request. Sending 400 BAD REQUEST\n");
    // Send 400 BAD REQUEST
    eXosip_lock();
    eXosip_call_send_answer (_tid, 400, NULL);
    eXosip_unlock();
    return 0;
  }
  /* TODO: else build an offer */

  // Remote Media IP
  eXosip_lock();
  sdp_connection_t *conn = eXosip_get_audio_connection (remote_sdp);
  eXosip_unlock();
  if (conn != NULL && conn->c_addr != NULL) {
      snprintf (_remote_sdp_audio_ip, 49, "%s", conn->c_addr);
      _debug("  Remote Audio IP: %s\n", _remote_sdp_audio_ip);
  }

  // Remote Media Port
  eXosip_lock();
  sdp_media_t *remote_med = eXosip_get_audio_media (remote_sdp);
  eXosip_unlock();
  if (remote_med == NULL || remote_med->m_port == NULL) {
    // no audio media proposed
    _debug("< Sending 415 Unsupported media type\n");
    eXosip_lock();
    eXosip_call_send_answer (_tid, 415, NULL);
    eXosip_unlock();
    sdp_message_free (remote_sdp);
    return 0;
  }
  _remote_sdp_audio_port = atoi(remote_med->m_port);
  _debug("  Remote Audio Port: %d\n", _remote_sdp_audio_port);

  // Remote Payload
  char *tmp = NULL;
  if (_remote_sdp_audio_port > 0 && _remote_sdp_audio_ip[0] != '\0') {
    int pos = 0;
    while (!osip_list_eol (remote_med->m_payloads, pos)) {
      tmp = (char *) osip_list_get (remote_med->m_payloads, pos);
      if (tmp != NULL && ( 0 == osip_strcasecmp (tmp, "0") || 0 == osip_strcasecmp (tmp, "8") )) {
        break;
      }
      tmp = NULL;
      pos++;
    }
  }
  if (tmp != NULL) {
    payload = atoi (tmp);
    _debug("  Payload: %d\n", payload);
    setAudioCodec(_cdv->at(0)->alloc(payload, "")); // codec builder for the mic
  } else {
    _debug("< Sending 415 Unsupported media type\n");
    eXosip_lock();
    eXosip_call_send_answer (_tid, 415, NULL);
    eXosip_unlock();
    sdp_message_free (remote_sdp);
    return 0;
  }

  osip_message_t *answer = 0;
  eXosip_lock();
  _debug("< Building Answer 183\n");
  if (0 == eXosip_call_build_answer (_tid, 183, &answer)) {
    if ( 0 != sdp_complete_message(remote_sdp, answer)) {
      osip_message_free(answer);
      // Send 415 Unsupported media type
      eXosip_call_send_answer (_tid, 415, NULL);
      _debug("< Sending Answer 415\n");
    } else {

      sdp_message_t *local_sdp = eXosip_get_sdp_info(answer);
      sdp_media_t *local_med = NULL;
      if (local_sdp != NULL) {
         local_med = eXosip_get_audio_media(local_sdp);
      }
      if (local_sdp != NULL && local_med != NULL) {
        /* search if stream is sendonly or recvonly */
        _remote_sendrecv = sdp_analyse_attribute (remote_sdp, remote_med);
        _local_sendrecv =  sdp_analyse_attribute (local_sdp, local_med);
        _debug("  Remote SendRecv: %d\n", _remote_sendrecv);
        _debug("  Local  SendRecv: %d\n", _local_sendrecv);
        if (_local_sendrecv == _SENDRECV) {
          if (_remote_sendrecv == _SENDONLY)      { _local_sendrecv = _RECVONLY; }
          else if (_remote_sendrecv == _RECVONLY) { _local_sendrecv = _SENDONLY; }
        }
        _debug("  Final Local SendRecv: %d\n", _local_sendrecv);
        sdp_message_free (local_sdp);
      }
      _debug("  < Sending answer 183\n");
      if (0 != eXosip_call_send_answer (_tid, 183, answer)) {
        _debug("SipCall::newIncomingCall: cannot send 183 progress?\n");
      }
    }
  }
  eXosip_unlock ();
  sdp_message_free (remote_sdp);

  return 0;
}


int 
SipCall::ringingCall (eXosip_event_t *event) {     

  this->_cid = event->cid;
  this->_did = event->did;
  this->_tid = event->tid;

  if (this->_did < 1 && this->_cid < 1) {
    return -1; 
  }

  osip_strncpy (_textinfo, event->textinfo, 255);

  if (event->response != NULL) {
    _status_code = event->response->status_code;
    snprintf (_reason_phrase, 49, "%s", event->response->reason_phrase);
  }

  if (event->request != NULL) {
    char *tmp = NULL;

    osip_from_to_str (event->request->from, &tmp);
    if (tmp != NULL) {
      snprintf (_remote_uri, 255, "%s", tmp);
      osip_free (tmp);
    }
  }
  	return 0;
}

int
SipCall::receivedAck (eXosip_event_t *event)
{
  _cid = event->cid;
  _did = event->did;
  return 0;
}



int 
SipCall::answeredCall(eXosip_event_t *event) {
    _cid = event->cid;
    _did = event->did;

  if (_did < 1 && _cid < 1)	{
    return -1; /* not enough information for this event?? */
  }
	osip_strncpy(this->_textinfo,   event->textinfo, 255);

  	if (event->response != NULL) {
  		_status_code = event->response->status_code;
    	snprintf (_reason_phrase, 49, "%s", event->response->reason_phrase);
  	}

  	if (event->request != NULL) {
      	char *tmp = NULL;

      	osip_from_to_str (event->request->from, &tmp);
      	if (tmp != NULL) {
          	snprintf (_remote_uri, 255, "%s", tmp);
          	osip_free (tmp);
       	}
    }
	
  	eXosip_lock ();
  	{
    osip_message_t *ack = NULL;
    int i;

    i = eXosip_call_build_ack (_did, &ack);
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

        eXosip_call_send_ack (_did, ack);
   	}
   }
  	eXosip_unlock ();

	return 0;
}

void
SipCall::answeredCall_without_hold (eXosip_event_t *event) 
{
  if (event->response == NULL ) { return; }

  // TODO: understand this code..
  if (_cid!=0) {
    eXosip_lock();
    sdp_message_t *sdp = eXosip_get_sdp_info (event->response);
    eXosip_unlock();
    if (sdp != NULL) {
       /* audio is started and session has just been modified */
      sdp_message_free (sdp);
    }
  }

  if (event->request != NULL) {   /* audio is started */ 

    eXosip_lock();
    sdp_message_t *local_sdp = eXosip_get_sdp_info (event->request);
    sdp_message_t *remote_sdp = eXosip_get_sdp_info (event->response);
    eXosip_unlock();

    sdp_media_t *remote_med = NULL;
    char *tmp = NULL;
    if (remote_sdp == NULL) {
      _debug("SipCall::answeredCall_without_hold: No remote SDP body found for call\n");
      /* TODO: remote_sdp = retreive from ack above */
    } else {
      eXosip_lock();
      sdp_connection_t *conn = eXosip_get_audio_connection (remote_sdp);
      if (conn != NULL && conn->c_addr != NULL) {
          snprintf (_remote_sdp_audio_ip, 49, "%s", conn->c_addr);
      }

      remote_med = eXosip_get_audio_media (remote_sdp);
      if (remote_med != NULL && remote_med->m_port != NULL) {
        _remote_sdp_audio_port = atoi (remote_med->m_port);
      }
      eXosip_unlock();

      if (_remote_sdp_audio_port > 0 && _remote_sdp_audio_ip[0] != '\0' && 
        remote_med != NULL) {
        tmp = (char *) osip_list_get (remote_med->m_payloads, 0);
      }

      if (tmp != NULL) {
        payload = atoi (tmp);
        _debug("SipCall::answeredCall_without_hold: For outgoing call: ca->payload = %d\n", payload);
        setAudioCodec(_cdv->at(0)->alloc(payload, ""));
      }
    }

    if (local_sdp == NULL) {
      _debug("SipCall::answeredCall_without_hold: SDP body was probably in the ACK (TODO)\n");
    }

    if (remote_sdp != NULL && local_sdp != NULL) {
      int audio_port = 0;
      eXosip_lock();
      sdp_media_t *local_med = eXosip_get_audio_media (local_sdp);
      eXosip_unlock();
      if (local_med != NULL && local_med->m_port != NULL) {
        audio_port = atoi (local_med->m_port);
      }

      if (tmp != NULL && audio_port > 0
          && _remote_sdp_audio_port > 0
          && _remote_sdp_audio_ip[0] != '\0') {

        /* search if stream is sendonly or recvonly */
        _remote_sendrecv = sdp_analyse_attribute (remote_sdp, remote_med);
        _local_sendrecv = sdp_analyse_attribute (local_sdp, local_med);
        if (_local_sendrecv == _SENDRECV) {
          if (_remote_sendrecv == _SENDONLY)
              _local_sendrecv = _RECVONLY;
          else if (_remote_sendrecv == _RECVONLY)
              _local_sendrecv = _SENDONLY;
        }
      }
    }
    sdp_message_free (local_sdp);
    sdp_message_free (remote_sdp);
  }
}

int
SipCall::sdp_complete_message(sdp_message_t * remote_sdp, 
		osip_message_t * msg)
{
  sdp_media_t *remote_med;
  char *tmp = NULL;
  char buf[4096];
  int pos;

  char localip[128];

  	// Format port to a char*
  	char port_tmp[64];
	bzero(port_tmp, 64);
	snprintf(port_tmp, 63, "%d", _local_audio_port);
	
  	if (remote_sdp == NULL) {
      	_debug("SipCall::sdp_complete_message: No remote SDP body found for call\n");
      	return -1;
    }
  	if (msg == NULL) {
    	_debug("SipCall::sdp_complete_message: No message to complete\n");
      	return -1;
    }

        // this exosip is locked and is protected by other function
  	eXosip_guess_localip (AF_INET, localip, 128);
  	snprintf (buf, 4096,
            "v=0\r\n"
            "o=user 0 0 IN IP4 %s\r\n"
            "s=session\r\n" "c=IN IP4 %s\r\n" "t=0 0\r\n", localip, localip);

  	pos = 0;
  	while (!osip_list_eol (remote_sdp->m_medias, pos)) {
      	char payloads[128];
      	int pos2;

      	memset (payloads, '\0', sizeof (payloads));
      	remote_med = (sdp_media_t *) osip_list_get (remote_sdp->m_medias, pos);

      	if (0 == osip_strcasecmp (remote_med->m_media, "audio")) {
          	pos2 = 0;
          	while (!osip_list_eol (remote_med->m_payloads, pos2)) {
              	tmp = (char *) osip_list_get (remote_med->m_payloads, pos2);
              	if (tmp != NULL && 
						(0 == osip_strcasecmp (tmp, "0")
                   		|| 0 == osip_strcasecmp (tmp, "8")
						|| 0 == osip_strcasecmp (tmp, "3"))) {
                  	strcat (payloads, tmp);
                  	strcat (payloads, " ");
                }
              	pos2++;
            }
          	strcat (buf, "m=");
          	strcat (buf, remote_med->m_media);
          	if (pos2 == 0 || payloads[0] == '\0') {
              	strcat (buf, " 0 RTP/AVP \r\n");
              	return -1;        /* refuse anyway */
          	} else {
			 	strcat (buf, " ");
              	strcat (buf, port_tmp);
              	strcat (buf, " RTP/AVP ");
              	strcat (buf, payloads);
              	strcat (buf, "\r\n");

              	if (NULL != strstr (payloads, " 0 ")
                  || (payloads[0] == '0' && payloads[1] == ' '))
                	strcat (buf, "a=rtpmap:0 PCMU/8000\r\n");
              	if (NULL != strstr (payloads, " 8 ")
                  || (payloads[0] == '8' && payloads[1] == ' '))
                	strcat (buf, "a=rtpmap:8 PCMA/8000\r\n");
				if (NULL != strstr (payloads, " 3 ")
                  || (payloads[0] == '8' && payloads[1] == ' '))
                	strcat (buf, "a=rtpmap:8 GSM/8000\r\n");
            }
      	} else {
          	strcat (buf, "m=");
          	strcat (buf, remote_med->m_media);
          	strcat (buf, " 0 ");
          	strcat (buf, remote_med->m_proto);
          	strcat (buf, " \r\n");
        }
      	pos++;
    }

  	osip_message_set_body (msg, buf, strlen (buf));
  	osip_message_set_content_type (msg, "application/sdp");
  	return 0;
}

int
SipCall::sdp_analyse_attribute (sdp_message_t * sdp, sdp_media_t * med)
{
  	int pos;
  	int pos_media;

  	/* test media attributes */
  	pos = 0;
  	while (!osip_list_eol (med->a_attributes, pos)) {
      	sdp_attribute_t *at;

      	at = (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);
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
  	while (!osip_list_eol (sdp->a_attributes, pos)) {
      	sdp_attribute_t *at;

      	at = (sdp_attribute_t *) osip_list_get (sdp->a_attributes, pos);
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

void
SipCall::alloc(void) {
  this->_reason_phrase = new char[50];
  this->_textinfo = new char[256];
  this->_remote_uri = new char[256];
  this->_remote_sdp_audio_ip = new char[50];

  // initialize the strings...
  this->_reason_phrase[0] = '\0';
  this->_textinfo[0] = '\0';
  this->_remote_uri[0] = '\0';
  strcpy(this->_remote_sdp_audio_ip, "127.0.0.1");
}

void
SipCall::dealloc(void) {
  delete [] _reason_phrase;       _reason_phrase = NULL;
  delete [] _textinfo;            _textinfo      = NULL;
  delete [] _remote_uri;          _remote_uri    = NULL;
  delete [] _remote_sdp_audio_ip; _remote_sdp_audio_ip = NULL;
}

void
SipCall::noSupportedCodec (void) {
	_debug("SipCall::noSupportedCodec: Codec no supported\n");
}

