/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Grégoire <olivier.gregoire@savoirfairelinux.com>
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
#include "smartools.h"
#include "media/media_decoder.h"
#include "media/video/video_input.h"
#include "media/video/video_device.h"
#include "dring/callmanager_interface.h"
#include "client/ring_signal.h"

namespace ring {

Smartools& Smartools::getInstance()
{
    // Meyers-Singleton
    static Smartools instance_;
    return instance_;
}

// Launch process() in new thread
Smartools::Smartools()
: loop_([this] { return true; }, [this] { process(); }, [] {})
{}

void
Smartools::sendInfo()
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    emitSignal<DRing::CallSignal::SmartInfo>(std::move(information_));
    //information_.clear();
}

void
Smartools::process()
{
    // Send the signal SmartInfo
    Smartools::sendInfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(refreshTimeMs_));
}

void
Smartools::start(int refreshTimeMs)
{
    refreshTimeMs_ = refreshTimeMs;
    loop_.stop();
    loop_.start();
}

void
Smartools::stop()
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    loop_.stop();
    information_.clear();
}

/*
 * Set all the information in the map
 */
void
Smartools::setFrameRate(const std::string& id, const std::string& fps)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    if(id == "local"){
        information_["local FPS"]= fps;
    } else {
        information_["remote FPS"]= fps;
    }
}

void
Smartools::setResolution(const std::string& id, const std::string& width, const std::string& height)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    if(id == "local"){
        information_["local width"] = width;
        information_["local height"] = height;
    } else {
        information_["remote width"] = width;
        information_["remote height"] = height;
    }
}

void
Smartools::setRemoteAudioCodec(const std::string& remoteAudioCodec)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["remote audio codec"]= remoteAudioCodec;
}

void
Smartools::setLocalAudioCodec(const std::string& localAudioCodec)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["local audio codec"]= localAudioCodec;
}

void
Smartools::setLocalVideoCodec(const std::string& localVideoCodec)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["local video codec"]= localVideoCodec;
}

void
Smartools::setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& callID)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["remote video codec"]= remoteVideoCodec;
    auto confID = Manager::instance().getCallFromCallID(callID)->getConfId();
    if (confID != ""){
        information_["type"]= "conference";
        information_["callID"]= confID;
    } else {
        information_["type"]= "no conference";
        information_["callID"]= callID;
    }
 }

 } // end namespace ring
