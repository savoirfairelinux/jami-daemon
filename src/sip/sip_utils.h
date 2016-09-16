/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef SIP_UTILS_H_
#define SIP_UTILS_H_

#include "ip_utils.h"
#include "media_codec.h"
#include "media/audio/audiobuffer.h"

#include <pjsip/sip_msg.h>
#include <pjlib.h>
#include <pj/pool.h>
#include <pjsip/sip_endpoint.h>

#include <utility>
#include <string>
#include <vector>
#include <cstring> // strcmp

struct pjsip_msg;
struct pjsip_dialog;

namespace ring { namespace sip_utils {

static constexpr int DEFAULT_SIP_PORT {5060};
static constexpr int DEFAULT_SIP_TLS_PORT {5061};

enum class KeyExchangeProtocol { NONE, SDES };

static constexpr const char* getKeyExchangeName(KeyExchangeProtocol kx) {
    return kx == KeyExchangeProtocol::SDES ? "sdes" : "";
}

static inline KeyExchangeProtocol getKeyExchangeProtocol(const char* name) {
    return !std::strcmp("sdes", name) ? KeyExchangeProtocol::SDES : KeyExchangeProtocol::NONE;
}

/**
 * Helper function to parser header from incoming sip messages
 * @return Header from SIP message
 */
std::string fetchHeaderValue(pjsip_msg *msg, const std::string &field);

pjsip_route_hdr *
createRouteSet(const std::string &route, pj_pool_t *hdr_pool);

void stripSipUriPrefix(std::string& sipUri);

std::string parseDisplayName(const pjsip_name_addr* sip_name_addr);
std::string parseDisplayName(const pjsip_from_hdr* header);
std::string parseDisplayName(const pjsip_contact_hdr* header);

std::string getHostFromUri(const std::string& sipUri);

void addContactHeader(const pj_str_t *contactStr, pjsip_tx_data *tdata);

std::string sip_strerror(pj_status_t code);

// Helper function that return a constant pj_str_t from an array of any types
// that may be staticaly casted into char pointer.
// Per convention, the input array is supposed to be null terminated.
template<typename T, std::size_t N>
constexpr const pj_str_t CONST_PJ_STR(T (&a)[N]) noexcept {
    return {const_cast<char*>(a), N-1};
}

// PJSIP dialog locking in RAII way
// Usage: declare local variable like this: sip_utils::PJDialogLock lock {dialog};
// The lock is kept until the local variable is deleted
class PJDialogLock {
public:
    explicit PJDialogLock(pjsip_dialog* dialog);
    ~PJDialogLock();
    PJDialogLock() = delete;
    PJDialogLock(const PJDialogLock&) = delete; // enough to disable all cp/mv stuff

private:
    pjsip_dialog* dialog_;
};

// Helper on PJSIP memory pool allocation from endpoint
// This encapsulate the allocated memory pool inside a unique_ptr
static inline std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&>
smart_alloc_pool(pjsip_endpoint* endpt, const char* const name, pj_size_t initial, pj_size_t inc) {
    auto pool = pjsip_endpt_create_pool(endpt, name, initial, inc);
    if (not pool)
        throw std::bad_alloc();
    return std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&>(pool, pj_pool_release);
}

}} // namespace ring::sip_utils

#endif // SIP_UTILS_H_
