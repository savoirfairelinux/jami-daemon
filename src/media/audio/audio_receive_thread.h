/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "audio_format.h"
#include "media/media_buffer.h"
#include "media/media_device.h"
#include "media/media_codec.h"
#include "noncopyable.h"
#include "observer.h"
#include "media/socket_pair.h"
#include "threadloop.h"

#include <functional>
#include <sstream>

namespace jami {

class MediaDecoder;
class MediaIOHandle;
struct MediaStream;
class RingBuffer;

class AudioReceiveThread : public Observable<std::shared_ptr<MediaFrame>>
{
public:
    AudioReceiveThread(const std::string& streamId,
                       const AudioFormat& format,
                       const std::string& sdp,
                       const uint16_t mtu);
    ~AudioReceiveThread();

    MediaStream getInfo() const;

    void addIOContext(SocketPair& socketPair);
    void startReceiver();
    void stopReceiver();

    void setSuccessfulSetupCb(const std::function<void(MediaType, bool)>& cb)
    {
        onSuccessfulSetup_ = cb;
    }

    void setRecorderCallback(const std::function<void(const MediaStream& ms)>& cb);

private:
    NON_COPYABLE(AudioReceiveThread);

    static constexpr auto SDP_FILENAME = "dummyFilename";

    static int interruptCb(void* ctx);
    static int readFunction(void* opaque, uint8_t* buf, int buf_size);

    /*-----------------------------------------------------------------*/
    /* These variables should be used in thread (i.e. process()) only! */
    /*-----------------------------------------------------------------*/
    const std::string streamId_;
    const AudioFormat& format_;

    DeviceParams args_;

    std::istringstream stream_;
    mutable std::mutex mutex_;
    std::unique_ptr<MediaDecoder> audioDecoder_;
    std::unique_ptr<MediaIOHandle> sdpContext_;
    std::unique_ptr<MediaIOHandle> demuxContext_;

    std::shared_ptr<RingBuffer> ringbuffer_;

    uint16_t mtu_;

    ThreadLoop loop_;
    bool setup();
    void process();
    void cleanup();

    std::function<void(MediaType, bool)> onSuccessfulSetup_;
    std::function<void(const MediaStream& ms)> recorderCallback_;
};

} // namespace jami
