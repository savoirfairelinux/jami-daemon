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
#include "client/signal.h"

namespace DRing {

using ring::videoManager;

void registerVideoHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
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


std::vector<std::map<std::string, std::string> >
getCodecs(const std::string &accountID)
{
    if (const auto acc = ::ring::Manager::instance().getAccount(accountID))
        return acc->getAllVideoCodecs();
    else
        return std::vector<std::map<std::string, std::string> >();
}

void
setCodecs(const std::string& accountID,
                        const std::vector<std::map<std::string, std::string> > &details)
{
    if (auto acc = ::ring::Manager::instance().getAccount(accountID)) {
        acc->setVideoCodecs(details);
        ::ring::Manager::instance().saveConfig();
    }
}

std::vector<std::string>
getDeviceList()
{
    return videoManager.videoDeviceMonitor_.getDeviceList();
}

VideoCapabilities
getCapabilities(const std::string& name)
{
    return videoManager.videoDeviceMonitor_.getCapabilities(name);
}

std::string
getDefaultDevice()
{
    return videoManager.videoDeviceMonitor_.getDefaultDevice();
}

void
setDefaultDevice(const std::string &name)
{
    RING_DBG("Setting device to %s", name.c_str());
    videoManager.videoDeviceMonitor_.setDefaultDevice(name);
}

std::map<std::string, std::string>
getSettings(const std::string& name) {
    return videoManager.videoDeviceMonitor_.getSettings(name);
}

void
applySettings(const std::string& name,
        const std::map<std::string, std::string>& settings)
{
    videoManager.videoDeviceMonitor_.applySettings(name, settings);
}

void
startCamera()
{
    videoManager.videoPreview_ = ring::getVideoCamera();
    videoManager.started_ = switchToCamera();
}

void
stopCamera()
{
    if (switchInput(""))
        videoManager.started_ = false;
    videoManager.videoPreview_.reset();
}

bool
switchInput(const std::string &resource)
{
    auto input = videoManager.videoInput_.lock();
    if (!input) {
        RING_WARN("Video input not initialized");
        return false;
    }
    return input->switchInput(resource);
}

bool
switchToCamera()
{
    return switchInput("v4l2://" + videoManager.videoDeviceMonitor_.getDefaultDevice());
}

bool
hasCameraStarted()
{
    return videoManager.started_;
}

std::string
getCurrentCodecName(const std::string & /*callID*/)
{
    return "";
}

} // namespace DRing
