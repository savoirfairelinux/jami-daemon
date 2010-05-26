/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 * Author:  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 * Motly borrowed from asterisk's sources (Steve Underwood <steveu@coppice.org>)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                              
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef _CODEC_AUDIO_H
#define _CODEC_AUDIO_H

#include <string> 
#include <iostream>
#include <dlfcn.h>

class AudioCodec {
protected:
  /** Holds SDP-compliant codec name */
  std::string _codecName; // what we put inside sdp
  
  /** Clock rate or sample rate of the codec, in Hz */
  int _clockRate;

  /** Number of channel 1 = mono, 2 = stereo */
  int _channel;

  /** codec frame size in samples*/
  int _frameSize;

  /** Bitrate */
  double _bitrate;
  /** Bandwidth */
  double _bandwidth;

private:
  int _payload;
  bool _hasDynamicPayload;
  bool _state;

public:
    AudioCodec(int payload, const std::string &codecName)
        : _codecName(codecName), _clockRate(8000), _channel(1),  _bitrate(0.0),_bandwidth(0),_payload(payload), _hasDynamicPayload(false),_state(true) {
  	
	_hasDynamicPayload = (_payload >= 96 && _payload <= 127) ? true : false;

	// If g722 (payload 9), we need to init libccrtp symetric sessions with using
	// dynamic payload format. This way we get control on rtp clockrate.
	
	if(_payload == 9)
	{
	    _hasDynamicPayload = true;
	}
	
}

    AudioCodec( const AudioCodec& codec )
        : _codecName(codec._codecName), _clockRate(codec._clockRate), _channel(codec._channel),  _bitrate(codec._bitrate),_bandwidth(codec._bandwidth),_payload(codec._payload), _hasDynamicPayload(false),_state(true) {
  	
	_hasDynamicPayload = (_payload >= 96 && _payload <= 127) ? true : false;

	// If g722 (payload 9), we need to init libccrtp symetric sessions with using
	// dynamic payload format. This way we get control on rtp clockrate.
	
	if(_payload == 9)
	{
	    _hasDynamicPayload = true;
	}
	
}

    virtual ~AudioCodec() {
	}
    /**
     * Decode an input buffer and fill the output buffer with the decoded data 
     * @return the number of bytes decoded
     */
    virtual int codecDecode(short *, unsigned char *, unsigned int) = 0;

    /**
     * Encode an input buffer and fill the output buffer with the encoded data 
     * @return the number of bytes encoded
     */
    virtual int codecEncode(unsigned char *, short *, unsigned int) = 0;   


  /** Value used for SDP negotiation */
  std::string getCodecName( void ) { return _codecName; }
  int getPayload( void ) { return _payload; }
  bool hasDynamicPayload( void ) { return _hasDynamicPayload; }
  int getClockRate( void ) { return _clockRate; }
  int getFrameSize( void ) { return _frameSize; }
  int getChannel( void ) { return _channel; }
  bool getState( void ) { return _state; }
  void setState(bool b) { _state = b; }
  double getBitRate( void ) { return _bitrate; }
  double getBandwidth( void ) { return _bandwidth; }

};

// the types of the class factories
typedef AudioCodec* create_t();
typedef void destroy_t(AudioCodec*);

#endif
