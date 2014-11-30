/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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

namespace sfl {

class IceTransport;
class IceTransportPool;

using IceTransportCompleteCb = std::function<void(IceTransport&, bool)>;

using IceRecvCb = std::function<ssize_t(unsigned char* buf, size_t len)>;

using IceCandidate = pj_ice_sess_cand;

class IceTransport {
    public:
        using Attribute = struct {
                std::string ufrag;
                std::string pwd;
        };

        IceTransport(const char* name, int component_count,
                     IceTransportCompleteCb on_initdone_cb,
                     IceTransportCompleteCb on_negodone_cb,
                     IceTransportPool& tp);

        void setInitiatorSession();
        void setSlaveSession();
        void unsetSession();

        bool start(const Attribute& rem_attrs,
                   const std::vector<IceCandidate>& rem_candidates);

        IpAddr getLocalAddress() const;
        std::vector<IpAddr> getLocalPorts() const;

        /* 2-string vector containing ufrag and pwd Ice attributes */
        const Attribute getIceAttributes() const;
        std::vector<std::string> getIceCandidates(unsigned comp_id) const;

        bool getCandidateFromSDP(const std::string line, IceCandidate& cand);

        void setRemoteDestination(const std::string& ip_addr, uint16_t port);
        IpAddr getRemote() const {
            return remoteAddr_;
        }

        ssize_t recv(int comp_id, unsigned char* buf, size_t len);
        void setOnRecv(unsigned comp_id, IceRecvCb cb);

        ssize_t send(int comp_id, const unsigned char* buf, size_t len);
        ssize_t getNextPacketSize(int comp_id);
        ssize_t waitForData(int comp_id, unsigned int timeout);

        bool isRunning() const {
            return running;
        }

    private:
        static void cb_on_rx_data(pj_ice_strans *ice_st,
                                  unsigned comp_id,
                                  void *pkt, pj_size_t size,
                                  const pj_sockaddr_t *src_addr,
                                  unsigned src_addr_len);

        static void cb_on_ice_complete(pj_ice_strans *ice_st,
                                       pj_ice_strans_op op,
                                       pj_status_t status);

        struct IceSTransDeleter {
                void operator ()(pj_ice_strans* ptr) {
                    pj_ice_strans_stop_ice(ptr);
                    pj_ice_strans_destroy(ptr);
                }
        };

        void onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                        pj_status_t status);
        void onReceiveData(unsigned comp_id, void *pkt, pj_size_t size);

        void createIceSession(pj_ice_sess_role role);

        void getUFragPwd();

        void getDefaultCanditates();

        IceTransportCompleteCb on_initdone_cb_ {};
        IceTransportCompleteCb on_negodone_cb_ {};
        std::unique_ptr<pj_ice_strans, IceSTransDeleter> icest_ {};
        unsigned component_count_;
        pj_ice_sess_cand cand_[PJ_ICE_ST_MAX_CAND] {};
        std::string local_ufrag_ {};
        std::string local_pwd_ {};
        pj_sockaddr remoteAddr_ {};

        // set to true when on_negodone_cb_ was called.
        std::atomic_bool running {false};

        struct Packet {
                Packet(void *pkt, pj_size_t size)
                    : data(new char[size]), datalen(size) {
                    memcpy(data.get(), pkt, size);
                }
                Packet(Packet&& p) {
                    data = std::move(p.data);
                    datalen = p.datalen;
                }
                std::unique_ptr<char> data;
                size_t datalen;
        };
        struct ComponentIO {
                std::mutex mutex {};
                std::condition_variable cv {};
                std::queue<Packet> queue {};
                IceRecvCb cb {};
        };
        std::vector<ComponentIO> compIO_;
};

class IceTransportPool {
    public:
        IceTransportPool();

        ~IceTransportPool();

        std::shared_ptr<IceTransport> createTransport(const char* name,
                                                      int component_count,
                                                      IceTransportCompleteCb&& on_initdone_cb,
                                                      IceTransportCompleteCb&& on_negodone_cb);

        IceTransport* getTransportFromIPv4(struct in_addr addr, int port);

        int processThread();

        const pj_ice_strans_cfg* getIceCfg() const { return &ice_cfg_; }

    private:
        int handleEvents(unsigned max_msec, unsigned *p_count);

        //std::map<pj_sockaddr, std::shared_ptr<IceTransport> > transports_ {};
        std::vector<std::shared_ptr<IceTransport> > transports_ {};
        pj_ice_strans_cfg ice_cfg_;
        std::unique_ptr<pj_thread_t, decltype(*pj_thread_destroy)> thread_ {nullptr, pj_thread_destroy};
        pj_bool_t thread_quit_flag_ = PJ_FALSE;
};

};

#endif /* ICE_TRANSPORT_H */
