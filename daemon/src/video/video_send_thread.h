/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
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

#ifndef _VIDEO_SEND_THREAD_H_
#define _VIDEO_SEND_THREAD_H_

#include <map>
#include <string>
#include <tr1/memory>
#include "noncopyable.h"
#include "shm_sink.h"
#include "video_provider.h"

extern "C" {
#include <libavformat/avformat.h>
}

class SwsContext;
class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVFrame;
class AVCodec;

namespace sfl_video {

class SocketPair;

class VideoSendThread : public VideoProvider {
    private:
        NON_COPYABLE(VideoSendThread);
        void forcePresetX264();
        void print_sdp();
        void setup();
        void prepareEncoderContext(AVCodec *encoder);
        void fillBuffer(void *data);
        static int interruptCb(void *ctx);

        std::map<std::string, std::string> args_;
        /*-------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. run()) only! */
        /*-------------------------------------------------------------*/
        uint8_t *scaledInputBuffer_;
        uint8_t *encoderBuffer_;
        AVCodecContext *inputDecoderCtx_;
        AVFrame *rawFrame_;
        AVFrame *scaledInput_;
        int streamIndex_;
        int encoderBufferSize_;
        AVCodecContext *encoderCtx_;
        AVStream *stream_;
        AVFormatContext *inputCtx_;
        AVFormatContext *outputCtx_;
        SwsContext *previewConvertCtx_;
        SwsContext *encoderConvertCtx_;
        std::string sdp_;

        SHMSink sink_;
        size_t bufferSize_;
        const std::string id_;

        AVIOInterruptCB interruptCb_;
        bool threadRunning_;
        int forceKeyFrame_;
        static void *runCallback(void *);
        pthread_t thread_;
        int frameNumber_;
        std::tr1::shared_ptr<AVIOContext> muxContext_;
        void run();
        bool captureFrame();
        void renderFrame();
        void encodeAndSendVideo();
        friend struct VideoTxContextHandle;

    public:
        VideoSendThread(const std::string &id, const std::map<std::string, std::string> &args);
        ~VideoSendThread();
        void addIOContext(SocketPair &sock);
        void start();
        std::string getSDP() const { return sdp_; }
        void forceKeyFrame();
};
}

#endif // _VIDEO_SEND_THREAD_H_
