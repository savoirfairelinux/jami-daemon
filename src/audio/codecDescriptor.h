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
#include <vector>
// To read directories content
#include <dirent.h>

#include "../global.h"
#include "../user_cfg.h"
#include "codecs/audiocodec.h"

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
  PAYLOAD_CODEC_SPEEX_8000 = 110,
  PAYLOAD_CODEC_SPEEX_16000 = 111,
  PAYLOAD_CODEC_SPEEX_32000 = 112
} AudioCodecType;

#include "codecs/audiocodec.h"

/* A codec is identified by its payload. A payload is associated with a name. */ 
typedef std::map<AudioCodecType, std::string> CodecMap;
/* The struct to reflect the order the user wants to use the codecs */
typedef std::vector<AudioCodecType> CodecOrder;

typedef std::pair<AudioCodec* , void*> CodecHandlePointer;
typedef std::map<AudioCodecType , AudioCodec*> CodecsMap;

class CodecDescriptor {
public:
  /**
   * Initialize all codec 
   */
  CodecDescriptor();
  ~CodecDescriptor(); 

  /*
   * Accessor to data structures
   */
  CodecsMap& getCodecsMap() { return _CodecsMap; }
  CodecOrder& getActiveCodecs() { return _codecOrder; }

  /**
   * Get codec name by its payload
   * @param payload the payload looked for
   *                same as getPayload()
   * @return the name of the codec
   */
  std::string getCodecName(AudioCodecType payload);
  
  /*
   * Get the codec object associated with the payload
   * @param payload The payload looked for
   * @return AudioCodec* A pointer on a AudioCodec object
   */
  AudioCodec* getCodec( AudioCodecType payload );

  /**
   * Initialiaze the map with all the supported codecs, even those inactive
   */  
  void init();

  /**
   * Set the default codecs order
   */   
  void setDefaultOrder();
  
  /**
   * Check in the map codec if the specified codec is supported 
   * @param payload unique identifier of a codec (RFC)
   * @return true if the codec specified is supported
   * 	     false otherwise
   */
  bool isActive(AudioCodecType payload);

 /**
  * Remove the codec with payload payload from the list
  * @param payload the codec to erase
  */ 
  void removeCodec(AudioCodecType payload);

 /**
  * Add a codec in the list.
  * @param payload the codec to add
  */
  void addCodec(AudioCodecType payload);

 /**
  * Get the bit rate of the specified codec.
  * @param payload The payload of the codec
  * @return double The bit rate 
  */  
  double getBitRate(AudioCodecType payload);

 /**
  * Get the bandwidth for one call with the specified codec.
  * The value has been calculated with the further information:
  * RTp communication, SIP protocol (the value with IAX2 is very close), no RTCP, one simultaneous call, for one channel (the incoming one).
  * @param payload The payload of the codec 
  * @return double The bandwidth
  */
  double getBandwidthPerCall(AudioCodecType payload);


 /**
  * Get the clock rate of the specified codec
  * @param payload The payload of the codec
  * @return int The clock rate of the specified codec
  */
  int getSampleRate(AudioCodecType payload);

  /*
   * Get the number of channels
   * @param payload The payload of the codec
   * @return int  Number of channels
   */
  int getChannel(AudioCodecType payload);

/**
 * Set the order of codecs by their payload
 * @param list The ordered list sent by DBus
 */
  void saveActiveCodecs(const std::vector<std::string>& list);
 

  std::string getDescription( std::string );
  /*
   * Get the number of codecs loaded in dynamic memory
   */
  int getCodecsNumber( void ) { return _nbCodecs; }
  
  /*
   * Unreferences the codecs loaded in memory
   */
  void deleteHandlePointer( void );
  
  /*
   * Get the first element of the CodecsMap struct. 
   * i.e the one with the lowest payload
   * @return AudioCodec	The pointer on the codec object
   */
  AudioCodec* getFirstCodecAvailable( void );

private:

  /*
   * Scan the installation directory ( --prefix configure option )
   * And load the dynamic library 
   * @return std::vector<AudioCodec*> The list of the codec object successfully loaded in memory
   */
  std::vector<AudioCodec *> scanCodecDirectory( void ); 
  
  /*
   * Load a codec
   * @param std::string	The path of the shared ( dynamic ) library.
   * @return AudioCodec*  the pointer of the object loaded.
   */
  AudioCodec* loadCodec( std::string );
  
  /*
   * Unload a codec
   * @param CodecHandlePointer	The map containing the pointer on the object and the pointer on the handle function
   */
  void unloadCodec( CodecHandlePointer );

  /*
   * Check if the files found in searched directories seems valid
   * @param std::string	The name of the file
   * @return true if the file name begins with libcodec_ and ends with .so
   *	     false otherwise
   */
  bool seemsValid( std::string );

  /*
   * Check if the codecs shared library has already been scanned during the session
   * Useful not to load twice the same codec saved in the different directory
   * @param std::string	The complete name of the shared directory ( without the path )
   * @return true if the codecs has been scanned
   *	    false otherwise
   */
  bool alreadyInCache( std::string );
  
  /*
   *  Check if the audiocodec object has been successfully created
   *  @param payload  The payload of the codec
   *  @return true if the audiocodec has been created
   *	      false otherwise
   */
  bool isCodecLoaded( int payload );
  
  /*
   * Map the payload of a codec and the object associated ( AudioCodec * )
   */
  CodecsMap _CodecsMap;
  
  /*
   * Vector containing the order of the codecs
   */
  CodecOrder _codecOrder;
  
  /*
   * Vector containing the complete name of the codec shared library scanned
   */
  std::vector<std::string> _Cache;

  /*
   * Number of codecs loaded
   */
  int _nbCodecs;
  
  /*
   * Vector containing pairs
   * Pair between pointer on function handle and pointer on audiocodec object
   */
  std::vector< CodecHandlePointer > _CodecInMemory;


};


#endif // __CODEC_DESCRIPTOR_H__
