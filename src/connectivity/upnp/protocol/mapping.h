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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "noncopyable.h"
#include "connectivity/ip_utils.h"
#include "scheduled_executor.h"
#include "igd.h"

#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <mutex>

namespace jami {
namespace upnp {

using sys_clock = std::chrono::system_clock;

enum class PortType { TCP, UDP };
enum class MappingState { PENDING, IN_PROGRESS, FAILED, OPEN };

enum class NatProtocolType;
class IGD;

class Mapping
{
    friend class UPnPContext;
    friend class NatPmp;
    friend class PUPnP;

public:
    using key_t = uint64_t;
    using sharedPtr_t = std::shared_ptr<Mapping>;
    using NotifyCallback = std::function<void(sharedPtr_t)>;

    static constexpr char const* MAPPING_STATE_STR[4] {"PENDING", "IN_PROGRESS", "FAILED", "OPEN"};
    static constexpr char const* UPNP_MAPPING_DESCRIPTION_PREFIX {"JAMI"};

    Mapping(PortType type,
            uint16_t portExternal = 0,
            uint16_t portInternal = 0,
            bool available = true);
    Mapping(const Mapping& other);
    Mapping(Mapping&& other) = delete;
    ~Mapping() = default;

    // Delete operators with confusing semantic.
    Mapping& operator=(Mapping&& other) = delete;
    bool operator==(const Mapping& other) = delete;
    bool operator!=(const Mapping& other) = delete;
    bool operator<(const Mapping& other) = delete;
    bool operator>(const Mapping& other) = delete;
    bool operator<=(const Mapping& other) = delete;
    bool operator>=(const Mapping& other) = delete;

    inline explicit operator bool() const { return isValid(); }

    void updateFrom(const Mapping& other);
    void updateFrom(const Mapping::sharedPtr_t& other);
    std::string getExternalAddress() const;
    uint16_t getExternalPort() const;
    std::string getExternalPortStr() const;
    std::string getInternalAddress() const;
    uint16_t getInternalPort() const;
    std::string getInternalPortStr() const;
    PortType getType() const;
    const char* getTypeStr() const;
    static const char* getTypeStr(PortType type) { return type == PortType::UDP ? "UDP" : "TCP"; }
    std::shared_ptr<IGD> getIgd() const;
    NatProtocolType getProtocol() const;
    const char* getProtocolName() const;
    bool isAvailable() const;
    MappingState getState() const;
    const char* getStateStr() const;
    static const char* getStateStr(MappingState state)
    {
        return MAPPING_STATE_STR[static_cast<int>(state)];
    }
    std::string toString(bool extraInfo = false) const;
    bool isValid() const;
    bool hasValidHostAddress() const;
    bool hasPublicAddress() const;
    void setNotifyCallback(NotifyCallback cb);
    void enableAutoUpdate(bool enable);
    bool getAutoUpdate() const;
    key_t getMapKey() const;
    static PortType getTypeFromMapKey(key_t key);
#if HAVE_LIBNATPMP
    sys_clock::time_point getRenewalTime() const;
#endif

private:
    NotifyCallback getNotifyCallback() const;
    void setInternalAddress(const std::string& addr);
    void setExternalPort(uint16_t port);
    void setInternalPort(uint16_t port);

    void setIgd(const std::shared_ptr<IGD>& igd);
    void setAvailable(bool val);
    void setState(const MappingState& state);
    void updateDescription();
#if HAVE_LIBNATPMP
    void setRenewalTime(sys_clock::time_point time);
#endif

    mutable std::mutex mutex_;
    PortType type_ {PortType::UDP};
    uint16_t externalPort_ {0};
    uint16_t internalPort_ {0};
    std::string internalAddr_;
    // Protocol and
    std::shared_ptr<IGD> igd_;
    // Track if the mapping is available to use.
    bool available_;
    // Track the state of the mapping
    MappingState state_;
    NotifyCallback notifyCb_;
    // If true, a new mapping will be requested on behave of the mapping
    // owner when the mapping state changes from "OPEN" to "FAILED".
    bool autoUpdate_;
#if HAVE_LIBNATPMP
    sys_clock::time_point renewalTime_;
#endif
};

} // namespace upnp
} // namespace jami
