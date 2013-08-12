/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author:  Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@wisdomvibes.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#ifndef OPUS_H_
#define OPUS_H_

#include "noncopyable.h"
#include "sfl_types.h"

#include "audiocodec.h"

#include <opus/opus.h>

class Opus : public sfl::AudioCodec {
public:
   Opus();
   ~Opus();

   static const uint8_t PAYLOAD_TYPE = 104; // dynamic payload type, out of range of video (96-99)

private:
   virtual int decode(SFLAudioSample *dst, unsigned char *buf, size_t buffer_size);
   virtual int encode(unsigned char *dst, SFLAudioSample *src, size_t buffer_size);

   //multichannel version
   virtual int decode(std::vector<std::vector<SFLAudioSample> > &dst, unsigned char *buf, size_t buffer_size, size_t dst_offset=0);
   virtual int encode(unsigned char *dst, std::vector<std::vector<SFLAudioSample> > &src, size_t buffer_size);

   NON_COPYABLE(Opus);
   //Attributes
   OpusEncoder *encoder_;
   OpusDecoder *decoder_;
   std::vector<opus_int16> interleaved_;

   static const int FRAME_SIZE = 160;
   static const int CLOCK_RATE = 16000;
   static const int CHANNELS   = 2;
};

#endif
