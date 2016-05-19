/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifndef __MEDIA_ENCODER_H__
#define __MEDIA_ENCODER_H__

#include "config.h"

#ifdef RING_VIDEO
#include "video/video_base.h"
#include "video/video_scaler.h"
#endif

#include "noncopyable.h"
#include "media_buffer.h"
#include "media_device.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class AVCodecContext;
class AVStream;
class AVFormatContext;
class AVDictionary;
class AVCodec;

namespace ring {

class AudioBuffer;
class MediaIOHandle;
class MediaDescription;
class AccountCodecInfo;

class MediaEncoderException : public std::runtime_error {
    public:
        MediaEncoderException(const char *msg) : std::runtime_error(msg) {}
};

class MediaEncoder {
public:
    MediaEncoder();
    ~MediaEncoder();

    void setInterruptCallback(int (*cb)(void*), void *opaque);

    void setDeviceOptions(const DeviceParams& args);
    void openOutput(const char *filename, const MediaDescription& args);
    void startIO();
    void setIOContext(const std::unique_ptr<MediaIOHandle> &ioctx);

#ifdef RING_VIDEO
    int encode(VideoFrame &input, bool is_keyframe, int64_t frame_number);
#endif // RING_VIDEO

    int encode_audio(const AudioBuffer &input);
    int flush();
    std::string print_sdp();

    /* getWidth and getHeight return size of the encoded frame.
     * Values have meaning only after openOutput call.
     */
    int getWidth() const { return device_.width; }
    int getHeight() const { return device_.height; }

    void setMuted(bool isMuted);
    void setInitSeqVal(uint16_t seqVal);
    uint16_t getLastSeqValue();
    std::string getEncoderName() const;

    bool useCodec(const AccountCodecInfo* codec) const noexcept;

private:
    NON_COPYABLE(MediaEncoder);
    void setOptions(const MediaDescription& args);
    void setScaleDest(void *data, int width, int height, int pix_fmt);
    void prepareEncoderContext(bool is_video);
    void forcePresetX264();
    void extractProfileLevelID(const std::string &parameters, AVCodecContext *ctx);

    AVCodec *outputEncoder_ = nullptr;
    AVCodecContext *encoderCtx_ = nullptr;
    AVFormatContext *outputCtx_ = nullptr;
    AVStream *stream_ = nullptr;
    unsigned sent_samples = 0;

#ifdef RING_VIDEO
    video::VideoScaler scaler_;
    VideoFrame scaledFrame_;
#endif // RING_VIDEO

    std::vector<uint8_t> scaledFrameBuffer_;
    int scaledFrameBufferSize_ = 0;
    int streamIndex_ = -1;
#if defined(LIBAVCODEC_VERSION_MAJOR) && (LIBAVCODEC_VERSION_MAJOR < 54)
    std::vector<uint8_t> encoderBuffer_;
    int encoderBufferSize_ = 0;
#endif
    bool is_muted = false;

protected:
    AVDictionary *options_ = nullptr;
    DeviceParams device_;
    std::shared_ptr<const AccountCodecInfo> codec_;
};

} // namespace ring

#endif // __MEDIA_ENCODER_H__
