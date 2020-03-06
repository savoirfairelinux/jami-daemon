/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#include "rational.h"
#include "observer.h"

#ifdef ENABLE_VIDEO
#include "video/video_base.h"
#include "video/video_scaler.h"
#endif // ENABLE_VIDEO

#ifdef RING_ACCEL
#include "video/accel.h"
#endif
#include "logger.h"

#include "audio/audiobuffer.h"

#include "media_device.h"
#include "media_stream.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <queue>

extern "C" {
struct AVCodecContext;
struct AVStream;
struct AVDictionary;
struct AVFormatContext;
struct AVCodec;
enum AVMediaType;
}

namespace DRing {
class AudioFrame;
}

namespace jami {

using AudioFrame = DRing::AudioFrame;
struct AudioFormat;
class RingBuffer;
class Resampler;
class MediaIOHandle;
class MediaDecoder;

enum class DecodeStatus {
    Success,
    FrameFinished,
    EndOfFile,
    ReadError,
    DecodeError,
    RestartRequired,
    FallBack
};

class MediaDemuxer {
public:
    MediaDemuxer();
    ~MediaDemuxer();

    enum class Status {
        Success,
        EndOfFile,
        ReadBufferOverflow,
        ReadError,
        FallBack
    };

    enum class CurrentState {
        Demuxing,
        Finished
    };
    using StreamCallback = std::function<DecodeStatus (AVPacket&)>;

    int openInput(const DeviceParams&);

    void setInterruptCallback(int (*cb)(void*), void *opaque);
    void setIOContext(MediaIOHandle *ioctx);

    void findStreamInfo();
    int selectStream(AVMediaType type);

    void setStreamCallback(unsigned stream, StreamCallback cb = {}) {
        if (streams_.size() <= stream)
            streams_.resize(stream + 1);
        streams_[stream] = std::move(cb);
    }

    void updateCurrentState(MediaDemuxer::CurrentState state) {
        currentState_ = state;
    }

    void setFileFinishedCb(std::function<void(bool)> cb);

    MediaDemuxer::CurrentState getCurrentState() {
        return currentState_;
    }

    AVStream* getStream(unsigned stream) {
        if (stream >= inputCtx_->nb_streams)
            throw std::invalid_argument("Invalid stream index");
        return inputCtx_->streams[stream];
    }

    Status decode();
    Status demuxe();

    int64_t getDuration() const;
    bool seekFrame(int stream_index, int64_t timestamp);
    void setNeedFrameCb(std::function<void()> cb);
    void emitFrame(bool isAudio);

private:
    bool streamInfoFound_ {false};
    AVFormatContext *inputCtx_ = nullptr;
    std::vector<StreamCallback> streams_;
    int64_t startTime_;
    DeviceParams inputParams_;
    AVDictionary *options_ = nullptr;
    MediaDemuxer::CurrentState currentState_;
    std::mutex audioBufferMutex_ {};
    std::mutex videoBufferMutex_ {};
    std::queue<std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>> videoBuffer_ {};
    std::queue<std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>> audioBuffer_ {};
    std::function<void()> needFrameCb_;
    std::function<void(bool)> fileFinishedCb_;
    void clearFrames();
    void pushFrameFrom(std::queue<std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>>& buffer, bool isAudio, std::mutex& mutex);
};

class MediaDecoder
{
public:

    MediaDecoder();
    MediaDecoder(MediaObserver observer);
    MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, int index);
    MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, int index, MediaObserver observer);
    MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, AVMediaType type) : MediaDecoder(demuxer, demuxer->selectStream(type)) {}
    ~MediaDecoder();

    void emulateRate() { emulateRate_ = true; }

    int openInput(const DeviceParams&);
    void setInterruptCallback(int (*cb)(void*), void *opaque);
    void setIOContext(MediaIOHandle *ioctx);

    int setup(AVMediaType type);
    int setupAudio() { return setup(AVMEDIA_TYPE_AUDIO); }
    int setupVideo() { return setup(AVMEDIA_TYPE_VIDEO); }

    MediaDemuxer::Status decode();
    DecodeStatus flush();

    int getWidth() const;
    int getHeight() const;
    std::string getDecoderName() const;

    rational<double> getFps() const;
    AVPixelFormat getPixelFormat() const;
    void setOptions(const std::map<std::string, std::string>& options);

    void updateStartTime(int64_t startTime);

    void emitFrame(bool isAudio);
    void flushBuffers();
    void setSeekTime(int64_t time);
#ifdef RING_ACCEL
    void enableAccel(bool enableAccel);
#endif

    MediaStream getStream(std::string name = "") const;

private:
    NON_COPYABLE(MediaDecoder);

    DecodeStatus decode(AVPacket&);

    rational<unsigned> getTimeBase() const;

    std::shared_ptr<MediaDemuxer> demuxer_;

    AVCodec *inputDecoder_ = nullptr;
    AVCodecContext *decoderCtx_ = nullptr;
    AVStream *avStream_ = nullptr;
    bool emulateRate_ = false;
    int64_t startTime_;
    int64_t lastTimestamp_ {0};

    DeviceParams inputParams_;

    int correctPixFmt(int input_pix_fmt);
    int setupStream();

    bool fallback_ = false;

#ifdef RING_ACCEL
    bool enableAccel_ = true;
    std::unique_ptr<video::HardwareAccel> accel_;
    unsigned short accelFailures_ = 0;
#endif
    MediaObserver callback_;
    int prepareDecoderContext();
    int64_t seekTime_ = -1;
    void resetSeekTime() {
        seekTime_ = -1;
    }

protected:
    AVDictionary *options_ = nullptr;
};

} // namespace jami
