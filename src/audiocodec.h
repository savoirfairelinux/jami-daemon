/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __CODEC_AUDIO_H__
#define __CODEC_AUDIO_H__

#include <string.h>
using namespace std;

typedef enum {
	PAYLOAD_CODEC_ULAW = 0,
	PAYLOAD_CODEC_GSM = 3,
	PAYLOAD_CODEC_ALAW = 8,
	PAYLOAD_CODEC_ILBC = 97,
	PAYLOAD_CODEC_SPEEX = 110
} codecType;

#define CODEC_ALAW			std::string("G711a")
#define CODEC_ULAW			std::string("G711u")
#define CODEC_GSM			std::string("GSM")
#define CODEC_ILBC			std::string("iLBC")
#define CODEC_SPEEX			std::string("SPEEX")

#define NB_CODECS			5

class AudioCodec {
public:
	AudioCodec 				(void);		
	~AudioCodec 			(void);	

	int 	handleCodecs[NB_CODECS];

	void 		noSupportedCodec	(void);
	static int	codecDecode 		(int, short *, unsigned char *, unsigned int);
	static int	codecEncode 		(int, unsigned char *, short *, unsigned int);
	int			matchPayloadCodec	(std::string);
	char *		rtpmapPayload 		(int);

private:
	
};

#endif // __CODEC_AUDIO_H__
