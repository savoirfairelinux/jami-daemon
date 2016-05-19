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

 namespace ring {
   Smartools::Smartools(int refreshTimeMs)
   : loop_([this] { return true; },[this] { process(); },[] {})
   {
     this -> refreshTime = refreshTimeMs;
   }
   void
   Smartools::process()
   {

     std::cout << "send smartInfo"<< std::endl;
     ring::Manager::instance().smartInfo();
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
     //std::terminate();
     loop_.stop();
   }

 }
