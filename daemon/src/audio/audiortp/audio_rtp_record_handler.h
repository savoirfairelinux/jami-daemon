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

#ifndef AUDIO_RTP_RECORD_HANDLER_H__
#define AUDIO_RTP_RECORD_HANDLER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>

using std::ptrdiff_t;

#include <ccrtp/rtp.h>
#include <array>
#include <list>
#include <mutex>
#include <atomic>

#include "noncopyable.h"
#include "audio/codecs/audiocodec.h"
#include "audio/audiobuffer.h"
#include "dtmf_event.h"

class SIPCall;
class SamplerateConverter;
class DSP;

namespace sfl {

struct AudioEncoder {
    AudioEncoder(AudioFormat f) : payloadType(0), frameSize(0), format(f) {}
    int payloadType;
    int frameSize;
    AudioFormat format;
};

struct AudioDecoder {
    AudioDecoder(AudioFormat f) : payloadType(0), frameSize(0), format(f) {}
    int payloadType;
    int frameSize;
    AudioFormat format;
};

/**
 * Class meant to store internal data in order to encode/decode,
 * resample, process, and packetize audio streams. This class should not be
 * handled directly. Use AudioRtpRecordHandler
 */
class AudioRtpRecord {
    public:
        AudioRtpRecord();
        ~AudioRtpRecord();
        void deleteCodecs();
        bool tryToSwitchPayloadTypes(int newPt);
        sfl::AudioCodec* getCurrentCodec() const;
        std::string callId_;

    private:
        void initBuffers();
#if HAVE_SPEEXDSP
        void initDSP();
#endif
        void setRtpMedia(const std::vector<AudioCodec*> &codecs);

        std::string getCurrentCodecNames();

        AudioEncoder encoder_;
        AudioDecoder decoder_;

        std::vector<AudioCodec*> audioCodecs_;
        std::mutex audioCodecMutex_;
        // these will have the same value unless we are sending
        // a different codec than we are receiving (asymmetric RTP)
        bool hasDynamicPayloadType_;
        // FIXME: probably need one for pre-encoder data, one for post-decoder data
        AudioBuffer rawBuffer_;
        AudioBuffer resampledDataEncode_;
        AudioBuffer resampledDataDecode_;
        std::array<unsigned char, RAW_BUFFER_SIZE> encodedData_;
        SamplerateConverter *converterEncode_;
        SamplerateConverter *converterDecode_;
        int converterSamplingRate_;
        double fadeFactor_;

#if HAVE_SPEEXDSP
        DSP *dspEncode_;
        DSP *dspDecode_;
        std::mutex audioProcessMutex_;
#endif

        bool isDead();
        friend class AudioRtpRecordHandler;

        /**
         * Decode audio data received from peer
         */
        void processDataDecode(unsigned char * spkrData, size_t size, int payloadType, const std::string &id);

        /**
         * Encode audio data from mainbuffer
         */
        int processDataEncode(const std::string &id);

        /**
        * Ramp In audio data to avoid audio click from peer
        */
        void fadeInRawBuffer();
        NON_COPYABLE(AudioRtpRecord);
        std::atomic<bool> dead_;
        size_t currentCodecIndex_;
        int warningInterval_;
};


class AudioRtpRecordHandler {
    public:
        AudioRtpRecordHandler(SIPCall &);
        virtual ~AudioRtpRecordHandler();

        /**
         *  Set rtp media for this session
         */
        void setRtpMedia(const std::vector<AudioCodec*> &codecs);

        AudioCodec *getAudioCodec() const {
            return audioRtpRecord_.audioCodecs_[0];
        }

        const AudioEncoder &getEncoder() const {
            return audioRtpRecord_.encoder_;
        }

        const AudioDecoder &getDecoder() const {
            return audioRtpRecord_.decoder_;
        }

        bool hasDynamicPayload() const {
            return audioRtpRecord_.hasDynamicPayloadType_;
        }

        bool hasDTMFPending() const {
            return not dtmfQueue_.empty();
        }

        const unsigned char *getMicDataEncoded() const {
            return audioRtpRecord_.encodedData_.data();
        }

        void initBuffers();

#if HAVE_SPEEXDSP
        void initDSP();
#endif

        /**
         * Encode audio data from mainbuffer
         */
        int processDataEncode();

        /**
         * Decode audio data received from peer
         */
        void processDataDecode(unsigned char * spkrData, size_t size, int payloadType);

        void setDtmfPayloadType(unsigned int payloadType) {
            dtmfPayloadType_ = payloadType;
        }

        unsigned int getDtmfPayloadType() const {
            return dtmfPayloadType_;
        }

        void putDtmfEvent(char digit);

        std::string getCurrentAudioCodecNames();

    protected:
        bool codecsDiffer(const std::vector<AudioCodec*> &codecs) const;
        AudioRtpRecord audioRtpRecord_;
        std::list<DTMFEvent> dtmfQueue_;

    private:
        unsigned int dtmfPayloadType_;
        const std::string id_;

};
}

#endif // AUDIO_RTP_RECORD_HANDLER_H__
