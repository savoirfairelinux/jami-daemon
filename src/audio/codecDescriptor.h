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


#ifndef __CODEC_DESCRIPTOR_H__
#define __CODEC_DESCRIPTOR_H__

#include <string>
#include <map>

typedef enum {
  PAYLOAD_CODEC_ULAW = 0,
  PAYLOAD_CODEC_GSM = 3,
  PAYLOAD_CODEC_ALAW = 8,
  PAYLOAD_CODEC_ILBC = 97,
  PAYLOAD_CODEC_SPEEX = 110
} codecType;

/**
 * This class should be singleton
 * But I didn't want to add Singleton template to it.. and an extra header
 * I didn't want to make my own Singleton structure to not 
 * add an extra function to delete the new CodecDescriptorMap
 * when the application stop
 */
typedef std::map<codecType, std::string> CodecMap;
class CodecDescriptorMap {
public:
  CodecDescriptorMap();
  ~CodecDescriptorMap() {};
  CodecMap getMap() const { return _codecMap; } // by Copy
private:
  CodecMap _codecMap;
};

class AudioCodec;
class CodecDescriptor 
{
public:

	CodecDescriptor (int payload);
	CodecDescriptor (const std::string& name);
	CodecDescriptor (int payload, const std::string& name);
	~CodecDescriptor (void);

	AudioCodec* alloc (int payload, const std::string& name);

	void setPayload (int payload);
	int getPayload (void);
	void setNameCodec (const std::string& name);
	std::string getNameCodec (void);
	
	/*
	 * Match codec name to the payload
	 */
	int	matchPayloadCodec (const std::string&);
	/*
	 * Match a payload to the codec name
	 */
	std::string rtpmapPayload (int);

private:
	void initCache();
	int _payload;
	std::string _codecName;

	AudioCodec* _ac1;
	AudioCodec* _ac2;
	AudioCodec* _ac3;
};

#endif // __CODEC_DESCRIPTOR_H__
