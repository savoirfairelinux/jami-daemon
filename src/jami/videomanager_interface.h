/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#ifndef DENABLE_VIDEOMANAGERI_H
#define DENABLE_VIDEOMANAGERI_H

#include "jami.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

extern "C" {
struct AVFrame;
struct AVPacket;
void av_frame_free(AVFrame** frame);
void av_packet_free(AVPacket** frame);
}

#include "def.h"

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <chrono>
#include <cstdint>
#include <cstdlib>

#ifdef __APPLE__
#import "TargetConditionals.h"
#endif

namespace jami {
struct AudioFormat;
}

namespace libjami {

[[deprecated("Replaced by registerSignalHandlers")]] LIBJAMI_PUBLIC void registerVideoHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

struct LIBJAMI_PUBLIC AVFrame_deleter
{
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

typedef std::unique_ptr<AVFrame, AVFrame_deleter> FrameBuffer;

struct LIBJAMI_PUBLIC AVPacket_deleter
{
    void operator()(AVPacket* pkt) const { av_packet_free(&pkt); }
};

typedef std::unique_ptr<AVPacket, AVPacket_deleter> PacketBuffer;

class LIBJAMI_PUBLIC MediaFrame
{
public:
    // Construct an empty MediaFrame
    MediaFrame();
    MediaFrame(const MediaFrame&) = delete;
    MediaFrame& operator=(const MediaFrame& o) = delete;
    MediaFrame(MediaFrame&& o) = delete;
    MediaFrame& operator=(MediaFrame&& o) = delete;

    virtual ~MediaFrame() = default;

    // Return a pointer on underlaying buffer
    const AVFrame* pointer() const noexcept { return frame_.get(); }
    AVFrame* pointer() noexcept { return frame_.get(); }
    AVPacket* packet() const noexcept { return packet_.get(); }

    // Fill this MediaFrame with data from o
    void copyFrom(const MediaFrame& o);
    void setPacket(PacketBuffer&& pkt);

    // Reset internal buffers (return to an empty MediaFrame)
    virtual void reset() noexcept;

    FrameBuffer getFrame() { return std::move(frame_); }

protected:
    FrameBuffer frame_;
    PacketBuffer packet_;
};

class LIBJAMI_PUBLIC AudioFrame : public MediaFrame
{
public:
    AudioFrame()
        : MediaFrame()
    {}
    AudioFrame(const jami::AudioFormat& format, size_t nb_samples = 0);
    ~AudioFrame() {};
    void mix(const AudioFrame& o);
    float calcRMS() const;
    jami::AudioFormat getFormat() const;
    size_t getFrameSize() const;
    bool has_voice {false};

private:
    void setFormat(const jami::AudioFormat& format);
    void reserve(size_t nb_samples = 0);
};

class LIBJAMI_PUBLIC VideoFrame : public MediaFrame
{
public:
    // Construct an empty VideoFrame
    VideoFrame()
        : MediaFrame()
    {}
    ~VideoFrame();

    // Reset internal buffers (return to an empty VideoFrame)
    void reset() noexcept override;

    // Fill this VideoFrame with data from o
    void copyFrom(const VideoFrame& o);

    // Return frame size in bytes
    std::size_t size() const noexcept;

    // Return pixel format
    int format() const noexcept;

    // Return frame width in pixels
    int width() const noexcept;

    // Return frame height in pixels
    int height() const noexcept;

    // Allocate internal pixel buffers following given specifications
    void reserve(int format, int width, int height);

    // Return orientation (in degrees) stored in the frame metadata, or 0 by default.
    int getOrientation() const;

    // Set internal pixel buffers on given memory buffer
    // This buffer must follow given specifications.
    void setFromMemory(uint8_t* data, int format, int width, int height) noexcept;
    void setFromMemory(uint8_t* data,
                       int format,
                       int width,
                       int height,
                       const std::function<void(uint8_t*)>& cb) noexcept;
    void setReleaseCb(const std::function<void(uint8_t*)>& cb) noexcept;

    void noise();

private:
    std::function<void(uint8_t*)> releaseBufferCb_ {};
    uint8_t* ptr_ {nullptr};
    bool allocated_ {false};
    void setGeometry(int format, int width, int height) noexcept;
};

struct LIBJAMI_PUBLIC SinkTarget
{
    std::function<FrameBuffer()> pull;
    std::function<void(FrameBuffer)> push;
    int /* AVPixelFormat */ preferredFormat {-1 /* AV_PIX_FMT_NONE */};
};

using VideoCapabilities = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

LIBJAMI_PUBLIC std::vector<std::string> getDeviceList();
LIBJAMI_PUBLIC VideoCapabilities getCapabilities(const std::string& deviceId);
LIBJAMI_PUBLIC std::map<std::string, std::string> getSettings(const std::string& deviceId);
LIBJAMI_PUBLIC void applySettings(const std::string& deviceId,
                                  const std::map<std::string, std::string>& settings);
LIBJAMI_PUBLIC void setDefaultDevice(const std::string& deviceId);
LIBJAMI_PUBLIC void setDeviceOrientation(const std::string& deviceId, int angle);
LIBJAMI_PUBLIC std::map<std::string, std::string> getDeviceParams(const std::string& deviceId);
LIBJAMI_PUBLIC std::string getDefaultDevice();
LIBJAMI_PUBLIC void startAudioDevice();
LIBJAMI_PUBLIC void stopAudioDevice();

LIBJAMI_PUBLIC std::string openVideoInput(const std::string& path);
LIBJAMI_PUBLIC bool closeVideoInput(const std::string& id);

LIBJAMI_PUBLIC std::string createMediaPlayer(const std::string& path);
LIBJAMI_PUBLIC bool closeMediaPlayer(const std::string& id);
LIBJAMI_PUBLIC bool pausePlayer(const std::string& id, const bool& pause);
LIBJAMI_PUBLIC bool mutePlayerAudio(const std::string& id, const bool& mute);
LIBJAMI_PUBLIC bool playerSeekToTime(const std::string& id, const int& time);
LIBJAMI_PUBLIC int64_t getPlayerPosition(const std::string& id);
LIBJAMI_PUBLIC int64_t getPlayerDuration(const std::string& id);
LIBJAMI_PUBLIC void setAutoRestart(const std::string& id, const bool& restart);

LIBJAMI_PUBLIC bool registerSinkTarget(const std::string& sinkId, SinkTarget target);
#ifdef ENABLE_SHM
LIBJAMI_PUBLIC void startShmSink(const std::string& sinkId, bool value);
#endif
LIBJAMI_PUBLIC std::map<std::string, std::string> getRenderer(const std::string& callId);

LIBJAMI_PUBLIC std::string startLocalMediaRecorder(const std::string& videoInputId,
                                                   const std::string& filepath);
LIBJAMI_PUBLIC void stopLocalRecorder(const std::string& filepath);

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
LIBJAMI_PUBLIC void addVideoDevice(
    const std::string& node, const std::vector<std::map<std::string, std::string>>& devInfo = {});
LIBJAMI_PUBLIC void removeVideoDevice(const std::string& node);
LIBJAMI_PUBLIC VideoFrame* getNewFrame(std::string_view id);
LIBJAMI_PUBLIC void publishFrame(std::string_view id);
#endif

LIBJAMI_PUBLIC bool getDecodingAccelerated();
LIBJAMI_PUBLIC void setDecodingAccelerated(bool state);
LIBJAMI_PUBLIC bool getEncodingAccelerated();
LIBJAMI_PUBLIC void setEncodingAccelerated(bool state);

// player signal type definitions
struct LIBJAMI_PUBLIC MediaPlayerSignal
{
    struct LIBJAMI_PUBLIC FileOpened
    {
        constexpr static const char* name = "FileOpened";
        using cb_type = void(const std::string& /*playerId*/,
                             std::map<std::string, std::string> /*playerInfo*/);
    };
};

// Video signal type definitions
struct LIBJAMI_PUBLIC VideoSignal
{
    struct LIBJAMI_PUBLIC DeviceEvent
    {
        constexpr static const char* name = "DeviceEvent";
        using cb_type = void(void);
    };
    struct LIBJAMI_PUBLIC DecodingStarted
    {
        constexpr static const char* name = "DecodingStarted";
        using cb_type = void(const std::string& /*id*/,
                             const std::string& /*shm_path*/,
                             int /*w*/,
                             int /*h*/,
                             bool /*is_mixer*/ id);
    };
    struct LIBJAMI_PUBLIC DecodingStopped
    {
        constexpr static const char* name = "DecodingStopped";
        using cb_type = void(const std::string& /*id*/,
                             const std::string& /*shm_path*/,
                             bool /*is_mixer*/);
    };
#ifdef __ANDROID__
    struct LIBJAMI_PUBLIC SetParameters
    {
        constexpr static const char* name = "SetParameters";
        using cb_type = void(const std::string& device,
                             const int format,
                             const int width,
                             const int height,
                             const int rate);
    };
    struct LIBJAMI_PUBLIC GetCameraInfo
    {
        constexpr static const char* name = "GetCameraInfo";
        using cb_type = void(const std::string& device,
                             std::vector<int>* formats,
                             std::vector<unsigned>* sizes,
                             std::vector<unsigned>* rates);
    };
    struct LIBJAMI_PUBLIC RequestKeyFrame
    {
        constexpr static const char* name = "RequestKeyFrame";
        using cb_type = void(const std::string& /*device*/);
    };
    struct LIBJAMI_PUBLIC SetBitrate
    {
        constexpr static const char* name = "SetBitrate";
        using cb_type = void(const std::string& /*device*/, const int bitrate);
    };
#endif
    struct LIBJAMI_PUBLIC StartCapture
    {
        constexpr static const char* name = "StartCapture";
        using cb_type = void(const std::string& /*device*/);
    };
    struct LIBJAMI_PUBLIC StopCapture
    {
        constexpr static const char* name = "StopCapture";
        using cb_type = void(const std::string& /*device*/);
    };
    struct LIBJAMI_PUBLIC DeviceAdded
    {
        constexpr static const char* name = "DeviceAdded";
        using cb_type = void(const std::string& /*device*/);
    };
    struct LIBJAMI_PUBLIC ParametersChanged
    {
        constexpr static const char* name = "ParametersChanged";
        using cb_type = void(const std::string& /*device*/);
    };
};

} // namespace libjami

#endif // DENABLE_VIDEOMANAGERI_H
