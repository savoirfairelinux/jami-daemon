/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#include "media_device.h"
#include "media_stream.h"
#include "threadloop.h"
#include "noncopyable.h"

#include <map>
#include <string>
#include <climits>
#include <sstream>
#include <memory>

namespace ring {
class SocketPair;
class MediaDecoder;
} // namespace ring

namespace ring { namespace video {

class SinkClient;

class VideoReceiveThread : public VideoGenerator {
public:
    VideoReceiveThread(const std::string &id, const std::string &sdp, uint16_t mtu);
    ~VideoReceiveThread();
    void startLoop();

    void addIOContext(SocketPair& socketPair);
    void setRequestKeyFrameCallback(void (*)(const std::string &));
    void enterConference();
    void exitConference();

    // as VideoGenerator
    int getWidth() const;
    int getHeight() const;
    int getPixelFormat() const;
    MediaStream getInfo() const;
    void triggerKeyFrameRequest();

    /**
      * Send rotation to the sink client
      *
      * Send angle to apply to the video flux by the sink client
      *
      * @param Angle of the rotation in degrees
      */
    void setRotation(int angle);

private:
    NON_COPYABLE(VideoReceiveThread);

    DeviceParams args_;

    /*-------------------------------------------------------------*/
    /* These variables should be used in thread (i.e. run()) only! */
    /*-------------------------------------------------------------*/
    std::unique_ptr<MediaDecoder> videoDecoder_;
    int dstWidth_;
    int dstHeight_;
    const std::string id_;
    std::istringstream stream_;
    MediaIOHandle sdpContext_;
    std::unique_ptr<MediaIOHandle> demuxContext_;
    std::shared_ptr<SinkClient> sink_;
    bool isReset_;
    uint16_t mtu_;
    int rotation_;
    AVBufferRef* frameDataBuffer;

    void (*requestKeyFrameCallback_)(const std::string &);
    void openDecoder();
    bool decodeFrame();
    static int interruptCb(void *ctx);
    static int readFunction(void *opaque, uint8_t *buf, int buf_size);

    ThreadLoop loop_;

    // used by ThreadLoop
    bool setup();
    void process();
    void cleanup();
};

}} // namespace ring::video

#endif // _VIDEO_RECEIVE_THREAD_H_
