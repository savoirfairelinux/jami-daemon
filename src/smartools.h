/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
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

#include "threadloop.h"
#include "manager.h"
#include <string>

namespace ring {
class Smartools
{
    // Use for the unit tests
    #ifdef TESTING
        friend class SmartoolsTest;
    #endif

    public:
        static Smartools& getInstance();
        void start(std::chrono::milliseconds refreshTimeMs);
        void stop();
        void setFrameRate(const std::string& id, const std::string& fps);
        void setResolution(const std::string& id, int width, int height);
        void setLocalVideoCodec(const std::string& localVideoCodec);
        void setRemoteVideoCodec(const std::string& remoteVideoCodec, const std::string& callID);
        void setRemoteAudioCodec(const std::string& remoteAudioCodec);
        void setLocalAudioCodec(const std::string& remoteAudioCodec);
        void sendInfo();

    private:
        Smartools();
        ~Smartools();
        void process();
        std::map<std::string, std::string> information_;
        std::mutex mutexInfo_; // Protect information_ from multithreading
        std::chrono::milliseconds refreshTimeMs_ {500};
        ThreadLoop loop_; // Has to be last member
};
} //ring namespace
