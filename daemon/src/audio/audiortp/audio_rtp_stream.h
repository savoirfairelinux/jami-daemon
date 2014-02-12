/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@wisdomvibes.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#ifndef AUDIO_RTP_STREAM_H__
#define AUDIO_RTP_STREAM_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>
#include <array>
#include <mutex>
#include <memory>
#include <atomic>

#include "noncopyable.h"
#include "audio/codecs/audiocodec.h"
#include "audio/audiobuffer.h"

class SamplerateConverter;
class DSP;

namespace sfl {

class AudioRtpContext {
    public:
    AudioRtpContext(AudioFormat f);
    ~AudioRtpContext();

    private:
    NON_COPYABLE(AudioRtpContext);

    void resetResampler();
    int payloadType;
    int frameSize;
    AudioFormat format;
    AudioBuffer resampledData;
    std::unique_ptr<SamplerateConverter> resampler;
#if HAVE_SPEEXDSP
    void resetDSP();
    void applyDSP(AudioBuffer &rawBuffer);
    std::unique_ptr<DSP> dsp;
    std::mutex dspMutex;
#endif

    friend class AudioRtpStream;
};

/**
 * Class meant to store internal data in order to encode/decode,
 * resample, process, and packetize audio streams.
 */
class AudioRtpStream {
    public:
        AudioRtpStream(const std::string &id);
        virtual ~AudioRtpStream();
        void initBuffers();
        void setRtpMedia(const std::vector<AudioCodec*> &codecs);
        /**
         * Decode audio data received from peer
         */
        void processDataDecode(unsigned char * spkrData, size_t size, int payloadType);

        /**
         * Encode audio data from mainbuffer
         */
        int processDataEncode();

        bool hasDynamicPayload() const {
            return hasDynamicPayloadType_;
        }

        const unsigned char *getMicDataEncoded() const {
            return encodedData_.data();
        }

        int getEncoderPayloadType() const;
        int getEncoderFrameSize() const;
        AudioFormat getEncoderFormat() const { return encoder_.format; }

#if HAVE_SPEEXDSP
        void initDSP();
        void resetDSP();
#endif
        bool codecsDiffer(const std::vector<AudioCodec*> &codecs) const;
        int getTransportRate() const;

    private:
        const std::string id_;

        AudioRtpContext encoder_;
        AudioRtpContext decoder_;

        void deleteCodecs();
        bool tryToSwitchPayloadTypes(int newPt);
        sfl::AudioCodec* getCurrentEncoder() const;
        sfl::AudioCodec* getCurrentDecoder() const;

        std::vector<AudioCodec*> audioCodecs_;
        std::mutex audioCodecMutex_;
        // these will have the same value unless we are sending
        // a different codec than we are receiving (asymmetric RTP)
        bool hasDynamicPayloadType_;
        // FIXME: probably need one for pre-encoder data, one for post-decoder data
        AudioBuffer rawBuffer_;
        std::array<unsigned char, RAW_BUFFER_SIZE> encodedData_;
        double fadeFactor_;

        bool isDead();

        /**
        * Ramp In audio data to avoid audio click from peer
        */
        void fadeInRawBuffer();
        NON_COPYABLE(AudioRtpStream);
        std::atomic<bool> dead_;
        size_t currentEncoderIndex_;
        size_t currentDecoderIndex_;
        int warningInterval_;
};
}

#endif // AUDIO_RTP_STREAM_H__
