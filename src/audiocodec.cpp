/*
 * Copyright (C) 2004 Savoir-Faire Linux inc.
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
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
#include "audiocodec.h"
#include "configuration.h"
#include "g711.h"
#include "../gsm/gsm.h"

#include <endian.h>
#include <string.h>

#include <string>

#define swab16(x) \
	((unsigned short)( \
	(((unsigned short)(x) & (unsigned short)0x00ffU) << 8) | \
	(((unsigned short)(x) & (unsigned short)0xff00U) >> 8) ))



using namespace std;

static gsm decode_gsmhandle;
static gsm encode_gsmhandle;

AudioCodec::AudioCodec (void) {
	// Init array handleCodecs
	handleCodecs[0] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec1"));
	handleCodecs[1] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec2"));
	handleCodecs[2] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec3"));
	handleCodecs[3] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec4"));
	handleCodecs[4] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec5"));
}

AudioCodec::~AudioCodec (void) {
}

int
AudioCodec::matchPayloadCodec (string codecname) {
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

char *
AudioCodec::rtpmapPayload (int payload) {
	switch (payload) {
		case PAYLOAD_CODEC_ALAW:
 			return "PCMA";
 			break;

 		case PAYLOAD_CODEC_ULAW:
 			return "PCMU";
 			break;

 		case PAYLOAD_CODEC_GSM:
 			return "GSM";
 			break;

 		case PAYLOAD_CODEC_ILBC:
 			return "iLBC";
 			break;

 		case PAYLOAD_CODEC_SPEEX:
 			return "speex";
 			break;

		default:
			break;
	}
	return NULL;
}

int
AudioCodec::codecDecode (int pt, short *dst, unsigned char *src, unsigned int size) {
	switch (pt) {
	case PAYLOAD_CODEC_ULAW:
		return G711::ULawDecode (dst, src, size);
		break;

	case PAYLOAD_CODEC_ALAW:
		return G711::ALawDecode (dst, src, size);
		break;

	case PAYLOAD_CODEC_GSM:
		if (gsm_decode(decode_gsmhandle, (gsm_byte*)src, (gsm_signal*)dst) < 0) 
			printf("AudioCodec: ERROR: gsm_decode\n");
		return 320;
		break;

	case PAYLOAD_CODEC_ILBC:
		// TODO
		break;

	case PAYLOAD_CODEC_SPEEX:
		// TODO
		break;

	default:
		break;
	}
	return 0;
}

int
AudioCodec::codecEncode (int pt, unsigned char *dst, short *src, unsigned int size) {	
	switch (pt) {
	case PAYLOAD_CODEC_ULAW:
		return G711::ULawEncode (dst, src, size);
		break;

	case PAYLOAD_CODEC_ALAW:
		return G711::ALawEncode (dst, src, size);
		break;

	case PAYLOAD_CODEC_GSM:
#if 0		
	{
		gsm_frame gsmdata; // dst
		int iii;
		
		bzero (gsmdata, sizeof(gsm_frame));

		printf ("Before gsm_encode: ");
		for (iii = 0; iii < 33; iii++) {
			unsigned char *ptr = gsmdata;
			printf ("%02X ", ptr[iii]);
		}
		gsm_signal sample[160];
		for (iii = 0; iii < 160; iii++) {
			unsigned short dat;
			dat = (unsigned short) src[iii];
			//dat = (unsigned short) sample[iii];
			//sample[iii] = (short) swab16(dat);
			sample[iii] = src[iii];
		}
		gsm_encode(encode_gsmhandle, sample, gsmdata);
		printf ("\nAfter gsm_encode: ");
		for (iii = 0; iii < 33; iii++) {
			unsigned char *ptr = gsmdata;
			printf ("%02X ", ptr[iii]);
			dst[iii] = ptr[iii];
		}
		printf ("\n------\n");
	}
#endif
#if 1
		gsm_encode(encode_gsmhandle, (gsm_signal*)src, (gsm_byte*)dst);
	
#endif
		return 33;
		break;

	case PAYLOAD_CODEC_ILBC:
		// TODO
		break;

	case PAYLOAD_CODEC_SPEEX:
		// TODO
		break;

	default:
		break;
	}
	return 0;
}

void
AudioCodec::noSupportedCodec (void) {
	printf("Codec no supported\n");
}

void
AudioCodec::gsmCreate (void) {
	if (!(decode_gsmhandle = gsm_create() )) 
		printf("AudioCodec: ERROR: decode_gsm_create\n");
	if (!(encode_gsmhandle = gsm_create() )) 
		printf("AudioCodec: ERROR: encode_gsm_create\n");
}

void
AudioCodec::gsmDestroy (void) {
	gsm_destroy(decode_gsmhandle);
	gsm_destroy(encode_gsmhandle);
}

int
AudioCodec::getSizeByPayload (int pt){
	switch (pt) {
	case PAYLOAD_CODEC_ULAW:
	case PAYLOAD_CODEC_ALAW:
		return 320;
		break;

	case PAYLOAD_CODEC_GSM:
		return 320;
		break;

	case PAYLOAD_CODEC_ILBC:
		// TODO
		break;

	case PAYLOAD_CODEC_SPEEX:
		// TODO
		break;

	default:
		break;
	}
	return 0;
}
