/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author:  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  Mostly borrowed from asterisk's sources (Steve Underwood <steveu@coppice.org>)
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
#ifndef __AUDIO_CODEC_H__
#define __AUDIO_CODEC_H__

#include <string>
#include <vector>
#include "sfl_types.h"

#define XSTR(s) STR(s)
#define STR(s) #s

/* bump for each release, and bump the 0.0.0.xth digit for each binary change */
#define AUDIO_CODEC_ENTRY create_1_3_0_2
#define AUDIO_CODEC_ENTRY_SYMBOL XSTR(AUDIO_CODEC_ENTRY)

// Opus documentation:
// "If this is less than the maximum packet duration (120ms; 5760 for 48kHz),
// opus_decode will not be capable of decoding some packets."
#define RAW_BUFFER_SIZE (120 * 48000 / 1000)

namespace sfl {

class AudioCodec {
    public:
        AudioCodec(uint8_t payload, const std::string &codecName, int clockRate, int frameSize, unsigned channels);

        /**
         * Copy constructor.
         */
        AudioCodec(const AudioCodec& codec);

        virtual AudioCodec * clone() = 0;

        virtual ~AudioCodec() {};

        std::string getMimeSubtype() const;

        /**
         * Decode an input buffer and fill the output buffer with the decoded data
         * @param buffer_size : the size of the input buffer
         * @return the number of samples decoded
         */
        virtual int decode(SFLAudioSample* /* dst */, unsigned char* /* buf */, size_t /* buffer_size */) {};

        /**
         * Encode an input buffer and fill the output buffer with the encoded data
         * @param buffer_size : the maximum size of encoded data buffer (dst)
         * @return the number of bytes encoded
         */
        virtual int encode(unsigned char* /* dst */, SFLAudioSample* /* src */, size_t /* buffer_size */) {};

        /**
         * Multichannel version of decode().
         * Default implementation calls mono version
         */
        virtual int decode(std::vector<std::vector<SFLAudioSample> > &dst, uint8_t *buf, size_t buf_size);

        /**
         * Multichannel version of encode().
         * Default implementation calls encode() on the first channel (assume 1 channel).
         * @return the number of bytes encoded
         */
        virtual size_t encode(std::vector<std::vector<SFLAudioSample> > &src, uint8_t *dst, size_t dst_size);

        uint8_t getPayloadType() const;

        void setPayloadType(uint8_t pt) {
            payload_ = pt;
        }

        /**
         * @return true if this payload is a dynamic one.
         */
        bool hasDynamicPayload() const;

        /**
         * @returns maximum supported clock rate (sample rate).
         */
        uint32_t getClockRate() const;

        /**
         * @returns currently configured clock rate (sample rate).
         */
        uint32_t getCurrentClockRate() const;

        /**
         * Clock-rate in SDP MAY be different than actual clock-rate
         * (in derived classes).
         * @returns Clock-rate as it should be reported by SDP
         *          during negociation.
         */
        virtual uint32_t getSDPClockRate() const;

        double getBitRate() const;

        /**
         * @returns maximum supported channel number.
         */
        uint8_t getChannels() const;

        /**
         * @returns currently configured channel number.
         */
        uint8_t getCurrentChannels() const;

        /**
         * Channels in SDP MAY be different than actual channels.
         * Should be an empty string EXCEPT for Opus which returns "2".
         * @returns Supported channel number as it should be reported by SDP
         *          during negociation.
         */
        virtual const char * getSDPChannels() const;

        /**
         * @return the framing size for this codec.
         */
        unsigned int getFrameSize() const;

        /**
         * Set the sampling rate and channel number preffered by the core.
         * May or may not be considered by the codec.
         * Use getClockRate() and getChannels() to get the format used by the codec.
         */
        virtual void setOptimalFormat(uint32_t /* sample_rate */ , uint8_t /* channels */ ) {}

    protected:
        /** Holds SDP-compliant codec name */
        std::string codecName_; // what we put inside sdp

        /** Clock rate or sample rate of the codec, in Hz */
        uint32_t clockRate_, clockRateCur_;

        /** Number of channels 1 = mono, 2 = stereo */
        uint8_t channels_, channelsCur_;

        /** codec frame size in samples*/
        unsigned frameSize_;

        /** Bitrate */
        double bitrate_;

        uint8_t payload_;
        bool hasDynamicPayload_;

    private:
        AudioCodec& operator=(const AudioCodec&);

};
} // end namespace sfl


typedef sfl::AudioCodec* create_t();
typedef void destroy_t(sfl::AudioCodec* codec);

#endif
