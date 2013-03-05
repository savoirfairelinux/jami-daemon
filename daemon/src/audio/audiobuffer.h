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

#include <vector>
#include <cstddef> // for size_t

#include "sfl_types.h"
#include "noncopyable.h"

class AudioBuffer {

	public:
		AudioBuffer(size_t sample_num=0, size_t channel_num=1, int sample_rate=8000);

		/**
		 * Copy constructor that by default only copies the buffer parameters (channel number, sample rate and buffer size).
		 * If copy_content is set to true, the buffer content is also copied.
		 */
		AudioBuffer(const AudioBuffer& other, bool copy_content=false);

		/**
		 * Returns the sample rate (in samples/sec) associated to this buffer.
		 */
		int getSampleRate();

		/**
		 * Set the sample rate (in samples/sec) associated to this buffer.
		 */
		void setSampleRate(int sr);

		/**
		 * Returns the number of samples in this buffer.
		 */
		size_t getChannelNum();

		/**
		 * Set the number of channels of this buffer.
		 *
		 * @param n: the new number of channels. If n < getChannelNum(), channels are deleted from the buffer, otherwise the behavior depends on copy_first.
		 *
		 * @param copy_first: if set to true and n > getChannelNum(), new channels are initialised with samples from the first channel. If set to false, new channels are initialised to 0.
		 */
		void setChannelNum(size_t n, bool copy_first=false);

		/**
		 * Returns the number of (multichannel) samples.
		 */
		size_t samples();

		/**
		 *
		 */
		void resize(size_t sample_num);

		/**
		 * Set the buffer to 0. Buffer size is kept.
		 */
		void clear();

		/**
		 * Resize the buffer to 0.
		 */
		void empty();

		/**
		 * Return the data (audio samples) for a given channel number.
		 * Channel data can be modified but size of individual channel vectors should not be changed manually.
		 */
		std::vector<SFLAudioSample> *getChannel(size_t chan=0);

		/**
		 * Write interleaved multichannel data to the out buffer.
		 * The out buffer must be at least of size getChannelNum()*samples()*sizeof(SFLAudioSample).
		 *
		 * @returns Number of samples writen.
		 */
		size_t interleave(SFLAudioSample* out);

		/**
		 * Write interleaved multichannel data to the out buffer, while samples are converted to float.
		 * The buffer must be at least of size getChannelNum()*samples()*sizeof(float).
		 *
		 * @returns Number of samples writen.
		 */
		size_t interleaveFloat(float* out);

		/**
		 * Import interleaved multichannel data. Internal buffer is resized as needed. Function will read sample_num*channel_num elements of the in buffer.
		 */
		void fromInterleaved(SFLAudioSample* in, size_t sample_num, size_t channel_num=1);

		/**
		 * In-place gain transformation with integer parameter.
		 *
		 * @param gain: 0 -> 100 scale
		 */
		void applyGain(int gain);

		/**
		 * In-place gain transformation.
		 *
		 * @param gain: 0.0 -> 1.0 scale
		 */
		void applyGain(double gain);

		/**
		 * Mix elements of the other buffer with this buffer (in-place simple addition).
		 * TODO: some kind of check for overflow/saturation.
		 *
		 * @returns Number of samples modified.
		 */
		size_t mix(const AudioBuffer& other);

		/**
		 * Copy sample_num samples from in (from sample pos_in) to this (at sample pos_out).
		 * If sample_num is -1 (the default), the entire in buffer is copied.
		 *
		 * The number of channels is changed to match the in channel number.
		 * Buffer sample number is also increased if required to hold the new requested samples.
		 */
		size_t copy(AudioBuffer& in, int sample_num=-1, size_t pos_in=0, size_t pos_out=0);

		/**
		 * Copy sample_num samples from in to this (at sample pos_out).
		 * Input data is treated as mono and samples are duplicated in the case of a multichannel buffer.
		 *
		 * Buffer sample number is increased if required to hold the new requested samples.
		 */
		size_t copy(SFLAudioSample* in, size_t sample_num, size_t pos_out=0);

	private:
        NON_COPYABLE(AudioBuffer);

        int sampleRate_;
        size_t channels_; // should allways be samples_.size()
        size_t sampleNum_;

        // main buffers holding data for each channels
        std::vector<std::vector<SFLAudioSample> > samples_;
};

#endif // _AUDIO_BUFFER_H
