/*
 *  Copyright (C) 2012-2016 Savoir-faire Linux Inc.
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

#ifndef DRING_VIDEOMANAGERI_H
#define DRING_VIDEOMANAGERI_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <cstdint>
#include <cstdlib>

#include "dring.h"

namespace DRing {

/* FrameBuffer is a generic video frame container */
struct FrameBuffer {
    uint8_t* ptr {nullptr};     // data as a plain raw pointer
    std::size_t ptrSize {0};      // size in byte of ptr array
    int format {0};             // as listed by AVPixelFormat (avutils/pixfmt.h)
    int width {0};              // frame width
    int height {0};             // frame height
    std::vector<uint8_t> storage;
};

struct SinkTarget {
    using FrameBufferPtr = std::unique_ptr<FrameBuffer>;
    std::function<FrameBufferPtr(std::size_t bytes)> pull;
    std::function<void(FrameBufferPtr)> push;
};

using VideoCapabilities = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

void registerVideoHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

std::vector<std::string> getDeviceList();
VideoCapabilities getCapabilities(const std::string& name);
std::map<std::string, std::string> getSettings(const std::string& name);
void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);
void setDefaultDevice(const std::string& name);
std::string getDefaultDevice();
std::string getCurrentCodecName(const std::string& callID);
void startCamera();
void stopCamera();
bool hasCameraStarted();
bool switchInput(const std::string& resource);
bool switchToCamera();
void registerSinkTarget(const std::string& sinkId, const SinkTarget& target);

#ifdef __ANDROID__
void addVideoDevice(const std::string &node);
void removeVideoDevice(const std::string &node);
void* obtainFrame(int length);
void releaseFrame(void* frame);
#endif

// Video signal type definitions
struct VideoSignal {
        struct DeviceEvent {
                constexpr static const char* name = "DeviceEvent";
                using cb_type = void(void);
        };
        struct DecodingStarted {
                constexpr static const char* name = "DecodingStarted";
                using cb_type = void(const std::string& /*id*/, const std::string& /*shm_path*/, int /*w*/, int /*h*/, bool /*is_mixer*/id);
        };
        struct DecodingStopped {
                constexpr static const char* name = "DecodingStopped";
                using cb_type = void(const std::string& /*id*/, const std::string& /*shm_path*/, bool /*is_mixer*/);
        };
#ifdef __ANDROID__
        struct GetCameraInfo {
            constexpr static const char* name = "GetCameraInfo";
            using cb_type = void(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates);
        };
        struct SetParameters {
            constexpr static const char* name = "SetParameters";
            using cb_type = void(const std::string& device, const int format, const int width, const int height, const int rate);
        };
        struct StartCapture {
            constexpr static const char* name = "StartCapture";
            using cb_type = void(const std::string& device);
        };
        struct StopCapture {
            constexpr static const char* name = "StopCapture";
            using cb_type = void(void);
        };
#endif
};

} // namespace DRing

#endif // DRING_VIDEOMANAGERI_H
