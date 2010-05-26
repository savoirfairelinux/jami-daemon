/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __CODEC_DESCRIPTOR_H__
#define __CODEC_DESCRIPTOR_H__

#include <map>
#include <vector>
#include <dirent.h>

#include "global.h"
#include "user_cfg.h"
#include "audio/codecs/audiocodec.h"

/** Enable us to keep the handle pointer on the codec dynamicaly loaded so that we could destroy when we dont need it anymore */
typedef std::pair<AudioCodec* , void*> CodecHandlePointer;
/** Maps a pointer on an audiocodec object to a payload */
typedef std::map<AudioCodecType , AudioCodec*> CodecsMap;
/** A codec is identified by its payload. A payload is associated with a name. */ 
typedef std::map<AudioCodecType, std::string> CodecMap;

/*
 * @file codecdescriptor.h
 * @brief Handle audio codecs, load them in memory
 */

class CodecDescriptor {
  public:
    /**
     * Constructor 
     */
    CodecDescriptor();

    /**
     * Destructor 
     */
    ~CodecDescriptor(); 

    /**
     * Accessor to data structures
     * @return CodecsMap& The available codec
     */
    CodecsMap& getCodecsMap() { return _CodecsMap; }

    /**
     * Accessor to data structures
     * @return CodecOrder& The list that reflects the user's choice
     */
    // CodecOrder& getActiveCodecs() { return _codecOrder; }

    /**
     * Get codec name by its payload
     * @param payload the payload looked for
     *                same as getPayload()
     * @return std::string  The name of the codec
     */
    std::string getCodecName(AudioCodecType payload);

    /**
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
     * Set the default codecs order. 
	 * This order will be apply to each account by default
     */   
    void setDefaultOrder();

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

	/**
     * Set the order of codecs by their payload
     * @param list The ordered list sent by DBus
     */
    void saveActiveCodecs(const std::vector<std::string>& list);

    /**
     * Get the number of codecs loaded in dynamic memory
     * @return int The number
     */
    int getCodecsNumber( void ) { return _nbCodecs; }

    /**
     * Unreferences the codecs loaded in memory
     */
    void deleteHandlePointer( void );

    /**
     * Get the first element of the CodecsMap struct. 
     * i.e the one with the lowest payload
     * @return AudioCodec	The pointer on the codec object
     */
    AudioCodec* getFirstCodecAvailable( void );

    /**
     * Instantiate a codec, used in AudioRTP to get an instance of Codec per call
     * @param CodecHandlePointer	The map containing the pointer on the object and the pointer on the handle function
     */
    AudioCodec* instantiateCodec(AudioCodecType payload);

	/**
	 * For a given codec, return its specification
	 *
	 * @param payload	The RTP payload of the codec
	 * @return std::vector <std::string>	A vector containing codec's name, sample rate, bandwidth and bit rate
	 */
	std::vector <std::string> getCodecSpecifications (const int32_t& payload);

	/**
     *  Check if the audiocodec object has been successfully created
     *  @param payload  The payload of the codec
     *  @return bool  True if the audiocodec has been created
     *		false otherwise
     */
    bool isCodecLoaded( int payload );

  private:

    /**
     * Scan the installation directory ( --prefix configure option )
     * And load the dynamic library 
     * @return std::vector<AudioCodec*> The list of the codec object successfully loaded in memory
     */
    std::vector<AudioCodec *> scanCodecDirectory( void ); 

    /**
     * Load a codec
     * @param std::string	The path of the shared ( dynamic ) library.
     * @return AudioCodec*  the pointer of the object loaded.
     */
    AudioCodec* loadCodec( std::string );

    /**
     * Unload a codec
     * @param CodecHandlePointer	The map containing the pointer on the object and the pointer on the handle function
     */
    void unloadCodec( CodecHandlePointer );

    /**
     * Check if the files found in searched directories seems valid
     * @param std::string	The name of the file
     * @return bool True if the file name begins with libcodec_ and ends with .so
     *		false otherwise
     */
    bool seemsValid( std::string );

    /**
     * Check if the codecs shared library has already been scanned during the session
     * Useful not to load twice the same codec saved in the different directory
     * @param std::string	The complete name of the shared directory ( without the path )
     * @return bool True if the codecs has been scanned
     *	    false otherwise
     */
    bool alreadyInCache( std::string );

    /**
     * Map the payload of a codec and the object associated ( AudioCodec * )
     */
    CodecsMap _CodecsMap;

    /**
     * Vector containing a default order for the codecs
     */
    CodecOrder _defaultCodecOrder;

    /**
     * Vector containing the complete name of the codec shared library scanned
     */
    std::vector<std::string> _Cache;

    /**
     * Number of codecs loaded
     */
    int _nbCodecs;

    /**
     * Vector containing pairs
     * Pair between pointer on function handle and pointer on audiocodec object
     */
    std::vector< CodecHandlePointer > _CodecInMemory;
};

#endif // __CODEC_DESCRIPTOR_H__
