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

private:
  //bool _active;
  int _payload;
  bool _hasDynamicPayload;

public:
    AudioCodec(int payload, const std::string &codecName)
        : _codecName(codecName) {
	_payload = payload;
  	_clockRate = 8000; // default
  	_channel   = 1; // default
  //	_active = false;

  	_hasDynamicPayload = (_payload >= 96 && _payload <= 127) ? true : false;
}

    virtual ~AudioCodec() {}

    /**
     * @return the number of bytes decoded
     */
    virtual int codecDecode(short *, unsigned char *, unsigned int) = 0;
    virtual int codecEncode(unsigned char *, short *, unsigned int) = 0;   

  /** Value used for SDP negotiation */
  std::string getCodecName() { return _codecName; }
  int getPayload() { return _payload; }
  bool hasDynamicPayload() { return _hasDynamicPayload; }
  unsigned int getClockRate() { return _clockRate; }
  unsigned int getChannel() { return _channel; }
  //bool isActive() { return _active; }
  //void setActive(bool active) { _active = active; }
  

};

// the types of the class factories
typedef AudioCodec* create_t();
typedef void destroy_t(AudioCodec*);

#endif
