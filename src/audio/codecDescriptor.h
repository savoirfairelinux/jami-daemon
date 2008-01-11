/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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


#ifndef __CODEC_DESCRIPTOR_H__
#define __CODEC_DESCRIPTOR_H__

#include <string>
#include <map>

typedef enum {
// http://www.iana.org/assignments/rtp-parameters
// http://www.gnu.org/software/ccrtp/doc/refman/html/formats_8h.html#a0
// 0 PCMU A 8000 1 [RFC3551]
  PAYLOAD_CODEC_ULAW = 0, 
// 3 GSM  A 8000 1 [RFC3551]
  PAYLOAD_CODEC_GSM = 3,
// 8 PCMA A 8000 1 [RFC3551]
  PAYLOAD_CODEC_ALAW = 8,
// http://www.ietf.org/rfc/rfc3952.txt
// 97 iLBC/8000
  PAYLOAD_CODEC_ILBC = 97,
// http://www.speex.org/drafts/draft-herlein-speex-rtp-profile-00.txt
//  97 speex/8000
// http://support.xten.com/viewtopic.php?p=8684&sid=3367a83d01fdcad16c7459a79859b08e
// 100 speex/16000
  PAYLOAD_CODEC_SPEEX = 110
} CodecType;

#include "audiocodec.h"
typedef std::map<CodecType, AudioCodec*> CodecMap;

class CodecDescriptorMap {
public:
  /**
   * Initialize all codec 
   */
  CodecDescriptorMap();
  ~CodecDescriptorMap() {};
  CodecMap getMap() { return _codecMap; }

  /**
   * Get codec with is associated payload
   * @param payload the payload associated with the payload
   *                same as getPayload()
   * @return the address of the codec or 0
   */
  AudioCodec* getCodec(CodecType payload);

  /**
   * Get codec with is associated payload
   * Put a codec active, with it's codec's _description
   * O(n) if not found where n is the number of element
   * @param codecDescription is the same as with getCodec(number)->getDescription()
   */
  void setActive(const std::string& codecDescription);
  void setInactive(const std::string& codecDescription);
private:
  CodecMap _codecMap;
};

#endif // __CODEC_DESCRIPTOR_H__
