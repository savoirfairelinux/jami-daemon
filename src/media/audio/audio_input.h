/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include <atomic>
#include <future>
#include <mutex>

#include "audio/audiobuffer.h"
#include "media_device.h"
#include "media_buffer.h"
#include "observer.h"
#include "threadloop.h"

namespace jami {
class MediaDecoder;
class MediaDemuxer;
}
namespace jami {

class AudioFrameResizer;
class MediaDecoder;
class MediaRecorder;
struct MediaStream;
class Resampler;
class RingBuffer;

class AudioInput : public Observable<std::shared_ptr<MediaFrame>>
{
public:
    AudioInput(const std::string& id);
    AudioInput(const std::string& id, const std::string& resource);
    ~AudioInput();

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

    bool isCapturing() const { return loop_.isRunning(); }
    void setFormat(const AudioFormat& fmt);
    void setMuted(bool isMuted);
    MediaStream getInfo() const;
    void updateStartTime(int64_t start);
    void setPaused(bool paused);
    void configureFilePlayback(const std::string& path, std::shared_ptr<MediaDemuxer>& demuxer, int index);
    void flushBuffers();
    void setSeekTime(int64_t time);

private:
    void readFromDevice();
    void readFromFile();
    void readFromQueue();
    bool initDevice(const std::string& device);
    bool initFile(const std::string& path);
    bool createDecoder();
    void frameResized(std::shared_ptr<AudioFrame>&& ptr);

    std::string id_;
    AudioBuffer micData_;
    bool muteState_ = false;
    uint64_t sent_samples = 0;
    mutable std::mutex fmtMutex_ {};
    AudioFormat format_;
    int frameSize_;
    std::atomic_bool paused_ {true};

    std::unique_ptr<Resampler> resampler_;
    std::unique_ptr<AudioFrameResizer> resizer_;
    std::unique_ptr<MediaDecoder> decoder_;

    std::string fileId_;
    std::shared_ptr<RingBuffer> fileBuf_;

    std::string currentResource_;
    std::atomic_bool switchPending_ {false};
    DeviceParams devOpts_;
    std::promise<DeviceParams> foundDevOpts_;
    std::shared_future<DeviceParams> futureDevOpts_;
    std::atomic_bool devOptsFound_ {false};
    void foundDevOpts(const DeviceParams& params);
    std::atomic_bool decodingFile_ {false};
    std::atomic_bool playingFile_ {false};

    ThreadLoop loop_;
    void process();
};

} // namespace jami
