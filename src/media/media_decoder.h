/*
 *  Copyright (C) 2013-2018 Savoir-faire Linux Inc.
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

#ifdef RING_ACCEL
#include "video/accel.h"
#endif

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
enum AVMediaType;

namespace ring {

struct AudioFrame;
class AudioFormat;
class RingBuffer;
class Resampler;
class MediaIOHandle;
struct DeviceParams;

class MediaDecoder {
    public:
        enum class Status {
            Success,
            FrameFinished,
            EOFError,
            ReadError,
            DecodeError,
            RestartRequired
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
#ifdef RING_ACCEL
        void enableAccel(bool enableAccel);
#endif

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
        int setupStream(AVMediaType mediaType);

        bool fallback_ = false;

#ifdef RING_ACCEL
        bool enableAccel_ = true;
        video::HardwareAccel accel_;
        unsigned short accelFailures_ = 0;
#endif

    protected:
        AVDictionary *options_ = nullptr;
};

} // namespace ring
