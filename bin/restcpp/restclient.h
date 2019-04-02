/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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

#include <memory>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <thread>

#include <functional>
#include <iterator>
#include <restbed>

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef ENABLE_VIDEO
#include "dring/videomanager_interface.h"
#endif
#include "logger.h"
#include "restconfigurationmanager.h"
#include "restvideomanager.h"

class RestClient {
    public:
        RestClient(int port, int flags, bool persistent);
        ~RestClient();

        int event_loop() noexcept;
        int exit() noexcept;

    private:
        int initLib(int flags);
        void endLib() noexcept;
        void initResources();

        bool pollNoMore_ = false;

        std::unique_ptr<RestConfigurationManager> configurationManager_;
        std::unique_ptr<RestVideoManager> videoManager_;

        // Restbed attributes
        restbed::Service service_;
        std::shared_ptr<restbed::Settings> settings_;
        std::thread restbed;
};
