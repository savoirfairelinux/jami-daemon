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


#ifndef __CODEC_DESCRIPTOR_H__
#define __CODEC_DESCRIPTOR_H__

#include <string>

using namespace std;

typedef enum {
	PAYLOAD_CODEC_ULAW = 0,
	PAYLOAD_CODEC_GSM = 3,
	PAYLOAD_CODEC_ALAW = 8,
	PAYLOAD_CODEC_ILBC = 97,
	PAYLOAD_CODEC_SPEEX = 110
} codecType;

#define CODEC_ALAW			string("G711a")
#define CODEC_ULAW			string("G711u")
#define CODEC_GSM			string("GSM")
#define CODEC_ILBC			string("iLBC")
#define CODEC_SPEEX			string("SPEEX")


class AudioCodec;
class CodecDescriptor 
{
public:
	CodecDescriptor (int payload);
	CodecDescriptor (int payload, const string& name);
	~CodecDescriptor (void);

	AudioCodec* alloc (int payload, const string& name);

	void setPayload (int payload);
	int getPayload (void);
	void setNameCodec (const string& name);
	string getNameCodec (void);
	
	/*
	 * Match codec name to the payload
	 */
	int	matchPayloadCodec (const string&);
	/*
	 * Match a payload to the codec name
	 */
	string rtpmapPayload (int);

private:
	int _payload;
	string _codecName;
};

#endif // __CODEC_DESCRIPTOR_H__
