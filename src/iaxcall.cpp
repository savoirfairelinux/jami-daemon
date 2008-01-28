/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#include "iaxcall.h"
#include "global.h" // for _debug

IAXCall::IAXCall(const CallID& id, Call::CallType type) : Call(id, type), _session(NULL) 
{
}

IAXCall::~IAXCall() 
{
  _session = NULL; // just to be sure to don't have unknown pointer, do not delete it!
}

void
IAXCall::setFormat(int format)
{
  _format = format;
  switch(format) {
  /*case AST_FORMAT_ULAW:
    setAudioCodec(_codecMap.getCodec(PAYLOAD_CODEC_ULAW)); break;
  case AST_FORMAT_GSM:
    setAudioCodec(_codecMap.getCodec(PAYLOAD_CODEC_GSM)); break;
  case AST_FORMAT_ALAW:
    setAudioCodec(_codecMap.getCodec(PAYLOAD_CODEC_ALAW)); break;
  case AST_FORMAT_ILBC:
    setAudioCodec(_codecMap.getCodec(PAYLOAD_CODEC_ILBC_20)); break;
  case AST_FORMAT_SPEEX:
    setAudioCodec(_codecMap.getCodec(PAYLOAD_CODEC_SPEEX)); break;*/
  case AST_FORMAT_ULAW:
    setAudioCodec(PAYLOAD_CODEC_ULAW); break;
  case AST_FORMAT_GSM:
    setAudioCodec(PAYLOAD_CODEC_GSM); break;
  case AST_FORMAT_ALAW:
    setAudioCodec(PAYLOAD_CODEC_ALAW); break;
  case AST_FORMAT_ILBC:
    setAudioCodec(PAYLOAD_CODEC_ILBC_20); break;
  case AST_FORMAT_SPEEX:
    setAudioCodec(PAYLOAD_CODEC_SPEEX); break;
  default:
    setAudioCodec((CodecType) -1);
    break;
  }
}


int
IAXCall::getSupportedFormat()
{
  CodecMap map = getCodecMap().getCodecMap();
  int format   = 0;

  CodecMap::iterator iter = map.begin();
  while(iter != map.end()) {
    switch(iter->first) {
    case PAYLOAD_CODEC_ULAW:
      format |= AST_FORMAT_ULAW;  break;
    case PAYLOAD_CODEC_GSM:
      format |= AST_FORMAT_GSM;   break;
    case PAYLOAD_CODEC_ALAW:
      format |= AST_FORMAT_ALAW;  break;
    case PAYLOAD_CODEC_ILBC_20:
      format |= AST_FORMAT_ILBC;  break;
    case PAYLOAD_CODEC_SPEEX:
      format |= AST_FORMAT_SPEEX; break;
    default:
      break;
    }
    iter++;
  }
  return format;

}

int
IAXCall::getFirstMatchingFormat(int needles)
{
  CodecMap map = getCodecMap().getCodecMap();
  int format   = 0;

  CodecMap::iterator iter = map.begin();
  while(iter != map.end()) {
    switch(iter->first) {
    case PAYLOAD_CODEC_ULAW:
      format = AST_FORMAT_ULAW;  break;
    case PAYLOAD_CODEC_GSM:
      format = AST_FORMAT_GSM;   break;
    case PAYLOAD_CODEC_ALAW:
      format = AST_FORMAT_ALAW;  break;
    case PAYLOAD_CODEC_ILBC_20:
      format = AST_FORMAT_ILBC;  break;
    case PAYLOAD_CODEC_SPEEX:
      format = AST_FORMAT_SPEEX; break;
    default:
      break;
    }

    // Return the first that matches
    if (format & needles)
      return format;
    iter++;
  }
  return 0;
}
