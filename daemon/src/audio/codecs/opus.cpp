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
#include "opus.h"
#include <stdexcept>
#include <dlfcn.h>

//BEGIN FUNCTION TYPE
#define OPUS_TYPE_ENCODER_CREATE   (OpusEncoder* (*)(int32_t,int,int,int*))
#define OPUS_TYPE_ENCODE          (int32_t (*)(OpusEncoder*,const int16_t*,int,unsigned char*,int32_t))
#define OPUS_TYPE_ENCODER_DESTROY  (void (*)(OpusEncoder*))

#define OPUS_TYPE_DECODER_CREATE   (OpusDecoder* (*)(int32_t,int,int*))
#define OPUS_TYPE_DECODE          (int (*)(OpusDecoder*,const unsigned char*,int32_t,int16_t*,int,int))
#define OPUS_TYPE_DECODER_DESTROY  (void (*)(OpusDecoder*))

//END FUNCTION TYPE

static const int Opus_PAYLOAD_TYPE = 104; // dynamic payload type, out of range of video (96-99)

Opus::OpusEncoder*(*Opus::opus_encoder_create              )(int32_t Fs, int channels, int application, int *error ) = 0;
int32_t           (*Opus::opus_encode                      )(Opus::OpusEncoder *st, const int16_t *pcm, int frame_size, unsigned char *data, int32_t max_data_bytes ) = 0;
void              (*Opus::opus_encoder_destroy             )(Opus::OpusEncoder *st) = 0;
Opus::OpusDecoder*(*Opus::opus_decoder_create              )(int32_t Fs, int channels, int *error ) = 0;
int               (*Opus::opus_decode                      )(Opus::OpusDecoder *st, const unsigned char *data, int32_t len, int16_t *pcm, int frame_size, int decode_fec ) = 0;
void              (*Opus::opus_decoder_destroy             )(Opus::OpusDecoder *st) = 0;

Opus::OpusEncoder* Opus::m_pEncoder = 0;
Opus::OpusDecoder* Opus::m_pDecoder = 0;
void*              Opus::m_pHandler = 0;


Opus::Opus() : sfl::AudioCodec(Opus_PAYLOAD_TYPE, "OPUS", CLOCK_RATE, FRAME_SIZE, CHANNELS)
{
   hasDynamicPayload_ = true;
   init();
}

bool Opus::init()
{
   m_pHandler = dlopen("libopus.so.0", RTLD_LAZY);
   if (!m_pHandler)
      return false;
   try {
       opus_encoder_create               = OPUS_TYPE_ENCODER_CREATE   dlsym(m_pHandler, "opus_encoder_create");
       loadError(dlerror());
       opus_encode                       = OPUS_TYPE_ENCODE           dlsym(m_pHandler, "opus_encode");
       loadError(dlerror());
       opus_encoder_destroy              = OPUS_TYPE_ENCODER_DESTROY  dlsym(m_pHandler, "opus_encoder_destroy");
       loadError(dlerror());
       opus_decoder_create               = OPUS_TYPE_DECODER_CREATE   dlsym(m_pHandler, "opus_decoder_create");
       loadError(dlerror());
       opus_decode                       = OPUS_TYPE_DECODE           dlsym(m_pHandler, "opus_decode");
       loadError(dlerror());
       opus_decoder_destroy              = OPUS_TYPE_DECODER_DESTROY  dlsym(m_pHandler, "opus_decoder_destroy");
       loadError(dlerror());

       int err = 0;
       m_pEncoder = opus_encoder_create(CLOCK_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
       if (err)
           return false;

       m_pDecoder = opus_decoder_create(CLOCK_RATE, CHANNELS, &err);
       if (err)
           return false;

   } catch (const std::exception & e) {
      return false;
   }

   return true;
}

Opus::~Opus()
{
   opus_encoder_destroy(m_pEncoder);
   opus_decoder_destroy(m_pDecoder);
   dlclose(m_pHandler);
}

int Opus::decode(short *dst, unsigned char *buf, size_t buffer_size)
{
   return opus_decode(m_pDecoder, buf, buffer_size, dst, FRAME_SIZE, 0);
}

int Opus::encode(unsigned char *dst, short *src, size_t buffer_size)
{
   return opus_encode(m_pEncoder, src, FRAME_SIZE, dst, buffer_size * 2);
}

void Opus::loadError(char* error)
{
   if (error != NULL)
      throw std::runtime_error("Opus failed to load");
}

// cppcheck-suppress unusedFunction
extern "C" sfl::Codec* CODEC_ENTRY()
{
    return new Opus;
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::Codec* a)
{
    delete a;
}

// cppcheck-suppress unusedFunction
extern "C" bool init()
{
    return Opus::init();
}


#undef OPUS_TYPE_ENCODER_CREATE
#undef OPUS_TYPE_ENCODE
#undef OPUS_TYPE_ENCODER_DESTROY

#undef OPUS_TYPE_DECODE
#undef OPUS_TYPE_DECODER_CREATE
#undef OPUS_TYPE_DECODER_DESTROY
