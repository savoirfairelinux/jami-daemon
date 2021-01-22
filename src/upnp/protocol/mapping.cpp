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

namespace jami {
namespace upnp {

Mapping::Mapping(uint16_t portExternal, uint16_t portInternal, PortType type, bool available)
    : externalAddr_()
    , internalAddr_()
    , type_(type)
    , igd_()
    , available_(available)
    , state_(MappingState::PENDING)
    , notifyCb_(nullptr)
    , timeoutTimer_(nullptr)
    , autoUpdate_(false)
#if HAVE_LIBNATPMP
    , renewalTime_(sys_clock::now())
#endif
{
    externalAddr_.port_ = portExternal;
    internalAddr_.port_ = portInternal;
}

Mapping::Mapping(Mapping&& other) noexcept
    : externalAddr_(std::move(other.externalAddr_))
    , internalAddr_(std::move(other.internalAddr_))
    , type_(other.type_)
    , igd_(other.igd_)
    , available_(other.available_)
    , state_(other.state_)
    , notifyCb_(std::move(other.notifyCb_))
    , timeoutTimer_(std::move(other.timeoutTimer_))
    , autoUpdate_(other.autoUpdate_)
#if HAVE_LIBNATPMP
    , renewalTime_(other.renewalTime_)
#endif
{}

Mapping::Mapping(const Mapping& other)
{
    std::lock_guard<std::mutex> lock(mutex_);

    externalAddr_ = other.externalAddr_;
    internalAddr_ = other.internalAddr_;
    type_ = other.type_;
    igd_ = other.igd_;
    available_ = other.available_;
    state_ = other.state_;
    notifyCb_ = other.notifyCb_;
    timeoutTimer_ = other.timeoutTimer_;
    autoUpdate_ = other.autoUpdate_;
#if HAVE_LIBNATPMP
    renewalTime_ = other.renewalTime_;
#endif
}

Mapping&
Mapping::operator=(Mapping&& other) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (this != &other) {
        externalAddr_ = other.externalAddr_;
        internalAddr_ = other.internalAddr_;
        type_ = other.type_;
        igd_ = other.igd_;
        other.igd_.reset();
        available_ = other.available_;
        other.available_ = false;
        state_ = other.state_;
        other.state_ = MappingState::PENDING;
        notifyCb_ = std::move(other.notifyCb_);
        other.notifyCb_ = nullptr;
        timeoutTimer_ = std::move(other.timeoutTimer_);
        other.timeoutTimer_ = nullptr;
        autoUpdate_ = other.autoUpdate_;
        other.autoUpdate_ = false;
#if HAVE_LIBNATPMP
        renewalTime_ = other.renewalTime_;
#endif
    }
    return *this;
}

bool
Mapping::operator==(const Mapping& other) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (externalAddr_.addr_ != other.externalAddr_.addr_)
        return false;
    if (externalAddr_.port_ != other.externalAddr_.port_)
        return false;
    if (internalAddr_.addr_ != other.internalAddr_.addr_)
        return false;
    if (internalAddr_.port_ != other.internalAddr_.port_)
        return false;
    if (type_ != other.type_)
        return false;
    return true;
}

bool
Mapping::operator!=(const Mapping& other) const noexcept
{
    return not(*this == other);
}

void
Mapping::setAvailable(bool val)
{
    JAMI_DBG("Changing mapping %s state from %s to %s",
             toString().c_str(),
             available_ ? "AVAILABLE" : "UNAVAILABLE",
             val ? "AVAILABLE" : "UNAVAILABLE");

    std::lock_guard<std::mutex> lock(mutex_);
    available_ = val;
}

void
Mapping::setState(const MappingState& state)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == state)
            return;
    }

    JAMI_DBG("Changed mapping %s state from %s to %s",
             toString().c_str(),
             getStateStr(),
             getStateStr(state));

    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

const char*
Mapping::getStateStr() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return getStateStr(state_);
}

std::string
Mapping::toString(bool addState) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream descr;
    descr << UPNP_MAPPING_DESCRIPTION_PREFIX << "-" << getTypeStr(type_);
    descr << ":" << std::to_string(externalAddr_.port_);

    if (addState)
        descr << " (state=" << getStateStr(state_) << ")";

    return descr.str();
}

bool
Mapping::isValid() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (externalAddr_.port_ == 0)
        return false;
    if (internalAddr_.port_ == 0)
        return false;
    if (state_ == MappingState::FAILED)
        return false;
    return true;
}

void
Mapping::setTimeoutTimer(std::shared_ptr<Task> timer)
{
    // Cancel current timer if any.
    cancelTimeoutTimer();

    std::lock_guard<std::mutex> lock(mutex_);
    timeoutTimer_ = std::move(timer);
}

void
Mapping::cancelTimeoutTimer()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (timeoutTimer_ != nullptr) {
        timeoutTimer_->cancel();
        timeoutTimer_ = nullptr;
    }
}

Mapping::key_t
Mapping::getMapKey() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    key_t mapKey = externalAddr_.port_;
    if (type_ == PortType::UDP)
        mapKey |= 1 << (sizeof(uint16_t) * 8);
    return mapKey;
}

PortType
Mapping::getTypeFromMapKey(key_t key)
{
    return (key >> (sizeof(uint16_t) * 8)) ? PortType::UDP : PortType::TCP;
}

void
Mapping::setExternalAddress(const std::string& addr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    externalAddr_.addr_ = addr;
}

std::string
Mapping::getExternalAddress() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return externalAddr_.addr_;
}

void
Mapping::setExternalPort(uint16_t port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    externalAddr_.port_ = port;
}

uint16_t
Mapping::getExternalPort() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return externalAddr_.port_;
}

std::string
Mapping::getExternalPortStr() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::to_string(externalAddr_.port_);
}

void
Mapping::setInternalAddress(const std::string& addr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    internalAddr_.addr_ = addr;
}

std::string
Mapping::getInternalAddress() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return internalAddr_.addr_;
}

void
Mapping::setInternalPort(uint16_t port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    internalAddr_.port_ = port;
}

uint16_t
Mapping::getInternalPort() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return internalAddr_.port_;
}

std::string
Mapping::getInternalPortStr() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::to_string(internalAddr_.port_);
}

PortType
Mapping::getType() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return type_;
}

const char*
Mapping::getTypeStr() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return getTypeStr(type_);
}

bool
Mapping::isAvailable() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return available_;
}

std::shared_ptr<IGD>
Mapping::getIgd() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return igd_;
}

NatProtocolType
Mapping::getProtocol() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (igd_)
        return igd_->getProtocol();
    return NatProtocolType::UNKNOWN;
}
const char*
Mapping::getProtocolName() const
{
    if (igd_) {
        if (igd_->getProtocol() == NatProtocolType::NAT_PMP)
            return "NAT-PMP";
        if (igd_->getProtocol() == NatProtocolType::PUPNP)
            return "PUPNP";
    }
    return "UNKNOWN";
}

void
Mapping::setIgd(const std::shared_ptr<IGD>& igd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    igd_ = igd;
}

MappingState
Mapping::getState() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

Mapping::NotifyCallback
Mapping::getNotifyCallback() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return notifyCb_;
}

void
Mapping::setNotifyCallback(NotifyCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    notifyCb_ = std::move(cb);
}

void
Mapping::enableAutoUpdate(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    autoUpdate_ = enable;
}

bool
Mapping::getAutoUpdate() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return autoUpdate_;
}

#if HAVE_LIBNATPMP
sys_clock::time_point
Mapping::getRenewalTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return renewalTime_;
}

void
Mapping::setRenewalTime(sys_clock::time_point time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    renewalTime_ = time;
}
#endif

} // namespace upnp
} // namespace jami
