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

AudioBuffer::AudioBuffer(size_t sample_num /* = 0 */, size_t channel_num /* = 1 */, int sample_rate /* = 8000 */)
 :  sampleRate_(sample_rate),
    sampleNum_(sample_num),
    channels_(channel_num),
    samples_(channel_num, vector<SFLAudioSample>(sample_num, 0))
{
}

AudioBuffer(const AudioBuffer& other, bool copy_content /* = false */)
 :  sampleRate_(other.sampleRate_),
    sampleNum_(other.sampleNum_),
    channels_(other.channels_),
    samples_(channel_num, vector<SFLAudioSample>())
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

int AudioBuffer::getSampleRate()
{
    return sampleRate_;
}

void setSampleRate(int sr)
{
    sampleRate_ = sr;
}

size_t AudioBuffer::getChannelNum()
{
    return channels_;
}

void AudioBuffer::setChannelNum(size_t n, bool copy_content /* = false */)
{
    if(channels_ != n) {
        channels_ = n;
        samples_.resize(n, std::vector<SFLAudioSample>(sampleNum_, (copy_content && samples_.size()>0)?samples_[0]:0));
    }
}

size_t AudioBuffer::samples()
{
    return sampleNum_;
}

void AudioBuffer::resize(size_t sample_num)
{
    unsigned i;
    if(sampleNum_ != sample_num) {
        sampleNum_ = sample_num;
        for(i=0; i<channels_; i++)
            samples_[i].resize(sample_num);
    }
}

void AudioBuffer::clear()
{
    unsigned i, j;
    for(i=0; i<channels_; i++)
        samples_[i].assign(sampleNum_, 0);
}

void AudioBuffer::empty()
{
    unsigned i;
    for(i=0; i<channels_; i++)
        samples_[i].clear();
    sampleNum_ = 0;
}

std::vector<SFLAudioSample> *
AudioBuffer::getChannel(size_t chan /* = 0 */)
{
    return samples_[chan];
}

void AudioBuffer::applyGain(AudioBuffer *src, unsigned int gain)
{
    if(gain != 100)
        applyGain(gain*0.01);
}

void AudioBuffer::applyGain(AudioBuffer *src, double gain)
{
    if(gain == 1.0) return;
    unsigned i, j;
    for(i=0; i<channels_; i++)
        for(j=0; j<sampleNum_; j++)
            samples_[i][j] *= gain;
}

size_t AudioBuffer::interleave(SFLAudioSample* out)
{
    unsigned i, j;
    for(i=0; i<sampleNum_; i++)
        for(j=0; j<channels_; j++)
            *out++ = samples_[j][i];
    return sampleNum_*channels_;
}

size_t AudioBuffer::interleaveFloat(float* out)
{
    unsigned i, j;
    for(i=0; i<sampleNum_; i++)
        for(j=0; j<channels_; j++)
            *out++ = (float) samples_[j][i] * .000030517578125f;
    return sampleNum_*channels_;
}

void AudioBuffer::fromInterleaved(SFLAudioSample* in, size_t sample_num, size_t channel_num)
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
    for(i=0; i<chan_num; i++) {
        unsigned j;
        for(j=0; j<samp_num; j++)
            samples_[i][j] += other.samples_[i][j];
    }
    return samp_num;
}

size_t AudioBuffer::sub(AudioBuffer& out, size_t pos)
{
    out.copy(this, samples(), pos);
}

size_t AudioBuffer::copy(AudioBuffer& in, int sample_num /* = -1 */, size_t pos_in /* = 0 */, size_t pos_out /* = 0 */)
{
    if(sample_num == -1)
        sample_num = in.samples();

    int to_copy = std::min(in.samples()-(int)pos_in, (int)sample_num);
    if(to_copy <= 0) return 0;

    const size_t chan_num = in.channels_;

    if(pos_out+to_copy > sampleNum_)
        resize(pos_out+to_copy);

    sampleRate_ = in.sampleRate_;
    setChannelNum(chan_num);

    unsigned i;
    for(i=0; i<chan_num; i++) {
        copy(in.samples_[i].begin()+pos_in, in.samples_[i].begin()+pos_in+to_copy, samples_[i].begin()+pos_out);
    }

    return to_copy;
}

size_t AudioBuffer::copy(SFLAudioSample* in, size_t sample_num, size_t pos_out /* = 0 */)
{
    if(pos_out+sample_num > sampleNum_)
        resize(pos_out+to_copy);

    const size_t chan_num = channels_;
    unsigned i;
    for(i=0; i<chan_num; i++) {
        copy(in, in+to_copy, samples_[i].begin()+pos_out);
    }
}
