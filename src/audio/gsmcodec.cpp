/**
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <iostream>
extern "C" {
#include <gsm/gsm.h>
}
#include "audiocodec.h"
#include "../global.h"

#define GSM_PACKED_FRAME_SIZE_IN_BYTES	33
#define GSM_UNPACKED_FRAME_SIZE_IN_BYTES	320
#define GSM_UNPACKED_FRAME_SIZE_IN_SHORTS	160
/*
void * GSMCodecHandle = 0;

void* (*gsm_session_create)() = 0;
void (*gsm_session_destroy)(void *) = 0;
void (*gsm_session_encode)(void*, unsigned char*, short*) = 0 ;
int (*gsm_session_decode)(void*, short* , unsigned char*) = 0 ;
*/
class Gsm : public AudioCodec {
public:
// 3 GSM A 8000 1 [RFC3551]
Gsm(int payload = 0) 
: AudioCodec(payload, "GSM")
{
  _clockRate = 8000;
  _channel   = 1;
 
  //initGSMStruct();
   
  if (!(_decode_gsmhandle = gsm_create() )) 
    _debug("ERROR: decode_gsm_create\n");
  if (!(_encode_gsmhandle = gsm_create() )) 
    _debug("AudioCodec: ERROR: encode_gsm_create\n");
  

   //_encode_state = gsm_session_create();
   //_decode_state = gsm_session_create();
   
}

~Gsm (void)
{
  //gsm_session_destroy(_decode_state);
  //gsm_session_destroy(_encode_state);
  gsm_destroy(_decode_gsmhandle);
  gsm_destroy(_encode_gsmhandle);
}

/*
bool initGSMStruct()
{
  if(GSMCodecHandle)  return true;

  GSMCodecHandle = dlopen("libgsm.so", RTLD_NOW | RTLD_GLOBAL);
  if(!GSMCodecHandle)  return false;
  gsm_session_create = (void * (*)()) dlsym(GSMCodecHandle, "gsm_create");
  gsm_session_destroy = (void (*) (void *)) dlsym(GSMCodecHandle, "gsm_destroy");
  gsm_session_encode = ( void (*) (void*, unsigned char*, short*)) dlsym(GSMCodecHandle, "gsm_encode");
  gsm_session_decode = (int (*) (void*, short*, unsigned char*)) dlsym(GSMCodecHandle, "gsm_decode");

  if(!(gsm_session_create && gsm_session_destroy && gsm_session_encode && gsm_session_decode)){
	dlclose(GSMCodecHandle);
	GSMCodecHandle = 0 ;
	return false;}
  return true;
}*/

virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) 
{
  if(gsm_decode(_decode_gsmhandle, (gsm_byte *)src, (gsm_signal*)dst) < 0)
    return 0;

  return GSM_UNPACKED_FRAME_SIZE_IN_BYTES;
}

virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) 
{
  if(size < GSM_UNPACKED_FRAME_SIZE_IN_BYTES) return 0;
  //if (gsm_session_encode( gsm_signal*)src, (gsm_byte*)dst);  
  gsm_encode( _encode_gsmhandle, (gsm_signal*)src, (gsm_byte*) dst);
  return GSM_PACKED_FRAME_SIZE_IN_BYTES;
}


private:
	gsm _decode_gsmhandle;
 	gsm _encode_gsmhandle;
	//void * _encode_state;
	//void * _decode_state;

};

// the class factories
extern "C" AudioCodec* create() {
    return new Gsm(3);
}

extern "C" void destroy(AudioCodec* a) {
    delete a;
}

