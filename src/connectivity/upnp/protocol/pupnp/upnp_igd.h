/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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

#include "connectivity/upnp/protocol/igd.h"

#include "noncopyable.h"
#include "connectivity/ip_utils.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>

namespace jami {
namespace upnp {

class UPnPIGD : public IGD
{
public:
    UPnPIGD(std::string&& UDN,
            std::string&& baseURL,
            std::string&& friendlyName,
            std::string&& serviceType,
            std::string&& serviceId,
            std::string&& locationURL,
            std::string&& controlURL,
            std::string&& eventSubURL,
            IpAddr&& localIp = {},
            IpAddr&& publicIp = {});

    ~UPnPIGD() {}

    bool operator==(IGD& other) const;
    bool operator==(UPnPIGD& other) const;

    const std::string& getBaseURL() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return baseURL_;
    }
    const std::string& getFriendlyName() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return friendlyName_;
    }
    const std::string& getServiceType() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return serviceType_;
    }
    const std::string& getServiceId() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return serviceId_;
    }
    const std::string& getLocationURL() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return locationURL_;
    }
    const std::string& getControlURL() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return controlURL_;
    }
    const std::string& getEventSubURL() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return eventSubURL_;
    }

    const std::string toString() const override { return controlURL_; }

private:
    std::string baseURL_ {};
    std::string friendlyName_ {};
    std::string serviceType_ {};
    std::string serviceId_ {};
    std::string locationURL_ {};
    std::string controlURL_ {};
    std::string eventSubURL_ {};
};

} // namespace upnp
} // namespace jami
