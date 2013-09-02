/*
 *  Copyright (C) 2011-2012 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef _VIDEO_RECEIVE_THREAD_H_
#define _VIDEO_RECEIVE_THREAD_H_

#include <map>
#include <string>
#include <climits>
#include <sstream>
#include <tr1/memory>
#include "shm_sink.h"
#include "noncopyable.h"
#include "video_provider.h"
#include "video_decoder.h"
#include "video_scaler.h"
#include "video_mixer.h"
#include "sflthread.h"


namespace sfl_video {

class SocketPair;

class VideoReceiveThread : public VideoProvider, public SFLThread  {
private:
    NON_COPYABLE(VideoReceiveThread);
    std::map<std::string, std::string> args_;

    /*-------------------------------------------------------------*/
    /* These variables should be used in thread (i.e. run()) only! */
    /*-------------------------------------------------------------*/
    VideoDecoder *videoDecoder_;
    VideoMixer *mixer_;

    int dstWidth_;
    int dstHeight_;

    SHMSink sink_;
    size_t bufferSize_;
    const std::string id_;
    std::istringstream stream_;
    VideoIOHandle sdpContext_;
    VideoIOHandle *demuxContext_;
    VideoScaler scaler_;
    VideoFrame previewFrame_;
    void (* requestKeyFrameCallback_)(const std::string &);

    void openDecoder();
    void fillBuffer(void *data);
    static int interruptCb(void *ctx);
    friend struct VideoRxContextHandle;

    bool decodeFrame();
    void renderFrame();
    static int readFunction(void *opaque, uint8_t *buf, int buf_size);

protected:
    // threading
    bool setup();
    void process();
    void cleanup();

public:
    VideoReceiveThread(const std::string &id,
                       const std::map<std::string, std::string> &args);
    void addIOContext(SocketPair &socketPair);
    void addDetails(std::map<std::string, std::string> &details);
    ~VideoReceiveThread();
    void setRequestKeyFrameCallback(void (*)(const std::string &));
};
}

#endif // _VIDEO_RECEIVE_THREAD_H_
