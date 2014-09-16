/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "videomanager.h"
#include "video/libav_utils.h"
#include "video/video_input.h"
#include "video/video_device_monitor.h"
#include "account.h"
#include "logger.h"
#include "manager.h"

VideoManager::VideoManager()
{
    libav_utils::sfl_avcodec_init();
}

void VideoManager::registerEvHandlers(struct sflph_video_ev_handlers* evHandlers)
{
    evHandlers_ = *evHandlers;
}

sfl_video::VideoDeviceMonitor &
VideoManager::getVideoDeviceMonitor()
{
    return videoDeviceMonitor_;
}

std::vector<std::map<std::string, std::string> >
VideoManager::getCodecs(const std::string &accountID)
{
    if (const auto acc = Manager::instance().getAccount(accountID))
        return acc->getAllVideoCodecs();
    else
        return std::vector<std::map<std::string, std::string> >();
}

void
VideoManager::setCodecs(const std::string& accountID,
                        const std::vector<std::map<std::string, std::string> > &details)
{
    if (auto acc = Manager::instance().getAccount(accountID)) {
        acc->setVideoCodecs(details);
        Manager::instance().saveConfig();
    }
}

std::vector<std::string>
VideoManager::getDeviceList()
{
    return videoDeviceMonitor_.getDeviceList();
}

sfl_video::VideoCapabilities
VideoManager::getCapabilities(const std::string& name)
{
    return videoDeviceMonitor_.getCapabilities(name);
}

std::string
VideoManager::getDefaultDevice()
{
    return videoDeviceMonitor_.getDefaultDevice();
}

void
VideoManager::setDefaultDevice(const std::string &name)
{
    SFL_DBG("Setting device to %s", name.c_str());
    videoDeviceMonitor_.setDefaultDevice(name);
}

std::map<std::string, std::string>
VideoManager::getSettings(const std::string& name) {
    return videoDeviceMonitor_.getSettings(name);
}

void
VideoManager::applySettings(const std::string& name,
        const std::map<std::string, std::string>& settings)
{
    videoDeviceMonitor_.applySettings(name, settings);
}

void
VideoManager::startCamera()
{
    videoPreview_ = getVideoCamera();
    started_ = switchToCamera();
}

void
VideoManager::stopCamera()
{
    if (switchInput(""))
        started_ = false;
    videoPreview_.reset();
}

bool
VideoManager::switchInput(const std::string &resource)
{
    auto input = videoInput_.lock();
    if (!input) {
        SFL_WARN("Video input not initialized");
        return false;
    }
    return input->switchInput(resource);
}

bool
VideoManager::switchToCamera()
{
    return switchInput("v4l2://" + videoDeviceMonitor_.getDefaultDevice());
}

std::shared_ptr<sfl_video::VideoFrameActiveWriter>
VideoManager::getVideoCamera()
{
    auto input = videoInput_.lock();
    if (!input) {
        started_ = false;
        input.reset(new sfl_video::VideoInput());
        videoInput_ = input;
    }
    return input;
}

bool
VideoManager::hasCameraStarted()
{
    return started_;
}

std::string
VideoManager::getCurrentCodecName(const std::string & /*callID*/)
{
    return "";
}

void VideoManager::deviceEvent()
{
    if (evHandlers_.on_device_event) {
        evHandlers_.on_device_event();
    }
}

void VideoManager::startedDecoding(const std::string &id, const std::string& shmPath, int w, int h, bool isMixer)
{
    if (evHandlers_.on_start_decoding) {
        evHandlers_.on_start_decoding(id, shmPath, w, h, isMixer);
    }
}

void VideoManager::stoppedDecoding(const std::string &id, const std::string& shmPath, bool isMixer)
{
    if (evHandlers_.on_stop_decoding) {
        evHandlers_.on_stop_decoding(id, shmPath, isMixer);
    }
}
