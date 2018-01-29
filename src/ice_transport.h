/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
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
#include <vector>

namespace ring {

namespace upnp {
class Controller;
}

class IceTransport;

using IceTransportCompleteCb = std::function<void(bool)>;
using IceRecvCb = std::function<ssize_t(unsigned char* buf, size_t len)>;
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
    IceTransportCompleteCb onNegoDone {};
    std::vector<StunServerInfo> stunServers;
    std::vector<TurnServerInfo> turnServers;
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
     * Start tranport negociation between local candidates and given remote
     * to find the right candidate pair.
     * This function doesn't block, the callback on_negodone_cb will be called
     * with the negotiation result when operation is really done.
     * Return false if negotiation cannot be started else true.
     */
    bool start(const Attribute& rem_attrs,
               const std::vector<IceCandidate>& rem_candidates);
    bool start(const std::vector<uint8_t>& attrs_candidates);

    /**
     * Stop a started or completed transport.
     */
    bool stop();

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
    std::vector<uint8_t> packIceMsg() const;

    bool getCandidateFromSDP(const std::string& line, IceCandidate& cand);

    // I/O methods

    void setOnRecv(unsigned comp_id, IceRecvCb cb);

    ssize_t recv(int comp_id, unsigned char* buf, size_t len);

    ssize_t send(int comp_id, const unsigned char* buf, size_t len);

    int waitForInitialization(unsigned timeout);

    int waitForNegotiation(unsigned timeout);

    ssize_t waitForData(int comp_id, unsigned int timeout, std::error_code& ec);

    unsigned getComponentCount() const;

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
    std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
    pj_ice_strans_cfg ice_cfg_;
};

};
