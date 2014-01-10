/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "opus_wrapper.h"
#include "sfl_types.h"
#include <stdexcept>
#include <iostream>


Opus::Opus() : sfl::AudioCodec(PAYLOAD_TYPE, "opus", CLOCK_RATE, FRAME_SIZE, CHANNELS),
    encoder_(0),
    decoder_(0)
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

Opus::~Opus()
{
    if (encoder_)
        opus_encoder_destroy(encoder_);

    if (decoder_)
        opus_decoder_destroy(decoder_);
}

sfl::AudioCodec *
Opus::clone()
{
    return new Opus;
}

// Reference: http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03#section-6.2
// "The RTP clock rate in "a=rtpmap" MUST be 48000..."
uint32_t Opus::getSDPClockRate() const
{
    return 48000;
}

// "...and the number of channels MUST be 2."
const char *
Opus::getSDPChannels() const
{
    return "2";
}

int Opus::decode(SFLAudioSample *dst, unsigned char *buf, size_t buffer_size)
{
    const int ret = opus_decode(decoder_, buf, buffer_size, dst, FRAME_SIZE, 0);
    if (ret < 0)
        std::cerr << opus_strerror(ret) << std::endl;
    return ret;
}

int Opus::encode(unsigned char *dst, SFLAudioSample *src, size_t buffer_size)
{
    const int ret = opus_encode(encoder_, src, FRAME_SIZE, dst, buffer_size * 2);
    if (ret < 0)
        std::cerr << opus_strerror(ret) << std::endl;
    return ret;
}

int Opus::decode(std::vector<std::vector<SFLAudioSample> > &dst, unsigned char *buf, size_t buffer_size)
{
    if (buf == NULL) return 0;

    const int ret = opus_decode(decoder_, buf, buffer_size, dst[0].data(), sizeof(SFLAudioSample) * FRAME_SIZE, 0);
    if (ret < 0)
        std::cerr << opus_strerror(ret) << std::endl;

    return ret;
}

int Opus::encode(unsigned char *dst, std::vector<std::vector<SFLAudioSample> > &src, size_t buffer_size)
{
    if (dst == NULL) return 0;

    const int ret = opus_encode(encoder_, src[0].data(), FRAME_SIZE, dst, buffer_size * sizeof(SFLAudioSample));
    if (ret < 0)
        std::cerr << opus_strerror(ret) << std::endl;

    return ret;
}


// cppcheck-suppress unusedFunction
extern "C" sfl::AudioCodec* AUDIO_CODEC_ENTRY()
{
    try {
        return new Opus;
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
