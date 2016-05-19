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
 #ifndef SMARTOOLS_H_
 #define SMARTOOLS_H_

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

 namespace ring {
 class Smartools{
    public:
      Smartools(std::string callID, int refreshTimeMs);
      void start();
      void stop();
      static void setLocalFramerate(const std::string& localFps);
      static void setRemoteFramerate(const std::string& remoteFps);
      static void setRemoteResolution(const std::string& remoteResolution);
      static void setRemoteCodec(const std::string& remoteCodec);

    private:
      void process();

      ThreadLoop loop_; // as to be last member
      static std::string localFps_;
      static std::string remoteFps_;
      static std::string remoteResolution_;
      static std::string remoteCodec_;
      int refreshTime;
  };
} //ring namespace
#endif //smartools.h
