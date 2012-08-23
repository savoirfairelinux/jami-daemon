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
#ifndef OPUS_H_
#define OPUS_H_

#include "noncopyable.h"
#include "opus/opus.h"

#include "audiocodec.h"

#define MAX_ENCODER_BUFFER 480
#define OPUS_APPLICATION_VOIP 2048
#define OPUS_APPLICATION_AUDIO 2049
#define OPUS_APPLICATION_RESTRICTED_LOWDELAY 2051


//From the old opus implementation
static const int frameLength[]      = {5, 10, 20, 40, 60};
static const uint32 opusClockRate[] = {8000, 12000, 16000, 24000, 48000};
static const uint8 opusNbChannel[]  = {1, 2};
static const double opusBitRate     = 1111;
static const double opusBandWidt    = 1111;
static const int operationMode[]    = {OPUS_APPLICATION_VOIP, OPUS_APPLICATION_AUDIO, OPUS_APPLICATION_RESTRICTED_LOWDELAY};

class Opus : public sfl::AudioCodec {
public:
   Opus();
   ~Opus();
   virtual int decode(short *dst, unsigned char *buf, size_t buffer_size);
   virtual int encode(unsigned char *dst, short *src, size_t buffer_size);

   typedef struct {
      int32_t  nChannelsAPI;
      int32_t  nChannelsInternal;
      int32_t  API_sampleRate;
      int32_t  internalSampleRate;
      int      payloadSize_ms;
      int      prevPitchLag;
   } silk_DecControlStruct;

   struct silk_EncControlStruct {
      int32_t  nChannelsAPI;
      int32_t  nChannelsInternal;
      int32_t  API_sampleRate;
      int32_t  maxInternalSampleRate;
      int32_t  minInternalSampleRate;
      int32_t  desiredInternalSampleRate;
      int      payloadSize_ms;
      int32_t  bitRate;
      int      packetLossPercentage;
      int      complexity;
      int      useInBandFEC;
      int      useDTX;
      int      useCBR;
      int      maxBits;
      int      toMono;
      int      opusCanSwitch;
      int32_t  internalSampleRate;
      int      allowBandwidthSwitch;
      int      inWBmodeWithoutVariableLP;
      int      stereoWidth_Q14;
      int      switchReady;
   };
   typedef struct silk_EncControlStruct silk_EncControlStruct;

   struct OpusEncoder {
      int          celt_enc_offset;
      int          silk_enc_offset;
      silk_EncControlStruct silk_mode;
      int          application;
      int          channels;
      int          delay_compensation;
      int          force_channels;
      int          signal_type;
      int          user_bandwidth;
      int          max_bandwidth;
      int          user_forced_mode;
      int          voice_ratio;
      int32_t      Fs;
      int          use_vbr;
      int          vbr_constraint;
      int32_t      bitrate_bps;
      int32_t      user_bitrate_bps;
      int          encoder_buffer;

      #define OPUS_ENCODER_RESET_START stream_channels
      int          stream_channels;
      int16_t      hybrid_stereo_width_Q14;
      int32_t      variable_HP_smth2_Q15;
      int32_t      hp_mem[4];
      int          mode;
      int          prev_mode;
      int          prev_channels;
      int          prev_framesize;
      int          bandwidth;
      int          silk_bw_switch;
      /* Sampling rate (at the API level) */
      int          first;
      int16_t      delay_buffer[MAX_ENCODER_BUFFER*2];

      uint32_t  rangeFinal;
   };

   struct OpusDecoder {
      int          celt_dec_offset;
      int          silk_dec_offset;
      int          channels;
      int32_t      Fs;          /** Sampling rate (at the API level) */
      silk_DecControlStruct DecControl;
      int          decode_gain;

      /* Everything beyond this point gets cleared on a reset */
   #define OPUS_DECODER_RESET_START stream_channels
      int          stream_channels;
      int          bandwidth;
      int          mode;
      int          prev_mode;
      int          frame_size;
      int          prev_redundancy;
      int32_t      rangeFinal;
   };

private:
   NON_COPYABLE(Opus);
   //Attributes
   void * handler_;
   OpusEncoder *encoder_;
   OpusDecoder *decoder_;
   static const int FRAME_SIZE = 160;
   static const int CLOCK_RATE = 16000;
   static const int CHANNELS   = 1;

   //Helpers
   static void loadError(const char *error);

protected:

   //Extern functions
   static OpusEncoder*      (*opus_encoder_create              )(int32_t Fs, int channels, int application, int *error);
   static int32_t           (*opus_encode                      )(OpusEncoder *st, const int16_t *pcm, int frame_size, unsigned char *data, int32_t max_data_bytes);
   static void              (*opus_encoder_destroy             )(OpusEncoder *st);
   static OpusDecoder*      (*opus_decoder_create              )(int32_t Fs, int channels, int *error);
   static int               (*opus_decode                      )(OpusDecoder *st, const unsigned char *data, int32_t len, int16_t *pcm, int frame_size, int decode_fec);
   static void              (*opus_decoder_destroy             )(OpusDecoder *st);
};

#endif
