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


#include "manager.h"
#include "sipcall.h"
#include "sipmanager.h"
#include "sipvoiplink.h"

#define DEFAULT_SIP_PORT  5068
#define RANDOM_SIP_PORT   rand() % 64000 + 1024
#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2

SIPManager *SIPManager::_current;
pjsip_inv_session* SIPManager::_invSession;

SIPManager::SIPManager() {
    _useStun = false;
    _localIPAddress = "127.0.0.1";
    SIPManager::_current = this;
}

SIPManager::~SIPManager() {
    _debug("Somebody call me!\n");
}

pj_status_t SIPManager::sipCreate() {
    pj_status_t status;
    //_debug("stun host is %s\n", _stunHost.ptr);

    /* Init pjsua data */
    //init_data();

    /* Set default logging settings */
    //pjsua_logging_config_default(&pjsua_var.log_cfg);

    /* Init PJLIB: */
    status = pj_init();
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Could not initialize PJSip\n");
        return status;
    }

    /* Init random seed */
    //init_random_seed();

    /* Init PJLIB-UTIL: */
    status = pjlib_util_init();
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Could not initialize PJ Util\n");
        return status;
    }

    /* Init PJNATH */
    status = pjnath_init();
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Could not initialize PJ Nath\n");
        return status;
    }

    /* Set default sound device ID */
    //pjsua_var.cap_dev = pjsua_var.play_dev = -1;

    /* Init caching pool. */
    pj_caching_pool_init(&_cp, NULL, 0);

    /* Create memory pool for application. */
    _pool = pj_pool_create(&_cp.factory, "sflphone", 4000, 4000, NULL);

    if (!_pool) {
        _debug("! SIP Failure: Could not initialize memory pool\n");
        return PJ_ENOMEM;
    }

    /* Create mutex */
    status = pj_mutex_create_recursive(_pool, "sflphone", &_mutex);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to create mutex\n");
        return status;
    }

    /* Must create SIP endpoint to initialize SIP parser. The parser
     * is needed for example when application needs to call pjsua_verify_url().
     */
    status = pjsip_endpt_create(&_cp.factory,
            pj_gethostname()->ptr,
            &_endpt);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to create mutex\n");
        return status;
    }

    return PJ_SUCCESS;
}

pj_status_t SIPManager::sipInit() {
    pj_status_t status;
    const pj_str_t STR_OPTIONS = {"OPTIONS", 7};

    /* Init SIP UA: */

    //FIXME! DNS initialize here! */

    /* Start resolving STUN server */
    // if we useStun and we failed to receive something on port 5060, we try a random port
    // If use STUN server, firewall address setup
    if (!loadSIPLocalIP()) {
        _debug("! SIP FAILURE: Unable to determine network capabilities\n");
        return false;
    }
    int errPjsip = 0;
    int port = DEFAULT_SIP_PORT;

    //_debug("stun host is %s\n", _stunHost.ptr);
    if (_useStun && !Manager::instance().behindNat(_stunServer, port)) {
        port = RANDOM_SIP_PORT;
        if (!Manager::instance().behindNat(_stunServer, port)) {
            _debug("! SIP Failure: Unable to check NAT setting\n");
            return false; // hoho we can't use the random sip port too...
        }
    }

    _localPort = port;
    if (_useStun) {
        // set by last behindNat() call (ish)...
        stunServerResolve();
        _localExternAddress = Manager::instance().getFirewallAddress();
        _localExternPort = Manager::instance().getFirewallPort();
    } else {
        _localExternAddress = _localIPAddress;
        _localExternPort = _localPort;
    }

    errPjsip = createUDPServer();
    if (errPjsip != 0) {
        _debug("* SIP Info: could not initialize SIP listener on port %d\n", port);
        port = RANDOM_SIP_PORT;
        _debug("! SIP Failure: SIP failed to listen on port %d\n", port);
        return errPjsip;
    }
    _debug("  SIP Init: listening on port %d\n", _localExternPort);

    /* Initialize transaction layer: */
    status = pjsip_tsx_layer_init_module(_endpt);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize transaction layer.\n");
        return status;
    }

    /* Initialize UA layer module: */
    status = pjsip_ua_init_module(_endpt, NULL);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize UA layer module.\n");
        return status;
    }


    /* Initialize Replaces support. */
    status = pjsip_replaces_init_module(_endpt);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize Replaces support.\n");
        return status;
    }

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(_endpt);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize 100rel support.\n");
        return status;
    }

    /* Initialize and register sflphone module. */
    {
        const pjsip_module mod_initializer ={
            NULL, NULL, /* prev, next.			*/
            { "mod-sflphone", 9
            }, /* Name.				*/
            -1, /* Id				*/
            PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
            NULL, /* load()				*/
            NULL, /* start()				*/
            NULL, /* stop()				*/
            NULL, /* unload()				*/
            &mod_on_rx_request, /* on_rx_request()			*/
            &mod_on_rx_response, /* on_rx_response()			*/
            NULL, /* on_tx_request.			*/
            NULL, /* on_tx_response()			*/
            NULL, /* on_tsx_state()			*/
        };

        _mod = mod_initializer;

        status = pjsip_endpt_register_module(_endpt, &_mod);
        if (status != PJ_SUCCESS) {
            _debug("! SIP Failure: Unable to register sflphone module.\n");
            return status;
        }
    }



    /* Initialize PJSUA call subsystem: */
    /*status = pjsua_call_subsys_init(ua_cfg);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize ")
        goto on_error;
    }*/


    /* Initialize PJSUA media subsystem */
    /*status = pjsua_media_subsys_init(media_cfg);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize ")
        goto on_error;
    }*/


    /* Init core SIMPLE module : */
    status = pjsip_evsub_init_module(_endpt);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize core SIMPLE module.\n");
        return status;
    }


    /* Init presence module: */
    status = pjsip_pres_init_module(_endpt, pjsip_evsub_instance());
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize presence module.\n");
        return status;
    }

    /* Init PUBLISH module */
    pjsip_publishc_init_module(_endpt);

    /* Init xfer/REFER module */
    status = pjsip_xfer_init_module(_endpt);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize xfer/PEFER module.\n");
        return status;
    }

    /* Init pjsua presence handler: */
    /*status = pjsua_pres_init();
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize ")
        goto on_error;
    }*/

    /* Init out-of-dialog MESSAGE request handler. */
    /*status = pjsua_im_init();
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize ")
        goto on_error;
    }*/

    {
        const pjsip_module handler ={
            NULL, NULL, /* prev, next.          */
            { "mod-pjsua-options", 17
            }, /* Name.                */
            -1, /* Id                   */
            PJSIP_MOD_PRIORITY_APPLICATION, /* Priority             */
            NULL, /* load()               */
            NULL, /* start()              */
            NULL, /* stop()               */
            NULL, /* unload()             */
            &options_on_rx_request, /* on_rx_request()      */
            NULL, /* on_rx_response()     */
            NULL, /* on_tx_request.       */
            NULL, /* on_tx_response()     */
            NULL, /* on_tsx_state()       */

        };
        _options_handler = handler;
    }

    /* Register OPTIONS handler */
    pjsip_endpt_register_module(_endpt, &_options_handler);

    /* Add OPTIONS in Allow header */
    pjsip_endpt_add_capability(_endpt, NULL, PJSIP_H_ALLOW,
            NULL, 1, &STR_OPTIONS);


    // Initialize invite session module
    // These callbacks will be called on incoming requests, media session state, etc.
    {
        pjsip_inv_callback inv_cb;

        // Init the callback for INVITE session: 
        pj_bzero(&inv_cb, sizeof (inv_cb));

        inv_cb.on_state_changed = &call_on_state_changed;
        inv_cb.on_new_session = &call_on_forked;
        inv_cb.on_media_update = &call_on_media_update;
        inv_cb.on_tsx_state_changed = &call_on_tsx_changed;

        _debug("AAAA VOIP callbacks initialized\n");

        // Initialize session invite module 
        status = pjsip_inv_usage_init(_endpt, &inv_cb);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }


    // Add endpoint capabilities (INFO, OPTIONS, etc) for this UA
    {
        pj_str_t allowed[] = {
            {"INFO", 4},
            {"REGISTER", 8
            }
        }; //  //{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6},  {"OPTIONS", 7}, 
        pj_str_t accepted = {"application/sdp", 15
        };

        // Register supported methods
        pjsip_endpt_add_capability(_endpt, &_mod, PJSIP_H_ALLOW, NULL, PJ_ARRAY_SIZE(allowed), allowed);

        // Register "application/sdp" in ACCEPT header
        pjsip_endpt_add_capability(_endpt, &_mod, PJSIP_H_ACCEPT, NULL, 1, &accepted);
    }

    _debug("sflphone version %s for %s initialized\n", pj_get_version(), PJ_OS_NAME);

    status = pj_thread_create(_pool, "sflphone", &worker_thread,
            NULL, 0, 0, &_thread);

    /* Done! */
    return PJ_SUCCESS;

on_error:
    //sipDestroy();
    return status;
}

bool SIPManager::addAccount(AccountID id, pjsip_regc *regc2, const std::string& server, const std::string& user, const std::string& passwd, const int& timeout) {
    pj_status_t status;
    AccountID *currentId = new AccountID(id);
    //pjsip_cred_info cred;
    char contactTmp[256];
    pjsip_regc *regc;

    pj_mutex_lock(_mutex);
    std::string tmp;

    status = pjsip_regc_create(_endpt, (void *) currentId, &regc_cb, &regc);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to create regc.\n");
        return status;
    }

    tmp = "sip:" + server;
    pj_str_t svr = pj_str((char *) tmp.data());
    tmp = "<sip:" + user + "@" + server + ">";
    pj_str_t aor = pj_str((char *) tmp.data());

    sprintf(contactTmp, "<sip:%s@%s:%d>", user.data(), _localExternAddress.data(), _localExternPort);
    pj_str_t contact = pj_str(contactTmp);

    _debug("Get in %s %d %s\n", svr.ptr, svr.slen, aor.ptr);
    _debug("after here! contact is %s\n", contact.ptr);
    status = pjsip_regc_init(regc, &svr, &aor, &aor, 1, &contact, 600); //timeout);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to initialize regc. %d\n", status); //, regc->str_srv_url.ptr);
        return status;
    }
    _debug("after here!\n");

    AccBaseInfo *info = new AccBaseInfo();

    pj_bzero(&info->cred, sizeof (info->cred));
    pj_strdup2(_pool, &info->cred.username, user.data());
    info->cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    pj_strdup2(_pool, &info->cred.data, passwd.data());
    pj_strdup2(_pool, &info->cred.realm, "*");
    pj_strdup2(_pool, &info->cred.scheme, "digest");
    pjsip_regc_set_credentials(regc, 1, &info->cred);
    _debug("after here!\n");

    pjsip_tx_data *tdata;
    status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to register regc.\n");
        return status;
    }
    _debug("after here!\n");

    status = pjsip_regc_send(regc, tdata);
    if (status != PJ_SUCCESS) {
        _debug("! SIP Failure: Unable to send regc request.\n");
        return status;
    }
    _debug("after here!\n");

    info->userName = user;
    info->server = server;
    info->id = id;
    pj_strdup2(_pool, &info->contact, contact.ptr);
    _accBaseInfoList.push_back(info);

    pj_mutex_unlock(_mutex);
    return true;
}

pj_str_t SIPManager::buildContact(char *userName) {
    pj_str_t contact;
    char tmp[256];

    //FIXME: IPV6 issue!!
    _debug("In build Contact %s %s %d\n", userName, _localExternAddress.data(), _localExternPort);
    sprintf(tmp, "<sip:%s@%s:%d>", userName, _localExternAddress.data(), _localExternPort);
    _debug("get tmp\n");
    return pj_str(tmp);
}

pj_status_t SIPManager::stunServerResolve() {
    pj_str_t stun_adr;
    pj_hostent he;
    pj_stun_config stunCfg;
    pj_status_t stun_status;
    pj_sockaddr stun_srv;

    // Initialize STUN configuration
    pj_stun_config_init(&stunCfg, &_cp.factory, 0, pjsip_endpt_get_ioqueue(_endpt), pjsip_endpt_get_timer_heap(_endpt));

    stun_status = PJ_EPENDING;

    // Init STUN socket
    stun_adr = pj_str((char *) _stunServer.data());
    stun_status = pj_sockaddr_in_init(&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);

    if (stun_status != PJ_SUCCESS) {
        _debug("***********Unresolved stud server***********!\n");
        stun_status = pj_gethostbyname(&stun_adr, &he);

        if (stun_status == PJ_SUCCESS) {
            pj_sockaddr_in_init(&stun_srv.ipv4, NULL, 0);
            stun_srv.ipv4.sin_addr = *(pj_in_addr*) he.h_addr;
            stun_srv.ipv4.sin_port = pj_htons((pj_uint16_t) 3478);
        }
    }

    return stun_status;
}

int SIPManager::createUDPServer() {
    pj_status_t status;
    pj_str_t ipAddr;
    pj_sockaddr_in bound_addr;

    // Init bound address to ANY
    pj_memset(&bound_addr, 0, sizeof (bound_addr));
    bound_addr.sin_addr.s_addr = PJ_INADDR_ANY;

    // Create UDP server socket
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &_sock);
    if (status != PJ_SUCCESS) {
        _debug("(%d) UDP socket() error\n", status);
        return status;
    }

    status = pj_sock_bind_in(_sock, pj_ntohl(bound_addr.sin_addr.s_addr), (pj_uint16_t) _localPort);
    if (status != PJ_SUCCESS) {
        _debug("(%d) UDP bind() error\n", status);
        pj_sock_close(_sock);
        return status;
    }

    _debug("Use IP: %s\n", _localExternAddress.data());

    // Create UDP-Server (default port: 5060)
    pjsip_host_port a_name;
    char tmpIP[32];
    strcpy(tmpIP, _localExternAddress.data());
    a_name.host = pj_str(tmpIP);
    a_name.port = (pj_uint16_t) _localExternPort;

    status = pjsip_udp_transport_attach(_endpt, _sock, &a_name, 1, NULL);
    if (status != PJ_SUCCESS) {
        _debug("(%d) Unable to start UDP transport!\n", status);
        return -1;
    } else {
        _debug("UDP server listening on port %d\n", _localExternPort);
    }

    return 0;
}

void SIPManager::setStunServer(const char *server) {
    _stunServer = std::string(server);
    _useStun = true;
}

void SIPManager::regc_cb(struct pjsip_regc_cbparam *param) {
    AccountID *id = static_cast<AccountID *> (param->token);
    _debug("Account ID is %s, Register result: %d\n", id->data(), param->status);
    if (param->status == PJ_SUCCESS) {
        Manager::instance().getAccountLink(*id)->setRegistrationState(VoIPLink::Registered);
    } else {
        Manager::instance().getAccountLink(*id)->setRegistrationState(VoIPLink::Error);
    }
}

bool
SIPManager::loadSIPLocalIP() {
    bool returnValue = true;
    if (_localIPAddress == "127.0.0.1") {
        pj_sockaddr ip_addr;
        if (pj_gethostip(pj_AF_INET(), &ip_addr) != PJ_SUCCESS) {
            // Update the registration state if no network capabilities found
            _debug("Get host ip failed!\n");
            //setRegistrationState( ErrorNetwork );
            returnValue = false;
        } else {
            _localIPAddress = std::string(pj_inet_ntoa(ip_addr.ipv4.sin_addr));
            _debug("  SIP Info: Checking network, setting local IP address to: %s\n", _localIPAddress.data());
        }
    }
    return returnValue;
}

/* Worker thread function. */
int SIPManager::worker_thread(void *arg) {
    //enum { TIMEOUT = 10 };

    PJ_UNUSED_ARG(arg);

    while (true) {
        //int count;

        pj_time_val timeout = {0, 10};
        pjsip_endpt_handle_events(getInstance()->getEndPoint(), &timeout);
    }

    return 0;
}

pj_bool_t SIPManager::mod_on_rx_request(pjsip_rx_data *rdata) {
    pj_status_t status;
    pj_str_t reason;
    unsigned options = 0;
    pjsip_dialog* dialog;
    pjsip_tx_data *tdata;
    pjmedia_sdp_session *r_sdp;

    _debug("Callback on_rx_request entered\n");

    PJ_UNUSED_ARG(rdata);

    pjsip_uri *uri = rdata->msg_info.to->uri;
    pjsip_sip_uri *sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(uri);

    std::string userName = std::string(sip_uri->user.ptr, sip_uri->user.slen);
    std::string server = std::string(sip_uri->host.ptr, sip_uri->host.slen);

    _debug("The receiver is : %s@%s\n", userName.data(), server.data());

    uri = rdata->msg_info.from->uri;
    sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(uri);
    
    std::string caller = std::string(sip_uri->user.ptr, sip_uri->user.slen);
    std::string callerServer = std::string(sip_uri->host.ptr, sip_uri->host.slen);
    std::string peerNumber = caller + "@" + callerServer;
    
    /* Respond statelessly any non-INVITE requests with 500 */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
        if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
            reason = pj_str((char*) " user agent unable to handle this request ");
            pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, MSG_METHOD_NOT_ALLOWED, &reason, NULL,
                    NULL);
            return PJ_TRUE;
        }
    }
    // Verify that we can handle the request
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL, getInstance()->getEndPoint(), NULL);
    if (status != PJ_SUCCESS) {
        reason = pj_str((char*) " user agent unable to handle this INVITE ");
        pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, MSG_METHOD_NOT_ALLOWED, &reason, NULL,
                NULL);
        return PJ_TRUE;
    }

    CallID id = Manager::instance().getNewCallID();

    _debug("####################\n##############call id is %s\n", id.c_str());
    SIPCall* call = new SIPCall(id, Call::Incoming);
    if (!call) {
        _debug("! SIP Failure: unable to create an incoming call");
        return PJ_FALSE;
    }

    getInstance()->setCallAudioLocal(call);
    call->setCodecMap(Manager::instance().getCodecDescriptorMap());
    call->setConnectionState(Call::Progressing);
    call->setIp(getInstance()->getLocalIP());
    call->setPeerNumber(peerNumber);
    if (call->SIPCallInvite(rdata, getInstance()->getAppPool())) {
        AccountID id = getInstance()->getAccountIdFromNameAndServer(userName, server);
        _debug("####################\n##############account id is %s\n", id.c_str());
        if (Manager::instance().incomingCall(call, id)) {
            Manager::instance().getAccountLink(id)->addCall(call);
            //addCall(call);
            _debug("OK\n");
        } else {
            delete call;
            call = 0;
        }
    } else {
        delete call;
        call = 0;
    }
    _debug("Here!!!!!\n");

    /* Create the local dialog (UAS) */
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, NULL, &dialog);
    if (status != PJ_SUCCESS) {
        pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, MSG_SERVER_INTERNAL_ERROR, &reason, NULL,
                NULL);
        return PJ_TRUE;
    }
    // Specify media capability during invite session creation
    status = pjsip_inv_create_uas(dialog, rdata,
            call->getLocalSDPSession(), 0, &_invSession);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    _invSession->mod_data[getInstance()->getModId()] = call;
    
    // Send a 180/Ringing response
    status = pjsip_inv_initial_answer(_invSession, rdata, PJSIP_SC_RINGING, NULL, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    status = pjsip_inv_send_msg(_invSession, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Update the connection state
    if (call) {
        call->setConnectionState(Call::Ringing);
    }

    /* Done */

    return PJ_SUCCESS;
}

AccountID SIPManager::getAccountIdFromNameAndServer(const std::string& userName, const std::string& server) {
    int size = _accBaseInfoList.size();
    for (int i = 0; i < size; i++) {
        if (_accBaseInfoList[i]->userName == userName &&
                _accBaseInfoList[i]->server == server) {
            _debug("Full match\n");
            return _accBaseInfoList[i]->id;
        }
    }

    for (int i = 0; i < size; i++) {
        if (_accBaseInfoList[i]->userName == userName) {
            _debug("Username match\n");
            return _accBaseInfoList[i]->id;
        }
    }

    return AccountNULL;
}

SIPManager::AccBaseInfo* SIPManager::getAccountInfoFromId(AccountID id) {
    int size = _accBaseInfoList.size();
    for (int i = 0; i < size; i++) {
        if (_accBaseInfoList[i]->id == id) {
            return _accBaseInfoList[i];
        }
    }

    return NULL;
}

bool
SIPManager::setCallAudioLocal(SIPCall* call) {
    // Setting Audio
    unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
    unsigned int callLocalExternAudioPort = callLocalAudioPort;
    if (_useStun) {
        // If use Stun server
        if (Manager::instance().behindNat(_stunServer, callLocalAudioPort)) {
            callLocalExternAudioPort = Manager::instance().getFirewallPort();
        }
    }
    _debug("            Setting local audio port to: %d\n", callLocalAudioPort);
    _debug("            Setting local audio port (external) to: %d\n", callLocalExternAudioPort);

    // Set local audio port for SIPCall(id)
    call->setLocalIp(_localIPAddress);
    call->setLocalAudioPort(callLocalAudioPort);
    call->setLocalExternAudioPort(callLocalExternAudioPort);

    return true;
}

int SIPManager::answer(SIPCall *call) {
    pj_status_t status;
    pjsip_tx_data *tdata;

    if (call->startNegociation(_pool)) {
        // Create and send a 200(OK) response
        _debug("Negociation success!\n");
        status = pjsip_inv_answer(_invSession, PJSIP_SC_OK, NULL, NULL, &tdata);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
        status = pjsip_inv_send_msg(_invSession, tdata);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

        return 0;
    }

    return 1;
}

bool SIPManager::makeOutgoingCall(const std::string& strTo, SIPCall* call, const AccountID& id) {
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_tx_data *tdata;
    pj_str_t from, to;

    AccBaseInfo* accBase = getAccountInfoFromId(id);
    std::string strFrom = "sip:" + accBase->userName + "@" + accBase->server;

    _debug("Make a new call from:%s to %s. Contact is %s\n", strFrom.data(), strTo.data(), accBase->contact.ptr);

    from = pj_str((char *) strFrom.data());
    to = pj_str((char *) strTo.data());

    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &from,
            &accBase->contact, //NULL,
            &to,
            NULL,
            &dialog);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    setCallAudioLocal(call);
    call->setIp(getInstance()->getLocalIP());

    // Building the local SDP offer
    call->createInitialOffer(_pool);

    status = pjsip_inv_create_uac(dialog,
            call->getLocalSDPSession(), 0, &_invSession);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pjsip_auth_clt_set_credentials(&dialog->auth_sess, 1, &accBase->cred);

    _invSession->mod_data[_mod.id] = call;

    status = pjsip_inv_invite(_invSession, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    status = pjsip_inv_send_msg(_invSession, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    return true;
}

void SIPManager::call_on_forked(pjsip_inv_session *inv, pjsip_event *e) {
    //_debug("HERE!\n");
}

void SIPManager::call_on_tsx_changed(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e) {
    PJ_UNUSED_ARG(inv);
    _debug("@@@@@@@@@@@@@@@ Here we are, the tsx->state is %d; tsx->role is %d; code is %d; method id is %s.\n",
            tsx->state, tsx->role, tsx->status_code, tsx->method.name.ptr);
    if (tsx->state == PJSIP_TSX_STATE_TERMINATED && tsx->role == PJSIP_ROLE_UAC) {
        int code;
        pjsip_rx_data *rdata;
        AccountID accId;

        //Retrieve the body message
        rdata = e->body.tsx_state.src.rdata;

        code = tsx->status_code;
        if (code == 200) {
            _debug("Use this to handle the incoming call!!!\n");
            SIPCall *call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
            if(call == NULL)
                return;
            
            _debug("The call id is %s\n", call->getCallId().data());
            //pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
            accId = Manager::instance().getAccountFromCall(call->getCallId());
            SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *>(Manager::instance().getAccountLink(accId));
            link->SIPCallAnswered(call, rdata);
        }

    } else if (tsx->state == PJSIP_TSX_STATE_TRYING && tsx->role == PJSIP_ROLE_UAS) {
        /*_debug("HERE!!!!!!!!!!!!!!!!!!!\n");
        // Incoming message request

        pjsip_rx_data *rdata;
        pjsip_msg *msg;
        pj_status_t status;
        std::string text;

        // Retrieve the body message
        rdata = e->body.tsx_state.src.rdata;
        msg = rdata->msg_info.msg;

        // Respond with OK message
        status = pjsip_dlg_respond(inv->dlg, rdata, PJSIP_SC_OK, NULL, NULL, NULL);*/
    } else if (tsx->state == PJSIP_TSX_STATE_PROCEEDING && tsx->role == PJSIP_ROLE_UAC) {
        int code;
        pjsip_rx_data *rdata;
        pjsip_msg *msg;
        AccountID accId;

        // Retrieve the body message
        rdata = e->body.tsx_state.src.rdata;
        msg = rdata->msg_info.msg;

        SIPCall *call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
        if(call == NULL)
            return;
        
        code = msg->line.status.code;
        switch (code) {
            case 180:
                _debug("HERE!!!!!!!!!!!!!!!!!!!\n");

                call->setConnectionState(Call::Ringing);
                Manager::instance().peerRingingCall(call->getCallId());
                break;
            default:
                break;
        }
    } else if (tsx->state == PJSIP_TSX_STATE_COMPLETED && tsx->role == PJSIP_ROLE_UAS) {
        if(tsx->status_code == 200 && tsx->method.id == PJSIP_BYE_METHOD) {
            _debug("Receive target phone hang up (bye) message\n");
            SIPCall *call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
            if(call == NULL) {
                _debug("Call has been removed!\n");
                return;
            }
            AccountID accId = Manager::instance().getAccountFromCall(call->getCallId());
            SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
            if(link) {
                link->SIPCallClosed(call);
                inv->mod_data[getInstance()->getModId()] = NULL;
            }
        } else if(tsx->status_code == 200 && tsx->method.id == PJSIP_CANCEL_METHOD) {
            _debug("Receive cancel message\n");
            SIPCall *call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
            if(call == NULL) {
                _debug("Call has been removed!\n");
                return;
            }
            AccountID accId = Manager::instance().getAccountFromCall(call->getCallId());
            SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
            if(link) {
                link->SIPCallReleased(call);
                inv->mod_data[getInstance()->getModId()] = NULL;
            }
        }
    } else if (tsx->state == PJSIP_TSX_STATE_COMPLETED && tsx->role == PJSIP_ROLE_UAC) {
        if (tsx->status_code/100 == 6 || tsx->status_code == 404) {
            _debug("Receive server error message\n");
            SIPCall *call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
            if(call == NULL) {
                _debug("Call has been removed!\n");
                return;
            }
            AccountID accId = Manager::instance().getAccountFromCall(call->getCallId());
            SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
            if(link) {
                link->SIPCallServerFailure(call);
                inv->mod_data[getInstance()->getModId()] = NULL;
            }
        }
    }
}

void SIPManager::call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e) {
    PJ_UNUSED_ARG(inv);
    /*if( inv->state == PJSIP_INV_STATE_CONFIRMED ) {
        _debug("Use this to handle the incoming call!!!\n");
        SIPCall *call = reinterpret_cast<SIPCall *>(inv->mod_data[getInstance()->getModId()]);
        
        
        _debug("The call id is %s\n", call->getCallId().data());
        pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
        AccountID accId = Manager::instance().getAccountFromCall(call->getCallId());
        Manager::instance().getAccountLink(accId)->callAnswered(call, rdata);
    }*/
}

bool SIPManager::onhold(SIPCall *call) {
    _debug("*********Before onhold pjsip_inv_reinite  begins***********\n");
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_attr *attr;


    /* Create re-INVITE with new offer */
    pjmedia_sdp_media_remove_all_attr(call->getLocalSDPSession()->media[0], "sendrecv");
    attr = pjmedia_sdp_attr_create(_pool, "sendonly", NULL);
    pjmedia_sdp_media_add_attr(call->getLocalSDPSession()->media[0], attr);

    status = pjsip_inv_reinvite( _invSession, NULL, call->getLocalSDPSession(), &tdata);
    /* Send the request */
    status = pjsip_inv_send_msg( _invSession, tdata);
 
    _debug("*********After pjsip_inv_reinite  begins***********\n");
}

bool SIPManager::offhold(SIPCall *call) {
    _debug("*********Before offhold pjsip_inv_reinite  begins***********\n");
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_attr *attr;


    /* Create re-INVITE with new offer */
    pjmedia_sdp_media_remove_all_attr(call->getLocalSDPSession()->media[0], "sendonly");
    attr = pjmedia_sdp_attr_create(_pool, "sendrecv", NULL);
    pjmedia_sdp_media_add_attr(call->getLocalSDPSession()->media[0], attr);

    status = pjsip_inv_reinvite( _invSession, NULL, call->getLocalSDPSession(), &tdata);
    /* Send the request */
    status = pjsip_inv_send_msg( _invSession, tdata);
 
    _debug("*********After pjsip_inv_reinite  begins***********\n");
}

bool SIPManager::hangup() {
    pj_status_t status;
    pjsip_tx_data *tdata;
    
    status = pjsip_inv_end_session(_invSession, 404, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    status = pjsip_inv_send_msg(_invSession, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    _invSession->mod_data[getInstance()->getModId()] = NULL;
    return true;
}

bool SIPManager::refuse()
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    
    status = pjsip_inv_end_session(_invSession, PJSIP_SC_DECLINE, NULL, &tdata); //603
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    status = pjsip_inv_send_msg(_invSession, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    _invSession->mod_data[getInstance()->getModId()] = NULL;
    return true;
}

bool SIPManager::transfer(SIPCall *call, const std::string& to)
{
    pjsip_evsub *sub;
    pjsip_tx_data *tdata;
    pjsip_dialog *dlg;
    pjsip_generic_string_hdr *gs_hdr;
    const pj_str_t str_ref_by = { "Referred-By", 11 };
    struct pjsip_evsub_user xfer_cb;
    pj_status_t status;
    pj_str_t dest;
    
    pj_strdup2(_pool, &dest, to.data());

    /* Create xfer client subscription. */
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &xfer_func_cb;
    
    status = pjsip_xfer_create_uac(_invSession->dlg, &xfer_cb, &sub);
    if (status != PJ_SUCCESS) {
        _debug("Unable to create xfer -- %d\n", status);
        return false;
    }
    
    _debug("xfer created!!!!!!!!!!!!!!!\n");
    /* Associate this call with the client subscription */
    AccountID accId = Manager::instance().getAccountFromCall(call->getCallId());
    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
    pjsip_evsub_set_mod_data(sub, _mod.id, link);

    _debug("call associated!!!!!!!!!!!!!!!\n");
    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate(sub, &dest, &tdata);
    if (status != PJ_SUCCESS) {
        _debug("Unable to create REFER request -- %d\n", status);
        return false;
    }

    _debug("Here1 !!!!!!!!!!!!!!!!!!!\n");
    /* Add Referred-By header */
    //gs_hdr = pjsip_generic_string_hdr_create(tdata->pool, &str_ref_by, &dest);
    
    //_debug("22222222222222222222222222\n");
    //pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)gs_hdr);
    //_debug("3333333333333333333333333333\n");

    /* Send. */
    status = pjsip_xfer_send_request(sub, tdata);
    if (status != PJ_SUCCESS) {
        _debug("Unable to send REFER request -- %d\n", status);
        return false;
    }

    return true;
}

void SIPManager::xfer_func_cb( pjsip_evsub *sub, pjsip_event *event)
{
    PJ_UNUSED_ARG(event);

    _debug("#############We are in the callback!!!\n");
    /*
     * When subscription is accepted (got 200/OK to REFER), check if 
     * subscription suppressed.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACCEPTED) {

        pjsip_rx_data *rdata;
        pjsip_generic_string_hdr *refer_sub;
        const pj_str_t REFER_SUB = { "Refer-Sub", 9 };

        SIPVoIPLink *link = reinterpret_cast<SIPVoIPLink *> (pjsip_evsub_get_mod_data(sub,
                getInstance()->getModId()));

        /* Must be receipt of response message */
        pj_assert(event->type == PJSIP_EVENT_TSX_STATE &&
                  event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
        rdata = event->body.tsx_state.src.rdata;

        /* Find Refer-Sub header */
        refer_sub = (pjsip_generic_string_hdr*)
                    pjsip_msg_find_hdr_by_name(rdata->msg_info.msg,
                                               &REFER_SUB, NULL);

        /* Check if subscription is suppressed */
        if (refer_sub && pj_stricmp2(&refer_sub->hvalue, "false")==0) {
            /* Since no subscription is desired, assume that call has been
             * transfered successfully.
             */
            if (link) {
                link->transferStep2();
            }

            /* Yes, subscription is suppressed.
             * Terminate our subscription now.
             */
            _debug("Xfer subscription suppressed, terminating event subcription...\n");
            pjsip_evsub_terminate(sub, PJ_TRUE);

        } else {
            /* Notify application about call transfer progress. 
             * Initially notify with 100/Accepted status.
             */
            _debug("Xfer subscription 100/Accepted received...\n");
        }
    }
    /*
     * On incoming NOTIFY, notify application about call transfer progress.
     */
    else if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACTIVE ||
             pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED)
    {
        pjsip_msg *msg;
        pjsip_msg_body *body;
        pjsip_status_line status_line;
        pj_bool_t is_last;
        pj_bool_t cont;
        pj_status_t status;

        SIPVoIPLink *link = reinterpret_cast<SIPVoIPLink *> (pjsip_evsub_get_mod_data(sub, 
                getInstance()->getModId()));

        /* When subscription is terminated, clear the xfer_sub member of 
         * the inv_data.
         */
        if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_set_mod_data(sub, getInstance()->getModId(), NULL);
            _debug("Xfer client subscription terminated\n");

        }

        if (!link || !event) {
            /* Application is not interested with call progress status */
            _debug("Either link or event is empty!\n");
            return;
        }

        /* This better be a NOTIFY request */
        if (event->type == PJSIP_EVENT_TSX_STATE &&
            event->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
        {
            pjsip_rx_data *rdata;

            rdata = event->body.tsx_state.src.rdata;

            /* Check if there's body */
            msg = rdata->msg_info.msg;
            body = msg->body;
            if (!body) {
                _debug("Warning: received NOTIFY without message body\n");
                return;
            }

            /* Check for appropriate content */
            if (pj_stricmp2(&body->content_type.type, "message") != 0 ||
                pj_stricmp2(&body->content_type.subtype, "sipfrag") != 0)
            {
                _debug("Warning: received NOTIFY with non message/sipfrag content\n");
                return;
            }

            /* Try to parse the content */
            status = pjsip_parse_status_line((char*)body->data, body->len,
                                             &status_line);
            if (status != PJ_SUCCESS) {
                _debug("Warning: received NOTIFY with invalid message/sipfrag content\n");
                return;
            }

        } else {
            _debug("Set code to 500!\n");
            status_line.code = 500;
            status_line.reason = *pjsip_get_status_text(500);
        }

        /* Notify application */
        is_last = (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED);
        cont = !is_last;
        
        if(status_line.code/100 == 2) {
            _debug("Try to stop rtp!\n");
            pjsip_tx_data *tdata;
            
            status = pjsip_inv_end_session(_invSession, PJSIP_SC_GONE, NULL, &tdata);
            if(status != PJ_SUCCESS) {
                _debug("Fail to create end session msg!\n");
            } else {
                status = pjsip_inv_send_msg(_invSession, tdata);
                if(status != PJ_SUCCESS) 
                    _debug("Fail to send end session msg!\n");
            }
            
            link->transferStep2();
            cont = PJ_FALSE;
        }
        
        if (!cont) {
            pjsip_evsub_set_mod_data(sub, getInstance()->getModId(), NULL);
        }
    }
         
}
