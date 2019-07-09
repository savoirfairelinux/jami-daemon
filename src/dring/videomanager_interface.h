/*
 *  Copyright (C) 2012-2019 Savoir-faire Linux Inc.
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

#include "dring.h"

extern "C" {
struct AVFrame;
struct AVPacket;
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

#if __APPLE__
#import "TargetConditionals.h"
#endif

namespace jami {
struct AudioFormat;
}

namespace DRing {

[[deprecated("Replaced by registerSignalHandlers")]] DRING_PUBLIC
void registerVideoHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

/* FrameBuffer is a generic video frame container */
struct DRING_PUBLIC FrameBuffer {
    uint8_t* ptr {nullptr};     // data as a plain raw pointer
    std::size_t ptrSize {0};      // size in byte of ptr array
    int format {0};             // as listed by AVPixelFormat (avutils/pixfmt.h)
    int width {0};              // frame width
    int height {0};             // frame height
    std::vector<uint8_t> storage;
};

struct DRING_PUBLIC SinkTarget {
    using FrameBufferPtr = std::unique_ptr<FrameBuffer>;
    std::function<FrameBufferPtr(std::size_t bytes)> pull;
    std::function<void(FrameBufferPtr)> push;
};

class DRING_PUBLIC MediaFrame {
public:
    // Construct an empty MediaFrame
    MediaFrame();
    MediaFrame(const MediaFrame&) = delete;
    MediaFrame& operator =(const MediaFrame& o) = delete;
    MediaFrame(MediaFrame&& o) = delete;
    MediaFrame& operator =(MediaFrame&& o) = delete;

    virtual ~MediaFrame() = default;

    // Return a pointer on underlaying buffer
    const AVFrame* pointer() const noexcept { return frame_.get(); }
    AVFrame* pointer() noexcept { return frame_.get(); }
    AVPacket* packet() const noexcept { return packet_.get(); }

    // Fill this MediaFrame with data from o
    void copyFrom(const MediaFrame& o);
    void setPacket(std::unique_ptr<AVPacket, void(*)(AVPacket*)>&& pkt);

    // Reset internal buffers (return to an empty MediaFrame)
    virtual void reset() noexcept;

    std::unique_ptr<AVFrame, void(*)(AVFrame*)> getFrame() {
        return std::move(frame_);
    }

protected:
    std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame_;
    std::unique_ptr<AVPacket, void(*)(AVPacket*)> packet_;
};

class DRING_PUBLIC AudioFrame : public MediaFrame {
public:
    AudioFrame() : MediaFrame() {}
    AudioFrame(const jami::AudioFormat& format, size_t nb_samples = 0);
    ~AudioFrame() {};
    void mix(const AudioFrame& o);
    float calcRMS() const;

private:
    void setFormat(const jami::AudioFormat& format);
    void reserve(size_t nb_samples = 0);
};

class DRING_PUBLIC VideoFrame : public MediaFrame {
public:
    // Construct an empty VideoFrame
    VideoFrame() : MediaFrame() {}
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

    // Set internal pixel buffers on given memory buffer
    // This buffer must follow given specifications.
    void setFromMemory(uint8_t* data, int format, int width, int height) noexcept;
    void setFromMemory(uint8_t* data, int format, int width, int height, const std::function<void(uint8_t*)>& cb) noexcept;
    void setReleaseCb(const std::function<void(uint8_t*)>& cb) noexcept;

    void noise();

private:
    std::function<void(uint8_t*)> releaseBufferCb_ {};
    uint8_t* ptr_ {nullptr};
    bool allocated_ {false};
    void setGeometry(int format, int width, int height) noexcept;
};

struct DRING_PUBLIC AVSinkTarget {
    std::function<void(std::unique_ptr<VideoFrame>)> push;
    int /* AVPixelFormat */ preferredFormat {-1 /* AV_PIX_FMT_NONE */};
};

using VideoCapabilities = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

DRING_PUBLIC std::vector<std::string> getDeviceList();
DRING_PUBLIC VideoCapabilities getCapabilities(const std::string& name);
DRING_PUBLIC std::map<std::string, std::string> getSettings(const std::string& name);
DRING_PUBLIC void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);
DRING_PUBLIC void setDefaultDevice(const std::string& name);
DRING_PUBLIC void setDeviceOrientation(const std::string& name, int angle);

DRING_PUBLIC std::map<std::string, std::string> getDeviceParams(const std::string& name);

DRING_PUBLIC std::string getDefaultDevice();
DRING_PUBLIC std::string getCurrentCodecName(const std::string& callID);
DRING_PUBLIC void startCamera();
DRING_PUBLIC void stopCamera();
DRING_PUBLIC bool hasCameraStarted();
DRING_PUBLIC void startAudioDevice();
DRING_PUBLIC void stopAudioDevice();
DRING_PUBLIC bool switchInput(const std::string& resource);
DRING_PUBLIC bool switchToCamera();
DRING_PUBLIC void registerSinkTarget(const std::string& sinkId, const SinkTarget& target);
DRING_PUBLIC void registerAVSinkTarget(const std::string& sinkId, const AVSinkTarget& target);
DRING_PUBLIC std::map<std::string, std::string> getRenderer(const std::string& callId);

DRING_PUBLIC std::string startLocalRecorder(const bool& audioOnly, const std::string& filepath);
DRING_PUBLIC void stopLocalRecorder(const std::string& filepath);

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
DRING_PUBLIC void addVideoDevice(const std::string &node, const std::vector<std::map<std::string, std::string>>* devInfo=nullptr);
DRING_PUBLIC void removeVideoDevice(const std::string &node);
DRING_PUBLIC void* obtainFrame(int length);
DRING_PUBLIC void releaseFrame(void* frame);

DRING_PUBLIC VideoFrame* getNewFrame();
DRING_PUBLIC void publishFrame();
#endif

DRING_PUBLIC bool getDecodingAccelerated();
DRING_PUBLIC void setDecodingAccelerated(bool state);
DRING_PUBLIC bool getEncodingAccelerated();
DRING_PUBLIC void setEncodingAccelerated(bool state);

// Video signal type definitions
struct DRING_PUBLIC VideoSignal {
        struct DRING_PUBLIC DeviceEvent {
                constexpr static const char* name = "DeviceEvent";
                using cb_type = void(void);
        };
        struct DRING_PUBLIC DecodingStarted {
                constexpr static const char* name = "DecodingStarted";
                using cb_type = void(const std::string& /*id*/, const std::string& /*shm_path*/, int /*w*/, int /*h*/, bool /*is_mixer*/id);
        };
        struct DRING_PUBLIC DecodingStopped {
                constexpr static const char* name = "DecodingStopped";
                using cb_type = void(const std::string& /*id*/, const std::string& /*shm_path*/, bool /*is_mixer*/);
        };
#if __ANDROID__
        struct DRING_PUBLIC SetParameters {
            constexpr static const char* name = "SetParameters";
            using cb_type = void(const std::string& device, const int format, const int width, const int height, const int rate);
        };
        struct DRING_PUBLIC GetCameraInfo {
            constexpr static const char* name = "GetCameraInfo";
            using cb_type = void(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates);
        };
        struct DRING_PUBLIC RequestKeyFrame {
            constexpr static const char* name = "RequestKeyFrame";
            using cb_type = void();
        };
#endif
        struct DRING_PUBLIC StartCapture {
            constexpr static const char* name = "StartCapture";
            using cb_type = void(const std::string& /*device*/);
        };
        struct DRING_PUBLIC StopCapture {
            constexpr static const char* name = "StopCapture";
            using cb_type = void(void);
        };
        struct DRING_PUBLIC DeviceAdded {
            constexpr static const char* name = "DeviceAdded";
            using cb_type = void(const std::string& /*device*/);
        };
        struct DRING_PUBLIC ParametersChanged {
            constexpr static const char* name = "ParametersChanged";
            using cb_type = void(const std::string& /*device*/);
        };
};

} // namespace DRing

#endif // DENABLE_VIDEOMANAGERI_H
