/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
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

#ifdef RING_ACCEL
#include "video/accel.h"
#endif

#include "noncopyable.h"
#include "media_buffer.h"
#include "media_codec.h"
#include "media_device.h"
#include "media_stream.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

struct AVCodecContext;
struct AVStream;
struct AVFormatContext;
struct AVDictionary;
struct AVCodec;

namespace ring {

class AudioBuffer;
class MediaIOHandle;
struct MediaDescription;
struct AccountCodecInfo;
class MediaRecorder;

class MediaEncoderException : public std::runtime_error {
    public:
        MediaEncoderException(const char *msg) : std::runtime_error(msg) {}
};

class MediaEncoder {
public:
    enum class Status {
        Success,
        FrameFinished,
        EndOfFile,
        ReadError,
        EncodeError,
        RestartRequired
    };

    MediaEncoder();
    ~MediaEncoder();

    void setInterruptCallback(int (*cb)(void*), void *opaque);

    void setDeviceOptions(const DeviceParams& args);
    void openLiveOutput(const std::string& filename, const MediaDescription& args);
    void openFileOutput(const std::string& filename, std::map<std::string, std::string> options);
    int addStream(const SystemCodecInfo& codec, std::string parameters = "");
    void startIO();
    void setIOContext(const std::unique_ptr<MediaIOHandle> &ioctx);

#ifdef RING_VIDEO
    int setupFromVideoData();
    Status encode(VideoFrame &input, bool is_keyframe, int64_t frame_number);
#endif // RING_VIDEO

    int encodeAudio(AudioFrame& frame);

    // frame should be ready to be sent to the encoder at this point
    Status encode(AVFrame* frame, int streamIdx);

    Status flush();
    std::string print_sdp();

    /* getWidth and getHeight return size of the encoded frame.
     * Values have meaning only after openLiveOutput call.
     */
    int getWidth() const { return device_.width; }
    int getHeight() const { return device_.height; }

    void setMuted(bool isMuted);
    void setInitSeqVal(uint16_t seqVal);
    uint16_t getLastSeqValue();
    std::string getEncoderName() const;

    bool useCodec(const AccountCodecInfo* codec) const noexcept;

#ifdef RING_ACCEL
    void enableAccel(bool enableAccel);
#endif
    unsigned getStreamCount() const;
    MediaStream getStream(const std::string& name, int streamIdx = -1) const;

private:
    NON_COPYABLE(MediaEncoder);
    void setOptions(const MediaDescription& args);
    void setScaleDest(void *data, int width, int height, int pix_fmt);
    AVCodecContext* prepareEncoderContext(AVCodec* outputCodec, bool is_video);
    void forcePresetX264(AVCodecContext* encoderCtx);
    void extractProfileLevelID(const std::string &parameters, AVCodecContext *ctx);

    std::vector<AVCodecContext*> encoders_;
    AVFormatContext *outputCtx_ = nullptr;
    int currentStreamIdx_ = -1;
    unsigned sent_samples = 0;

    bool fallback_ = false;

#ifdef RING_VIDEO
    video::VideoScaler scaler_;
    VideoFrame scaledFrame_;
#endif // RING_VIDEO

#ifdef RING_ACCEL
    bool enableAccel_ = true;
    video::HardwareAccel accel_;
    unsigned short accelFailures_ = 0;
#endif

    std::vector<uint8_t> scaledFrameBuffer_;
    int scaledFrameBufferSize_ = 0;
    bool is_muted = false;

protected:
    AVDictionary *options_ = nullptr;
    DeviceParams device_;
    std::shared_ptr<const AccountCodecInfo> codec_;
};

} // namespace ring

#endif // __MEDIA_ENCODER_H__
