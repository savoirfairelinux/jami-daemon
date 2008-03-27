/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
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

#include "audiocodec.h"
#include <cstdio>
#include <speex/speex.h>

class Speex : public AudioCodec{
public:
	Speex(int payload=0)
 	: AudioCodec(payload, "speex")
	{
  	  _clockRate = 8000;
  	  _channel = 1;
	  _bitrate = 0;
	  _bandwidth = 0; 
  	  initSpeex();
	}

	void initSpeex() {
	/*
  	  if (_clockRate < 16000 ) {
    		_speexModePtr = &speex_nb_mode;
  	  } else if (_clockRate < 32000) {
    		_speexModePtr = &speex_wb_mode;
  	  } else {
    		_speexModePtr = &speex_uwb_mode;
  	  }
	*/
 	  // 8000 HZ --> Narrow-band mode
 	  // TODO Manage the other modes
 	  _speexModePtr = &speex_nb_mode; 

	// Init the decoder struct
  	  speex_bits_init(&_speex_dec_bits);
  	  _speex_dec_state = speex_decoder_init(_speexModePtr);

	// Init the encoder struct
  	  speex_bits_init(&_speex_enc_bits);
  	  _speex_enc_state = speex_encoder_init(_speexModePtr);

  	  speex_decoder_ctl(_speex_dec_state, SPEEX_GET_FRAME_SIZE, &_speex_frame_size); 
	}

	~Speex() 
	{
  	  terminateSpeex();
	}

	void terminateSpeex() {
	// Destroy the decoder struct
  	  speex_bits_destroy(&_speex_dec_bits);
  	  speex_decoder_destroy(_speex_dec_state);
  	  _speex_dec_state = 0;

	// Destroy the encoder struct
  	  speex_bits_destroy(&_speex_enc_bits);
  	  speex_encoder_destroy(_speex_enc_state);
  	  _speex_enc_state = 0;
	}

	virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) 
	{
    	  int ratio = 320 / _speex_frame_size;
  	  speex_bits_read_from(&_speex_dec_bits, (char*)src, size);
  	  speex_decode_int(_speex_dec_state, &_speex_dec_bits, dst);
  	  return _speex_frame_size * ratio; 
	}

	virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) 
	{
  	  speex_bits_reset(&_speex_enc_bits);
  	  speex_encoder_ctl(_speex_enc_state,SPEEX_SET_SAMPLING_RATE,&_clockRate);

  	  speex_encode_int(_speex_enc_state, src, &_speex_enc_bits);
  	  int nbBytes = speex_bits_write(&_speex_enc_bits, (char*)dst, size); 
  	  return nbBytes;
	}

private:
	const SpeexMode* _speexModePtr;
  	SpeexBits  _speex_dec_bits;
  	SpeexBits  _speex_enc_bits;
  	void *_speex_dec_state;
  	void *_speex_enc_state;
  	int _speex_frame_size;
};

// the class factories
extern "C" AudioCodec* create() {
    return new Speex(110);
}

extern "C" void destroy(AudioCodec* a) {
    delete a;
}

