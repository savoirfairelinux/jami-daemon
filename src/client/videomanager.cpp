/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include "videomanager_interface.h"
#include "videomanager.h"
#include "libav_utils.h"
#include "video/video_input.h"
#include "video/video_device_monitor.h"
#include "account.h"
#include "logger.h"
#include "manager.h"
#include "system_codec_container.h"
#include "video/sinkclient.h"
#include "client/ring_signal.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace DRing {

using ring::videoManager;

void
registerVideoHandlers(const std::map<std::string,
                      std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}


std::vector<std::string>
getDeviceList()
{
    return videoManager.videoDeviceMonitor.getDeviceList();
}

VideoCapabilities
getCapabilities(const std::string& name)
{
    return videoManager.videoDeviceMonitor.getCapabilities(name);
}

std::string
getDefaultDevice()
{
    return videoManager.videoDeviceMonitor.getDefaultDevice();
}

void
setDefaultDevice(const std::string& name)
{
    RING_DBG("Setting default device to %s", name.c_str());
    videoManager.videoDeviceMonitor.setDefaultDevice(name);
}

std::map<std::string, std::string>
getSettings(const std::string& name)
{
    return videoManager.videoDeviceMonitor.getSettings(name).to_map();
}

void
applySettings(const std::string& name,
              const std::map<std::string, std::string>& settings)
{
    videoManager.videoDeviceMonitor.applySettings(name, settings);
}

void
startCamera()
{
    videoManager.videoPreview = ring::getVideoCamera();
    videoManager.started = switchToCamera();
}

void
stopCamera()
{
    if (switchInput(""))
        videoManager.started = false;
    videoManager.videoPreview.reset();
}

bool
switchInput(const std::string& resource)
{
    if (auto call = ring::Manager::instance().getCurrentCall()) {
        // TODO remove this part when clients are updated to use CallManager::switchInput
        call->switchInput(resource);
        return true;
    } else {
        if (auto input = videoManager.videoInput.lock())
            return input->switchInput(resource).valid();
        RING_WARN("Video input not initialized");
    }
    return false;
}

bool
switchToCamera()
{
    return switchInput(videoManager.videoDeviceMonitor.getMRLForDefaultDevice());
}

bool
hasCameraStarted()
{
    return videoManager.started;
}

template <class T>
static void
registerSinkTarget_(const std::string& sinkId, T&& cb)
{
    if (auto sink = ring::Manager::instance().getSinkClient(sinkId))
        sink->registerTarget(std::forward<T>(cb));
    else
        RING_WARN("No sink found for id '%s'", sinkId.c_str());
}

void
registerSinkTarget(const std::string& sinkId,
                   const std::function<void(std::shared_ptr<std::vector<unsigned char> >&, int, int)>& cb)
{
    registerSinkTarget_(sinkId, cb);
}

void
registerSinkTarget(const std::string& sinkId,
                   std::function<void(std::shared_ptr<std::vector<unsigned char> >&, int, int)>&& cb)
{
    registerSinkTarget_(sinkId, cb);
}

#ifdef __ANDROID__
void
addVideoDevice(const std::string &node)
{
    videoManager.videoDeviceMonitor.addDevice(node);
}

void
removeVideoDevice(const std::string &node)
{
    videoManager.videoDeviceMonitor.removeDevice(node);
}

void*
obtainFrame(int length)
{
    if (auto input = videoManager.videoInput.lock())
        return (*input).obtainFrame(length);

    return nullptr;
}

void
releaseFrame(void* frame)
{
    if (auto input = videoManager.videoInput.lock())
        (*input).releaseFrame(frame);
}
#endif

} // namespace DRing

namespace ring {

VideoManager videoManager;

std::shared_ptr<video::VideoFrameActiveWriter>
getVideoCamera()
{
    if (auto input = videoManager.videoInput.lock())
        return input;

    videoManager.started = false;
    auto input = std::make_shared<video::VideoInput>();
    videoManager.videoInput = input;
    return input;
}

video::VideoDeviceMonitor&
getVideoDeviceMonitor()
{
    return videoManager.videoDeviceMonitor;
}

} // namespace ring
