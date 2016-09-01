/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *
 *  Author: Caspar Cedro
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

#include <string>
#include <map>
#include <functional>
#include <vector>

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"

namespace ring { namespace upnp {

// defines a UPnP capable Ring Device (any device running Ring)
class RingDevice
{
public:

    // local device address of RingDevice
    IpAddr localIp;

    // constructors
    RingDevice() {}
    RingDevice(std::string UDN,
        std::string deviceType,
        std::string friendlyName,
        std::string baseURL,
        std::string relURL,
        std::string ringAccounts)
        : UDN_(UDN)
        , deviceType_(deviceType)
        , friendlyName_(friendlyName)
        , baseURL_(baseURL)
        , relURL_(relURL)
        , ringAccounts_(ringAccounts)
        {}

    // move constructor and operator
    RingDevice(RingDevice&&) = default;
    RingDevice& operator=(RingDevice&&) = default;

    ~RingDevice() = default;

    const std::string& getUDN() const { return UDN_; };
    const std::string& getDeviceType() const { return deviceType_; };
    const std::string& getFriendlyName() const { return friendlyName_; };
    const std::string& getBaseURL() const { return baseURL_; };
    const std::string& getrelURL() const { return relURL_; };
    std::vector<std::string> getRingAccounts();

private:
    NON_COPYABLE(RingDevice);

    // root device info
    std::string UDN_ {}; // used to uniquely identify this UPnP device
    std::string deviceType_ {};
    std::string friendlyName_ {};
    std::string baseURL_ {};
    std::string relURL_ {};
    std::string ringAccounts_ {};

};

}} // namespace ring::upnp
