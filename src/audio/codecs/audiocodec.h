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
  unsigned int _clockRate;

  /** Number of channel 1 = mono, 2 = stereo */
  unsigned int _channel;

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
        : _codecName(codecName) {
	_payload = payload;
  	_clockRate = 8000; // default
  	_channel   = 1; // default

  	_hasDynamicPayload = (_payload >= 96 && _payload <= 127) ? true : false;
	_state = true;
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
  unsigned int getClockRate( void ) { return _clockRate; }
  unsigned int getChannel( void ) { return _channel; }
  bool getState( void ) { return _state; }
  void setState(bool b) { _state = b; }
  double getBitRate( void ) { return _bitrate; }
  double getBandwidth( void ) { return _bandwidth; }

};

// the types of the class factories
typedef AudioCodec* create_t();
typedef void destroy_t(AudioCodec*);

#endif
