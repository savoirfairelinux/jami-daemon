/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include "mapping.h"
#include "logger.h"


#define LOCK_MUTEX(mutexToLock)  \
    std::lock_guard<std::mutex> lock(mutexToLock);

namespace jami {
namespace upnp {

const std::string Mapping::MAPPING_STATE_STR[] = { "NEW", "PENDING", "IN_PROGRESS", "FAILED", "OPEN" };
const std::string Mapping::UPNP_DEFAULT_MAPPING_DESCRIPTION = "JAMI";

Mapping::Mapping(uint16_t portExternal,
                 uint16_t portInternal,
                 PortType type,
                 bool available)
    : portExternal_(portExternal)
    , portInternal_(portInternal)
    , type_(type)
    , available_(available)
    , state_(MappingState::NEW)
    , notifyCb_(nullptr)
    , timeoutTimer_(nullptr)
    {};

Mapping::Mapping(Mapping&& other) noexcept
    : portExternal_(other.portExternal_)
    , portInternal_(other.portInternal_)
    , type_(other.type_)
    , available_(other.available_)
    , state_(other.state_)
    , notifyCb_(std::move(other.notifyCb_))
    , timeoutTimer_(std::move(other.timeoutTimer_))
#if HAVE_LIBNATPMP
    , renewal_(other.renewal_)
#endif
{
    other.portExternal_ = 0;
    other.portInternal_ = 0;
}

Mapping::Mapping(const Mapping& other)
{
    LOCK_MUTEX(mutex_);
    std::lock_guard<std::mutex> lockOther(other.mutex_);

    portExternal_ = other.portExternal_;
    portInternal_ = other.portInternal_;
    type_ = other.type_;
    available_ = other.available_;
    state_ = other.state_;
    notifyCb_ = other.notifyCb_;
    timeoutTimer_ = other.timeoutTimer_;
#if HAVE_LIBNATPMP
    renewal_ = other.renewal_;
#endif
}

Mapping&
Mapping::operator=(Mapping&& other) noexcept
{
    LOCK_MUTEX(mutex_);

    if (this != &other) {
        portExternal_ = other.portExternal_;
        other.portExternal_ = 0;
        portInternal_ = other.portInternal_;
        other.portInternal_ = 0;
        type_ = other.type_;
        available_ = other.available_;
        other.available_ = false;
        state_ = other.state_;
        other.state_ = MappingState::NEW;
        notifyCb_ = std::move(other.notifyCb_);
        other.notifyCb_ = nullptr;
        timeoutTimer_ = std::move(other.timeoutTimer_);
        other.timeoutTimer_ = nullptr;

#if HAVE_LIBNATPMP
        renewal_ = other.renewal_;
#endif
    }
    return *this;
}

bool
Mapping::operator==(const Mapping& other) const noexcept
{
    LOCK_MUTEX(mutex_);
    std::lock_guard<std::mutex> lockOther(other.mutex_);

    // TODO. Must have the same IGD !
    return (portExternal_ == other.portExternal_ && portInternal_ == other.portInternal_
            && type_ == other.type_);
}

bool
Mapping::operator!=(const Mapping& other) const noexcept
{
    return not (*this == other);
}

void
Mapping::setAvailable(bool val)
{
    JAMI_DBG("Changing mapping %s state from %s to %s",
        toString().c_str(),
        available_ ? "AVAILABLE":"UNAVAILABLE", val ? "AVAILABLE":"UNAVAILABLE");

    LOCK_MUTEX(mutex_);
    available_ = val;
}

void
Mapping::setState(const MappingState& state)
{
    {
        LOCK_MUTEX(mutex_);
        if (state_ == state)
            return;
    }

    JAMI_DBG("Changed mapping %s state from %s to %s",
        toString().c_str(), getStateStr().c_str(), getStateStr(state).c_str());

    LOCK_MUTEX(mutex_);
    state_ = state;
}

const std::string& Mapping::getStateStr() const
{
    LOCK_MUTEX(mutex_);
    return getStateStr(state_);
}

std::string
Mapping::toString() const
{
    return getPortExternalStr() + ":" + getPortInternalStr() + " [" + getTypeStr() + "]" +
        " \"" + getDescription() + "\"";
}

bool
Mapping::isValid() const
{
    LOCK_MUTEX(mutex_);
    return portExternal_ == 0 or portInternal_ == 0 ? false : true;
}
void Mapping::setTimeoutTimer(std::shared_ptr<Task> timer)
{
    // Cancel current timer if any.
    cancelTimeoutTimer();

    LOCK_MUTEX(mutex_);
    timeoutTimer_ = timer;
}

void Mapping::cancelTimeoutTimer()
{
    LOCK_MUTEX(mutex_);

    if (timeoutTimer_ != nullptr) {
        timeoutTimer_->cancel();
        timeoutTimer_ = nullptr;
    }
}

Mapping::key_t Mapping::getMapKey() const
{
    LOCK_MUTEX(mutex_);

    key_t mapKey = portExternal_ << 1;
    if (type_ == PortType::UDP)
        mapKey |= 1;
    return mapKey;
}

PortType Mapping::getTypeFromMapKey(key_t key) {
    return key % 2 ? PortType::UDP : PortType::TCP;
}

uint16_t Mapping::getPortFromMapKey(key_t key) {
    return static_cast<uint16_t>(key >> 1);
}

void Mapping::setPortExternal(uint16_t port)
{
    LOCK_MUTEX(mutex_);
    portExternal_ = port;
}

uint16_t Mapping::getPortExternal() const
{
    LOCK_MUTEX(mutex_);
    return portExternal_;
}

std::string Mapping::getPortExternalStr() const
{
    LOCK_MUTEX(mutex_);
    return std::to_string(portExternal_);
}

void Mapping::setPortInternal(uint16_t port)
{
    LOCK_MUTEX(mutex_);
    portInternal_ = port;
}

uint16_t Mapping::getPortInternal() const
{
    LOCK_MUTEX(mutex_);
    return portInternal_;
}

std::string Mapping::getPortInternalStr() const
{
    LOCK_MUTEX(mutex_);
    return std::to_string(portInternal_);
}

PortType Mapping::getType() const
{
    LOCK_MUTEX(mutex_);
    return type_;
}

std::string Mapping::getTypeStr() const
{
    LOCK_MUTEX(mutex_);
    return getTypeStr(type_);
}

bool Mapping::isAvailable() const
{
    LOCK_MUTEX(mutex_);
    return available_;
}

const MappingState& Mapping::getState() const
{
    LOCK_MUTEX(mutex_);
    return state_;
}

Mapping::NotifyCallback Mapping::getNotifyCallback() const
{
    LOCK_MUTEX(mutex_);
    return notifyCb_;
}

void Mapping::setNotifyCallback(NotifyCallback cb)
{
    LOCK_MUTEX(mutex_);
    notifyCb_ = cb;
}

#if HAVE_LIBNATPMP
std::chrono::system_clock::time_point Mapping::getRenewal() const
{
    LOCK_MUTEX(mutex_);
    return renewal_;
}

void Mapping::setRenewal(std::chrono::system_clock::time_point time)
{
    LOCK_MUTEX(mutex_);
    renewal_ = time;
}
#endif

} // namespace upnp
} // namespace jami
