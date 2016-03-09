/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
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
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getDeviceList();
}

VideoCapabilities
getCapabilities(const std::string& name)
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getCapabilities(name);
}

std::string
getDefaultDevice()
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getDefaultDevice();
}

void
setDefaultDevice(const std::string& name)
{
    RING_DBG("Setting default device to %s", name.c_str());
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.setDefaultDevice(name);
}

std::map<std::string, std::string>
getSettings(const std::string& name)
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getSettings(name).to_map();
}

void
applySettings(const std::string& name,
              const std::map<std::string, std::string>& settings)
{
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.applySettings(name, settings);
}

void
startCamera()
{
    ring::Manager::instance().getVideoManager().videoPreview = ring::getVideoCamera();
    ring::Manager::instance().getVideoManager().started = switchToCamera();
}

void
stopCamera()
{
    if (switchInput(""))
        ring::Manager::instance().getVideoManager().started = false;
    ring::Manager::instance().getVideoManager().videoPreview.reset();
}

bool
switchInput(const std::string& resource)
{
    if (auto call = ring::Manager::instance().getCurrentCall()) {
        // TODO remove this part when clients are updated to use Callring::Manager::switchInput
        call->switchInput(resource);
        return true;
    } else {
        if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
            return input->switchInput(resource).valid();
        RING_WARN("Video input not initialized");
    }
    return false;
}

bool
switchToCamera()
{
    return switchInput(ring::Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice());
}

bool
hasCameraStarted()
{
    return ring::Manager::instance().getVideoManager().started;
}

void
registerSinkTarget(const std::string& sinkId, const SinkTarget& target)
{
   if (auto sink = ring::Manager::instance().getSinkClient(sinkId))
       sink->registerTarget(target);
   else
       RING_WARN("No sink found for id '%s'", sinkId.c_str());
}

#ifdef __ANDROID__
void
addVideoDevice(const std::string &node)
{
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.addDevice(node);
}

void
removeVideoDevice(const std::string &node)
{
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.removeDevice(node);
}

void*
obtainFrame(int length)
{
    if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
        return (*input).obtainFrame(length);

    return nullptr;
}

void
releaseFrame(void* frame)
{
    if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
        (*input).releaseFrame(frame);
}
#endif

} // namespace DRing

namespace ring {

std::shared_ptr<video::VideoFrameActiveWriter>
getVideoCamera()
{
    auto& vmgr = Manager::instance().getVideoManager();
    if (auto input = vmgr.videoInput.lock())
        return input;

    vmgr.started = false;
    auto input = std::make_shared<video::VideoInput>();
    vmgr.videoInput = input;
    return input;
}

video::VideoDeviceMonitor&
getVideoDeviceMonitor()
{
    return Manager::instance().getVideoManager().videoDeviceMonitor;
}

} // namespace ring
