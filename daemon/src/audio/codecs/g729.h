/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 * Author:  Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
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
#ifndef G729_H
#define G729_H
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdlib>
#include "noncopyable.h"
#include "g729/decoder.h"
#include "g729/encoder.h"
#include "g729/typedef.h"

#include "audiocodec.h"
#include "sfl_types.h"

#define G729_TYPE_ENCODERCHANNEL (void (*)(bcg729EncoderChannelContextStruct*))
#define G729_TYPE_ENCODER        (void (*)(bcg729EncoderChannelContextStruct*, int16_t[], uint8_t[]))
#define G729_TYPE_DECODERCHANNEL (void(*)(bcg729DecoderChannelContextStruct*))
#define G729_TYPE_DECODER        (void (*)(bcg729DecoderChannelContextStruct*, uint8_t[], uint8_t, int16_t[]))

#define G729_TYPE_DECODER_INIT   (bcg729DecoderChannelContextStruct*(*)())
#define G729_TYPE_ENCODER_INIT   (bcg729EncoderChannelContextStruct*(*)())


class G729 : public sfl::AudioCodec {
public:
   G729();
   ~G729();
   static bool init();
   virtual int decode(short *dst, unsigned char *buf, size_t buffer_size);
   virtual int encode(unsigned char *dst, short *src, size_t buffer_size);

private:
   NON_COPYABLE(G729);
   //Attributes
   static bcg729DecoderChannelContextStruct* m_spDecStruct;
   static bcg729EncoderChannelContextStruct* m_spEncStruct;
   static void* m_pHandler;

   //Extern functions
   static void (*closeBcg729EncoderChannel) ( bcg729EncoderChannelContextStruct *encoderChannelContext                                                                   );
   static void (*bcg729Encoder)             ( bcg729EncoderChannelContextStruct *encoderChannelContext, int16_t inputFrame[], uint8_t bitStream[]                        );
   static void (*closeBcg729DecoderChannel) ( bcg729DecoderChannelContextStruct *decoderChannelContext                                                                   );
   static void (*bcg729Decoder)             ( bcg729DecoderChannelContextStruct *decoderChannelContext, uint8_t bitStream[] , uint8_t frameErasureFlag, int16_t signal[] );

   static void loadError(char* error);
};

#endif
