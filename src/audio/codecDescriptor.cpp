/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
#include "codecDescriptor.h"
/*#ifdef HAVE_SPEEX
 #include "CodecSpeex.h"
#endif*/

CodecDescriptor::CodecDescriptor() 
{
  // Default codecs
  _codecMap[PAYLOAD_CODEC_ALAW] = "PCMA";
  _codecMap[PAYLOAD_CODEC_ULAW] = "PCMU";
  _codecMap[PAYLOAD_CODEC_GSM] = "GSM";
#ifdef HAVE_SPEEX
  //_codecMap[PAYLOAD_CODEC_SPEEX] = new CodecSpeex(PAYLOAD_CODEC_SPEEX); // TODO: this is a variable payload!
#endif
// theses one are not implemented yet..
//  _codecMap[PAYLOAD_CODEC_ILBC] = Ilbc();
//  _codecMap[PAYLOAD_CODEC_SPEEX] = Speex();
}

std::string&
CodecDescriptor::getCodecName(CodecType payload)
{
  CodecMap::iterator iter = _codecMap.find(payload);
  if (iter!=_codecMap.end()) {
    return (iter->second);
  }
  //return ;
}

bool 
CodecDescriptor::isSupported(CodecType payload) 
{
  CodecMap::iterator iter = _codecMap.begin();
  while(iter!=_codecMap.end()) {
      if (iter->first == payload) {
	// codec is already in the map --> nothing to do
	_debug("Codec with payload %i already in the map\n", payload);
        //break;
        return true;
      }
    iter++;
  }
   return false;
}

void 
CodecDescriptor::removeCodec(CodecType payload)
{
  CodecMap::iterator iter = _codecMap.begin();
  while(iter!=_codecMap.end()) {
      if (iter->first == payload) {
	_debug("Codec %s removed from the list", getCodecName(payload).data());
	_codecMap.erase(iter);
        break;
      }
    iter++;
  }
	
}

void
CodecDescriptor::addCodec(CodecType payload)
{
}


