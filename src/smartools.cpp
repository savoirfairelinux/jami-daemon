/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
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
#include "string_utils.h"

namespace jami {

Smartools& Smartools::getInstance()
{
    // Meyers-Singleton
    static Smartools instance_;
    return instance_;
}

// Launch process() in new thread
Smartools::Smartools()
: loop_([] { return true; }, [this] { process(); }, [] {})
{}

Smartools::~Smartools()
{
    loop_.join();
}

void
Smartools::sendInfo()
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    emitSignal<DRing::CallSignal::SmartInfo>(information_);
    information_.clear();
}

void
Smartools::process()
{
    // Send the signal SmartInfo
    Smartools::sendInfo();
    std::this_thread::sleep_for(refreshTimeMs_);
}

void
Smartools::start(std::chrono::milliseconds refreshTimeMs)
{
    JAMI_DBG("Start SmartInfo");
    refreshTimeMs_ = refreshTimeMs;
    loop_.stop();
    loop_.start();
}

void
Smartools::stop()
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    JAMI_DBG("Stop SmartInfo");
    loop_.stop();
    information_.clear();
}


//Set all the information in the map

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
Smartools::setResolution(const std::string& id, int width, int height)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    if(id == "local"){
        information_["local width"] = to_string(width);
        information_["local height"] = to_string(height);
    } else {
        information_["remote width"] = to_string(width);
        information_["remote height"] = to_string(height);
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
    auto call = Manager::instance().getCallFromCallID(callID);
    if (!call) {
        return;
    }
    auto confID = call->getConfId();
    if (confID != ""){
        information_["type"]= "conference";
        information_["callID"]= confID;
    } else {
        information_["type"]= "no conference";
        information_["callID"]= callID;
    }
 }

 } // end namespace jami
