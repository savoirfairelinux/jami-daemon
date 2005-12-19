/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#ifndef __CODEC_SPEEX_H_
#define __CODEC_SPEEX_H_

#include "audiocodec.h"
#include <speex/speex.h>

class CodecSpeex : public AudioCodec
{
public:
  CodecSpeex(int payload);
  ~CodecSpeex();
  int codecDecode(short *, unsigned char *, unsigned int);
  int codecEncode(unsigned char *, short *, unsigned int);

  // only for speex
  int getFrameSize() { return _speex_frame_size; }

private:
  unsigned int _clockRate;
  unsigned int _channel;

  void initSpeex();
  void terminateSpeex();
  SpeexMode* _speexModePtr;
  SpeexBits  _speex_dec_bits;
  SpeexBits  _speex_enc_bits;
  void *_speex_dec_state;
  void *_speex_enc_state;
  int _speex_frame_size;
};

#endif // __CODEC_SPEEX_H_

