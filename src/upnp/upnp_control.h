/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *    Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp_context.h"
#include "protocol/global_mapping.h"

#include "noncopyable.h"
#include "logger.h"
#include "ip_utils.h"

#include <list>
#include <memory>
#include <chrono>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>

namespace jami {
class IpAddr;
}

namespace jami { namespace upnp {

using namespace std::placeholders;

class UPnPContext;

class Controller
{
public:
    using NotifyServiceCallback = std::function<void(uint16_t*, bool)>;
    using cbMap = std::map<std::pair<uint16_t, uint16_t>, NotifyServiceCallback>;
    using cbMapItr = std::map<std::pair<uint16_t, uint16_t>, NotifyServiceCallback>::iterator;
    using portMapItr = PortMapLocal::iterator;

    struct ControllerData {
        std::string memAddr;
        bool keepCb;
        NotifyControllerCallback cbAdd;
        NotifyControllerCallback cbRm;
    };

    Controller(bool keepCb = true);
    ~Controller();

    // Checks if a valid IGD is available.
    bool hasValidIgd();

    // Sends out request to upnp protocol to open a port. Gives option to use unique port (i.e. not one that is already in use).
    void addMapping(NotifyServiceCallback&& cb, uint16_t portDesired /*external port*/, PortType type, bool unique, uint16_t portLocal = 0 /*internal port*/);

    // Callback function for when mapping is added.
    void onAddMapping(Mapping* mapping, bool success);

    // Sends out request to upnp protocol to close all opened ports from this controller.
    void removeAllLocalMappings();

    // Callback function for when mapping is removed.
    void onRemoveMapping(Mapping* mapping, bool success);

    // Gets the external ip of the first valid IGD in the list.
    IpAddr getExternalIP() const;

    // Gets the local ip that interface with the first valid IGD in the list.
    IpAddr getLocalIP() const;

private:
    // Removes all mappings of the given type.
    void removeLocalMappings(PortType type);

    // Checks if the map is present locally given a port and type.
    bool isLocalMapPresent(const unsigned int portExternal, PortType type);

    // Checks if the map is present locally given a mapping.
    bool isLocalMapPresent(Mapping map);

    // Adds a mapping locally to the list.
    void addLocalMap(Mapping map);

    // Removes a mapping locally from the list.
    void removeLocalMap(Mapping map);

    // Adds callback to callback list.
    void registerCallback(unsigned int portExternal, unsigned int portInternal, NotifyServiceCallback&& cb);

    // Removes callback from callback list.
    void unregisterCallback(Mapping map);

    // Calls corresponding callback.
    void dispatchCallback(Mapping map, bool success);

private:
    std::shared_ptr<UPnPContext> upnpContext_;  // Context from which the controller executes the wanted commands.

    std::mutex mapListMutex_;                   // Mutex to protect mappings list.
    PortMapLocal udpMappings_;                  // List of UDP mappings created by this instance.
    PortMapLocal tcpMappings_;                  // List of TCP mappings created by this instance.

    std::mutex cbListMutex_;                    // Mutex to protect local callback list.
    bool keepCb_ {false};                       // Variable that indicates if the controller wants to keep it's callback in the list after a connectivity change.
    cbMap mapCbList_;                           // List of mappings with their corresponding callbacks.

    const void* memAddr_ {nullptr};             // Variable to store address of instance.
    std::string memAddrStr_ {};                 // Variable to store string of address to be used as the unique identifier.
};

}} // namespace jami::upnp
