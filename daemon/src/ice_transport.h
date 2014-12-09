/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef ICE_TRANSPORT_H
#define ICE_TRANSPORT_H

#include "ice_socket.h"
#include "ip_utils.h"
#include "upnp/upnp.h"

#include <pjnath.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <map>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace ring {

class IceTransport;

using IceTransportCompleteCb = std::function<void(IceTransport&, bool)>;
using IceRecvCb = std::function<ssize_t(unsigned char* buf, size_t len)>;
using IceCandidate = pj_ice_sess_cand;

class IceTransport {
    public:
        using Attribute = struct {
                std::string ufrag;
                std::string pwd;
        };

        /**
         * Constructor
         */
        IceTransport(const char* name, int component_count,
                     bool master = true,
                     bool upnp_enabled = false,
                     IceTransportCompleteCb on_initdone_cb={},
                     IceTransportCompleteCb on_negodone_cb={});

        /**
         * Set/change transport role as initiator.
         * Should be called before start method.
         */
        bool setInitiatorSession();

        /**
         * Set/change transport role as slave.
         * Should be called before start method.
         */
        bool setSlaveSession();

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

        bool isInitialized() const;

        bool isStarted() const;

        bool isCompleted() const;

        IpAddr getLocalAddress(unsigned comp_id) const;

        IpAddr getRemoteAddress(unsigned comp_id) const;

        IpAddr getDefaultLocalAddress() const {
            return getLocalAddress(0);
        }

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
        std::vector<uint8_t> getLocalAttributesAndCandidates() const;

        bool getCandidateFromSDP(const std::string& line, IceCandidate& cand);

        // I/O methods

        void setOnRecv(unsigned comp_id, IceRecvCb cb);

        ssize_t recv(int comp_id, unsigned char* buf, size_t len);

        ssize_t send(int comp_id, const unsigned char* buf, size_t len);

        ssize_t getNextPacketSize(int comp_id);

        int waitForInitialization(unsigned timeout);

        int waitForNegotiation(unsigned timeout);

        ssize_t waitForData(int comp_id, unsigned int timeout);

        unsigned getComponentCount() const {return component_count_;};

    private:
        static constexpr int MAX_CANDIDATES {32};

        // New line character used for (de)serialisation
        static constexpr char NEW_LINE = '\n';

        static void cb_on_rx_data(pj_ice_strans *ice_st,
                                  unsigned comp_id,
                                  void *pkt, pj_size_t size,
                                  const pj_sockaddr_t *src_addr,
                                  unsigned src_addr_len);

        static void cb_on_ice_complete(pj_ice_strans *ice_st,
                                       pj_ice_strans_op op,
                                       pj_status_t status);

        static std::string unpackLine(std::vector<uint8_t>::const_iterator& begin,
                                      std::vector<uint8_t>::const_iterator& end);

        struct IceSTransDeleter {
                void operator ()(pj_ice_strans* ptr) {
                    pj_ice_strans_stop_ice(ptr);
                    pj_ice_strans_destroy(ptr);
                }
        };

        void onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                        pj_status_t status);

        void onReceiveData(unsigned comp_id, void *pkt, pj_size_t size);

        bool createIceSession(pj_ice_sess_role role);

        void getUFragPwd();

        void getDefaultCanditates();

        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
        IceTransportCompleteCb on_initdone_cb_;
        IceTransportCompleteCb on_negodone_cb_;
        bool iceTransportInitDone_ {false};
        bool iceTransportNegoDone_ {false};
        std::unique_ptr<pj_ice_strans, IceSTransDeleter> icest_;
        unsigned component_count_;
        pj_ice_sess_cand cand_[MAX_CANDIDATES] {};
        std::string local_ufrag_;
        std::string local_pwd_;
        pj_sockaddr remoteAddr_;
        std::condition_variable iceCV_ {};
        mutable std::mutex iceMutex_ {};

        struct Packet {
                Packet(void *pkt, pj_size_t size);
                std::unique_ptr<char> data;
                size_t datalen;
        };
        struct ComponentIO {
                std::mutex mutex;
                std::condition_variable cv;
                std::deque<Packet> queue;
                IceRecvCb cb;
        };
        std::vector<ComponentIO> compIO_;

        bool initiator_session_ {true};

        /**
         * Returns the IP of each candidate for a given component in the ICE session
         */
        std::vector<IpAddr> getLocalCandidatesAddr(unsigned comp_id) const;

        /**
         * Adds candidate to ICE session
         */
        void addCandidate(int comp_id, const IpAddr& addr);

        /**
         * Creates UPnP port mappings and adds ICE candidates based on those mappings
         */
        void selectUPnPIceCandidates();

        bool upnp_enabled_ {false};

        upnp::Controller upnp_;
};

class IceTransportFactory {
    public:
        IceTransportFactory();
        ~IceTransportFactory();

        std::shared_ptr<IceTransport> createTransport(const char* name,
                                                      int component_count,
                                                      bool master = true,
                                                      bool upnp_enabled = false,
                                                      IceTransportCompleteCb&& on_initdone_cb={},
                                                      IceTransportCompleteCb&& on_negodone_cb={});

        int processThread();

        /**
         * PJSIP specifics
         */
        const pj_ice_strans_cfg* getIceCfg() const { return &ice_cfg_; }
        pj_pool_factory* getPoolFactory() { return &cp_.factory; }

    private:
        int handleEvents(unsigned max_msec, unsigned *p_count);

        pj_caching_pool cp_;
        std::unique_ptr<pj_pool_t, decltype(pj_pool_release)&> pool_;
        std::unique_ptr<pj_thread_t, decltype(pj_thread_destroy)&> thread_;
        pj_ice_strans_cfg ice_cfg_;
        pj_bool_t thread_quit_flag_ {PJ_FALSE};
};

};

#endif /* ICE_TRANSPORT_H */
