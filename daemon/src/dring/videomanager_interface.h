/*
 *  Copyright (C) 2012-2014 Savoir-Faire Linux Inc.
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

#ifndef VIDEOMANAGERI_H_
#define VIDEOMANAGERI_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory> // for weak/shared_ptr
#include <vector>
//#include <atomic>
#include <map>
#include <string>
#include <functional>

#include "dring.h"

namespace DRing {

using VideoCapabilities = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

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
};

void registerVideoHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

std::vector<unsigned> getVideoCodecList(const std::string& accountID);
std::vector<std::string> getVideoCodecDetails(unsigned codecId);
void setVideoCodecList(const std::string& accountID, const std::vector<unsigned>& list);
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
void registerSinkTarget(const std::string& sinkId, std::function<void(const unsigned char*)>& cb);

void registerFrameListener(std::string conversationID, std::function<void(std::vector<unsigned char>& frame)> cb);

} // namespace DRing

#endif // VIDEOMANAGERI_H_
