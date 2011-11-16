/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "global.h"
#include "audiocodec.h"
#include "noncopyable.h"
#include <cstdio>
#include <speex/speex.h>
#include <cassert>

static const unsigned int clockRate [3] = {
    8000,
    16000,
    32000
};
static const unsigned int frameSize[3] = {
    160,
    320,
    640,
};
static const unsigned int bitRate[3] = { // FIXME : not using VBR?
    24,
    42,
    0,
};
static const bool dynamicPayload[3] = {
    true,
    false,
    true,
};

const SpeexMode* speexMode[3] = {
    &speex_nb_mode,
    &speex_wb_mode,
    &speex_uwb_mode, // wb
};

class Speex : public sfl::AudioCodec {
    public:
        Speex(int payload) :
            sfl::AudioCodec(payload, "speex"), speex_dec_bits_(),
            speex_enc_bits_(), speex_dec_state_(0), speex_enc_state_(0),
            speex_frame_size_(0) {
                assert(payload >= 110 && payload <= 112);
                assert(110 == PAYLOAD_CODEC_SPEEX_8000 &&
                        111 == PAYLOAD_CODEC_SPEEX_16000 &&
                        112 == PAYLOAD_CODEC_SPEEX_32000);
                int type = payload - 110;

                clockRate_ = clockRate[type];
                frameSize_ = frameSize[type];
                channel_ = 1;
                bitrate_ = bitRate[type];
                hasDynamicPayload_ = dynamicPayload[type];

                // Init the decoder struct
                speex_bits_init(&speex_dec_bits_);
                speex_dec_state_ = speex_decoder_init(speexMode[type]);

                // Init the encoder struct
                speex_bits_init(&speex_enc_bits_);
                speex_enc_state_ = speex_encoder_init(speexMode[type]);

                speex_encoder_ctl(speex_enc_state_, SPEEX_SET_SAMPLING_RATE, &clockRate_);
                speex_decoder_ctl(speex_dec_state_, SPEEX_GET_FRAME_SIZE, &speex_frame_size_);
        }

        NON_COPYABLE(Speex);

        ~Speex() {
            // Destroy the decoder struct
            speex_bits_destroy(&speex_dec_bits_);
            speex_decoder_destroy(speex_dec_state_);
            speex_dec_state_ = 0;

            // Destroy the encoder struct
            speex_bits_destroy(&speex_enc_bits_);
            speex_encoder_destroy(speex_enc_state_);
            speex_enc_state_ = 0;
        }

        virtual int decode(short *dst, unsigned char *src, size_t buf_size) {
            speex_bits_read_from(&speex_dec_bits_, (char*) src, buf_size);
            speex_decode_int(speex_dec_state_, &speex_dec_bits_, dst);
            return frameSize_;
        }

        virtual int encode(unsigned char *dst, short *src, size_t buf_size) {
            speex_bits_reset(&speex_enc_bits_);
            speex_encode_int(speex_enc_state_, src, &speex_enc_bits_);
            return speex_bits_write(&speex_enc_bits_, (char*) dst, buf_size);
        }

    private:
        SpeexBits speex_dec_bits_;
        SpeexBits speex_enc_bits_;
        void *speex_dec_state_;
        void *speex_enc_state_;
        int speex_frame_size_;
};
