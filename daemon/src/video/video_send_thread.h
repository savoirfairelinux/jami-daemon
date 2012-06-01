/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include <cc++/thread.h>
#include <map>
#include <string>
#include "noncopyable.h"

class SwsContext;
class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVFrame;
class AVCodec;

namespace sfl_video {

class VideoSendThread : public ost::Thread {
    private:
        NON_COPYABLE(VideoSendThread);
        void forcePresetX264();
        void print_and_save_sdp();
        void setup();
        void prepareEncoderContext(AVCodec *encoder);
        void createScalingContext();
        ost::Event sdpReady_;

        std::map<std::string, std::string> args_;
        /*-------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. run()) only! */
        /*-------------------------------------------------------------*/
        uint8_t *scaledPictureBuf_;
        uint8_t *outbuf_;
        AVCodecContext *inputDecoderCtx_;
        AVFrame *rawFrame_;
        AVFrame *scaledPicture_;
        int streamIndex_;
        int outbufSize_;
        AVCodecContext *encoderCtx_;
        AVStream *stream_;
        AVFormatContext *inputCtx_;
        AVFormatContext *outputCtx_;
        SwsContext *imgConvertCtx_;
        std::string sdp_;
    public:
        explicit VideoSendThread(const std::map<std::string, std::string> &args);
        virtual ~VideoSendThread();
        // called from main thread 
        void waitForSDP();
        virtual void run();
        std::string getSDP() const { return sdp_; }
};
}

#endif // _VIDEO_SEND_THREAD_H_
