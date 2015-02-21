/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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

#ifndef __MEDIA_DECODER_H__
#define __MEDIA_DECODER_H__

#include "config.h"

#ifdef RING_VIDEO
#include "video/video_base.h"
#include "video/video_scaler.h"
#endif // RING_VIDEO

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

#ifdef RING_VIDEO
namespace video {
class VideoPacket;
} // namespace ring::video
#endif // RING_VIDEO

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
        Status decode(VideoFrame&, video::VideoPacket&);
        Status flush(VideoFrame&);
 #endif // RING_VIDEO

        int setupFromAudioData(const AudioFormat format);
        Status decode(const AudioFrame&);
        void writeToRingBuffer(const AudioFrame&, RingBuffer&, const AudioFormat);

        int getWidth() const;
        int getHeight() const;
        int getPixelFormat() const;

        void setOptions(const std::map<std::string, std::string>& options);

    private:
        NON_COPYABLE(MediaDecoder);

        AVCodec *inputDecoder_ = nullptr;
        AVCodecContext *decoderCtx_ = nullptr;
        AVFormatContext *inputCtx_ = nullptr;
        std::unique_ptr<Resampler> resampler_;
        int streamIndex_ = -1;
        bool emulateRate_ = false;
        int64_t startTime_;
        int64_t lastDts_;
        std::chrono::time_point<std::chrono::system_clock> lastFrameClock_ = {};

        void extract(const std::map<std::string, std::string>& map, const std::string& key);

    protected:
        AVDictionary *options_ = nullptr;
};

} // namespace ring

#endif // __MEDIA_DECODER_H__
