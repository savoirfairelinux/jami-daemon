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
#include <gsm/gsm.h>
#include "audiocodec.h"
#include "../global.h"


class Gsm : public AudioCodec {
public:
// 3 GSM A 8000 1 [RFC3551]
Gsm(int payload = 0) 
: AudioCodec(payload, "GSM")
{
  //_description = "GSM";
  _clockRate = 8000;
  _channel   = 1;
  
  if (!(_decode_gsmhandle = gsm_create() )) 
    _debug("ERROR: decode_gsm_create\n");
  if (!(_encode_gsmhandle = gsm_create() )) 
    _debug("AudioCodec: ERROR: encode_gsm_create\n");
  
}

~Gsm (void)
{
  gsm_destroy(_decode_gsmhandle);
  gsm_destroy(_encode_gsmhandle);
}

virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) 
{
  (void)size;
  if (gsm_decode(_decode_gsmhandle, (gsm_byte*)src, (gsm_signal*)dst) < 0) {
    _debug("ERROR: gsm_decode\n");
  }
  return 320;
}

virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) 
{
  (void)size;
  gsm_encode(_encode_gsmhandle, (gsm_signal*)src, (gsm_byte*)dst);  
  return 33;
}


private:
	gsm _decode_gsmhandle;
 	gsm _encode_gsmhandle;

};

// the class factories
extern "C" AudioCodec* create() {
    return new Gsm(3);
}

extern "C" void destroy(AudioCodec* a) {
    delete a;
}
