/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_VIDEO
#include "video/video_base.h"
#include "video/video_scaler.h"
#endif

#include "noncopyable.h"
#include "media_buffer.h"
#include "media_codec.h"
#include "media_stream.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

extern "C" {
struct AVCodecContext;
struct AVFormatContext;
struct AVDictionary;
struct AVCodec;
}

namespace jami {

struct MediaDescription;

#ifdef ENABLE_HWACCEL
namespace video {
class HardwareAccel;
}
#endif

class MediaEncoderException : public std::runtime_error
{
public:
    MediaEncoderException(const char* msg)
        : std::runtime_error(msg)
    {}
    MediaEncoderException(const std::string& msg)
        : std::runtime_error(msg)
    {}
};

class MediaEncoder
{
public:
    MediaEncoder();
    ~MediaEncoder();

    void setVideoPassthrough(bool passthrough);
    void openOutput(const std::string& filename, const std::string& format = "");
    void setMetadata(const std::string& title, const std::string& description);
    void setOptions(const MediaStream& opts);
    void setOptions(const MediaDescription& args);
    int addStream(const SystemCodecInfo& codec);
    void setIOContext(AVIOContext* ioctx) { ioCtx_ = ioctx; }
    void resetStreams(int width, int height);

    bool send(AVPacket& packet, int streamIdx = -1);

#ifdef ENABLE_VIDEO
    int encode(const std::shared_ptr<VideoFrame>& input, bool is_keyframe, int64_t frame_number);
#endif // ENABLE_VIDEO

    int encodeAudio(AudioFrame& frame);

    // frame should be ready to be sent to the encoder at this point
    int encode(AVFrame* frame, int streamIdx);

    int flush();
    std::string print_sdp();

    /* getWidth and getHeight return size of the encoded frame.
     * Values have meaning only after openLiveOutput call.
     */
    int getWidth() const { return videoOpts_.width; };
    int getHeight() const { return videoOpts_.height; };

    void setInitSeqVal(uint16_t seqVal);
    uint16_t getLastSeqValue();

    int setBitrate(uint64_t br);
    int setPacketLoss(uint64_t pl);

#ifdef ENABLE_HWACCEL
    void enableAccel(bool enableAccel);
#endif

    static std::string testH265Accel();

    /**
     * Parse the H.264 profile-level-id from SDP fmtp parameters (RFC 6184)
     * and configure the codec context profile and level accordingly.
     * Defaults to Constrained Baseline level 1.3 when absent.
     */
    static void extractProfileLevelID(const std::string& parameters, AVCodecContext* ctx);

    /**
     * Parse the H.265 profile parameters from SDP fmtp parameters
     * (RFC 7798) and configure the codec context profile and level
     * accordingly. Defaults to the Main profile, level 3.1, when absent.
     */
    static void extractH265Profile(const std::string& parameters, AVCodecContext* ctx);

    unsigned getStreamCount() const;
    MediaStream getStream(const std::string& name, int streamIdx = -1) const;

    int getCurrentAudioAVCtxFrameSize();

private:
    NON_COPYABLE(MediaEncoder);
    AVCodecContext* prepareEncoderContext(const AVCodec* outputCodec, bool is_video);
    void forcePresetX2645(AVCodecContext* encoderCtx);
    int initVideoStream(AVBufferRef* framesCtx = {});
    int initStream(const SystemCodecInfo& systemCodecInfo, AVBufferRef* framesCtx = {});
    void openIOContext();
    void startIO();
    AVCodecContext* getCurrentVideoAVCtx();
    AVCodecContext* getCurrentAudioAVCtx();
    void stopEncoder();
    AVCodecContext* initCodec(AVMediaType mediaType, AVCodecID avcodecId, uint64_t br);
    void initH264(AVCodecContext* encoderCtx, uint64_t br);
    void initH265(AVCodecContext* encoderCtx, uint64_t br);
    void initVP8(AVCodecContext* encoderCtx, uint64_t br);
    void initMPEG4(AVCodecContext* encoderCtx, uint64_t br);
    void initH263(AVCodecContext* encoderCtx, uint64_t br);
    void initOpus(AVCodecContext* encoderCtx);
    bool isDynBitrateSupported(AVCodecID codecid);
    bool isDynPacketLossSupported(AVCodecID codecid);
    void initAccel(AVCodecContext* encoderCtx, uint64_t br);
    void updatePassthroughVideoTimestamp(AVPacket& pkt);
#ifdef ENABLE_VIDEO
    int getHWFrame(const std::shared_ptr<VideoFrame>& input, std::shared_ptr<VideoFrame>& output);
    std::shared_ptr<VideoFrame> getUnlinkedHWFrame(const VideoFrame& input);
    std::shared_ptr<VideoFrame> getHWFrameFromSWFrame(const VideoFrame& input);
    std::shared_ptr<VideoFrame> getScaledSWFrame(const VideoFrame& input);
#endif

    std::vector<AVCodecContext*> encoders_;
    AVFormatContext* outputCtx_ = nullptr;
    AVIOContext* ioCtx_ = nullptr;
    int currentStreamIdx_ = -1;
    unsigned sent_samples = 0;
    bool initialized_ {false};
    bool fileIO_ {false};
    const AVCodec* outputCodec_ = nullptr;
    std::mutex encMutex_;
    bool linkableHW_ {false};
    RateMode mode_ {RateMode::CRF_CONSTRAINED};
    bool fecEnabled_ {false};
    bool videoPassthrough_ {false};
    int64_t passthroughVideoStartUs_ = 0;
    int64_t passthroughVideoLastPts_ = 0;
    bool passthroughVideoClockStarted_ {false};
    bool passthroughVideoLastPtsValid_ {false};
    AVFormatContext* passthroughVideoTimestampContext_ = nullptr;

#ifdef ENABLE_VIDEO
    video::VideoScaler scaler_;
    std::shared_ptr<VideoFrame> scaledFrame_;
#endif // ENABLE_VIDEO

    std::vector<uint8_t> scaledFrameBuffer_;
    int scaledFrameBufferSize_ = 0;

#ifdef ENABLE_HWACCEL
    bool enableAccel_ {false};
    std::unique_ptr<video::HardwareAccel> accel_;
#endif

protected:
    void readConfig(AVCodecContext* encoderCtx);
    AVDictionary* options_ = nullptr;
    MediaStream videoOpts_;
    MediaStream audioOpts_;
    std::optional<SystemCodecInfo> videoCodecInfo_ {};
};

} // namespace jami
