/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "config.h"

#ifdef RING_VIDEO
#include "video/video_base.h"
#include "video/video_scaler.h"
#endif // RING_VIDEO

#include "audio/audiobuffer.h"

#include "rational.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <memory>
#include <chrono>

class AVCodecContext;
class AVStream;
class AVDictionary;
class AVFormatContext;
class AVCodec;

namespace ring {

class AudioFrame;
class AudioFormat;
class RingBuffer;
class Resampler;
class MediaIOHandle;
class DeviceParams;

class MediaDecoder {
    public:
        enum class Status {
            Success,
            FrameFinished,
            EOFError,
            ReadError,
            DecodeError
        };

        MediaDecoder();
        ~MediaDecoder();

        void emulateRate() { emulateRate_ = true; }
        void setInterruptCallback(int (*cb)(void*), void *opaque);
        int openInput(const DeviceParams&);

        void setIOContext(MediaIOHandle *ioctx);
#ifdef RING_VIDEO
        int setupFromVideoData();
        Status decode(VideoFrame&);
        Status flush(VideoFrame&);
 #endif // RING_VIDEO

        int setupFromAudioData(const AudioFormat format);
        Status decode(const AudioFrame&);
        void writeToRingBuffer(const AudioFrame&, RingBuffer&, const AudioFormat);

        int getWidth() const;
        int getHeight() const;
        std::string getDecoderName() const;

        rational<double> getFps() const;
        int getPixelFormat() const;

        void setOptions(const std::map<std::string, std::string>& options);

    private:
        NON_COPYABLE(MediaDecoder);

        rational<unsigned> getTimeBase() const;

        AVCodec *inputDecoder_ = nullptr;
        AVCodecContext *decoderCtx_ = nullptr;
        AVFormatContext *inputCtx_ = nullptr;
        AVStream *avStream_ = nullptr;
        std::unique_ptr<Resampler> resampler_;
        int streamIndex_ = -1;
        bool emulateRate_ = false;
        int64_t startTime_;

        AudioBuffer decBuff_;
        AudioBuffer resamplingBuff_;

        void extract(const std::map<std::string, std::string>& map, const std::string& key);
        int correctPixFmt(int input_pix_fmt);

        // Jitter buffer options: they are default values set in libav
        // maximum of packet jitter buffer can queue
        const unsigned jitterBufferMaxSize_ {500};
        // maximum time a packet can be queued (in ms)
        const unsigned jitterBufferMaxDelay_ {100000};

    protected:
        AVDictionary *options_ = nullptr;
};

} // namespace ring
