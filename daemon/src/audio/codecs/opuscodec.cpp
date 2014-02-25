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
#include "opuscodec.h"
#include "sfl_types.h"
#include <stdexcept>
#include <iostream>
#include <array>

constexpr uint32_t Opus::VALID_SAMPLING_RATE[];

Opus::Opus() : sfl::AudioCodec(PAYLOAD_TYPE, "opus", CLOCK_RATE, FRAME_SIZE, CHANNELS),
    encoder_(nullptr),
    decoder_(nullptr)
{
    hasDynamicPayload_ = true;
    setOptimalFormat(CLOCK_RATE, 1);
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

void Opus::setOptimalFormat(uint32_t sample_rate, uint8_t channels)
{
    // Use a SR higher or equal to sample_rate.
    // Typical case: 44.1kHz => 48kHz.
    unsigned i = 0;
    while (i < VALID_SAMPLING_RATE_NUM - 1 and VALID_SAMPLING_RATE[i] < sample_rate)
        i++;
    sample_rate = VALID_SAMPLING_RATE[i];

    // Opus supports 1 or 2 channels.
    channels = std::max(std::min(channels, (uint8_t) 2), (uint8_t) 1);

    if (not (!encoder_ || !decoder_ || sample_rate != clockRateCur_ || channels != channelsCur_))
        return;

    clockRateCur_ = sample_rate;
    channelsCur_ = channels;

    std::cerr << "Opus switch mode: " << sample_rate << "kHz, " << (int) channels << " channels." << std::endl;

    int err;
    if (encoder_)
        opus_encoder_destroy(encoder_);
    encoder_ = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
    if (err)
        throw std::runtime_error("opus: could not create encoder");

    if (decoder_)
        opus_decoder_destroy(decoder_);
    decoder_ = opus_decoder_create(sample_rate, channels, &err);
    if (err)
        throw std::runtime_error("opus: could not create decoder");
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

int Opus::decode(std::vector<std::vector<SFLAudioSample> > &dst, uint8_t *buf, size_t buf_size)
{
    if (buf == nullptr) return 0;

    int ret;
    if(channelsCur_ == 1) {
        ret = opus_decode(decoder_, buf, buf_size, dst[0].data(), MAX_PACKET_SIZE, 0);
    } else {
        std::array<SFLAudioSample, 2*MAX_PACKET_SIZE> ibuf; // deinterleave on stack, 11.25KiB used.
        ret = opus_decode(decoder_, buf, buf_size, ibuf.data(), MAX_PACKET_SIZE, 0);
        for(int i=0; i<ret; i++) {
            dst[0][i] = ibuf[2*i];
            dst[1][i] = ibuf[2*i+1];
        }
    }
    if (ret < 0)
        std::cerr << opus_strerror(ret) << std::endl;
    return ret;
}

size_t Opus::encode(std::vector<std::vector<SFLAudioSample> > &src, uint8_t *dst, size_t dst_size)
{
    if (dst == nullptr) return 0;

    int ret;
    if(channelsCur_ == 1) {
        ret = opus_encode(encoder_, src[0].data(), FRAME_SIZE, dst, dst_size);
    } else {
        std::array<SFLAudioSample, 2 * FRAME_SIZE> ibuf; // interleave on stack, 1.875KiB used;
        for (unsigned i = 0; i < FRAME_SIZE; i++) {
            ibuf[2 * i] = src[0][i];
            ibuf[2 * i + 1] = src[1][i];
        }
        ret = opus_encode(encoder_, ibuf.data(), FRAME_SIZE, dst, dst_size);
    }
    if (ret < 0) {
        std::cerr << opus_strerror(ret) << std::endl;
        ret = 0;
    }
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
