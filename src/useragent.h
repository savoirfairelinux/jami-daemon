/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef _SIPMANAGER_H
#define	_SIPMANAGER_H

#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjnath/stun_config.h>

//TODO Remove this include if we don't need anything from it
#include <pjsip_simple.h>

#include <pjsip_ua.h>
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>

#include <string>
#include <vector>

#define PJ_LOG_LEVEL	5

typedef std::string AccountID;

class SIPCall;

class UserAgent
{
private:
    /** PJSIP Endpoint */
    pjsip_endpoint *_endpt;
    pj_sock_t _sock;
    //pjsip_module _appMod;
    pj_caching_pool _cp;
    pj_pool_t *_pool;    
    pj_mutex_t *_mutex;     /** Mutex protection for this data */
    pjsip_module _mod;       /** PJSIP module. */
    pjsip_module _options_handler;
    bool _useStun;
    pj_str_t _stunHost;
    std::string _stunServer;

    /** Local Extern Address is the IP address seen by peers for SIP listener */
    std::string _localExternAddress;
    std::string _localIPAddress;

    /** Local Extern Port is the port seen by peers for SIP listener */
    unsigned int _localExternPort;
    unsigned int _localPort;

    /** For registration use only */
    unsigned int _regPort;
    
    pj_thread_t *_thread;
    
    static UserAgent *_current;
    
    /* Sleep with polling */
    void busy_sleep(unsigned msec);
    void sipDestory();
public:
    UserAgent();
    ~UserAgent();
    
    pj_status_t sipCreate();
    
    /**
     * This method is used to initialize the pjsip
     */
    pj_status_t sipInit();
    
    /** Create SIP UDP Listener */
    int createUDPServer();

    /** Set whether it will use stun server */
    void setStunServer(const char *server); 

    /** Set the port number user designated */
    void setRegPort(unsigned int port) { _regPort = port; }
 
    unsigned int getRegPort() { return _regPort; }
    
    pj_str_t getStunServer() { return _stunHost; }
    
    bool addAccount(AccountID id, pjsip_regc **regc, const std::string& server, const std::string& user, const std::string& passwd
						   , const int& timeout,  const unsigned int& port);
    bool removeAccount(pjsip_regc *regc);
    
    pj_str_t buildContact(char *userName);
    
    bool loadSIPLocalIP();
    
    pj_status_t stunServerResolve();
    
    pjsip_endpoint* getEndPoint() {return _endpt;}
    
    std::string getLocalIP() {return _localExternAddress;}
    
    int getModId() {return _mod.id;}
    
    bool setCallAudioLocal(SIPCall* call);
    
    int answer(SIPCall* call);
    
    bool hangup(SIPCall* call);
    
    bool refuse(SIPCall* call);
    
    bool onhold(SIPCall *call);
    bool offhold(SIPCall *call);
 
    bool transfer(SIPCall *call, const std::string& to);
    
    void onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata);
    
    bool makeOutgoingCall(const std::string& to, SIPCall* call, const AccountID& id);
    pj_pool_t *getAppPool() {return _pool;}
    static pj_bool_t mod_on_rx_request(pjsip_rx_data *rdata);
    static pj_bool_t mod_on_rx_response(pjsip_rx_data *rdata UNUSED) {return PJ_SUCCESS;}
    static void regc_cb(struct pjsip_regc_cbparam *param);
    static void xfer_func_cb( pjsip_evsub *sub, pjsip_event *event);
    static void xfer_svr_cb(pjsip_evsub *sub, pjsip_event *event);
    static void call_on_media_update( pjsip_inv_session *inv UNUSED, pj_status_t status UNUSED) {}
    static void call_on_state_changed( pjsip_inv_session *inv, pjsip_event *e);
    static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);
    static void call_on_tsx_changed(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);
    static int start_thread(void *arg);
    static UserAgent* getInstance() {return _current;}

    static void set_voicemail_info( AccountID account, pjsip_msg_body *body );
private:

    // Copy Constructor
    UserAgent(const UserAgent& rh);

    // Assignment Operator
    UserAgent& operator=( const UserAgent& rh);
};


#endif /* _SIPMANAGER_H */

