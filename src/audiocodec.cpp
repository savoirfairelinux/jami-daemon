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

#include <qapplication.h>

#include "audiocodec.h"
#include "configuration.h"
#include "g711.h"

#include <string>
#ifdef  CCXX_NAMESPACES
using namespace std;
#endif

AudioCodec::AudioCodec (void) {
	// Init array handleCodecs
#if 0
	handleCodecs[0] = matchPayloadCodec(
		Config::gets(QString("Audio/Codecs.codec1")));
	handleCodecs[1] = matchPayloadCodec(
		Config::gets(QString("Audio/Codecs.codec2")));
	handleCodecs[2] = matchPayloadCodec(
		Config::gets(QString("Audio/Codecs.codec3")));
	handleCodecs[3] = matchPayloadCodec(
		Config::gets(QString("Audio/Codecs.codec4")));
	handleCodecs[4] = matchPayloadCodec(
		Config::gets(QString("Audio/Codecs.codec5")));
#endif
	handleCodecs[0] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec1"));
	handleCodecs[1] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec2"));
	handleCodecs[2] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec3"));
	handleCodecs[3] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec4"));
	handleCodecs[4] = matchPayloadCodec(Config::gets("Audio", "Codecs.codec5"));
}

AudioCodec::~AudioCodec (void) {
}

int
AudioCodec::matchPayloadCodec (std::string codecname) {
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
		// TODO: 
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
				// TODO
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
	qDebug("Codec no supported");
}
