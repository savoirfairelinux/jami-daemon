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

 namespace ring {
   //initialise static string
   std::string Smartools::localFps_ = "";
   std::string Smartools::remoteFps_ = "";
   std::string Smartools::remoteResolution_ = "";

   //launch process() every refreshTimeMs milliseconds
   Smartools::Smartools(std::string callID, int refreshTimeMs)
   : loop_([this] { return true; },[this] { process(); },[] {})
   {
     this -> refreshTime = refreshTimeMs;
   }
   void
   Smartools::process()
   {
     static int val = 0;
     val++;
     std::map<std::string,std::string> info;
     info["remoteFps"]= remoteFps_;
     info["localFps"]= localFps_;
     info["remoteResolution"]= remoteResolution_;
     Manager::instance().smartInfo(info);
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

   void Smartools::setLocalFramerate(const std::string& localFps) {
    Smartools::localFps_ = localFps;
   }

   void Smartools::setRemoteFramerate(const std::string& remoteFps) {
    Smartools::remoteFps_ = remoteFps;
   }

   void Smartools::setRemoteResolution(const std::string& remoteResolution){
     Smartools::remoteResolution_ = remoteResolution;
   }

 } //end namespace ring and video
