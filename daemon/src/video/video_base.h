/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
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

#ifndef _VIDEO_FRAME_H_
#define _VIDEO_FRAME_H_

#include "noncopyable.h"

#include <cstdlib>
#include <cstdint>


class AVFrame;
class AVPacket;
class AVDictionary;

#ifndef AVFORMAT_AVIO_H
class AVIOContext;
#endif

enum VideoPixelFormat {
    VIDEO_PIXFMT_BGRA = -1,
};

namespace sfl_video {
    typedef int(*io_readcallback)(void *opaque, uint8_t *buf, int buf_size);
    typedef int(*io_writecallback)(void *opaque, uint8_t *buf, int buf_size);
    typedef int64_t(*io_seekcallback)(void *opaque, int64_t offset, int whence);

    class VideoPacket {
    public:
        VideoPacket();
        ~VideoPacket();
        AVPacket* get() { return packet_; };

    private:
        NON_COPYABLE(VideoPacket);
        AVPacket *packet_;
    };

    class VideoIOHandle {
    public:
        VideoIOHandle(ssize_t buffer_size,
                      bool writeable,
                      io_readcallback read_cb,
                      io_writecallback write_cb,
                      io_seekcallback seek_cb,
                      void *opaque);
        ~VideoIOHandle();

        AVIOContext* get() { return ctx_; }

    private:
        NON_COPYABLE(VideoIOHandle);
        AVIOContext *ctx_;
        unsigned char *buf_;
    };

    class VideoCodec {
    public:
        VideoCodec();
        virtual ~VideoCodec() {};

        void setOption(const char *name, const char *value);

    private:
        NON_COPYABLE(VideoCodec);

    protected:
        AVDictionary *options_;
    };

    class VideoFrame {
    public:
        VideoFrame();
        ~VideoFrame();

        AVFrame* get() { return frame_; };
        void setGeometry(int width, int height, int pix_fmt);
        void setDestination(void *data);
        size_t getSize();

    private:
        NON_COPYABLE(VideoFrame);
        AVFrame *frame_;
    };
}

#endif // _VIDEO_BASE_H_
