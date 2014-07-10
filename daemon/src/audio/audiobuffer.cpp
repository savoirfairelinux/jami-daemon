/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "audiobuffer.h"
#include "logger.h"


const AudioFormat AudioFormat::MONO = AudioFormat(AudioFormat::DEFAULT_SAMPLE_RATE, 1);
const AudioFormat AudioFormat::STEREO = AudioFormat(AudioFormat::DEFAULT_SAMPLE_RATE, 2);

std::ostream& operator <<(std::ostream& stream, const AudioFormat& f) {
    stream << f.toString();
    return stream;
}

AudioBuffer::AudioBuffer(size_t sample_num, AudioFormat format)
    :  sampleRate_(format.sample_rate),
       samples_(std::max(1U, format.nb_channels),
                std::vector<SFLAudioSample>(sample_num, 0))
{
}

AudioBuffer::AudioBuffer(const SFLAudioSample* in, size_t sample_num, AudioFormat format)
    :  sampleRate_(format.sample_rate),
       samples_((std::max(1U, format.nb_channels)), std::vector<SFLAudioSample>(sample_num, 0))
{
    deinterleave(in, sample_num, format.nb_channels);
}

AudioBuffer::AudioBuffer(const AudioBuffer& other, bool copy_content /* = false */)
    :  sampleRate_(other.sampleRate_),
       samples_(copy_content ? other.samples_ :
                std::vector<std::vector<SFLAudioSample> >(other.samples_.size(), std::vector<SFLAudioSample>(other.frames())))
{}

AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) {
    samples_ = other.samples_;
    sampleRate_ = other.sampleRate_;
    return *this;
}
        
AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) {
    samples_ = std::move( other.samples_ );
    sampleRate_ = other.sampleRate_;
    return *this;
}

int AudioBuffer::getSampleRate() const
{
    return sampleRate_;
}

void AudioBuffer::setSampleRate(int sr)
{
    sampleRate_ = sr;
}

void AudioBuffer::setChannelNum(unsigned n, bool mix /* = false */)
{
    const unsigned c = samples_.size();
    if (n == c)
        return;

    n = std::max(1U, n);

    if (!mix or c == 0) {
        if (n < c)
            samples_.resize(n);
        else
            samples_.resize(n, std::vector<SFLAudioSample>(frames(), 0));
        return;
    }

    // 2ch->1ch
    if (n == 1) {
        std::vector<SFLAudioSample>& chan1 = samples_[0];
        std::vector<SFLAudioSample>& chan2 = samples_[1];
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

    WARN("Unsupported channel mixing: %dch->%dch", c, n);
    samples_.resize(n, samples_[0]);
}

void AudioBuffer::setFormat(AudioFormat format)
{
    setChannelNum(format.nb_channels);
    setSampleRate(format.sample_rate);
}

void AudioBuffer::resize(size_t sample_num)
{
    if (frames() == sample_num)
        return;

    // will add zero padding if buffer is growing
    for (auto &s : samples_)
        s.resize(sample_num, 0);
}

std::vector<SFLAudioSample> * AudioBuffer::getChannel(unsigned chan /* = 0 */)
{
    if (chan < samples_.size())
        return &samples_[chan];

    ERROR("Audio channel %u out of range", chan);
    return nullptr;
}

void AudioBuffer::applyGain(double gain)
{
    if (gain == 1.0) return;

    const double g = std::max(std::min(1.0, gain), -1.0);
    if (g != gain)
        DEBUG("Normalizing %f to [-1.0, 1.0]", gain);

    for (auto &channel : samples_)
        for (auto &sample : channel)
            sample *= g;
}

size_t AudioBuffer::interleave(SFLAudioSample* out) const
{
    for (unsigned i=0, f=frames(), c=channels(); i < f; ++i)
        for (unsigned j = 0; j < c; ++j)
            *out++ = samples_[j][i];

    return frames() * channels();
}

size_t AudioBuffer::interleave(std::vector<SFLAudioSample>& out) const
{
    out.resize(capacity());
    return interleave(out.data());
}

std::vector<SFLAudioSample> AudioBuffer::interleave() const
{
    std::vector<SFLAudioSample> data(capacity());
    interleave(data.data());
    return data;
}

size_t AudioBuffer::interleaveFloat(float* out) const
{
    for (unsigned i=0, f=frames(), c=channels(); i < f; i++)
        for (unsigned j = 0; j < c; j++)
            *out++ = (float) samples_[j][i] * .000030517578125f;

    return frames() * samples_.size();
}

void AudioBuffer::deinterleave(const SFLAudioSample* in, size_t frame_num, unsigned nb_channels)
{
    if (in == nullptr)
        return;

    // Resize buffer
    setChannelNum(nb_channels);
    resize(frame_num);

    for (unsigned i=0, f=frames(), c=channels(); i < f; i++)
        for (unsigned j = 0; j < c; j++)
            samples_[j][i] = *in++;
}

void AudioBuffer::deinterleave(const std::vector<SFLAudioSample>& in, AudioFormat format)
{
    sampleRate_ = format.sample_rate;
    deinterleave(in.data(), in.size()/format.nb_channels, format.nb_channels);
}

size_t AudioBuffer::mix(const AudioBuffer& other, bool up /* = true */)
{
    const bool upmix = up && (other.samples_.size() < samples_.size());
    const size_t samp_num = std::min(frames(), other.frames());
    const unsigned chan_num = upmix ? samples_.size() : std::min(samples_.size(), other.samples_.size());

    for (unsigned i = 0; i < chan_num; i++) {
        unsigned src_chan = upmix ? std::min<unsigned>(i, other.samples_.size() - 1) : i;

        for (unsigned j = 0; j < samp_num; j++)
            samples_[i][j] += other.samples_[src_chan][j];
    }

    return samp_num;
}

size_t AudioBuffer::copy(AudioBuffer& in, int sample_num /* = -1 */, size_t pos_in /* = 0 */, size_t pos_out /* = 0 */, bool up /* = true */)
{
    if (sample_num == -1)
        sample_num = in.frames();

    int to_copy = std::min((int)in.frames() - (int)pos_in, sample_num);

    if (to_copy <= 0) return 0;

    const bool upmix = up && (in.samples_.size() < samples_.size());
    const size_t chan_num = upmix ? samples_.size() : std::min(in.samples_.size(), samples_.size());

    if ((pos_out + to_copy) > frames())
        resize(pos_out + to_copy);

    sampleRate_ = in.sampleRate_;

    for (unsigned i = 0; i < chan_num; i++) {
        size_t src_chan = upmix ? std::min<size_t>(i, in.samples_.size() - 1U) : i;
        std::copy(in.samples_[src_chan].begin() + pos_in, in.samples_[src_chan].begin() + pos_in + to_copy, samples_[i].begin() + pos_out);
    }

    return to_copy;
}

size_t AudioBuffer::copy(SFLAudioSample* in, size_t sample_num, size_t pos_out /* = 0 */)
{
    if (in == nullptr || sample_num == 0) return 0;

    if ((pos_out + sample_num) > frames())
        resize(pos_out + sample_num);

    const size_t chan_num = samples_.size();
    for (unsigned i = 0; i < chan_num; i++)
        std::copy(in, in + sample_num, samples_[i].begin() + pos_out);

    return sample_num;
}
