/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef _VIDEO_RTP_THREAD_H_
#define _VIDEO_RTP_THREAD_H_

#include <cc++/thread.h>
#include <map>
#include <string>

class SwsContext;
class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVFrame;

namespace sfl_video {
class VideoRtpReceiveThread : public ost::Thread {
    private:
        std::map<std::string, std::string> args_;
        volatile int interrupted_;

        /*-------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. run()) only! */
        /*-------------------------------------------------------------*/
        uint8_t *scaledPictureBuf_;
        uint8_t *shmBuffer_;
        int shmID_;
        int semSetID_;
        AVCodecContext *decoderCtx_;
        AVFrame *rawFrame_;
        AVFrame *scaledPicture_;
        int videoStreamIndex_;
        AVFormatContext *inputCtx_;

        void setup();
        void cleanup();
        SwsContext * createScalingContext();
    public:
        VideoRtpReceiveThread(const std::map<std::string, std::string> &args);
        virtual ~VideoRtpReceiveThread();
        virtual void run();
        void stop();
};

class VideoRtpSendThread : public ost::Thread {
    private:
        static void print_error(const char *filename, int err);
        void forcePresetX264();
        void print_and_save_sdp();
        void setup();
        void cleanup();
        SwsContext * createScalingContext();

        std::map<std::string, std::string> args_;
        volatile int interrupted_;
        /*-------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. run()) only! */
        /*-------------------------------------------------------------*/
        uint8_t *scaledPictureBuf_;
        uint8_t *outbuf_;
        AVCodecContext *inputDecoderCtx_;
        AVFrame *rawFrame_;
        AVFrame *scaledPicture_;
        int videoStreamIndex_;
        int outbufSize_;
        AVCodecContext *encoderCtx_;
        AVStream *videoStream_;
        AVFormatContext *inputCtx_;
        AVFormatContext *outputCtx_;
    public:
        VideoRtpSendThread(const std::map<std::string, std::string> &args);
        virtual ~VideoRtpSendThread();
        virtual void run();
        void stop();
};
}

#endif // _VIDEO_RTP_THREAD_H_
