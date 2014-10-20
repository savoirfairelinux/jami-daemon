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

#include <pjnath.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <map>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>

namespace sfl {

class ICETransport;
class ICETransportPool;

typedef std::function<void(ICETransport&)> ICETransportCompleteCb;

class ICETransport {
    public:
        using Attribute = struct {
                std::string ufrag;
                std::string pwd;
        };

        using Candidate = pj_ice_sess_cand;

        ICETransport(const char* name, int component_count,
                     ICETransportCompleteCb on_complete_cb,
                     ICETransportPool& tp);

        void setInitiatorSession();
        void setSlaveSession();
        void unsetSession();

        bool start(const Attribute& rem_attrs,
                   const std::vector<Candidate>& rem_candidates);

        pj_sockaddr getLocalAddress() const;
        std::vector<pj_sockaddr> getLocalPorts() const;

        /* 2-string vector containing ufrag and pwd ICE attributes */
        const Attribute getICEAttributes() const;

        std::vector<std::string> getICECandidates(unsigned comp_id) const;

        bool getCandidateFromSDP(const std::string line, Candidate& cand);

        size_t recv(int comp_id, unsigned char* buf, size_t len);
        size_t send(int comp_id, const unsigned char* buf, size_t len);

    private:
        static void cb_on_ice_complete(pj_ice_strans *ice_st,
                                       pj_ice_strans_op op,
                                       pj_status_t status);

        static void cb_on_rx_data(pj_ice_strans *ice_st,
                                  unsigned comp_id,
                                  void *pkt, pj_size_t size,
                                  const pj_sockaddr_t *src_addr,
                                  unsigned src_addr_len);

        struct ICESTransDeleter {
                void operator ()(pj_ice_strans* ptr) {
                    pj_ice_strans_destroy(ptr);
                }
        };

        void onComplete(pj_ice_strans* ice_st, pj_ice_strans_op op,
                        pj_status_t status);
        void onReceiveData(unsigned comp_id, void *pkt, pj_size_t size);

        void createICESession(pj_ice_sess_role role);

        void getUFragPwd();

        void getDefaultCanditates();

        ICETransportCompleteCb on_complete_cb_ {};
        std::unique_ptr<pj_ice_strans, ICESTransDeleter> icest_ {};
        std::atomic_bool complete_ {};
        unsigned component_count_;
        pj_ice_sess_cand cand_[PJ_ICE_ST_MAX_CAND] {};
        std::string local_ufrag_ {};
        std::string local_pwd_ {};
};

class ICETransportPool {
    public:
        ICETransportPool();

        ~ICETransportPool();

        std::shared_ptr<ICETransport> createTransport(const char* name,
                                                      int component_count,
                                                      ICETransportCompleteCb&& on_complete_cb);

        ICETransport* getTransportFromIPv4(struct in_addr addr, int port);

        int processThread();

        const pj_ice_strans_cfg* getICECfg() const { return &ice_cfg_; }

    private:
        int handleEvents(unsigned max_msec, unsigned *p_count);

        struct PJThreadDeleter {
                void operator ()(pj_thread_t* ptr) {
                    pj_thread_destroy(ptr);
                }
        };

        //std::map<pj_sockaddr, std::shared_ptr<ICETransport> > transports_ {};
        std::vector<std::shared_ptr<ICETransport> > transports_ {};
        pj_ice_strans_cfg ice_cfg_;
        std::unique_ptr<pj_thread_t, PJThreadDeleter> thread_ {};
        pj_bool_t thread_quit_flag_ = PJ_FALSE;
};

};

#endif /* ICE_TRANSPORT_H */
