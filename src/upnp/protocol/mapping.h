/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Eden Abitbol <eden.abitbol@savoirfairelinux.com>
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

#include "noncopyable.h"
#include "ip_utils.h"
#include "string_utils.h"
#include "scheduled_executor.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <mutex>

namespace jami {
namespace upnp {


enum class PortType { UDP, TCP };

enum class MappingState { NEW, PENDING, IN_PROGRESS, FAILED, OPEN };

class Mapping
{

public:
    using key_t = u_int64_t;
    using sharedPtr_t = std::shared_ptr<Mapping>;
    using NotifyCallback = std::function<void(sharedPtr_t)>;

    static const std::string MAPPING_STATE_STR[];
    static const std::string UPNP_DEFAULT_MAPPING_DESCRIPTION;
    constexpr static uint16_t UPNP_PORT_MIN = 20000;
    constexpr static uint16_t UPNP_PORT_MAX = 20100;

    Mapping(uint16_t portExternal = 0,
            uint16_t portInternal = 0,
            PortType type = PortType::UDP,
            bool available = false);
    Mapping(Mapping&& other) noexcept;
    Mapping(const Mapping& other);
    ~Mapping() = default;

    // TODO. Use shared_ptr every and delete copy constructors

    Mapping& operator=(Mapping&& other) noexcept;
    bool operator==(const Mapping& other) const noexcept;
    bool operator!=(const Mapping& other) const noexcept;

    // Delete operators with confusing semantic.
    bool operator<(const Mapping& other) = delete;
    bool operator>(const Mapping& other) = delete;
    bool operator<=(const Mapping& other) = delete;
    bool operator>=(const Mapping& other) = delete;

    inline explicit operator bool() const { return isValid(); }

    void setPortExternal(uint16_t port);
    uint16_t getPortExternal() const;
    std::string getPortExternalStr() const;
    void setPortInternal(uint16_t port);
    uint16_t getPortInternal() const;
    std::string getPortInternalStr() const;
    PortType getType() const;
    std::string getTypeStr() const;
    static const std::string getTypeStr(PortType type) { return type == PortType::UDP ? "UDP" : "TCP"; }
    bool isAvailable() const;
    void setAvailable(bool val);
    const MappingState& getState() const;
    void setState(const MappingState& state);
    const std::string& getStateStr() const;
    static const std::string& getStateStr(MappingState state) {
        return MAPPING_STATE_STR[static_cast<int>(state)];}
    static const std::string& getDescription() { return UPNP_DEFAULT_MAPPING_DESCRIPTION; }
    std::string toString() const;
    bool isValid() const;
    NotifyCallback getNotifyCallback() const;
    void setNotifyCallback(NotifyCallback cb);
    void setTimeoutTimer(std::shared_ptr<Task> timer);
    void cancelTimeoutTimer();
    key_t getMapKey() const;
    static PortType getTypeFromMapKey(key_t key);
    static uint16_t getPortFromMapKey(key_t key);
#if HAVE_LIBNATPMP
    std::chrono::system_clock::time_point getRenewal() const;
    void setRenewal(std::chrono::system_clock::time_point time);
#endif

private:
protected:

    mutable std::mutex mutex_;
    uint16_t portExternal_;
    uint16_t portInternal_;
    PortType type_;
    // Track if the mapping is available to use.
    bool available_;
    // Track the state of the mapping
    MappingState state_;
    NotifyCallback notifyCb_;
    std::shared_ptr<Task> timeoutTimer_;
#if HAVE_LIBNATPMP
    std::chrono::system_clock::time_point renewal_ { std::chrono::system_clock::time_point::min() };
#endif
};

} // namespace upnp
} // namespace jami
