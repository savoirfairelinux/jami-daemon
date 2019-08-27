/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "ice_socket.h"
#include "ip_utils.h"

#include <pjnath.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <functional>
#include <memory>
#include <msgpack.hpp>
#include <vector>

namespace jami {

namespace upnp {
class Controller;
}

class IceTransport;

using IceTransportCompleteCb = std::function<void(bool)>;
using IceRecvInfo = std::function<void(void)>;
using IceRecvCb = std::function<ssize_t(unsigned char *buf, size_t len)>;
using IceCandidate = pj_ice_sess_cand;

struct StunServerInfo {
    StunServerInfo& setUri(const std::string& args) { uri = args; return *this; }

    std::string uri;      // server URI, mandatory
};

struct TurnServerInfo {
    TurnServerInfo& setUri(const std::string& args) { uri = args; return *this; }
    TurnServerInfo& setUsername(const std::string& args) { username = args; return *this; }
    TurnServerInfo& setPassword(const std::string& args) { password = args; return *this; }
    TurnServerInfo& setRealm(const std::string& args) { realm = args; return *this; }

    std::string uri;      // server URI, mandatory
    std::string username; // credentials username (optional, empty if not used)
    std::string password; // credentials password (optional, empty if not used)
    std::string realm;    // credentials realm (optional, empty if not used)
};

struct IceTransportOptions {
    bool upnpEnable {false};
    IceTransportCompleteCb onInitDone {};
    IceTransportCompleteCb onNegoDone{};
    IceRecvInfo onRecvReady{}; // Detect that we have data to read but without destroying the buffer
    std::vector<StunServerInfo> stunServers;
    std::vector<TurnServerInfo> turnServers;
    bool tcpEnable {false}; // If we want to use TCP
    // See https://tools.ietf.org/html/rfc5245#section-8.1.1.2
    // Make negotiation aggressive by default to avoid latencies.
    bool aggressive {true};
};

struct SDP {
    std::string ufrag;
    std::string pwd;

    std::vector<std::string> candidates;
    MSGPACK_DEFINE(ufrag, pwd, candidates)
};

class IceTransport {
public:
    using Attribute = struct {
        std::string ufrag;
        std::string pwd;
    };

    /**
     * Constructor
     */
    IceTransport(const char* name, int component_count, bool master,
                 const IceTransportOptions& options = {});
    /**
     * Get current state
     */
    bool isInitiator() const;

    /**
     * Start tranport negotiation between local candidates and given remote
     * to find the right candidate pair.
     * This function doesn't block, the callback on_negodone_cb will be called
     * with the negotiation result when operation is really done.
     * Return false if negotiation cannot be started else true.
     */
    bool start(const Attribute& rem_attrs,
               const std::vector<IceCandidate>& rem_candidates);
    bool start(const SDP& sdp);

    /**
     * Stop a started or completed transport.
     */
    bool stop();

    /**
     * Cancel operations
     */
    void cancelOperations();

    /**
     * Returns true if ICE transport has been initialized
     * [mutex protected]
     */
    bool isInitialized() const;

    /**
     * Returns true if ICE negotiation has been started
     * [mutex protected]
     */
    bool isStarted() const;

    /**
     * Returns true if ICE negotiation has completed with success
     * [mutex protected]
     */
    bool isRunning() const;

    /**
     * Return true if a start operations fails or if stop() has been called
     * [mutex protected]
     */
    bool isStopped() const;

    /**
     * Returns true if ICE transport is in failure state
     * [mutex protected]
     */
    bool isFailed() const;

    IpAddr getLocalAddress(unsigned comp_id) const;

    IpAddr getRemoteAddress(unsigned comp_id) const;

    std::string getLastErrMsg() const;

    IpAddr getDefaultLocalAddress() const {
        return getLocalAddress(0);
    }

    bool registerPublicIP(unsigned compId, const IpAddr& publicIP);

    /**
     * Return ICE session attributes
     */
    const Attribute getLocalAttributes() const;

    /**
     * Return ICE session attributes
     */
    std::vector<std::string> getLocalCandidates(unsigned comp_id) const;

    /**
     * Returns serialized ICE attributes and candidates.
     */
    std::vector<uint8_t> packIceMsg(uint8_t version = 1) const;

    bool getCandidateFromSDP(const std::string& line, IceCandidate& cand) const;

    // I/O methods

    void setOnRecv(unsigned comp_id, IceRecvCb cb);

    ssize_t recv(int comp_id, unsigned char* buf, size_t len, std::error_code& ec);
    ssize_t recvfrom(int comp_id, char *buf, size_t len, std::error_code& ec);

    ssize_t send(int comp_id, const unsigned char* buf, size_t len);

    int waitForInitialization(std::chrono::milliseconds timeout);

    int waitForNegotiation(std::chrono::milliseconds timeout);

    ssize_t waitForData(int comp_id, std::chrono::milliseconds timeout, std::error_code& ec);

    /**
     * Return without waiting how many bytes are ready to read
     * @param comp_id   Ice component
     * @return the number of bytes ready to read
     */
    ssize_t isDataAvailable(int comp_id);

    unsigned getComponentCount() const;

    // Set session state
    bool setSlaveSession();
    bool setInitiatorSession();

    /**
     * Get SDP messages list
     * @param msg     The payload to parse
     * @return the list of SDP messages
     */
    static std::vector<SDP> parseSDPList(const std::vector<uint8_t>& msg);

    bool isTCPEnabled();

  private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

class IceTransportFactory {
public:
    IceTransportFactory();
    ~IceTransportFactory();

    std::shared_ptr<IceTransport> createTransport(const char* name,
                                                  int component_count,
                                                  bool master,
                                                  const IceTransportOptions& options = {});

    /**
     * PJSIP specifics
     */
    pj_ice_strans_cfg getIceCfg() const { return ice_cfg_; }
    pj_pool_factory* getPoolFactory() { return &cp_.factory; }

private:
    pj_caching_pool cp_;
    std::unique_ptr<pj_pool_t, std::function<void(pj_pool_t*)>> pool_;
    pj_ice_strans_cfg ice_cfg_;
};

};
