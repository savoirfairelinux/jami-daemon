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

AudioBuffer::AudioBuffer(size_t sample_num /* = 0 */, unsigned channel_num /* = 1 */, int sample_rate /* = 8000 */)
 :  sampleRate_(sample_rate),
    sampleNum_(sample_num),
    channels_(channel_num),
    samples_(channel_num, std::vector<SFLAudioSample>(sample_num, 0))
{
}

AudioBuffer::AudioBuffer(const AudioBuffer& other, bool copy_content /* = false */)
 :  sampleRate_(other.sampleRate_),
    sampleNum_(other.sampleNum_),
    channels_(other.channels_),
    samples_(channels_, std::vector<SFLAudioSample>())
{
    unsigned i;
    if(copy_content) {
        for(i=0; i<channels_; i++)
            samples_[i] = other.samples_[i];
    } else {
        for(i=0; i<channels_; i++)
            samples_[i].resize(sampleNum_, 0);
    }
}

int AudioBuffer::getSampleRate() const
{
    return sampleRate_;
}

void AudioBuffer::setSampleRate(int sr)
{
    sampleRate_ = sr;
}

unsigned AudioBuffer::getChannelNum() const
{
    return channels_;
}

void AudioBuffer::setChannelNum(unsigned n, bool copy_content /* = false */)
{
    if(channels_ != n) {
        channels_ = n;
        samples_.resize(n, (copy_content && samples_.size()>0)?samples_[0]:std::vector<SFLAudioSample>(sampleNum_, 0));
    }
}

size_t AudioBuffer::samples() const
{
    return sampleNum_;
}

void AudioBuffer::resize(size_t sample_num)
{
    if(sampleNum_ != sample_num) {
        sampleNum_ = sample_num;
        for(unsigned i=0; i<channels_; i++)
            samples_[i].resize(sample_num);
    }
}

void AudioBuffer::clear()
{
    unsigned i, j;
    for(i=0; i<channels_; i++)
        samples_[i].assign(sampleNum_, 0);
}

void AudioBuffer::empty()
{
    for(unsigned i=0; i<channels_; i++)
        samples_[i].clear();
    sampleNum_ = 0;
}

std::vector<SFLAudioSample> *
AudioBuffer::getChannel(unsigned chan /* = 0 */)
{
    return &samples_[chan];
}

void AudioBuffer::applyGain(unsigned int gain)
{
    if(gain != 100)
        applyGain(gain*0.01);
}

void AudioBuffer::applyGain(double gain)
{
    if(gain == 1.0) return;
    unsigned i, j;
    for(i=0; i<channels_; i++)
        for(j=0; j<sampleNum_; j++)
            samples_[i][j] *= gain;
}

size_t AudioBuffer::interleave(SFLAudioSample* out) const
{
    unsigned i, j;
    for(i=0; i<sampleNum_; i++)
        for(j=0; j<channels_; j++)
            *out++ = samples_[j][i];
    return sampleNum_*channels_;
}

size_t AudioBuffer::interleaveFloat(float* out) const
{
    unsigned i, j;
    for(i=0; i<sampleNum_; i++)
        for(j=0; j<channels_; j++)
            *out++ = (float) samples_[j][i] * .000030517578125f;
    return sampleNum_*channels_;
}

void AudioBuffer::fromInterleaved(const SFLAudioSample* in, size_t sample_num, unsigned channel_num)
{
    unsigned i;

    // Resize buffer
    setChannelNum(channel_num);
    resize(sample_num);

    for(i=0; i<sampleNum_; i++) {
        unsigned j;
        for(j=0; j<channels_; j++)
            samples_[j][i] = *in++;
    }
}

size_t AudioBuffer::mix(const AudioBuffer& other)
{
    const size_t samp_num = std::min(sampleNum_, other.sampleNum_);
    const size_t chan_num = std::min(channels_, other.channels_);
    unsigned i;
    for(i=0; i<chan_num; i++) {
        unsigned j;
        for(j=0; j<samp_num; j++)
            samples_[i][j] += other.samples_[i][j];
    }
    return samp_num;
}

size_t AudioBuffer::copy(AudioBuffer& in, int sample_num /* = -1 */, size_t pos_in /* = 0 */, size_t pos_out /* = 0 */)
{
    if(sample_num == -1)
        sample_num = in.samples();

    int to_copy = std::min((int)in.samples()-(int)pos_in, sample_num);
    if(to_copy <= 0) return 0;

    const size_t chan_num = std::min(in.channels_, channels_);

    if(pos_out+to_copy > sampleNum_)
        resize(pos_out+to_copy);

    sampleRate_ = in.sampleRate_;
    //setChannelNum(chan_num);

    unsigned i;
    for(i=0; i<chan_num; i++) {
        std::copy(in.samples_[i].begin()+pos_in, in.samples_[i].begin()+pos_in+to_copy, samples_[i].begin()+pos_out);
    }

    return to_copy;
}

size_t AudioBuffer::copy(SFLAudioSample* in, size_t sample_num, size_t pos_out /* = 0 */)
{
    if(in == NULL) return 0;
    if(pos_out+sample_num > sampleNum_)
        resize(pos_out+sample_num);

    const size_t chan_num = channels_;
    unsigned i;
    for(i=0; i<chan_num; i++) {
        std::copy(in, in+sample_num, samples_[i].begin()+pos_out);
    }
    return sample_num;
}
