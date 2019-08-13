/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#ifdef ENABLE_VIDEO
#include "video/video_base.h"
#include "video/video_scaler.h"
#endif

#include "noncopyable.h"
#include "media_buffer.h"
#include "media_codec.h"
#include "media_stream.h"

#include <map>
#include <memory>
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
struct AccountCodecInfo;

#ifdef RING_ACCEL
namespace video {
class HardwareAccel;
}
#endif

class MediaEncoderException : public std::runtime_error {
    public:
        MediaEncoderException(const char *msg) : std::runtime_error(msg) {}
};

class MediaEncoder {
public:
    MediaEncoder();
    ~MediaEncoder();

    void openOutput(const std::string& filename, const std::string& format="");
    void setMetadata(const std::string& title, const std::string& description);
    void setOptions(const MediaStream& opts);
    void setOptions(const MediaDescription& args);
    int addStream(const SystemCodecInfo& codec);
    void setIOContext(AVIOContext* ioctx) { ioCtx_ = ioctx; }

    bool send(AVPacket& packet, int streamIdx = -1);

#ifdef ENABLE_VIDEO
    int encode(VideoFrame &input, bool is_keyframe, int64_t frame_number);
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

    const std::string& getAudioCodec() const { return audioCodec_; }
    const std::string& getVideoCodec() const { return videoCodec_; }

    void setBitrate(uint64_t br);

#ifdef RING_ACCEL
    void enableAccel(bool enableAccel);
#endif

    unsigned getStreamCount() const;
    MediaStream getStream(const std::string& name, int streamIdx = -1) const;

private:
    NON_COPYABLE(MediaEncoder);
    AVCodecContext* prepareEncoderContext(AVCodec* outputCodec, bool is_video);
    void forcePresetX264(AVCodecContext* encoderCtx);
    void extractProfileLevelID(const std::string &parameters, AVCodecContext *ctx);
    int initStream(const std::string& codecName, AVBufferRef* framesCtx, int format);
    int initStream(const SystemCodecInfo& systemCodecInfo, AVBufferRef* framesCtx, int format);
    void openIOContext();
    void startIO();
    AVCodecContext* getCurrentVideoAVCtx();
    void stopEncoder();
    AVCodecContext* initCodec(AVMediaType mediaType, AVCodecID avcodecId, AVBufferRef* framesCtx, uint64_t br, int format);

    std::vector<AVCodecContext*> encoders_;
    AVFormatContext *outputCtx_ = nullptr;
    AVIOContext* ioCtx_ = nullptr;
    int currentStreamIdx_ = -1;
    unsigned sent_samples = 0;
    bool initialized_ {false};
    bool fileIO_ {false};
    unsigned int currentVideoCodecID_ {0};
    AVCodec* outputCodec_ = nullptr;
    std::mutex encMutex_;

    void initH264(AVCodecContext* encoderCtx, uint64_t br);
    void initVP8(AVCodecContext* encoderCtx, uint64_t br);
    void initMPEG4(AVCodecContext* encoderCtx, uint64_t br);
    void initH263(AVCodecContext* encoderCtx, uint64_t br);

#ifdef ENABLE_VIDEO
    video::VideoScaler scaler_;
    VideoFrame scaledFrame_;
#endif // ENABLE_VIDEO

    std::vector<uint8_t> scaledFrameBuffer_;
    int scaledFrameBufferSize_ = 0;

#ifdef RING_ACCEL
    bool enableAccel_ = true;
    std::unique_ptr<video::HardwareAccel> accel_;
#endif

protected:
    void readConfig(AVDictionary** dict, AVCodecContext* encoderCtx);
    AVDictionary* options_ = nullptr;
    MediaStream videoOpts_;
    MediaStream audioOpts_;
    std::string videoCodec_;
    std::string audioCodec_;
};

} // namespace jami

#endif // __MEDIA_ENCODER_H__
