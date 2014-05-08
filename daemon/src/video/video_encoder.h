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

#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

#include "video_base.h"
#include "video_scaler.h"
#include "noncopyable.h"

#include <map>
#include <string>

class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVCodec;

namespace sfl_video {

class VideoEncoderException : public std::runtime_error {
    public:
        VideoEncoderException(const char *msg) : std::runtime_error(msg) {}
};

class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();

    void setOptions(const std::map<std::string, std::string>& options);

    void setInterruptCallback(int (*cb)(void*), void *opaque);
    void setIOContext(const std::unique_ptr<VideoIOHandle> &ioctx);
    void openOutput(const char *enc_name, const char *short_name,
                   const char *filename, const char *mime_type);
    void startIO();
    int encode(VideoFrame &input, bool is_keyframe, int64_t frame_number);
    int flush();
    void print_sdp(std::string &sdp_);

    /* getWidth and getHeight return size of the encoded frame.
     * Values have meaning only after openOutput call.
     */
    int getWidth() const { return dstWidth_; }
    int getHeight() const { return dstHeight_; }

private:
    NON_COPYABLE(VideoEncoder);
    void setScaleDest(void *data, int width, int height, int pix_fmt);
    void prepareEncoderContext();
    void forcePresetX264();
    void extractProfileLevelID(const std::string &parameters, AVCodecContext *ctx);

    AVCodec *outputEncoder_ = nullptr;
    AVCodecContext *encoderCtx_ = nullptr;
    AVFormatContext *outputCtx_ = nullptr;
    AVStream *stream_ = nullptr;
    VideoScaler scaler_;
    VideoFrame scaledFrame_;

    uint8_t *scaledFrameBuffer_ = nullptr;
    int scaledFrameBufferSize_ = 0;
    int streamIndex_ = -1;
    int dstWidth_ = 0;
    int dstHeight_ = 0;
#if (LIBAVCODEC_VERSION_MAJOR < 54)
    uint8_t *encoderBuffer_ = nullptr;
    int encoderBufferSize_ = 0;
#endif

protected:
    AVDictionary *options_ = nullptr;
};

}

#endif // __VIDEO_ENCODER_H__
