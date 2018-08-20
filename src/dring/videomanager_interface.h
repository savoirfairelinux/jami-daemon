/*
 *  Copyright (C) 2012-2018 Savoir-faire Linux Inc.
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

#include "def.h"

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <cstdint>
#include <cstdlib>

#include "dring.h"

#if __APPLE__
#import "TargetConditionals.h"
#endif

namespace DRing {

DRING_PUBLIC [[deprecated("Replaced by registerSignalHandlers")]]
void registerVideoHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

/* FrameBuffer is a generic video frame container */
DRING_PUBLIC struct FrameBuffer {
    uint8_t* ptr {nullptr};     // data as a plain raw pointer
    std::size_t ptrSize {0};      // size in byte of ptr array
    int format {0};             // as listed by AVPixelFormat (avutils/pixfmt.h)
    int width {0};              // frame width
    int height {0};             // frame height
    std::vector<uint8_t> storage;
};

DRING_PUBLIC struct SinkTarget {
    using FrameBufferPtr = std::unique_ptr<FrameBuffer>;
    std::function<FrameBufferPtr(std::size_t bytes)> pull;
    std::function<void(FrameBufferPtr)> push;
};

using VideoCapabilities = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

DRING_PUBLIC std::vector<std::string> getDeviceList();
DRING_PUBLIC VideoCapabilities getCapabilities(const std::string& name);
DRING_PUBLIC std::map<std::string, std::string> getSettings(const std::string& name);
DRING_PUBLIC void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);
DRING_PUBLIC void setDefaultDevice(const std::string& name);

DRING_PUBLIC std::map<std::string, std::string> getDeviceParams(const std::string& name);

DRING_PUBLIC std::string getDefaultDevice();
DRING_PUBLIC std::string getCurrentCodecName(const std::string& callID);
DRING_PUBLIC void startCamera();
DRING_PUBLIC void stopCamera();
DRING_PUBLIC bool hasCameraStarted();
DRING_PUBLIC bool switchInput(const std::string& resource);
DRING_PUBLIC bool switchToCamera();
DRING_PUBLIC void registerSinkTarget(const std::string& sinkId, const SinkTarget& target);

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
DRING_PUBLIC void addVideoDevice(const std::string &node, const std::vector<std::map<std::string, std::string>>* devInfo=nullptr);
DRING_PUBLIC void removeVideoDevice(const std::string &node);
DRING_PUBLIC void* obtainFrame(int length);
DRING_PUBLIC void releaseFrame(void* frame);
#endif

DRING_PUBLIC bool getDecodingAccelerated();
DRING_PUBLIC void setDecodingAccelerated(bool state);

// Video signal type definitions
DRING_PUBLIC struct VideoSignal {
        DRING_PUBLIC struct DeviceEvent {
                constexpr static const char* name = "DeviceEvent";
                using cb_type = void(void);
        };
        DRING_PUBLIC struct DecodingStarted {
                constexpr static const char* name = "DecodingStarted";
                using cb_type = void(const std::string& /*id*/, const std::string& /*shm_path*/, int /*w*/, int /*h*/, bool /*is_mixer*/id);
        };
        DRING_PUBLIC struct DecodingStopped {
                constexpr static const char* name = "DecodingStopped";
                using cb_type = void(const std::string& /*id*/, const std::string& /*shm_path*/, bool /*is_mixer*/);
        };
#if __ANDROID__
        DRING_PUBLIC struct SetParameters {
            constexpr static const char* name = "SetParameters";
            using cb_type = void(const std::string& device, const int format, const int width, const int height, const int rate);
        };
        DRING_PUBLIC struct GetCameraInfo {
            constexpr static const char* name = "GetCameraInfo";
            using cb_type = void(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates);
        };
#endif
        DRING_PUBLIC struct StartCapture {
            constexpr static const char* name = "StartCapture";
            using cb_type = void(const std::string& /*device*/);
        };
        DRING_PUBLIC struct StopCapture {
            constexpr static const char* name = "StopCapture";
            using cb_type = void(void);
        };
        DRING_PUBLIC struct DeviceAdded {
            constexpr static const char* name = "DeviceAdded";
            using cb_type = void(const std::string& /*device*/);
        };
        DRING_PUBLIC struct ParametersChanged {
            constexpr static const char* name = "ParametersChanged";
            using cb_type = void(const std::string& /*device*/);
        };
};

} // namespace DRing

#endif // DRING_VIDEOMANAGERI_H
