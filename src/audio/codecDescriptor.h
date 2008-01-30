/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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


#ifndef __CODEC_DESCRIPTOR_H__
#define __CODEC_DESCRIPTOR_H__

#include <string>
#include <map>

#include "../global.h"
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
  PAYLOAD_CODEC_ILBC_20 = 97,
  PAYLOAD_CODEC_ILBC_30 = 98,
// http://www.speex.org/drafts/draft-herlein-speex-rtp-profile-00.txt
//  97 speex/8000
// http://support.xten.com/viewtopic.php?p=8684&sid=3367a83d01fdcad16c7459a79859b08e
// 100 speex/16000
  PAYLOAD_CODEC_SPEEX = 110
} CodecType;

#include "audiocodec.h"

/* A codec is identified by its payload. A payload is associated with a name. */ 
typedef std::map<CodecType, std::string> CodecMap;

class CodecDescriptor {
public:
  /**
   * Initialize all codec 
   */
  CodecDescriptor();
  ~CodecDescriptor() {};
  CodecMap& getCodecMap() { return _codecMap; }

  /**
   * Get codec with is associated payload
   * @param payload the payload associated with the payload
   *                same as getPayload()
   * @return the name of the codec
   */
  std::string& getCodecName(CodecType payload);

  void init();

  /**
   * Check in the map codec if the specified codec is supported 
   * @param payload unique identifier of a codec (RFC)
   * @return true if the codec specified is supported
   * 	     false otherwise
   */
  bool isSupported(CodecType payload);

 /**
  * Remove the codec with payload payload from the list
  * @param payload the codec to erase
  */ 
  void removeCodec(CodecType payload);

 /**
  * Add a codec in the list.
  * @param payload the codec to add
  */
  void addCodec(CodecType payload);

 /**
  * Get the bit rate of the specified codec.
  * @param payload The payload of the codec
  * @return double The bit rate 
  */  
  double getBitRate(CodecType payload);

 /**
  * Get the bandwidth for one call with the specified codec.
  * The value has been calculated with the further information:
  * RTp communication, SIP protocol (the value with IAX2 is very close), no RTCP, one simultaneous call, for one channel (the incoming one).
  * @param payload The payload of the codec 
  * @return double The bandwidth
  */
  double getBandwidthPerCall(CodecType payload);


 /**
  * Get the clock rate of the specified codec
  * @param payload The payload of the codec
  * @return int The clock rate of the specified codec
  */
  int getSampleRate(CodecType payload);
private:
  CodecMap _codecMap;
};

#endif // __CODEC_DESCRIPTOR_H__
