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
#include "media/media_decoder.h"
#include "media/video/video_input.h"
#include "media/video/video_device.h"
#include <string>

namespace ring {
    Smartools& Smartools::getInstance(){
        static Smartools instance_;
        return instance_;
    }

    //launch process() in new thread
    Smartools::Smartools()
    : loop_([this] { return true; },
            [this] { process(); },
            [] {}){}

    void
    Smartools::process()
    {
        //send the signal SmartInfo
        Manager::instance().smartInfo(information);
        information.clear();
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
        information.clear();
    }

    void Smartools::setFrameRate(const std::string& id, const std::string& fps){
        Smartools::information[id]["local FPS"]= fps;

        if(id == "local"){
            //Smartools::information["local FPS"]= fps;
        }
        else{
            //Smartools::information["remote FPS"]= fps;
        }
    }

    void Smartools::setRemoteResolution(const std::string& remoteWidth, const std::string& remoteHeight){
        //Smartools::information["remote width"] = remoteWidth;
        ////Smartools::information["remote height"] = remoteHeight;
    }
    void Smartools::setLocalResolution(const std::string& width, const std::string& height){
        //RING_ERR("%s %s",width,height );
        //Smartools::information["local width"] = width;
        //Smartools::information["local height"] = height;
    }

    void Smartools::setResolution(const std::string& id, const std::string& width, const std::string& height){
        if(id == "local"){
            //Smartools::information["local width"] = width;
            //Smartools::information["local height"] = height;
        }
        else{
            //Smartools::information["remote width"] = width;
            //Smartools::information["remote height"] = height;
        }//
    }

    void Smartools::setRemoteAudioCodec(const std::string& remoteAudioCodec){
        //Smartools::information["remote audio codec"]= remoteAudioCodec;
    }

    void Smartools::setLocalAudioCodec(const std::string& localAudioCodec){
        //Smartools::information["local"]["local audio codec"]= localAudioCodec;
    }

    void Smartools::setLocalVideoCodec(const std::string& localVideoCodec){
        //Smartools::information["local video codec"]= localVideoCodec;
    }

    void Smartools::setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& id){
        //Smartools::information["remote video codec"]= remoteVideoCodec;
        //Smartools::information["callID"]= callID;
        RING_ERR("Enter in setRemoteVideoCodec");
        Smartools::information[id]["remote video codec"]= remoteVideoCodec;
     }

 } //end namespace ring and video
