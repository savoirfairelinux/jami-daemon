/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author:  Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
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
#ifndef G729_H_
#define G729_H_

#include <cstdlib>
#include "sfl_types.h"
#include "noncopyable.h"

#include "audiocodec.h"

class bcg729DecoderChannelContextStruct;
class bcg729EncoderChannelContextStruct;

class G729 : public sfl::AudioCodec {
public:
   G729();
   ~G729();
private:
   AudioCodec * clone();
   virtual int decode(SFLAudioSample *pcm, unsigned char *data, size_t len);
   virtual int encode(unsigned char *data, SFLAudioSample *pcm, size_t max_data_bytes);

   NON_COPYABLE(G729);
   //Attributes
   bcg729DecoderChannelContextStruct* decoderContext_;
   bcg729EncoderChannelContextStruct* encoderContext_;
   void* handler_;

   //Extern functions
   void (*encoder_) (bcg729EncoderChannelContextStruct *encoderChannelContext, SFLAudioSample inputFrame[], uint8_t bitStream[]);
   void (*decoder_) (bcg729DecoderChannelContextStruct *decoderChannelContext, uint8_t bitStream[], uint8_t frameErasureFlag, SFLAudioSample signal[]);

   static void loadError(const char *error);
};

#endif  // G729_H_
