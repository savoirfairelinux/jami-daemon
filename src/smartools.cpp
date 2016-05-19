/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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
#include "./media/media_decoder.h"
#include "./media/video/video_input.h"
#include "./media/video/video_device.h"
#include <string>
#include "./manager.cpp"

namespace ring {

    Smartools& Smartools::getInstance()
    {
        //Singleton
        static Smartools instance_;
        return instance_;
    }
    //launch process() in new thread
    Smartools::Smartools()
    : loop_([this] { return true; },[this] { process(); },[] {})
    {}

    void
    Smartools::sendInfo()
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Manager::instance().smartInfo(info);
        info.clear();
    }

    void
    Smartools::process()
    {
        //send the signal SmartInfo
        Smartools::sendInfo();
        std::this_thread::sleep_for(std::chrono::milliseconds(refreshTime));
    }

    void
    Smartools::start()
    {
        loop_.stop();
        loop_.start();
    }

    void
    Smartools::stop()
    {
        loop_.stop();
        info.clear();
    }

    void
    Smartools::setFrameRate(const std::string& id, const std::string& fps)
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        if(id == "local"){
            Smartools::info["local FPS"]= fps;
        }
        else{
            Smartools::info["remote FPS"]= fps;
        }
    }

    void
    Smartools::setRemoteResolution(const std::string& remoteWidth, const std::string& remoteHeight)
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Smartools::info["remote width"] = remoteWidth;
        Smartools::info["remote height"] = remoteHeight;
    }

    void
    Smartools::setLocalResolution(const std::string& width, const std::string& height)
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Smartools::info["local width"] = width;
        Smartools::info["local height"] = height;
    }

    void
    Smartools::setResolution(const std::string& id, const std::string& width, const std::string& height)
    {
        //RING_DBG("callID: %s",id);
        std::lock_guard<std::mutex> lk(_mutexInfo);
        if(id == "local"){
            Smartools::info["local width"] = width;
            Smartools::info["local height"] = height;
        }
        else{
            Smartools::info["remote width"] = width;
            Smartools::info["remote height"] = height;
        }
    }

    void
    Smartools::setRemoteAudioCodec(const std::string& remoteAudioCodec)
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Smartools::info["remote audio codec"]= remoteAudioCodec;
    }

    void
    Smartools::setLocalAudioCodec(const std::string& localAudioCodec)
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Smartools::info["local audio codec"]= localAudioCodec;
    }

    void
    Smartools::setLocalVideoCodec(const std::string& localVideoCodec)
    {
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Smartools::info["local video codec"]= localVideoCodec;
    }

    void
    Smartools::setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& callID)
    {
        //RING_DBG("callID: %s",callID);
        std::lock_guard<std::mutex> lk(_mutexInfo);
        Smartools::info["remote video codec"]= remoteVideoCodec;
        std::string confID = Manager::instance().getCallFromCallID(callID)->getConfId();
        if (confID != ""){
            Smartools::info["state"]= "conference";
            Smartools::info["callID"]= confID;
        }
        else{
            Smartools::info["state"]= "no conference";
            Smartools::info["callID"]= callID;
        }
     }
 } //end namespace ring and video
