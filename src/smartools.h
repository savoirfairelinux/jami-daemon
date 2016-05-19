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
#pragma once

#include <chrono>

#include "threadloop.h"
#include "manager.h"
#include <thread>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <atomic>

namespace ring {
class Smartools
{
    public:
        static Smartools& getInstance();
        void start();
        void stop();
        void setFrameRate(const std::string& id, const std::string& fps);
        void setRemoteResolution(const std::string& width, const std::string& height);
        void setLocalResolution(const std::string& remoteWidth, const std::string& remoteHeight);
        void setResolution(const std::string& id, const std::string& width, const std::string& height);
        void setLocalVideoCodec(const std::string& localVideoCodec);
        void setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& callID);
        void setRemoteAudioCodec(const std::string& remoteAudioCodec);
        void setLocalAudioCodec(const std::string& remoteAudioCodec);
        void sendInfo();

        int refreshTime;

    private:
        Smartools();
        void process();
        std::map<std::string,std::string> info;
        std::mutex _mutexInfo;//protect info from multithreading
        ThreadLoop loop_; // as to be last member
};
} //ring namespace
