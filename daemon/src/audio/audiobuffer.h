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

#ifndef _AUDIO_BUFFER_H
#define _AUDIO_BUFFER_H

#include <iostream>
#include <vector>
#include <cstddef> // for size_t

#include "sfl_types.h"

class AudioBuffer {
    public:
        /**
         * Default constructor.
         */
        AudioBuffer(size_t sample_num, unsigned channel_num = 1, int sample_rate = 8000);

        /**
         * Construtor from existing interleaved data (copied into the buffer).
         */
        AudioBuffer(const SFLAudioSample* in, size_t sample_num, unsigned channel_num = 1, int sample_rate = 8000);

        /**
         * Copy constructor that by default only copies the buffer parameters (channel number, sample rate and buffer size).
         * If copy_content is set to true, the other buffer content is also copied.
         */
        AudioBuffer(const AudioBuffer& other, bool copy_content = false);

        void reset() {
            for (std::vector<std::vector<SFLAudioSample> >::iterator i = samples_.begin(); i != samples_.end(); ++i)
                std::fill(i->begin(), i->end(), 0);
        }

		inline size_t size() {
			return samples_[0].size() * channels() * sizeof(SFLAudioSample);
		}

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
        inline unsigned channels() const {
            return samples_.size();
        }

        /**
         * Set the number of channels of this buffer.
         *
         * @param n: the new number of channels. If n < channels(), channels are removed from the buffer, otherwise the behavior depends on copy_first.
         *
         * @param copy_first: if set to true and n > channels(), new channels are initialised as a copy of the first channel (channel 0). If set to false, new channels are initialised to 0.
         */
        void setChannelNum(unsigned n, bool copy_first = false);

        /**
         * Returns the number of (multichannel) samples in this buffer.
         */
        inline size_t samples() const {
			return samples_[0].size();
		}

        /**
         * Return the total number of single samples in the buffer (same as samples()*channels()).
         */
        inline size_t capacity() const {
            return samples() * channels();
        }

        /**
         * Resize the buffer to make it able to hold sample_num multichannel samples.
         */
        void resize(size_t sample_num);

        /**
         * Set all samples in this buffer to 0. Buffer size is not changed.
         */
        void clear();

        /**
         * Resize the buffer to 0. All samples are lost but the number of channels and sample rate are kept.
         */
        void empty();

        /**
         * Return the data (audio samples) for a given channel number.
         * Channel data can be modified but size of individual channel vectors should not be changed manually.
         */
        std::vector<SFLAudioSample> *getChannel(unsigned chan);

        /**
         * Return a pointer to the raw data in this buffer.
         */
        inline std::vector<std::vector<SFLAudioSample> > &getData() {
            return samples_;
        }

        /**
         * Write interleaved multichannel data to the out buffer (fixed-point 16-bits).
         * The out buffer must be at least large by capacity()*sizeof(SFLAudioSample) bytes.
         *
         * @returns Number of samples writen.
         */
        size_t interleave(SFLAudioSample* out) const;

        /**
         * Write interleaved multichannel data to the out buffer, while samples are converted to float.
         * The buffer must be at least of size getChannelNum()*samples()*sizeof(float).
         *
         * @returns Number of samples writen.
         */
        size_t interleaveFloat(float* out) const;

        /**
         * Import interleaved multichannel data. Internal buffer is resized as needed. Function will read sample_num*channel_num elements of the in buffer.
         */
        void deinterleave(const SFLAudioSample* in, size_t sample_num, unsigned channel_num = 1);

        /**
         * In-place gain transformation with integer parameter.
         *
         * @param gain: 0 -> 100 scale
         */
        void applyGain(unsigned int gain);

        /**
         * In-place gain transformation.
         *
         * @param gain: 0.0 -> 1.0 scale
         */
        void applyGain(double gain);

        /**
         * Mix elements of the other buffer within this buffer (in-place simple addition).
         * If other.channels() is higher than this.channels(), only the first this.channels() channels are imported.
         * If other.channels() is lower than this.channels(), behavior depends on upmix.
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
         * Copy sample_num samples from in (from sample pos_in) to this buffer (at sample pos_out).
         * If sample_num is -1 (the default), the entire in buffer is copied.
         *
         * The number of channels is changed to match the in channel number.
         * Buffer sample number is also increased if required to hold the new requested samples.
         */
        size_t copy(AudioBuffer& in, int sample_num = -1, size_t pos_in = 0, size_t pos_out = 0, bool upmix = true);

        /**
         * Copy sample_num samples from in to this buffer (at sample pos_out).
         * Input data is treated as mono and samples are duplicated in the case of a multichannel buffer.
         *
         * Buffer sample number is increased if required to hold the new requested samples.
         */
        size_t copy(SFLAudioSample* in, size_t sample_num, size_t pos_out = 0);

        /**
         * Overloading << and >> to easily import/export a multichannel stream
         */
        friend std::ostream& operator<<(std::ostream& os, const AudioBuffer& buf);
        friend std::istream& operator>>(std::istream& is, AudioBuffer& buf);

    private:
        int sampleRate_;

        // main buffers holding data for each channels
        std::vector<std::vector<SFLAudioSample> > samples_;
};

#endif // _AUDIO_BUFFER_H
