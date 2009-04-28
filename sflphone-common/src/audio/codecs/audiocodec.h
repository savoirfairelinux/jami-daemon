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

  /** codec frame size in samples*/
  unsigned int _frameSize;

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
}

    AudioCodec( const AudioCodec& codec )
        : _codecName(codec._codecName), _clockRate(codec._clockRate), _channel(codec._channel),  _bitrate(codec._bitrate),_bandwidth(codec._bandwidth),_payload(codec._payload), _hasDynamicPayload(false),_state(true) {
  	
	_hasDynamicPayload = (_payload >= 96 && _payload <= 127) ? true : false;
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
  unsigned int getFrameSize( void ) { return _frameSize; }
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
