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

#ifndef _VIDEO_RECEIVE_THREAD_H_
#define _VIDEO_RECEIVE_THREAD_H_

#include "cc_thread.h"
#include <map>
#include <string>
#include <climits>
#include "noncopyable.h"

class SwsContext;
class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVFrame;

namespace sfl_video {
class SharedMemory;

class VideoReceiveThread : public ost::Thread {
    private:
        NON_COPYABLE(VideoReceiveThread);
        std::map<std::string, std::string> args_;
        unsigned frameNumber_;

        /*-------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. run()) only! */
        /*-------------------------------------------------------------*/

        AVCodecContext *decoderCtx_;
        AVFrame *rawFrame_;
        AVFrame *scaledPicture_;
        int streamIndex_;
        AVFormatContext *inputCtx_;
        SwsContext *imgConvertCtx_;

        int dstWidth_;
        int dstHeight_;

        SharedMemory &sharedMemory_;
        void setup();
        void createScalingContext();
        void loadSDP();

    public:
        bool receiving_;
        VideoReceiveThread(const std::map<std::string, std::string> &args,
                           SharedMemory &handle);
        virtual ~VideoReceiveThread();
        virtual void run();
};
}

#endif // _VIDEO_RECEIVE_THREAD_H_
