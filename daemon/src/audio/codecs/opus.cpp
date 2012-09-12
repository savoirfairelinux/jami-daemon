/*
 * Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include <iostream>
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

Opus::Opus() : sfl::AudioCodec(Opus_PAYLOAD_TYPE, "OPUS", CLOCK_RATE, FRAME_SIZE, CHANNELS),
    handler_(0),
    encoder_(0),
    decoder_(0)
{
   hasDynamicPayload_ = true;

   handler_ = dlopen("libopus.so.0", RTLD_NOW);
   if (!handler_)
       throw std::runtime_error("opus: did not open shared lib");

   opus_encoder_create  = OPUS_TYPE_ENCODER_CREATE dlsym(handler_, "opus_encoder_create");
   loadError(dlerror());
   opus_encode          = OPUS_TYPE_ENCODE dlsym(handler_, "opus_encode");
   loadError(dlerror());
   opus_encoder_destroy = OPUS_TYPE_ENCODER_DESTROY dlsym(handler_, "opus_encoder_destroy");
   loadError(dlerror());
   opus_decoder_create  = OPUS_TYPE_DECODER_CREATE dlsym(handler_, "opus_decoder_create");
   loadError(dlerror());
   opus_decode          = OPUS_TYPE_DECODE dlsym(handler_, "opus_decode");
   loadError(dlerror());
   opus_decoder_destroy = OPUS_TYPE_DECODER_DESTROY dlsym(handler_, "opus_decoder_destroy");
   loadError(dlerror());

   int err = 0;
   encoder_ = opus_encoder_create(CLOCK_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
   if (err)
       throw std::runtime_error("opus: could not create encoder");

   decoder_ = opus_decoder_create(CLOCK_RATE, CHANNELS, &err);
   if (err)
       throw std::runtime_error("opus: could not create decoder");
}

Opus::~Opus()
{
    if (encoder_)
        opus_encoder_destroy(encoder_);
    if (decoder_)
        opus_decoder_destroy(decoder_);
    if (handler_)
        dlclose(handler_);
}

int Opus::decode(short *dst, unsigned char *buf, size_t buffer_size)
{
   return opus_decode(decoder_, buf, buffer_size, dst, FRAME_SIZE, 0);
}

int Opus::encode(unsigned char *dst, short *src, size_t buffer_size)
{
   return opus_encode(encoder_, src, FRAME_SIZE, dst, buffer_size * 2);
}

void Opus::loadError(const char *error)
{
   if (error != NULL)
      throw std::runtime_error("opus failed to load");
}

// cppcheck-suppress unusedFunction
extern "C" sfl::Codec* CODEC_ENTRY()
{
    try {
        return new Opus;
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return 0;
    }
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::Codec* a)
{
    delete a;
}
