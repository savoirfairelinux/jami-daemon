/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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

#include "audio/audiocodec.h"
#include "audio/gsmcodec.h"
#include "audio/alaw.h"
#include "audio/ulaw.h"
#include "codecDescriptor.h"

CodecDescriptor::CodecDescriptor (int payload) 
{
	_payload = payload;
	_codecName = "";
}

CodecDescriptor::CodecDescriptor (int payload, const string& name) 
{
	_payload = payload;
	_codecName = name;
}

CodecDescriptor::~CodecDescriptor (void)
{
}

AudioCodec*
CodecDescriptor::alloc (int payload, const string& name)
{
	switch(payload) {
	case PAYLOAD_CODEC_ULAW:
		return new Ulaw(payload, name);
		break;
	case PAYLOAD_CODEC_ALAW:
		return new Alaw(payload, name);
		break;
	case PAYLOAD_CODEC_GSM:
		return new Gsm(payload, name);
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
CodecDescriptor::setNameCodec (const string& name)
{
	_codecName = name;
}
	
string 
CodecDescriptor::getNameCodec (void)
{
	return _codecName;
}

int
CodecDescriptor::matchPayloadCodec (const string& codecname) {
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

string
CodecDescriptor::rtpmapPayload (int payload) {
	switch (payload) {
		case PAYLOAD_CODEC_ALAW:
 			return string("PCMA");
 			break;

 		case PAYLOAD_CODEC_ULAW:
 			return string("PCMU");
 			break;

 		case PAYLOAD_CODEC_GSM:
 			return string("GSM");
 			break;

 		case PAYLOAD_CODEC_ILBC:
 			return string("iLBC");
 			break;

 		case PAYLOAD_CODEC_SPEEX:
 			return string("speex");
 			break;

		default:
			break;
	}
	return "";
}

