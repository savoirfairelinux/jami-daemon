/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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

#include <mutex>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "noncopyable.h"
#include "connectivity/ip_utils.h"

#include "connectivity/upnp/protocol/mapping.h"

#ifdef _MSC_VER
typedef uint16_t in_port_t;
#endif

namespace jami {
namespace upnp {

enum class NatProtocolType { UNKNOWN, PUPNP, NAT_PMP };

class IGD
{
public:
    // Max error before moving the IGD to invalid state.
    constexpr static int MAX_ERRORS_COUNT = 10;

    IGD(NatProtocolType prot);
    virtual ~IGD() = default;
    bool operator==(IGD& other) const;
    IGD& operator=(IGD&& other) = delete;
    IGD& operator=(IGD& other) = delete;

    NatProtocolType getProtocol() const { return protocol_; }

    char const* getProtocolName() const
    {
        return protocol_ == NatProtocolType::NAT_PMP ? "NAT-PMP" : "UPNP";
    };

    IpAddr getLocalIp() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return localIp_;
    }
    IpAddr getPublicIp() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return publicIp_;
    }
    void setLocalIp(const IpAddr& addr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        localIp_ = addr;
    }
    void setPublicIp(const IpAddr& addr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        publicIp_ = addr;
    }
    void setUID(const std::string& uid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uid_ = uid;
    }
    std::string getUID() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return uid_;
    }

    void setValid(bool valid);
    bool isValid() const { return valid_; }
    bool incrementErrorsCounter();
    int getErrorsCount() const;

    virtual const std::string toString() const = 0;

protected:
    const NatProtocolType protocol_ {NatProtocolType::UNKNOWN};
    std::atomic_bool valid_ {false};
    std::atomic<int> errorsCounter_ {0};

    mutable std::mutex mutex_;
    IpAddr localIp_ {};  // Local IP of the IGD (typically the same as the gateway).
    IpAddr publicIp_ {}; // External/public IP of IGD.
    std::string uid_ {};

private:
    NON_COPYABLE(IGD);
};

} // namespace upnp
} // namespace jami
