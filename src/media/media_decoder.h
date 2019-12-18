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
#include "threadloop.h"

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

class MediaDemuxer {
public:
    MediaDemuxer();
    ~MediaDemuxer();

    enum class Status {
        Success,
        EndOfFile,
        ReadError
    };
    using StreamCallback = std::function<void(AVPacket&)>;
  //  using StreamSync = std::function<void(int64_t)>;

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

//    void setStreamSync(unsigned stream, StreamSync sync = {}) {
//        streamSyncs_[stream] = std::move(sync);
//    }

    AVStream* getStream(unsigned stream) {
        if (stream >= inputCtx_->nb_streams)
            throw std::invalid_argument("Invalid stream index");
        return inputCtx_->streams[stream];
    }

    Status decode();

    int64_t getDuration() const;
    bool seekFrame(int stream_index, int64_t timestamp);

private:
    bool streamInfoFound_ {false};
    AVFormatContext *inputCtx_ = nullptr;
    std::vector<StreamCallback> streams_;
   // std::map<unsigned, StreamSync> streamSyncs_;
    int64_t startTime_;
    DeviceParams inputParams_;
    AVDictionary *options_ = nullptr;
};

class MediaDecoder
{
public:
    enum class Status {
        Success,
        FrameFinished,
        EndOfFile,
        ReadError,
        DecodeError,
        RestartRequired
    };

    MediaDecoder();
    MediaDecoder(MediaObserver observer);
    MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, int index);
    MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, int index, MediaObserver observer);
    MediaDecoder(const std::shared_ptr<MediaDemuxer>& demuxer, AVMediaType type) : MediaDecoder(demuxer, demuxer->selectStream(type)) {}
    ~MediaDecoder();

    void emulateRate() { emulateRate_ = true; }
    void skipFrames() { skipFrames_ = true; }
    void syncToOtherStream() { syncToOtherStream_ = true; }

    int openInput(const DeviceParams&);
    void setInterruptCallback(int (*cb)(void*), void *opaque);
    void setIOContext(MediaIOHandle *ioctx);

    int setup(AVMediaType type);
    int setupAudio() { return setup(AVMEDIA_TYPE_AUDIO); }
    int setupVideo() { return setup(AVMEDIA_TYPE_VIDEO); }

    MediaDemuxer::Status decode();
    Status flush();

    int getWidth() const;
    int getHeight() const;
    std::string getDecoderName() const;
    void updateStartTime(int64_t startTime);

    rational<double> getFps() const;
    AVPixelFormat getPixelFormat() const;
    void flushBuffers();
    void setOptions(const std::map<std::string, std::string>& options);
    void setSeekTime(int64_t time);
    void syncToStream(int index);
    void emitNewFrame();
    using StreamSync = std::function<void(int64_t)>;
    void setStreamSynk(StreamSync sync);
    StreamSync streamSync;
    void playFramesBeforeTimestamp(int64_t timestamp);
#ifdef RING_ACCEL
    void enableAccel(bool enableAccel);
#endif

    MediaStream getStream(std::string name = "") const;

private:
    NON_COPYABLE(MediaDecoder);

    Status decode(AVPacket&);

    rational<unsigned> getTimeBase() const;

    std::shared_ptr<MediaDemuxer> demuxer_;

    AVCodec *inputDecoder_ = nullptr;
    AVCodecContext *decoderCtx_ = nullptr;
    AVStream *avStream_ = nullptr;
    bool emulateRate_ = false;
    int64_t startTime_;
    int seekTime_ {-1};
    bool skipFrames_ {false}; //used for media player to skeep frames after seeking
    int64_t lastTimestamp_ {0};
    int64_t lastSynctamp_ {-INT_MAX};
    std::map<int64_t, std::shared_ptr<MediaFrame>> framesBuffer {};
    //void emitNewFrame();
    bool syncToOtherStream_ = false;
    void resetSeekTime() {
        seekTime_ = -1;
    }
    void clearFramesBeforeTimestamp(int64_t timestamp);
   // void playFramesBeforeTimestamp(int64_t timestamp);

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

protected:
    AVDictionary *options_ = nullptr;
};

} // namespace jami
