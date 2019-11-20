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
#include "manager.h"
#include "dring/callmanager_interface.h"
#include "client/ring_signal.h"

namespace jami {

Smartools& Smartools::getInstance()
{
    // Meyers-Singleton
    static Smartools instance_;
    return instance_;
}

Smartools::~Smartools() {
    stop();
}

void
Smartools::sendInfo()
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    emitSignal<DRing::CallSignal::SmartInfo>(information_);
    information_.clear();
}

void
Smartools::start(std::chrono::milliseconds refreshTimeMs)
{
    JAMI_DBG("Start SmartInfo");
    auto task = Manager::instance().scheduler().scheduleAtFixedRate([this]{
        sendInfo();
        return true;
    }, refreshTimeMs);
    task_.swap(task);
    if (task)
        task->cancel();
}

void
Smartools::stop()
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    JAMI_DBG("Stop SmartInfo");
    if (auto t = std::move(task_))
        t->cancel();
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
        information_["local width"] = std::to_string(width);
        information_["local height"] = std::to_string(height);
    } else {
        information_["remote width"] = std::to_string(width);
        information_["remote height"] = std::to_string(height);
    }
}

void
Smartools::setRemoteAudioCodec(const std::string& remoteAudioCodec)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["remote audio codec"] = remoteAudioCodec;
}

void
Smartools::setLocalAudioCodec(const std::string& localAudioCodec)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["local audio codec"] = localAudioCodec;
}

void
Smartools::setLocalVideoCodec(const std::string& localVideoCodec)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["local video codec"] = localVideoCodec;
}

void
Smartools::setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& callID)
{
    std::lock_guard<std::mutex> lk(mutexInfo_);
    information_["remote video codec"]= remoteVideoCodec;
    if (auto call = Manager::instance().getCallFromCallID(callID)) {
        auto confID = call->getConfId();
        if (not confID.empty()) {
            information_["type"]= "conference";
            information_["callID"]= confID;
        } else {
            information_["type"]= "no conference";
            information_["callID"]= callID;
        }
    }
 }

 } // end namespace jami
