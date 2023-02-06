/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */

#include "libav_deps.h"
#include "audiobuffer.h"
#include "logger.h"
#include <string.h>
#include <cstring> // memset
#include <algorithm>

namespace jami {

std::ostream&
operator<<(std::ostream& stream, const AudioFormat& f)
{
    stream << f.toString();
    return stream;
}

AudioBuffer::AudioBuffer(size_t sample_num, AudioFormat format)
    : sampleRate_(format.sample_rate)
    , samples_(std::max(1U, format.nb_channels), std::vector<AudioSample>(sample_num, 0))
{}

AudioBuffer::AudioBuffer(const AudioSample* in, size_t sample_num, AudioFormat format)
    : sampleRate_(format.sample_rate)
    , samples_((std::max(1U, format.nb_channels)), std::vector<AudioSample>(sample_num, 0))
{
    deinterleave(in, sample_num, format.nb_channels);
}

AudioBuffer::AudioBuffer(const AudioBuffer& other, bool copy_content /* = false */)
    : sampleRate_(other.sampleRate_)
    , samples_(copy_content
                   ? other.samples_
                   : std::vector<std::vector<AudioSample>>(other.samples_.size(),
                                                           std::vector<AudioSample>(other.frames())))
{}

AudioBuffer&
AudioBuffer::operator=(const AudioBuffer& other)
{
    samples_ = other.samples_;
    sampleRate_ = other.sampleRate_;
    return *this;
}

AudioBuffer&
AudioBuffer::operator=(AudioBuffer&& other)
{
    samples_ = std::move(other.samples_);
    sampleRate_ = other.sampleRate_;
    return *this;
}

int
AudioBuffer::getSampleRate() const
{
    return sampleRate_;
}

void
AudioBuffer::setSampleRate(int sr)
{
    sampleRate_ = sr;
}

void
AudioBuffer::setChannelNum(unsigned n, bool mix /* = false */)
{
    const unsigned c = samples_.size();
    if (n == c)
        return;

    n = std::max(1U, n);

    if (!mix or c == 0) {
        if (n < c)
            samples_.resize(n);
        else
            samples_.resize(n, std::vector<AudioSample>(frames(), 0));
        return;
    }

    // 2ch->1ch
    if (n == 1) {
        std::vector<AudioSample>& chan1 = samples_[0];
        std::vector<AudioSample>& chan2 = samples_[1];
        for (unsigned i = 0, f = frames(); i < f; i++)
            chan1[i] = chan1[i] / 2 + chan2[i] / 2;
        samples_.resize(1);
        return;
    }

    // 1ch->Nch
    if (c == 1) {
        samples_.resize(n, samples_[0]);
        return;
    }

    JAMI_WARN("Unsupported channel mixing: %dch->%dch", c, n);
    samples_.resize(n, samples_[0]);
}

void
AudioBuffer::setFormat(AudioFormat format)
{
    setChannelNum(format.nb_channels);
    setSampleRate(format.sample_rate);
}

void
AudioBuffer::resize(size_t sample_num)
{
    if (frames() == sample_num)
        return;

    // will add zero padding if buffer is growing
    for (auto& s : samples_)
        s.resize(sample_num, 0);
}

std::vector<AudioSample>*
AudioBuffer::getChannel(unsigned chan /* = 0 */)
{
    if (chan < samples_.size())
        return &samples_[chan];

    JAMI_ERR("Audio channel %u out of range", chan);
    return nullptr;
}

void
AudioBuffer::applyGain(double gain)
{
    if (gain == 1.0)
        return;

    const double g = std::max(std::min(1.0, gain), -1.0);
    if (g != gain)
        JAMI_DBG("Normalizing %f to [-1.0, 1.0]", gain);

    for (auto& channel : samples_)
        for (auto& sample : channel)
            sample *= g;
}

size_t
AudioBuffer::channelToFloat(float* out, const int& channel) const
{
    for (int i = 0, f = frames(); i < f; i++)
        *out++ = (float) samples_[channel][i] * .000030517578125f;

    return frames() * samples_.size();
}

size_t
AudioBuffer::interleave(AudioSample* out) const
{
    for (unsigned i = 0, f = frames(), c = channels(); i < f; ++i)
        for (unsigned j = 0; j < c; ++j)
            *out++ = samples_[j][i];

    return frames() * channels();
}

size_t
AudioBuffer::fillWithZero(AudioSample* out) const
{
    const auto n = channels() * frames();
    std::memset(out, 0, n * sizeof(*out));
    return n;
}

size_t
AudioBuffer::interleave(std::vector<AudioSample>& out) const
{
    out.resize(capacity());
    return interleave(out.data());
}

std::vector<AudioSample>
AudioBuffer::interleave() const
{
    std::vector<AudioSample> data(capacity());
    interleave(data.data());
    return data;
}

size_t
AudioBuffer::interleaveFloat(float* out) const
{
    for (unsigned i = 0, f = frames(), c = channels(); i < f; i++)
        for (unsigned j = 0; j < c; j++)
            *out++ = (float) samples_[j][i] * .000030517578125f;

    return frames() * samples_.size();
}

void
AudioBuffer::deinterleave(const AudioSample* in, size_t frame_num, unsigned nb_channels)
{
    if (in == nullptr)
        return;

    // Resize buffer
    setChannelNum(nb_channels);
    resize(frame_num);

    for (unsigned i = 0, f = frames(), c = channels(); i < f; i++)
        for (unsigned j = 0; j < c; j++)
            samples_[j][i] = *in++;
}

void
AudioBuffer::deinterleave(const std::vector<AudioSample>& in, AudioFormat format)
{
    sampleRate_ = format.sample_rate;
    deinterleave(in.data(), in.size() / format.nb_channels, format.nb_channels);
}

void
AudioBuffer::convertFloatPlanarToSigned16(uint8_t** extended_data,
                                          size_t frame_num,
                                          unsigned nb_channels)
{
    if (extended_data == nullptr)
        return;

    // Resize buffer
    setChannelNum(nb_channels);
    resize(frame_num);

    for (unsigned j = 0, c = channels(); j < c; j++) {
        float* inputChannel = (float*) extended_data[j];
        for (unsigned i = 0, f = frames(); i < f; i++) {
            float inputChannelVal = *inputChannel++;
            // avoid saturation: limit val between -1 and 1
            inputChannelVal = std::max(-1.0f, std::min(inputChannelVal, 1.0f));
            samples_[j][i] = (int16_t)(inputChannelVal * 32768.0f);
        }
    }
}

size_t
AudioBuffer::mix(const AudioBuffer& other, bool up /* = true */)
{
    const bool upmix = up && (other.samples_.size() < samples_.size());
    const size_t samp_num = std::min(frames(), other.frames());
    const unsigned chan_num = upmix ? samples_.size()
                                    : std::min(samples_.size(), other.samples_.size());

    for (unsigned i = 0; i < chan_num; i++) {
        unsigned src_chan = upmix ? std::min<unsigned>(i, other.samples_.size() - 1) : i;

        for (unsigned j = 0; j < samp_num; j++) {
            // clamp result to min/max
            // result must be larger than 16 bits to check for over/underflow
            int32_t n = static_cast<int32_t>(samples_[i][j])
                        + static_cast<int32_t>(other.samples_[src_chan][j]);
            if (n < std::numeric_limits<AudioSample>::min())
                n = std::numeric_limits<AudioSample>::min();
            else if (n > std::numeric_limits<AudioSample>::max())
                n = std::numeric_limits<AudioSample>::max();
            samples_[i][j] = n;
        }
    }

    return samp_num;
}

size_t
AudioBuffer::copy(AudioBuffer& in,
                  int sample_num /* = -1 */,
                  size_t pos_in /* = 0 */,
                  size_t pos_out /* = 0 */,
                  bool up /* = true */)
{
    if (sample_num == -1)
        sample_num = in.frames();

    int to_copy = std::min((int) in.frames() - (int) pos_in, sample_num);

    if (to_copy <= 0)
        return 0;

    const bool upmix = up && (in.samples_.size() < samples_.size());
    const size_t chan_num = upmix ? samples_.size() : std::min(in.samples_.size(), samples_.size());

    if ((pos_out + to_copy) > frames())
        resize(pos_out + to_copy);

    sampleRate_ = in.sampleRate_;

    for (unsigned i = 0; i < chan_num; i++) {
        size_t src_chan = upmix ? std::min<size_t>(i, in.samples_.size() - 1U) : i;
        std::copy(in.samples_[src_chan].begin() + pos_in,
                  in.samples_[src_chan].begin() + pos_in + to_copy,
                  samples_[i].begin() + pos_out);
    }

    return to_copy;
}

size_t
AudioBuffer::copy(AudioSample* in, size_t sample_num, size_t pos_out /* = 0 */)
{
    if (in == nullptr || sample_num == 0)
        return 0;

    if ((pos_out + sample_num) > frames())
        resize(pos_out + sample_num);

    const size_t chan_num = samples_.size();
    for (unsigned i = 0; i < chan_num; i++)
        std::copy(in, in + sample_num, samples_[i].begin() + pos_out);

    return sample_num;
}

std::unique_ptr<AudioFrame>
AudioBuffer::toAVFrame() const
{
    auto audioFrame = std::make_unique<AudioFrame>(getFormat(), frames());
    interleave(reinterpret_cast<AudioSample*>(audioFrame->pointer()->data[0]));
    return audioFrame;
}

int
AudioBuffer::append(const AudioFrame& audioFrame)
{
    auto frame = audioFrame.pointer();
    // FIXME we assume frame is s16 interleaved
    if (channels() != static_cast<unsigned>(frame->channels)
        || getSampleRate() != frame->sample_rate) {
        auto newFormat = AudioFormat {(unsigned) frame->sample_rate, (unsigned) frame->channels};
        setFormat(newFormat);
    }

    auto f = frames();
    auto newSize = f + frame->nb_samples;
    resize(newSize);

    auto in = reinterpret_cast<const AudioSample*>(frame->extended_data[0]);
    for (unsigned c = channels(); f < newSize; f++)
        for (unsigned j = 0; j < c; j++)
            samples_[j][f] = *in++;

    return 0;
}

} // namespace jami
