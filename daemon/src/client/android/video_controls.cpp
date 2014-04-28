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

#include "video_controls.h"
#include "video/libav_utils.h"
#include "video/video_input_selector.h"
#include "account.h"
#include "logger.h"
#include "manager.h"

namespace {
const char * const SERVER_PATH = "/org/sflphone/SFLphone/VideoControls";
}

VideoControls::VideoControls() :
    videoInputSelector_()
    , videoPreference_()
    , inputClients_(0)
{
    // initialize libav libraries
    libav_utils::sfl_avcodec_init();
}

VideoPreference &
VideoControls::getVideoPreferences()
{
    return videoPreference_;
}

std::vector<std::map<std::string, std::string> >
VideoControls::getCodecs(const std::string &accountID)
{
    Account *acc = Manager::instance().getAccount(accountID);

    if (acc != NULL)
        return acc->getAllVideoCodecs();
    else
        return std::vector<std::map<std::string, std::string> >();
}

void
VideoControls::setCodecs(const std::string& accountID,
                         const std::vector<std::map<std::string, std::string> > &details)
{
    Account *acc = Manager::instance().getAccount(accountID);
    if (acc != NULL) {
        acc->setVideoCodecs(details);
        Manager::instance().saveConfig();
    }
}

std::vector<std::string>
VideoControls::getDeviceList()
{
    return videoPreference_.getDeviceList();
}

std::vector<std::string>
VideoControls::getDeviceChannelList(const std::string &dev)
{
    return videoPreference_.getChannelList(dev);
}

std::vector<std::string>
VideoControls::getDeviceSizeList(const std::string &dev, const std::string &channel)
{
    return videoPreference_.getSizeList(dev, channel);
}

std::vector<std::string>
VideoControls::getDeviceRateList(const std::string &dev, const std::string &channel, const std::string &size)
{
    return videoPreference_.getRateList(dev, channel, size);
}

std::string
VideoControls::getActiveDevice()
{
    return videoPreference_.getDevice();
}

std::string
VideoControls::getActiveDeviceChannel()
{
    return videoPreference_.getChannel();
}

std::string
VideoControls::getActiveDeviceSize()
{
    return videoPreference_.getSize();
}

std::string
VideoControls::getActiveDeviceRate()
{
    return videoPreference_.getRate();
}

void
VideoControls::setActiveDevice(const std::string &device)
{
    DEBUG("Setting device to %s", device.c_str());
    videoPreference_.setDevice(device);
}

void
VideoControls::setActiveDeviceChannel(const std::string &channel)
{
    videoPreference_.setChannel(channel);
}

void
VideoControls::setActiveDeviceSize(const std::string &size)
{
    videoPreference_.setSize(size);
}

void
VideoControls::setActiveDeviceRate(const std::string &rate)
{
    videoPreference_.setRate(rate);
}

std::map<std::string, std::string>
VideoControls::getSettingsFor(const std::string& device) {
    return videoPreference_.getSettingsFor(device);
}

std::map<std::string, std::string>
VideoControls::getSettings() {
    return videoPreference_.getSettings();
}

void
VideoControls::startCamera()
{
    inputClients_++;
    if (hasCameraStarted()) {
        WARN("Video preview was already started!");
        return;
    }

    const std::string device = "v4l2://" + videoPreference_.getDevice();
    videoInputSelector_.reset(new sfl_video::VideoInputSelector(device));
}

void
VideoControls::stopCamera()
{
    if (hasCameraStarted()) {
        DEBUG("Stopping video preview");
        inputClients_--;
        if (inputClients_ <= 0)
            videoInputSelector_.reset();
    } else {
        WARN("Video preview was already stopped");
    }
}

bool
VideoControls::switchInput(const std::string &resource)
{
    if (not hasCameraStarted()) {
        ERROR("Input selector not initialized");
        return false;
    }

    return videoInputSelector_->switchInput(resource);
}

std::weak_ptr<sfl_video::VideoFrameActiveWriter>
VideoControls::getVideoCamera()
{
    return videoInputSelector_;
}

bool
VideoControls::hasCameraStarted()
{
    // see http://stackoverflow.com/a/7580064/21185
    return static_cast<bool>(videoInputSelector_);
}

std::string
VideoControls::getCurrentCodecName(const std::string & /*callID*/)
{
    return "";
}

void
VideoControls::startedDecoding(const std::string& callID, const std::string& shmPath, int const& width, int const& height)
{

}

void
VideoControls::stoppedDecoding(const std::string& callID, const std::string& shmPath)
{

}
