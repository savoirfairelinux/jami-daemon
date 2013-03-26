/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author:  Adrien Beraud <adrienberaud@gmail.com>
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
#include "opus_stereo.h"
#include <stdexcept>
#include <iostream>

static const int OpusStereo_PAYLOAD_TYPE = 105; // dynamic payload type, out of range of video (96-99)

OpusStereo::OpusStereo() : sfl::AudioCodec(OpusStereo_PAYLOAD_TYPE, "Opus", CLOCK_RATE, FRAME_SIZE, CHANNELS),
    encoder_(0),
    decoder_(0),
    interleaved_(FRAME_SIZE*CHANNELS)
{
   hasDynamicPayload_ = true;

   int err = 0;
   encoder_ = opus_encoder_create(CLOCK_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
   if (err)
       throw std::runtime_error("opus: could not create encoder");

   decoder_ = opus_decoder_create(CLOCK_RATE, CHANNELS, &err);
   if (err)
       throw std::runtime_error("opus: could not create decoder");
}

OpusStereo::~OpusStereo()
{
    if (encoder_)
        opus_encoder_destroy(encoder_);
    if (decoder_)
        opus_decoder_destroy(decoder_);
}

int OpusStereo::decode(short *dst, unsigned char *buf, size_t buffer_size)
{
   return 0;
}

int OpusStereo::encode(unsigned char *dst, short *src, size_t buffer_size)
{
   return 0;
}

int OpusStereo::decode(std::vector<std::vector<short> > *dst, unsigned char *buf, size_t buffer_size, size_t dst_offset /* = 0 */)
{
    if(dst == NULL || buf == NULL || dst->size()<2) return 0;
    interleaved_.resize(4*FRAME_SIZE);
    //return decode(&(*((*dst)[0].begin()+dst_offset)), buf, buffer_size);
    unsigned samples = opus_decode(decoder_, buf, buffer_size, interleaved_.data(), 2*FRAME_SIZE, 0);

    std::vector<opus_int16>::iterator left_it = dst->at(0).begin()+dst_offset;
    std::vector<opus_int16>::iterator right_it = dst->at(1).begin()+dst_offset;
    std::vector<opus_int16>::iterator it = interleaved_.begin();

    // hard-coded 2-channels as it is the stereo version
    for(unsigned i=0; i<samples; i++) {
        *left_it++ = *it++;
        *right_it++ = *it++;
    }

    return samples;
}

int OpusStereo::encode(unsigned char *dst, std::vector<std::vector<short> > *src, size_t buffer_size)
{
    if(dst == NULL || src == NULL || src->size()<2) return 0;
    const unsigned samples = src->at(0).size();
    interleaved_.resize(2*samples);
    std::vector<opus_int16>::iterator it = interleaved_.begin();

    // hard-coded 2-channels as it is the stereo version
    for(unsigned i=0; i<samples; i++) {
        *it++ = src->at(0)[i];
        *it++ = src->at(1)[i];
    }

    return opus_encode(encoder_, interleaved_.data(), FRAME_SIZE, dst, buffer_size * 2);
}


// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    try {
        return new OpusStereo;
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return 0;
    }
}

// cppcheck-suppress unusedFunction
extern "C" void destroy(sfl::AudioCodec* a)
{
    delete a;
}
