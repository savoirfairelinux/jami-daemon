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
#include "global.h"
#include <stdexcept>

//BEGIN FUNCTION TYPE
//Make the code more readable, keep in .cpp. There is no point to make them "public" in the .h. Internal use only
#define OPUS_TYPE_ENCODER_INIT     (int (*)(OpusEncoder*,int32_t,int,int))
#define OPUS_TYPE_ENCODER_CREATE   (OpusEncoder* (*)(int32_t,int,int,int*))
#define OPUS_TYPE_ENCODER          (int32_t (*)(OpusEncoder*,const int16_t*,int,unsigned char*,int32_t))
#define OPUS_TYPE_ENCODER_FLOAT    (int32_t (*)(OpusEncoder*,const float*,int, unsigned char*,int32_t))
#define OPUS_TYPE_ENCODER_DESTROY  (void (*)(OpusEncoder*))
#define OPUS_TYPE_ENCODER_CTL      (int (*)(OpusEncoder*,int,...))
#define OPUS_TYPE_ENCODER_SIZE     (int (*)(int))

#define OPUS_TYPE_DECODER_INIT     (int (*)(OpusDecoder*,int32_t,int))
#define OPUS_TYPE_DECODER          (int (*)(OpusDecoder*,const unsigned char*,int32_t,int16_t*,int,int))
#define OPUS_TYPE_DECODER_FLOAT    (int (*)(OpusDecoder*,const unsigned char*,int32_t,float*,int,int))
#define OPUS_TYPE_DECODER_CREATE   (OpusDecoder* (*)(int32_t,int,int*))
#define OPUS_TYPE_DECODER_DESTROY  (void (*)(OpusDecoder*))
#define OPUS_TYPE_DECODER_CTL      (int (*)(OpusDecoder*,int,...))
#define OPUS_TYPE_DECODER_SIZE     (int (*)(int))
#define OPUS_TYPE_DECODER_NBSAMPLE (int (*)(const OpusDecoder*,const unsigned char*,int32_t))

#define OPUS_TYPE_PACKET_PARSE     (int (*)(const unsigned char*,int32_t,unsigned char*,const unsigned char**,short*,int*))
#define OPUS_TYPE_PACKET_BANDWIDTH (int (*)(const unsigned char*))
#define OPUS_TYPE_PACKET_SAMPLE    (int (*)(const unsigned char*,int32_t))
#define OPUS_TYPE_PACKET_NBCHAN    (int (*)(const unsigned char*))
#define OPUS_TYPE_PACKET_NBFRM     (int (*)(const unsigned char*,int32_t))
#define OPUS_TYPE_PACKET_NBSAMPLE  (int (*)(const OpusDecoder*,const unsigned char*,int32_t))

#define OPUS_TYPE_REPACK_INIT      (OpusRepacketizer* (*)(OpusRepacketizer*))
#define OPUS_TYPE_REPACK_SIZE      (int (*)(void))
#define OPUS_TYPE_REPACK_CREATE    (OpusRepacketizer* (*)(void))
#define OPUS_TYPE_REPACK_DESTROY   (void (*)(OpusRepacketizer*))
#define OPUS_TYPE_REPACK_CAT       (int (*)(OpusRepacketizer*,const unsigned char*, int32_t))
#define OPUS_TYPE_REPACK_RANGE     (int32_t (*)(OpusRepacketizer*,int,int,unsigned char*,int32_t))
#define OPUS_TYPE_REPACK_NBFRM     (int (*)(OpusRepacketizer*))
#define OPUS_TYPE_REPACK_OUT       (int32_t (*)(OpusRepacketizer*,unsigned char*,int32_t))
//END FUNCTION TYPE

static const int Opus_PAYLOAD_TYPE = 89; //FAKE VALUE

int               (*Opus::opus_encoder_get_size            )(int channels) = 0;
Opus::OpusEncoder*(*Opus::opus_encoder_create              )(int32_t Fs, int channels, int application, int *error ) = 0;
int               (*Opus::opus_encoder_init                )(Opus::OpusEncoder *st, int32_t Fs, int channels, int application ) = 0;
int32_t           (*Opus::opus_encode                      )(Opus::OpusEncoder *st, const int16_t *pcm, int frame_size, unsigned char *data, int32_t max_data_bytes ) = 0;
int32_t           (*Opus::opus_encode_float                )(Opus::OpusEncoder *st, const float *pcm, int frame_size, unsigned char *data, int32_t max_data_bytes ) = 0;
void              (*Opus::opus_encoder_destroy             )(Opus::OpusEncoder *st) = 0;
int               (*Opus::opus_encoder_ctl                 )(Opus::OpusEncoder *st, int request, ...) = 0;
int               (*Opus::opus_decoder_get_size            )(int channels) = 0;
Opus::OpusDecoder*(*Opus::opus_decoder_create              )(int32_t Fs, int channels, int *error ) = 0;
int               (*Opus::opus_decoder_init                )(Opus::OpusDecoder *st, int32_t Fs, int channels ) = 0;
int               (*Opus::opus_decode                      )(Opus::OpusDecoder *st, const unsigned char *data, int32_t len, int16_t *pcm, int frame_size, int decode_fec ) = 0;
int               (*Opus::opus_decode_float                )(Opus::OpusDecoder *st, const unsigned char *data, int32_t len, float *pcm, int frame_size, int decode_fec ) = 0;
int               (*Opus::opus_decoder_ctl                 )(Opus::OpusDecoder *st, int request, ...) = 0;
void              (*Opus::opus_decoder_destroy             )(Opus::OpusDecoder *st) = 0;
int               (*Opus::opus_packet_parse                )( const unsigned char *data, int32_t len, unsigned char *out_toc, const unsigned char *frames[48], short size[48], int *payload_offset ) = 0;
int               (*Opus::opus_packet_get_bandwidth        )(const unsigned char *data) = 0;
int               (*Opus::opus_packet_get_samples_per_frame)(const unsigned char *data, int32_t Fs) = 0;
int               (*Opus::opus_packet_get_nb_channels      )(const unsigned char *data) = 0;
int               (*Opus::opus_packet_get_nb_frames        )(const unsigned char packet[], int32_t len) = 0;
int               (*Opus::opus_decoder_get_nb_samples      )(const Opus::OpusDecoder *dec, const unsigned char packet[], int32_t len) = 0;
int               (*Opus::opus_repacketizer_get_size       )(void) = 0;
Opus::OpusRepacketizer* (*Opus::opus_repacketizer_init     )(Opus::OpusRepacketizer *rp) = 0;
Opus::OpusRepacketizer* (*Opus::opus_repacketizer_create   )(void) = 0;
void              (*Opus::opus_repacketizer_destroy        )(Opus::OpusRepacketizer *rp) = 0;
int               (*Opus::opus_repacketizer_cat            )(Opus::OpusRepacketizer *rp, const unsigned char *data, int32_t len) = 0;
int32_t           (*Opus::opus_repacketizer_out_range      )(Opus::OpusRepacketizer *rp, int begin, int end, unsigned char *data, int32_t maxlen) = 0;
int               (*Opus::opus_repacketizer_get_nb_frames  )(Opus::OpusRepacketizer *rp) = 0;
int32_t           (*Opus::opus_repacketizer_out            )(Opus::OpusRepacketizer *rp, unsigned char *data, int32_t maxlen) = 0;


Opus::OpusEncoder* Opus::m_pEncoder =0;
Opus::OpusDecoder* Opus::m_pDecoder =0;
void*              Opus::m_pHandler =0;


Opus::Opus() : sfl::AudioCodec(Opus_PAYLOAD_TYPE, "OPUS", CLOCK_RATE, FRAME_SIZE, CHANNAL)
{
   init();
}

bool Opus::init()
{
   m_pHandler = dlopen("libopus.so.0", RTLD_LAZY);
   if (!m_pHandler)
      return false;
   try {
         opus_encoder_get_size             = OPUS_TYPE_ENCODER_SIZE     dlsym(m_pHandler, "opus_encoder_get_size");
         loadError(dlerror());
         opus_encoder_create               = OPUS_TYPE_ENCODER_CREATE   dlsym(m_pHandler, "opus_encoder_create");
         loadError(dlerror());
         opus_encoder_init                 = OPUS_TYPE_ENCODER_INIT     dlsym(m_pHandler, "opus_encoder_init");
         loadError(dlerror());
         opus_encode                       = OPUS_TYPE_ENCODER          dlsym(m_pHandler, "opus_encode");
         loadError(dlerror());
         opus_encode_float                 = OPUS_TYPE_ENCODER_FLOAT    dlsym(m_pHandler, "opus_encode_float");
         loadError(dlerror());
         opus_encoder_destroy              = OPUS_TYPE_ENCODER_DESTROY  dlsym(m_pHandler, "opus_encoder_destroy");
         loadError(dlerror());
         opus_encoder_ctl                  = OPUS_TYPE_ENCODER_CTL      dlsym(m_pHandler, "opus_encoder_ctl");
         loadError(dlerror());
         opus_decoder_get_size             = OPUS_TYPE_DECODER_SIZE     dlsym(m_pHandler, "opus_decoder_get_size");
         loadError(dlerror());
         opus_decoder_create               = OPUS_TYPE_DECODER_CREATE   dlsym(m_pHandler, "opus_decoder_create");
         loadError(dlerror());
         opus_decoder_init                 = OPUS_TYPE_DECODER_INIT     dlsym(m_pHandler, "opus_decoder_init");
         loadError(dlerror());
         opus_decode                       = OPUS_TYPE_DECODER          dlsym(m_pHandler, "opus_decode");
         loadError(dlerror());
         opus_decode_float                 = OPUS_TYPE_DECODER_FLOAT    dlsym(m_pHandler, "opus_decode_float");
         loadError(dlerror());
         opus_decoder_ctl                  = OPUS_TYPE_DECODER_CTL      dlsym(m_pHandler, "opus_decoder_ctl");
         loadError(dlerror());
         opus_decoder_destroy              = OPUS_TYPE_DECODER_DESTROY  dlsym(m_pHandler, "opus_decoder_destroy");
         loadError(dlerror());
         opus_decoder_get_nb_samples       = OPUS_TYPE_DECODER_NBSAMPLE dlsym(m_pHandler, "opus_decoder_get_nb_samples");
         loadError(dlerror());
         opus_packet_parse                 = OPUS_TYPE_PACKET_PARSE     dlsym(m_pHandler, "opus_packet_parse");
         loadError(dlerror());
         opus_packet_get_bandwidth         = OPUS_TYPE_PACKET_BANDWIDTH dlsym(m_pHandler, "opus_packet_get_bandwidth");
         loadError(dlerror());
         opus_packet_get_samples_per_frame = OPUS_TYPE_PACKET_SAMPLE    dlsym(m_pHandler, "opus_packet_get_samples_per_frame");
         loadError(dlerror());
         opus_packet_get_nb_channels       = OPUS_TYPE_PACKET_NBCHAN    dlsym(m_pHandler, "opus_packet_get_nb_channels");
         loadError(dlerror());
         opus_packet_get_nb_frames         = OPUS_TYPE_PACKET_NBFRM     dlsym(m_pHandler, "opus_packet_get_nb_frames");
         loadError(dlerror());
         opus_repacketizer_get_size        = OPUS_TYPE_REPACK_SIZE      dlsym(m_pHandler, "opus_repacketizer_get_size");
         loadError(dlerror());
         opus_repacketizer_init            = OPUS_TYPE_REPACK_INIT      dlsym(m_pHandler, "opus_repacketizer_init");
         loadError(dlerror());
         opus_repacketizer_create          = OPUS_TYPE_REPACK_CREATE    dlsym(m_pHandler, "opus_repacketizer_create");
         loadError(dlerror());
         opus_repacketizer_destroy         = OPUS_TYPE_REPACK_DESTROY   dlsym(m_pHandler, "opus_repacketizer_destroy");
         loadError(dlerror());
         opus_repacketizer_cat             = OPUS_TYPE_REPACK_CAT       dlsym(m_pHandler, "opus_repacketizer_cat");
         loadError(dlerror());
         opus_repacketizer_out_range       = OPUS_TYPE_REPACK_RANGE     dlsym(m_pHandler, "opus_repacketizer_out_range");
         loadError(dlerror());
         opus_repacketizer_get_nb_frames   = OPUS_TYPE_REPACK_NBFRM     dlsym(m_pHandler, "opus_repacketizer_get_nb_frames");
         loadError(dlerror());
         opus_repacketizer_out             = OPUS_TYPE_REPACK_OUT       dlsym(m_pHandler, "opus_repacketizer_out");
         loadError(dlerror());

         int err;
         m_pEncoder = opus_encoder_create(CLOCK_RATE, CHANNAL, OPUS_APPLICATION_VOIP, &err);
         if (err) {
            return false;
         }
//          if (int err = opus_encoder_init(m_pEncoder, CLOCK_RATE, CHANNAL, OPUS_APPLICATION_VOIP )) {
//             printf("FAILED1 %d\n",err);
//             return false;
//          }

         m_pDecoder = opus_decoder_create(CLOCK_RATE, CHANNAL, &err );
         if (err) {
            return false;
         }
//          if (int err = opus_decoder_init(m_pDecoder, CLOCK_RATE, CHANNAL )) {
//             printf("FAILED2 %d\n",err);
//             return false;
//          }
   }
   catch(std::exception const& e) {
      return false;
   }

   return true;
}

Opus::~Opus()
{
   dlclose(m_pHandler);
}

int Opus::decode(short *dst, unsigned char *buf, size_t buffer_size)
{
   return opus_decode(m_pDecoder, buf, buffer_size, dst, FRAME_SIZE, 1 );
}

int Opus::encode(unsigned char *dst, short *src, size_t buffer_size)
{
   return opus_encode(m_pEncoder, src, FRAME_SIZE, dst, buffer_size);
}

void Opus::loadError(char* error)
{
   if ((error) != NULL)
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


#undef OPUS_TYPE_ENCODER_INIT
#undef OPUS_TYPE_ENCODER_CREATE
#undef OPUS_TYPE_ENCODER
#undef OPUS_TYPE_ENCODER_FLOAT
#undef OPUS_TYPE_ENCODER_DESTROY
#undef OPUS_TYPE_ENCODER_CTL

#undef OPUS_TYPE_DECODER_INIT
#undef OPUS_TYPE_DECODER
#undef OPUS_TYPE_DECODER_FLOAT
#undef OPUS_TYPE_DECODER_CREATE
#undef OPUS_TYPE_DECODER_DESTROY
#undef OPUS_TYPE_DECODER_CTL
#undef OPUS_TYPE_DECODER_SIZE

#undef OPUS_TYPE_PACKET_PARSE
#undef OPUS_TYPE_PACKET_BANDWIDTH
#undef OPUS_TYPE_PACKET_SAMPLE
#undef OPUS_TYPE_PACKET_NBCHAN
#undef OPUS_TYPE_PACKET_NBFRM
#undef OPUS_TYPE_PACKET_NBSAMPLE

#undef OPUS_TYPE_REPACKETIZER_INIT
#undef OPUS_TYPE_REPACKETIZER_SIZE
#undef OPUS_TYPE_REPACKETIZER_CREATE
#undef OPUS_TYPE_REPACKETIZER_DESTROY
#undef OPUS_TYPE_REPACKETIZER_CAT
#undef OPUS_TYPE_REPACKETIZER_RANGE
#undef OPUS_TYPE_REPACKETIZER_NBFRM
#undef OPUS_TYPE_REPACKETIZER_OUT