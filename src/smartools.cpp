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
     Smartools *Smartools::singleton = NULL;
     int Smartools::refreshTime = 0;

     //initialise static string
     std::string Smartools::localFps_ = "";
     std::string Smartools::remoteFps_ = "";
     std::string Smartools::remoteResolution_ = "";
     std::string Smartools::remoteVideoCodec_ = "";
     std::string Smartools::remoteAudioCodec_ = "";
     std::string Smartools::localVideoCodec_ = "";
     std::string Smartools::callID_ = "";

     Smartools& Smartools::getInstance(){
         static Smartools instance_;

         return instance_;
     }
   //launch process() every refreshTimeMs milliseconds
   Smartools::Smartools()
   : loop_([this] { return true; },[this] { process(); },[] {})
   {

   }
   void
   Smartools::process()
   {
       static int val = 0;
       val++;
       //std::map<std::string,std::string> info;


       Smartools::info["callID"]= callID_;
       Smartools::info["remoteFps"]= remoteFps_;
       Smartools::info["localFps"]= localFps_;
       Smartools::info["remoteResolution"]= remoteResolution_;
       Smartools::info["localVideoCodec_"]= localVideoCodec_;
       Smartools::info["remoteVideoCodec"]= remoteVideoCodec_;
       Smartools::info["remoteAudioCodec"]= remoteAudioCodec_;
       Manager::instance().smartInfo(info);
       info.clear();
       std::this_thread::sleep_for(std::chrono::milliseconds(refreshTime));
   }

    void
    Smartools::start()
    {
       loop_.start();
    }

    void
    Smartools::stop()
    {
        loop_.stop();
    }

    void Smartools::setFrameRate(const std::string& id, const std::string& fps){

        Smartools::info[id+ " FPS"]= fps;
    }

    /*void Smartools::implementFrameRateMap(const std::string& id, const std::string& fps){

    }*/

     void Smartools::setLocalFramerate(const std::string& localFps) {
         Smartools::localFps_ = localFps;
     }

     void Smartools::setRemoteFramerate(const std::string& remoteFps) {
         Smartools::remoteFps_ = remoteFps;
     }

     void Smartools::setRemoteResolution(const std::string& remoteResolution){
         Smartools::remoteResolution_ = remoteResolution;
     }

   /*void Smartools::setRemoteAudioCodec(const std::string& remoteAudioCodec){
     Smartools::remoteAudioCodec_= remoteAudioCodec;
   }*/

     void Smartools::setLocalVideoCodec(const std::string& localVideoCodec){
         Smartools::localVideoCodec_ = localVideoCodec;
     }

     void Smartools::setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& callID){
         Smartools::remoteVideoCodec_=remoteVideoCodec;
         Smartools::callID_=callID;
     }

 } //end namespace ring and video
