/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <iostream>

#include "audiocodec.h"
#include "gsmcodec.h"
#include "alaw.h"
#include "ulaw.h"
#include "codecDescriptor.h"

const char* CODEC_ALAW = "G711a";
const char* CODEC_ULAW = "G711u";
const char* CODEC_GSM = "GSM";
const char* CODEC_ILBC = "iLBC";
const char* CODEC_SPEEX = "SPEEX";

CodecDescriptorMap::CodecDescriptorMap() 
{
  _codecMap[PAYLOAD_CODEC_ALAW] = CODEC_ALAW;
  _codecMap[PAYLOAD_CODEC_ULAW] = CODEC_ULAW;
  _codecMap[PAYLOAD_CODEC_GSM] = CODEC_GSM;
// theses one are not implemented yet..
//  _codecMap[PAYLOAD_CODEC_ILBC] = CODEC_ILBC;
//  _codecMap[PAYLOAD_CODEC_SPEEX] = CODEC_SPEEX;
}

CodecDescriptor::CodecDescriptor (int payload) 
{
	_payload = payload;
	_codecName = rtpmapPayload(_payload);
	initCache();
}

CodecDescriptor::CodecDescriptor (const std::string& name) 
{
	_codecName = name;
	_payload = matchPayloadCodec(name);
	initCache();
}

CodecDescriptor::CodecDescriptor (int payload, const std::string& name) 
{
	_payload = payload;
	_codecName = name;
	initCache();
}

void CodecDescriptor::initCache() 
{
  _ac1 = NULL;  
  _ac2 = NULL;
  _ac3 = NULL;
}


CodecDescriptor::~CodecDescriptor (void)
{	
  delete _ac1;
  delete _ac2;
  delete _ac3;
}

AudioCodec*
CodecDescriptor::alloc (int payload, const std::string& name)
{
	// _ac1, _ac2 and _ac3 are caching...
	switch(payload) {
	case PAYLOAD_CODEC_ULAW:
		if ( _ac1 == NULL ) {
			_ac1 = new Ulaw(payload, name);
      //return new Ulaw(payload, name);
		}
		return _ac1;
		break;
	case PAYLOAD_CODEC_ALAW:
		if ( _ac2 == NULL ) {
			_ac2 = new Alaw(payload, name);
      //return new Alaw(payload, name);
		}
		return _ac2;
		break;
	case PAYLOAD_CODEC_GSM:
		if ( _ac3 == NULL ) {
			_ac3 = new Gsm(payload, name);
      //return new Gsm(payload, name);
		}
		return _ac3;
		break;
	default:
		return NULL;
		break;
	}
}

void 
CodecDescriptor::setPayload (int payload)
{
	_payload =payload;
}
	
int 
CodecDescriptor::getPayload (void)
{
	return _payload;
}
	
void 
CodecDescriptor::setNameCodec (const std::string& name)
{
	_codecName = name;
}
	
std::string 
CodecDescriptor::getNameCodec (void)
{
	return _codecName;
}

int
CodecDescriptor::matchPayloadCodec (const std::string& codecname) {
	if (codecname == CODEC_ALAW) {
		return PAYLOAD_CODEC_ALAW;
	} else if (codecname == CODEC_ULAW) {
		return PAYLOAD_CODEC_ULAW;
	} else if (codecname == CODEC_GSM) {
		return PAYLOAD_CODEC_GSM;
	} else if (codecname == CODEC_ILBC) {
		return PAYLOAD_CODEC_ILBC;
	} else if (codecname == CODEC_SPEEX) {
		return PAYLOAD_CODEC_SPEEX;
	} else 
		return -1;
}

std::string
CodecDescriptor::rtpmapPayload (int payload) {
  // we don't want to use the CodecDescriptorMap Here
  // we create one, but in MainManager for the list
	switch (payload) {
		case PAYLOAD_CODEC_ALAW:
 			return std::string("PCMA");
 			break;

 		case PAYLOAD_CODEC_ULAW:
 			return std::string("PCMU");
 			break;

 		case PAYLOAD_CODEC_GSM:
 			return std::string("GSM");
 			break;

 		case PAYLOAD_CODEC_ILBC:
 			return std::string("iLBC");
 			break;

 		case PAYLOAD_CODEC_SPEEX:
 			return std::string("speex");
 			break;

		default:
			break;
	}
	return "";
}

