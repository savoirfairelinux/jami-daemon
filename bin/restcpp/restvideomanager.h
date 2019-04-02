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

#include <vector>
#include <map>
#include <string>
#include <restbed>

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef ENABLE_VIDEO
#include "dring/videomanager_interface.h"
#endif
#include "logger.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class RestVideoManager
{
    public:
        RestVideoManager();

        std::vector<std::shared_ptr<restbed::Resource>> getResources();

    private:
        // Attributes
        std::vector<std::shared_ptr<restbed::Resource>> resources_;

        // Methods
        std::map<std::string, std::string> parsePost(const std::string& post);
        void populateResources();
        void defaultRoute(const std::shared_ptr<restbed::Session> session);

        void getDeviceList(const std::shared_ptr<restbed::Session> session);
        void getCapabilities(const std::shared_ptr<restbed::Session> session);
        void getSettings(const std::shared_ptr<restbed::Session> session);
        void applySettings(const std::shared_ptr<restbed::Session> session);
        void setDefaultDevice(const std::shared_ptr<restbed::Session> session);
        void getDefaultDevice(const std::shared_ptr<restbed::Session> session);
        void startCamera(const std::shared_ptr<restbed::Session> session);
        void stopCamera(const std::shared_ptr<restbed::Session> session);
        void switchInput(const std::shared_ptr<restbed::Session> session);
        void hasCameraStarted(const std::shared_ptr<restbed::Session> session);
};
