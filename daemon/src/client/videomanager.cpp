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
#include "video/video_preferences.h"
#include "account.h"
#include "logger.h"
#include "manager.h"

VideoPreference &
VideoManager::getVideoPreferences()
{
    return videoPreference_;
}

std::vector<std::map<std::string, std::string> >
VideoManager::getCodecs(const std::string &accountID)
{
    Account *acc = Manager::instance().getAccount(accountID);

    if (acc != NULL)
        return acc->getAllVideoCodecs();
    else
        return std::vector<std::map<std::string, std::string> >();
}

void
VideoManager::setCodecs(const std::string& accountID,
                         const std::vector<std::map<std::string, std::string> > &details)
{
    Account *acc = Manager::instance().getAccount(accountID);
    if (acc != NULL) {
        acc->setVideoCodecs(details);
        Manager::instance().saveConfig();
    }
}

std::vector<std::string>
VideoManager::getDeviceList()
{
    return videoPreference_.getDeviceList();
}

std::vector<std::string>
VideoManager::getDeviceChannelList(const std::string &dev)
{
    return videoPreference_.getChannelList(dev);
}

std::vector<std::string>
VideoManager::getDeviceSizeList(const std::string &dev, const std::string &channel)
{
    return videoPreference_.getSizeList(dev, channel);
}

std::vector<std::string>
VideoManager::getDeviceRateList(const std::string &dev, const std::string &channel, const std::string &size)
{
    return videoPreference_.getRateList(dev, channel, size);
}

std::map<std::string, std::map<std::string, std::vector<std::string>>>
VideoManager::getCapabilities(const std::string& name)
{
    return videoPreference_.getCapabilities(name);
}

std::string
VideoManager::getActiveDevice()
{
    return videoPreference_.getDevice();
}

std::string
VideoManager::getActiveDeviceChannel()
{
    return videoPreference_.getChannel();
}

std::string
VideoManager::getActiveDeviceSize()
{
    return videoPreference_.getSize();
}

std::string
VideoManager::getActiveDeviceRate()
{
    return videoPreference_.getRate();
}

void
VideoManager::setActiveDevice(const std::string &device)
{
    DEBUG("Setting device to %s", device.c_str());
    videoPreference_.setDevice(device);
}

void
VideoManager::setActiveDeviceChannel(const std::string &channel)
{
    videoPreference_.setChannel(channel);
}

void
VideoManager::setActiveDeviceSize(const std::string &size)
{
    videoPreference_.setSize(size);
}

void
VideoManager::setActiveDeviceRate(const std::string &rate)
{
    videoPreference_.setRate(rate);
}

std::map<std::string, std::string>
VideoManager::getSettingsFor(const std::string& device) {
    return videoPreference_.getSettingsFor(device);
}

std::map<std::string, std::string>
VideoManager::getSettings() {
    return videoPreference_.getSettings();
}

std::map<std::string, std::string>
VideoManager::getPreferences(const std::string& name) {
    return videoPreference_.getPreferences(name);
}

void
VideoManager::setPreferences(const std::string& name,
        const std::map<std::string, std::string>& pref)
{
    videoPreference_.setPreferences(name, pref);
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
        WARN("Video input not initialized");
        return false;
    }
    return input->switchInput(resource);
}

bool
VideoManager::switchToCamera()
{
    return switchInput("v4l2://" + videoPreference_.getDevice());
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
