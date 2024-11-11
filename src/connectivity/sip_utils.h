/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "connectivity/ip_utils.h"
#include "media/media_codec.h"
#include "noncopyable.h"

#include <utility>
#include <string>
#include <vector>
#include <cstring> // strcmp

#include <dhtnet/ip_utils.h>

#include <pjsip/sip_msg.h>
#include <pjlib.h>
#include <pj/pool.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>

namespace jami {
namespace sip_utils {

using namespace std::literals;

// SIP methods. Only list methods that need to be explicitly
// handled

namespace SIP_METHODS {
constexpr std::string_view MESSAGE = "MESSAGE"sv;
constexpr std::string_view INFO = "INFO"sv;
constexpr std::string_view OPTIONS = "OPTIONS"sv;
constexpr std::string_view PUBLISH = "PUBLISH"sv;
constexpr std::string_view REFER = "REFER"sv;
constexpr std::string_view NOTIFY = "NOTIFY"sv;
} // namespace SIP_METHODS

static constexpr int DEFAULT_SIP_PORT {5060};
static constexpr int DEFAULT_SIP_TLS_PORT {5061};
static constexpr int DEFAULT_AUTO_SELECT_PORT {0};

/// PjsipErrorCategory - a PJSIP error category for std::error_code
class PjsipErrorCategory final : public std::error_category
{
public:
    const char* name() const noexcept override { return "pjsip"; }
    std::string message(int condition) const override;
};

/// PJSIP related exception
/// Based on std::system_error with code() returning std::error_code with PjsipErrorCategory category
class PjsipFailure : public std::system_error
{
private:
    static constexpr const char* what_ = "PJSIP call failed";

public:
    PjsipFailure()
        : std::system_error(std::error_code(PJ_EUNKNOWN, PjsipErrorCategory()), what_)
    {}

    explicit PjsipFailure(pj_status_t status)
        : std::system_error(std::error_code(status, PjsipErrorCategory()), what_)
    {}
};

std::string sip_strerror(pj_status_t code);

// Helper function that return a constant pj_str_t from an array of any types
// that may be statically casted into char pointer.
// Per convention, the input array is supposed to be null terminated.
template<typename T, std::size_t N>
constexpr const pj_str_t
CONST_PJ_STR(T (&a)[N]) noexcept
{
    return {const_cast<char*>(a), N - 1};
}

inline const pj_str_t
CONST_PJ_STR(const std::string& str) noexcept
{
    return {const_cast<char*>(str.c_str()), (pj_ssize_t) str.size()};
}

inline constexpr pj_str_t
CONST_PJ_STR(const std::string_view& str) noexcept
{
    return {const_cast<char*>(str.data()), (pj_ssize_t) str.size()};
}

inline constexpr std::string_view
as_view(const pj_str_t& str) noexcept
{
    return {str.ptr, (size_t) str.slen};
}

static constexpr const char*
getKeyExchangeName(KeyExchangeProtocol kx)
{
    return kx == KeyExchangeProtocol::SDES ? "sdes" : "";
}

static inline KeyExchangeProtocol
getKeyExchangeProtocol(std::string_view name)
{
    return name == "sdes"sv ? KeyExchangeProtocol::SDES : KeyExchangeProtocol::NONE;
}

/**
 * Helper function to parser header from incoming sip messages
 * @return Header from SIP message
 */
std::string fetchHeaderValue(pjsip_msg* msg, const std::string& field);

pjsip_route_hdr* createRouteSet(const std::string& route, pj_pool_t* hdr_pool);

std::string_view stripSipUriPrefix(std::string_view sipUri);

std::string parseDisplayName(const pjsip_name_addr* sip_name_addr);
std::string parseDisplayName(const pjsip_from_hdr* header);
std::string parseDisplayName(const pjsip_contact_hdr* header);

std::string_view getHostFromUri(std::string_view sipUri);

void addContactHeader(const std::string& contact, pjsip_tx_data* tdata);
void addUserAgentHeader(const std::string& userAgent, pjsip_tx_data* tdata);
std::string_view getPeerUserAgent(const pjsip_rx_data* rdata);
std::vector<std::string> getPeerAllowMethods(const pjsip_rx_data* rdata);
void logMessageHeaders(const pjsip_hdr* hdr_list);

constexpr std::string_view DEFAULT_VIDEO_STREAMID = "video_0";
constexpr std::string_view DEFAULT_AUDIO_STREAMID = "audio_0";

std::string streamId(const std::string& callId, std::string_view label);

// PJSIP dialog locking in RAII way
// Usage: declare local variable like this: sip_utils::PJDialogLock lock {dialog};
// The lock is kept until the local variable is deleted
class PJDialogLock
{
public:
    explicit PJDialogLock(pjsip_dialog* dialog)
        : dialog_(dialog)
    {
        pjsip_dlg_inc_lock(dialog_);
    }

    ~PJDialogLock() { pjsip_dlg_dec_lock(dialog_); }

private:
    NON_COPYABLE(PJDialogLock);
    pjsip_dialog* dialog_ {nullptr};
};

// Helper on PJSIP memory pool allocation from endpoint
// This encapsulate the allocated memory pool inside a unique_ptr
static inline std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&>
smart_alloc_pool(pjsip_endpoint* endpt, const char* const name, pj_size_t initial, pj_size_t inc)
{
    auto pool = pjsip_endpt_create_pool(endpt, name, initial, inc);
    if (not pool)
        throw std::bad_alloc();
    return std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&>(pool, pj_pool_release);
}

void sockaddr_to_host_port(pj_pool_t* pool, pjsip_host_port* host_port, const pj_sockaddr* addr);

static constexpr int POOL_TP_INIT {512};
static constexpr int POOL_TP_INC {512};
static constexpr int TRANSPORT_INFO_LENGTH {64};

} // namespace sip_utils
} // namespace jami
