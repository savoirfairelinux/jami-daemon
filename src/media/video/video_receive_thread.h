/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#ifndef _VIDEO_RECEIVE_THREAD_H_
#define _VIDEO_RECEIVE_THREAD_H_

#include "video_base.h"
#include "media_codec.h"
#include "media_io_handle.h"
#include "media_codec.h"
#include "media_device.h"
#include "media_stream.h"
#include "threadloop.h"
#include "noncopyable.h"
#include "libav_utils.h"

#include <functional>
#include <map>
#include <string>
#include <climits>
#include <sstream>
#include <memory>

namespace jami {
class SocketPair;
class MediaDecoder;
} // namespace jami

namespace jami {
namespace video {

class SinkClient;

class VideoReceiveThread : public VideoGenerator, private std::enable_shared_from_this<VideoReceiveThread>
{
public:
    VideoReceiveThread(const std::string& id, bool useSink, const std::string& sdp, uint16_t mtu);
    ~VideoReceiveThread();

    void startLoop();
    void stopLoop();
    void decodeFrame();
    void addIOContext(SocketPair& socketPair);
    void setRequestKeyFrameCallback(std::function<void(void)> cb)
    {
        keyFrameRequestCallback_ = std::move(cb);
    };
    void startSink();
    void stopSink();
    std::shared_ptr<SinkClient>& getSink() { return sink_; }

    // as VideoGenerator
    int getWidth() const;
    int getHeight() const;
    AVPixelFormat getPixelFormat() const;
    MediaStream getInfo() const;

    /**
     * Set angle of rotation to apply to the video by the decoder
     *
     * @param angle Angle of rotation in degrees (counterclockwise)
     */
    void setRotation(int angle);

    void setSuccessfulSetupCb(const std::function<void(MediaType, bool)>& cb)
    {
        onSuccessfulSetup_ = cb;
    }

    void setRecorderCallback(
        const std::function<void(const std::shared_ptr<VideoFrameActiveWriter>& vg, const MediaStream& ms)>& cb);

private:
    NON_COPYABLE(VideoReceiveThread);

    DeviceParams args_;

    /*-------------------------------------------------------------*/
    /* These variables should be used in thread (i.e. run()) only! */
    /*-------------------------------------------------------------*/
    std::unique_ptr<MediaDecoder> videoDecoder_;
    int dstWidth_ {0};
    int dstHeight_ {0};
    const std::string id_;
    bool useSink_;
    std::istringstream stream_;
    MediaIOHandle sdpContext_;
    std::unique_ptr<MediaIOHandle> demuxContext_;
    std::shared_ptr<SinkClient> sink_;
    bool isVideoConfigured_ {false};
    uint16_t mtu_;
    int rotation_ {0};

    std::mutex rotationMtx_;
    libav_utils::AVBufferPtr displayMatrix_;

    static int interruptCb(void* ctx);
    static int readFunction(void* opaque, uint8_t* buf, int buf_size);
    bool configureVideoOutput();

    ThreadLoop loop_;

    // used by ThreadLoop
    bool setup();
    void process();
    void cleanup();

    std::function<void(void)> keyFrameRequestCallback_;
    std::function<void(MediaType, bool)> onSuccessfulSetup_;
    std::function<void(std::shared_ptr<VideoFrameActiveWriter> vg, const MediaStream& ms)> recorderCallback_;
};

} // namespace video
} // namespace jami

#endif // _VIDEO_RECEIVE_THREAD_H_
