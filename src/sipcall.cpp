/*
 * Copyright (C) 2004 Savoir-Faire Linux inc.
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

#include "sipcall.h"

SipCall::SipCall (void) {
	this->alloc();
}

SipCall::SipCall (Manager *mngr) {
	this->alloc();
	this->manager = mngr;
	this->usehold = false;
}

SipCall::~SipCall (void) {
	this->dealloc();
}

void
SipCall::setLocalAudioPort (int newport) {
	this->local_audio_port = newport;
}

int
SipCall::getLocalAudioPort (void) {
	return this->local_audio_port;
}

// newIncomingCall is called when the IP-Phone user receives a new call.
int 
SipCall::newIncomingCall (eXosip_event_t *event) {	
	SipCall *ca = this;
	
  	this->cid = event->cid;
  	this->did = event->did;

  	if (this->did < 1 && this->cid < 1) {
      	return -1; /* not enough information for this event?? */
    }
	
  	osip_strncpy (this->textinfo, event->textinfo, 255);
  	osip_strncpy (this->req_uri, event->req_uri, 255);
  	osip_strncpy (this->local_uri, event->local_uri, 255);
  	osip_strncpy (this->local_uri, event->remote_uri, 255);
  	osip_strncpy (this->subject, event->subject, 255);
	
	osip_strncpy (ca->remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
	ca->remote_sdp_audio_port = event->remote_sdp_audio_port;
	ca->payload = event->payload;
	osip_strncpy (ca->payload_name, event->payload_name, 49);
	
  	if (event->reason_phrase[0] != '\0') {
      	osip_strncpy(this->reason_phrase, event->reason_phrase, 49);
      	this->status_code = event->status_code;
    }
	
  	this->state = event->type;
  	return 0;
}


int 
SipCall::ringingCall (eXosip_event_t *event) {     
	SipCall *ca = this;
	
    this->cid = event->cid;
    this->did = event->did;
      
    if (this->did < 1 && this->cid < 1) {
	  return -1; 
	}

  	osip_strncpy(this->textinfo,   event->textinfo, 255);
  	osip_strncpy(this->req_uri,    event->req_uri, 255);
  	osip_strncpy(this->local_uri,  event->local_uri, 255);
  	osip_strncpy(this->remote_uri, event->remote_uri, 255);
  	osip_strncpy(this->subject,    event->subject, 255);

	osip_strncpy(ca->remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
	ca->remote_sdp_audio_port = event->remote_sdp_audio_port;
	ca->payload = event->payload;
	osip_strncpy(ca->payload_name, event->payload_name, 49);
	
  	if (event->reason_phrase[0]!='\0') {
      	osip_strncpy(this->reason_phrase, event->reason_phrase, 49);
      	this->status_code = event->status_code;
    }
  	this->state = event->type;;
  	return 0;
}


int 
SipCall::answeredCall(eXosip_event_t *event) {
	SipCall *ca = this;
	
    this->cid = event->cid;
    this->did = event->did;
      
    if (this->did < 1 && this->cid < 1)	{
	  return -1; /* not enough information for this event?? */
	}

	osip_strncpy(this->textinfo,   event->textinfo, 255);
	osip_strncpy(this->req_uri,    event->req_uri, 255);
    osip_strncpy(this->local_uri,  event->local_uri, 255);
    osip_strncpy(this->remote_uri, event->remote_uri, 255);
    osip_strncpy(this->subject,    event->subject, 255);

	osip_strncpy(ca->remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
	ca->remote_sdp_audio_port = event->remote_sdp_audio_port;
	ca->payload = event->payload;
	osip_strncpy(ca->payload_name, event->payload_name, 49);
		
	if (event->reason_phrase[0]!='\0') {
		osip_strncpy(this->reason_phrase, event->reason_phrase, 49);
		this->status_code = event->status_code;
    }
	this->state = event->type;
	return 0;
}

int
SipCall::onholdCall (eXosip_event_t *event) {
	SipCall *ca = this;

	osip_strncpy(ca->textinfo, event->textinfo, 255);

    osip_strncpy(ca->remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
    ca->remote_sdp_audio_port = event->remote_sdp_audio_port;

    osip_strncpy(ca->reason_phrase, event->reason_phrase, 49);
    ca->status_code = event->status_code;
  
	ca->state = event->type;
  	return 0;
}

int
SipCall::offholdCall (eXosip_event_t *event) {
	SipCall *ca = this;

	osip_strncpy(ca->textinfo, event->textinfo, 255);

    osip_strncpy(ca->remote_sdp_audio_ip, event->remote_sdp_audio_ip, 49);
    ca->remote_sdp_audio_port = event->remote_sdp_audio_port;

    osip_strncpy(ca->reason_phrase, event->reason_phrase, 49);
    ca->status_code = event->status_code;
  
	ca->state = event->type;
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
    this->state = NOT_USED;
	return 0;
}

int
SipCall::closedCall (void) {
	SipCall *ca = this;

  	if (ca->enable_audio > 0) {
      	manager->closeSound(ca);
    }

  	return 0;
}

void
SipCall::alloc(void) {
	this->reason_phrase = new char[50];
  	this->textinfo = new char[256];
  	this->req_uri = new char[256];
  	this->local_uri = new char[256];
  	this->remote_uri = new char[256];
  	this->subject = new char[256];
	this->remote_sdp_audio_ip = new char[50];
  	this->payload_name = new char[50];
}

void
SipCall::dealloc(void) {
	delete reason_phrase;
  	delete textinfo;
  	delete req_uri;
  	delete local_uri;
  	delete remote_uri;
  	delete subject;
	delete remote_sdp_audio_ip;
  	delete payload_name;
}

