/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
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
#include <string.h>

#include <iostream>

#include "audio/audiocodec.h"
#include "global.h"
#include "sipcall.h"

using namespace std;

SipCall::SipCall (short id, CodecDescriptorVector* cdv) 
{
	_id = id;	// Same id of Call object
	alloc();
	_cdv = cdv;
	_audiocodec = NULL;
	_standby = false;
	//this->usehold = false;
}


SipCall::~SipCall (void) 
{
	dealloc();
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
SipCall::setId (short id)
{
	_id = id;
}

short
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
	_audiocodec = ac;
}

// newIncomingCall is called when the IP-Phone user receives a new call.
int 
SipCall::newIncomingCall (eXosip_event_t *event) {	
	SipCall *ca = this;
	
  	_cid = event->cid;
  	_did = event->did;

  	if (_did < 1 && _cid < 1) {
      	return -1; /* not enough information for this event?? */
    }
	
  	osip_strncpy (_textinfo, event->textinfo, 255);
  	osip_strncpy (_req_uri, event->req_uri, 255);
  	osip_strncpy (_local_uri, event->local_uri, 255);
  	osip_strncpy (_local_uri, event->remote_uri, 255);
  	osip_strncpy (_subject, event->subject, 255);
	
	osip_strncpy (ca->_remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
	ca->_remote_sdp_audio_port = event->remote_sdp_audio_port;
	
	ca->payload = event->payload;
	_debug("For incoming ca->_payload = %d\n", ca->payload);
	osip_strncpy (ca->_payload_name, event->payload_name, 49);
	setAudioCodec(_cdv->at(0)->alloc(ca->payload, ca->_payload_name));	

	osip_strncpy (ca->_sdp_body, event->sdp_body, 1000);
	osip_strncpy (ca->_sdp_body, event->sdp_body, 1000);
	_debug("\n%s\n", ca->_sdp_body);
	
  	if (event->reason_phrase[0] != '\0') {
      	osip_strncpy(this->_reason_phrase, event->reason_phrase, 49);
      	this->_status_code = event->status_code;
    }
	
  	this->_state = event->type;
  	return 0;
}


int 
SipCall::ringingCall (eXosip_event_t *event) {     
	SipCall *ca = this;
	
    this->_cid = event->cid;
    this->_did = event->did;
      
    if (this->_did < 1 && this->_cid < 1) {
	  return -1; 
	}

  	osip_strncpy(this->_textinfo,   event->textinfo, 255);
  	osip_strncpy(this->_req_uri,    event->req_uri, 255);
  	osip_strncpy(this->_local_uri,  event->local_uri, 255);
  	osip_strncpy(this->_remote_uri, event->remote_uri, 255);
  	osip_strncpy(this->_subject,    event->subject, 255);

	osip_strncpy(ca->_remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
	ca->_remote_sdp_audio_port = event->remote_sdp_audio_port;
	ca->payload = event->payload;
	osip_strncpy(ca->_payload_name, event->payload_name, 49);
	
  	if (event->reason_phrase[0]!='\0') {
      	osip_strncpy(this->_reason_phrase, event->reason_phrase, 49);
      	this->_status_code = event->status_code;
    }
  	this->_state = event->type;;
  	return 0;
}


int 
SipCall::answeredCall(eXosip_event_t *event) {
	SipCall *ca = this;
	
    _cid = event->cid;
    _did = event->did;
      
    if (_did < 1 && _cid < 1)	{
	  return -1; /* not enough information for this event?? */
	}

	osip_strncpy(this->_textinfo,   event->textinfo, 255);
	osip_strncpy(this->_req_uri,    event->req_uri, 255);
    osip_strncpy(this->_local_uri,  event->local_uri, 255);
    osip_strncpy(this->_remote_uri, event->remote_uri, 255);
    osip_strncpy(this->_subject,    event->subject, 255);

	osip_strncpy(ca->_remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
	ca->_remote_sdp_audio_port = event->remote_sdp_audio_port;
	osip_strncpy(ca->_payload_name, event->payload_name, 49);
	osip_strncpy (ca->_sdp_body, event->sdp_body, 1000);
	_debug("\n%s\n", ca->_sdp_body);

	// For outgoing calls, find the first payload of the remote user
	int i, size;
	char temp[64];
	bzero(temp, 64);
	size = _cdv->size();
	// Codec array in common with the 2 parts
	int m_audio[size];
	for (int a = 0; a < size; a++)
		m_audio[a] = -1;
	
	sdp_message_t *sdp;
	i = sdp_message_init(&sdp);
	if (i != 0) _debug("Cannot allocate a SDP packet\n");
	i = sdp_message_parse(sdp, ca->_sdp_body);
	if (i != 0) _debug("Cannot parse the SDP body\n");
	int pos = 0;
	if (sdp == NULL) _debug("SDP = NULL\n");
/*********/	
	while (!sdp_message_endof_media (sdp, pos)) {
		int k = 0;
	    char *tmp = sdp_message_m_media_get (sdp, pos);
		
	    if (0 == osip_strncasecmp (tmp, "audio", 5)) {
		  	char *payload = NULL;
			int c = 0;
		  	do {
		    	payload = sdp_message_m_payload_get (sdp, pos, k);
				for (int j = 0; j < size; j++) {
					snprintf(temp, 63, "%d", _cdv->at(j)->getPayload());
					if (payload != NULL and strcmp(temp, payload) == 0) {
					   	m_audio[c] = _cdv->at(j)->getPayload();
						c++;
						break;
					}
				}
				k++;
			} while (payload != NULL);
		}
		pos++;
	}
/***********/
	if (m_audio[0] == -1) {
		noSupportedCodec();
	} else {
		ca->payload = m_audio[0];
		setAudioCodec(_cdv->at(0)->alloc(ca->payload, ""));
	}

	_debug("For outgoing call: ca->_payload = %d\n", ca->payload);
	
	sdp_message_free(sdp);

	if (event->reason_phrase[0]!='\0') {
		osip_strncpy(this->_reason_phrase, event->reason_phrase, 49);
		this->_status_code = event->status_code;
    }
	this->_state = event->type;
//	delete ac;
	return 0;
}

int
SipCall::onholdCall (eXosip_event_t *event) {
	SipCall *ca = this;

	osip_strncpy(ca->_textinfo, event->textinfo, 255);

    osip_strncpy(ca->_remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
    ca->_remote_sdp_audio_port = event->remote_sdp_audio_port;

    osip_strncpy(ca->_reason_phrase, event->reason_phrase, 49);
    ca->_status_code = event->status_code;
		
	ca->_state = event->type;
  	return 0;
}

int
SipCall::offholdCall (eXosip_event_t *event) {
	SipCall *ca = this;

	osip_strncpy(ca->_textinfo, event->textinfo, 255);

    osip_strncpy(ca->_remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
    ca->_remote_sdp_audio_port = event->remote_sdp_audio_port;

    osip_strncpy(ca->_reason_phrase, event->reason_phrase, 49);
    ca->_status_code = event->status_code;
  
	ca->_state = event->type;
  	return 0;
}

int 
SipCall::redirectedCall(eXosip_event_t *event) {
	/*
	if (ca->enable_audio>0) {
		ca->enable_audio = -1;
		os_sound_close(ca);
    }
	*/
    this->_state = NOT_USED;
	return 0;
}

int
SipCall::closedCall (void) {
	SipCall *ca = this;

  	if (ca->enable_audio > 0) {
 //     	manager->closeSound(ca);
    }

  	return 0;
}

void
SipCall::alloc(void) {
	this->_reason_phrase = new char[50];
  	this->_textinfo = new char[256];
  	this->_req_uri = new char[256];
  	this->_local_uri = new char[256];
  	this->_remote_uri = new char[256];
  	this->_subject = new char[256];
	this->_remote_sdp_audio_ip = new char[50];
  	this->_payload_name = new char[50];
  	this->_sdp_body = new char[1000];
}

void
SipCall::dealloc(void) {
	delete _reason_phrase;
  	delete _textinfo;
  	delete _req_uri;
  	delete _local_uri;
  	delete _remote_uri;
  	delete _subject;
	delete _remote_sdp_audio_ip;
  	delete _payload_name;
	delete _sdp_body;
}

void
SipCall::noSupportedCodec (void) {
	_debug("Codec no supported\n");
}

