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

#pragma once

#if __cplusplus >= 201103L
#undef isblank
#endif

extern "C" {
#include <libavutil/samplefmt.h>
struct AVFrame;
}

#include "ring_types.h"
#include "media_buffer.h"

#include <fmt/core.h>

#include <ciso646> // fix windows compiler bug
#include <sstream>
#include <vector>
#include <string>
#include <cstddef> // for size_t

namespace jami {

/**
 * Structure to hold sample rate and channel number associated with audio data.
 */
struct AudioFormat
{
    unsigned sample_rate;
    unsigned nb_channels;
    AVSampleFormat sampleFormat;

    constexpr AudioFormat(unsigned sr, unsigned c, AVSampleFormat f = AV_SAMPLE_FMT_S16)
        : sample_rate(sr)
        , nb_channels(c)
        , sampleFormat(f)
    {}

    inline bool operator==(const AudioFormat& b) const
    {
        return ((b.sample_rate == sample_rate) && (b.nb_channels == nb_channels)
                && (b.sampleFormat == sampleFormat));
    }

    inline bool operator!=(const AudioFormat& b) const { return !(*this == b); }

    inline std::string toString() const
    {
        return fmt::format("{{{}, {} channels, {}Hz}}", av_get_sample_fmt_name(sampleFormat), nb_channels, sample_rate);
    }

    /**
     * Returns bytes necessary to hold one frame of audio data.
     */
    inline size_t getBytesPerFrame() const { return sizeof(AudioSample) * nb_channels; }

    /**
     * Bytes per second (default), or bytes necessary
     * to hold delay_ms milliseconds of audio data.
     */
    inline size_t getBandwidth(unsigned delay_ms = 1000) const
    {
        return (getBytesPerFrame() * sample_rate * delay_ms) / 1000;
    }

    static const constexpr unsigned DEFAULT_SAMPLE_RATE = 48000;
    static const constexpr AudioFormat DEFAULT() { return AudioFormat {16000, 1}; }
    static const constexpr AudioFormat NONE() { return AudioFormat {0, 0}; }
    static const constexpr AudioFormat MONO() { return AudioFormat {DEFAULT_SAMPLE_RATE, 1}; }
    static const constexpr AudioFormat STEREO() { return AudioFormat {DEFAULT_SAMPLE_RATE, 2}; }
};

std::ostream& operator<<(std::ostream& stream, const AudioFormat& f);

class AudioBuffer
{
public:
    /**
     * Default constructor
     */
    AudioBuffer()
        : AudioBuffer {0, AudioFormat::NONE()}
    {}

    /**
     * Construct from given sample number and audio format
     */
    AudioBuffer(size_t sample_num, AudioFormat format);

    /**
     * Construct from existing interleaved data (copied into the buffer).
     */
    AudioBuffer(const AudioSample* in, size_t sample_num, AudioFormat format);

    /**
     * Copy constructor that by default only copies the buffer parameters (channel number, sample rate
     * and buffer size). If copy_content is set to true, the other buffer content is also copied.
     */
    AudioBuffer(const AudioBuffer& other, bool copy_content = false);

    /**
     * Move constructor
     */
    AudioBuffer(AudioBuffer&& other)
        : sampleRate_(other.sampleRate_)
        , samples_(std::move(other.samples_)) {};

    /**
     * Copy operator
     */
    AudioBuffer& operator=(const AudioBuffer& other);

    /**
     * Move operator
     */
    AudioBuffer& operator=(AudioBuffer&& other);

    /**
     * Buffer description for debugging
     */
    inline std::string toString() const
    {
        return fmt::format("[AudioBuffer {} frames; {}]", frames(), getFormat().toString());
    }

    inline size_t size() const { return frames() * channels() * sizeof(AudioSample); }

    /**
     * Returns the sample rate (in samples/sec) associated to this buffer.
     */
    int getSampleRate() const;

    /**
     * Set the sample rate (in samples/sec) associated to this buffer.
     */
    void setSampleRate(int sr);

    /**
     * Returns the number of channels in this buffer.
     */
    inline unsigned channels() const { return samples_.size(); }

    /**
     * Set the number of channels of this buffer, eventually with up or downmixing.
     *
     * Currently supported remixing: 1ch->Nch, 2ch->1ch.
     *
     * @param n: the new number of channels.
     *
     * @param mix: if false, no buffer data is changed, removed channels
     *              are lost and new channels are initialised to 0.
     *              If true, up or downmixing happens.
     */
    void setChannelNum(unsigned n, bool mix = false);

    /**
     * Set the buffer format (channels and sample rate).
     * No data conversion is performed.
     */
    void setFormat(AudioFormat format);

    inline AudioFormat getFormat() const { return AudioFormat(sampleRate_, samples_.size()); }

    /**
     * Returns the number of (multichannel) frames in this buffer.
     */
    inline size_t frames() const
    {
        if (not samples_.empty())
            return samples_[0].size();
        else
            return 0;
    }

    /**
     * Return the total number of single samples in the buffer
     */
    inline size_t capacity() const { return frames() * channels(); }

    /**
     * Resize the buffer to make it able to hold sample_num
     * multichannel samples.
     * If the requested size is larger than the current buffer size,
     * the new samples are set to zero.
     */
    void resize(size_t sample_num);

    /**
     * Resize the buffer to 0. All samples are lost but the number of channels and sample rate are kept.
     */
    void clear()
    {
        for (auto& c : samples_)
            c.clear();
    }

    /**
     * Set all samples in this buffer to 0. Buffer size is not changed.
     */
    void reset()
    {
        for (auto& c : samples_)
            std::fill(c.begin(), c.end(), 0);
    }

    /**
     * Return the data (audio samples) for a given channel number.
     * Channel data can be modified but size of individual channel vectors should not be changed by
     * the user.
     */
    std::vector<AudioSample>* getChannel(unsigned chan);

    /**
     * Return a pointer to the raw data in this buffer.
     */
    inline std::vector<std::vector<AudioSample>>& getData() { return samples_; }

    /**
     * Returns pointers to non-interleaved raw data.
     * Caller should not store result because pointer validity is
     * limited in time.
     */
    inline const std::vector<AudioSample*> getDataRaw()
    {
        const unsigned chans = samples_.size();
        std::vector<AudioSample*> raw_data(chans, nullptr);
        for (unsigned i = 0; i < chans; i++)
            raw_data[i] = samples_[i].data();
        return raw_data;
    }

    /**
     * Convert fixed-point channel to float and write in the out buffer (Float 32-bits).
     * The out buffer must be at least of size capacity()*sizeof(float) bytes.
     *
     * @returns Number of samples written.
     */
    size_t channelToFloat(float* out, const int& channel) const;

    /**
     * Write interleaved multichannel data to the out buffer (fixed-point 16-bits).
     * The out buffer must be at least of size capacity()*sizeof(AudioSample) bytes.
     *
     * @returns Number of samples written.
     */
    size_t interleave(AudioSample* out) const;

    /**
     * Write null data (silence) to the out buffer (fixed-point 16-bits).
     * The out buffer must be at least of size capacity()*sizeof(AudioSample) bytes.
     *
     * @returns Number of samples written.
     */
    size_t fillWithZero(AudioSample* out) const;

    /**
     * Write interleaved multichannel data to the out buffer (fixed-point 16-bits).
     * The out buffer is resized to hold the full content of this buffer.
     *
     * @returns Number of samples written.
     */
    size_t interleave(std::vector<AudioSample>& out) const;

    /**
     * Returns vector of interleaved data (fixed-point 16-bits).
     */
    std::vector<AudioSample> interleave() const;

    /**
     * Write interleaved multichannel data to the out buffer, while samples are converted to float.
     * The out buffer must be at least of size capacity()*sizeof(float) bytes.
     *
     * @returns Number of samples written.
     */
    size_t interleaveFloat(float* out) const;

    /**
     * Import interleaved multichannel data. Internal buffer is resized as needed.
     * Function will read sample_num*channel_num elements of the in buffer.
     */
    void deinterleave(const AudioSample* in, size_t frame_num, unsigned nb_channels = 1);

    /**
     * Import interleaved multichannel data. Internal buffer is resized as needed.
     * Sample rate is set according to format.
     */
    void deinterleave(const std::vector<AudioSample>& in, AudioFormat format);

    /**
     * convert float planar data to signed 16
     */
    void convertFloatPlanarToSigned16(uint8_t** extended_data,
                                      size_t frame_num,
                                      unsigned nb_channels = 1);

    /**
     * In-place gain transformation.
     *
     * @param gain: 0.0 -> 1.0 scale
     */
    void applyGain(double gain);

    /**
     * Mix samples from the other buffer within this buffer (in-place simple addition).
     * If the other buffer has more channels than this one, only the first this.channels() channels
     * are imported. If the other buffer has less channels than this one, behavior depends on upmix.
     * Sample rate is not considered by this function.
     *
     * TODO: some kind of check for overflow/saturation.
     *
     * @param other: the other buffer to mix in this one.
     * @param upmix: if true, upmixing occurs when other.channels() < this.channels().
     *              If false, only the first other.channels() channels are edited in this buffer.
     *
     * @returns Number of samples modified.
     */
    size_t mix(const AudioBuffer& other, bool upmix = true);

    /**
     * Copy sample_num samples from in (from sample sample pos_in) to this buffer (at sample sample
     * pos_out). If sample_num is -1 (the default), the entire in buffer is copied.
     *
     * Buffer sample number is increased if required to hold the new requested samples.
     */
    size_t copy(AudioBuffer& in,
                int sample_num = -1,
                size_t pos_in = 0,
                size_t pos_out = 0,
                bool upmix = true);

    /**
     * Copy sample_num samples from in to this buffer (at sample pos_out).
     * Input data is treated as mono and samples are duplicated in the case of a multichannel buffer.
     *
     * Buffer sample number is increased if required to hold the new requested samples.
     */
    size_t copy(AudioSample* in, size_t sample_num, size_t pos_out = 0);

    std::unique_ptr<AudioFrame> toAVFrame() const;

    int append(const AudioFrame& frame);

private:
    int sampleRate_;

    // buffers holding data for each channels
    std::vector<std::vector<AudioSample>> samples_;
};

} // namespace jami
