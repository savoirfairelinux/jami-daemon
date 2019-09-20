/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
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

// Scaler used to convert the image to RGB
#include "media/video/video_scaler.h"

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

namespace jami { namespace video {

class SinkClient;

class VideoReceiveThread : public VideoGenerator {
public:
    VideoReceiveThread(const std::string &id, const std::string &sdp, uint16_t mtu);
    ~VideoReceiveThread();
    void startLoop(const std::function<void(MediaType)>& cb);

    void addIOContext(SocketPair& socketPair);
    void setRequestKeyFrameCallback(std::function<void (void)>);
    void enterConference();
    void exitConference();

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

private:


    //==============================
    // An instance of the scaler
    video::VideoScaler scaler;
    int i{0};
    //==============================
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
    std::shared_ptr<AVBufferRef> displayMatrix_;

    void openDecoder();
    void decodeFrame();
    static int interruptCb(void *ctx);
    static int readFunction(void *opaque, uint8_t *buf, int buf_size);

    std::function<void(MediaType)> onSetupSuccess_;

    ThreadLoop loop_;

    // used by ThreadLoop
    bool setup();
    void process();
    void cleanup();
};

}} // namespace jami::video

#endif // _VIDEO_RECEIVE_THREAD_H_
