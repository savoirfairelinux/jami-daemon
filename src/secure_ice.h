/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "ice_transport.h"

#include <opendht/crypto.h>
#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>
#include <gnutls/abstract.h>

#include <string>
#include <memory>
#include <chrono>
#include <functional>

namespace ring { namespace ice {

enum class TlsConnectionState {
    DISCONNECTED,
    COOKIE,
    HANDSHAKING,
    ESTABLISHED
};

struct TlsParams {
    std::string ca_list;
    dht::crypto::Identity id;
    std::shared_future<std::unique_ptr<gnutls_dh_params_int, decltype(gnutls_dh_params_deinit)&>> dh_params;
    std::chrono::steady_clock::duration timeout;
    std::function<int>(unsigned status, const gnutls_datum_t* cert_list, unsigned cert_list_size)> cert_check;
};

class SecureIceTransport : public IceTransport
{
public:
    using clock = std::chrono::steady_clock;
    using TransportData = struct {
        pjsip_transport base; // DO NOT MOVE, SHOULD be the fist struct member
        SipsIceTransport* self {nullptr};
    };
    static_assert(std::is_standard_layout<TransportData>::value,
                  "TranportData requires standard-layout");

    SecureIceTransport(const char* name, int component_count, bool master,
                       const TlsParams& tls_param, const IceTransportOptions& options = {});

    ~SecureIceTransport();

    void stop();

    std::shared_ptr<SecureIceTransport> getSharedPtr() { return shared_from_this(); }

    // I/O methods

    void setOnRecv(unsigned comp_id, IceRecvCb cb);

    ssize_t recv(int comp_id, uint8_t* buf, size_t len);

    ssize_t send(int comp_id, const uint8_t* buf, size_t len);

    ssize_t getNextPacketSize(int comp_id);

    int waitForInitialization(unsigned timeout);

    int waitForNegotiation(unsigned timeout);

    ssize_t waitForData(int comp_id, unsigned int timeout);

private:
    using DelayedTxData = struct {
        pjsip_tx_data_op_key *tdata_op_key;
        clock::time_point timeout;
    };

    void handleEvents();

    // ThreadLoop
    bool setup();
    void loop();
    void clean();

    TlsParams param_;

    gnutls_session_t session_ {nullptr};
    std::unique_ptr<gnutls_certificate_credentials_st, decltype(gnutls_certificate_free_credentials)&> xcred_;
    gnutls_priority_t priority_cache_ {nullptr};
    gnutls_datum_t cookie_key_ {nullptr, 0};
    gnutls_dtls_prestate_st prestate_;

    std::condition_variable_any cv_;
    std::atomic<bool> canRead_ {false};
    std::atomic<bool> canWrite_ {false};

    int last_err_;

    std::atomic<TlsConnectionState> state_ {TlsConnectionState::DISCONNECTED};
    clock::time_point handshakeStart_;

    std::list<DelayedTxData> outputBuff_;
    std::list<std::pair<DelayedTxData, ssize_t>> outputAckBuff_; // acknowledged outputBuff_
    std::list<std::vector<uint8_t>> txBuff_;
    std::mutex outputBuffMtx_;

    std::mutex rxMtx_;
    std::condition_variable_any rxCv_;
    std::list<std::vector<uint8_t>> rxPending_;

    // data buffer pool
    std::list<std::vector<uint8_t>> buffPool_;
    std::mutex buffPoolMtx_;
    void getBuff(decltype(buffPool_)& l, const uint8_t* b, const uint8_t* e);
    void getBuff(decltype(buffPool_)& l, const size_t s);
    void putBuff(decltype(buffPool_)&& l);

    // GnuTLS <-> ICE
    ssize_t tlsSend(const void*, size_t);
    ssize_t tlsRecv(void* d , size_t s);
    int waitForTlsData(unsigned ms);
    std::mutex inputBuffMtx_;
    std::list<std::vector<uint8_t>> tlsInputBuff_;

    pj_status_t startTlsSession();
    void closeTlsSession();

    pj_status_t tryHandshake();
    void certUpdate();
    int verifyCertificate();

    ThreadLoop tlsThread_;
};

}} // namespace ring::ice
