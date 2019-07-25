/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include "config.h"
#include <sdbus-c++/sdbus-c++.h>
#include "dbuscallmanager.adaptor.h"
#include "dbusconfigurationmanager.adaptor.h"
#include "dbusdatatransfer.adaptor.h"
#include "dbusinstance.adaptor.h"
#include "dbuspresencemanager.adaptor.h"
#ifdef ENABLE_VIDEO
#include "dbusvideomanager.adaptor.h"
#endif

#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "datatransfer_interface.h"
#include "presencemanager_interface.h"
#ifdef ENABLE_VIDEO
#include "videomanager_interface.h"
#endif

#include <sigc++/sigc++.h>
#include <tuple>
#include <iostream>
#include "media/audio/audio_api_names.h"

class DBusDaemon1 : public sdbus::AdaptorInterfaces<net::jami::daemon1::CallManager_adaptor
                                                   ,net::jami::daemon1::ConfigurationManager_adaptor
                                                   ,net::jami::daemon1::DataTransfer_adaptor
                                                   ,net::jami::daemon1::Instance_adaptor
                                                   ,net::jami::daemon1::PresenceManager_adaptor
#ifdef ENABLE_VIDEO
                                                   ,net::jami::daemon1::VideoManager_adaptor
#endif
                                                   >
{
public:
    DBusDaemon1(sdbus::IConnection& connection, std::string objectPath)
                         : sdbus::AdaptorInterfaces<net::jami::daemon1::CallManager_adaptor
                                                   ,net::jami::daemon1::ConfigurationManager_adaptor
                                                   ,net::jami::daemon1::DataTransfer_adaptor
                                                   ,net::jami::daemon1::Instance_adaptor
                                                   ,net::jami::daemon1::PresenceManager_adaptor
#ifdef ENABLE_VIDEO
                                                   ,net::jami::daemon1::VideoManager_adaptor
#endif
                                                   >(connection, std::move(objectPath))
    {
        registerAdaptor();
    }

    ~DBusDaemon1()
    {
        unregisterAdaptor();
    }

    uint_fast16_t numberOfClients() { return numberOfClients_; }
    sigc::signal<void(uint_fast16_t)> signal_numberOfClientsChanged() { return signal_numberOfClientsChanged_; }

protected:
#include "net.jami.daemon1.CallManager.cpp"
#include "net.jami.daemon1.ConfigurationManager.cpp"
#include "net.jami.daemon1.DataTransfer.cpp"
#include "net.jami.daemon1.Instance.cpp"
#include "net.jami.daemon1.PresenceManager.cpp"
#ifdef ENABLE_VIDEO
#include "net.jami.daemon1.VideoManager.cpp"
#endif

    sigc::signal<void(uint_fast16_t)> signal_numberOfClientsChanged_;

private:
    uint_fast16_t numberOfClients_ = 0;
};
