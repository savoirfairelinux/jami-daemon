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

#include <iostream>

#include "manager.h"
#include "sipcall.h"
#include "useragent.h"
#include "sipvoiplink.h"
#include "sipaccount.h"

#define DEFAULT_SIP_PORT  5060
#define RANDOM_SIP_PORT   rand() % 64000 + 1024
#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2

UserAgent *UserAgent::_current;

UserAgent::UserAgent():_endpt(NULL) ,_sock(NULL), _cp(), _pool(NULL), _mutex(NULL), _mod(), _options_handler(), _useStun(false), _stunHost(),
        _stunServer(""), _localExternAddress(""), _localIPAddress("127.0.0.1"), _localExternPort(0), _localPort(0), _thread(NULL) {
    //_useStun = false;
    //_localIPAddress = "127.0.0.1";
    UserAgent::_current = this;
}

UserAgent::~UserAgent() {
    _debug("UserAgent: In dtor!\n");
    sipDestory();
}

pj_status_t UserAgent::sipCreate() {

    pj_status_t status;

    // Init PJLIB: must be called before any call to the pjsip library
    status = pj_init();
    // Use pjsip macros for sanity check
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Init PJLIB-UTIL library 
    status = pjlib_util_init();
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Set the pjsip log level
    pj_log_set_level( PJ_LOG_LEVEL );

    // Init PJNATH 
    status = pjnath_init();
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
    
    // Create a pool factory to allocate memory
    pj_caching_pool_init(&_cp, &pj_pool_factory_default_policy, 0);

    // Create memory pool for application. 
    _pool = pj_pool_create(&_cp.factory, "sflphone", 4000, 4000, NULL);

    if (!_pool) {
        _debug("UserAgent: Could not initialize memory pool\n");
        return PJ_ENOMEM;
    }

    // Create a recursive mutex. Simple wrapper for pj_mutex_create 
    status = pj_mutex_create_recursive(_pool, "sflphone", &_mutex);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Create the SIP endpoint 
    status = pjsip_endpt_create(&_cp.factory, pj_gethostname()->ptr, &_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    return PJ_SUCCESS;
}

pj_status_t UserAgent::sipInit() {
    
    pj_status_t status;
    
    /* Init SIP UA: */

    //FIXME! DNS initialize here! */

    /* Start resolving STUN server */
    // if we useStun and we failed to receive something on port 5060, we try a random port
    // If use STUN server, firewall address setup
    if (!loadSIPLocalIP()) {
        _debug("UserAgent: Unable to determine network capabilities\n");
        return false;
    }
    int errPjsip = 0;
    int port = DEFAULT_SIP_PORT;

    //_debug("stun host is %s\n", _stunHost.ptr);
    if (_useStun && !Manager::instance().behindNat(_stunServer, port)) {
        port = RANDOM_SIP_PORT;
        if (!Manager::instance().behindNat(_stunServer, port)) {
            _debug("UserAgent: Unable to check NAT setting\n");
            return false; // hoho we can't use the random sip port too...
        }
    } else {
        //FIXME! check port number availability
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
        _debug("UserAgent: Could not initialize SIP listener on port %d\n", port);
        port = RANDOM_SIP_PORT;
        _debug("UserAgent: SIP failed to listen on port %d\n", port);
        return errPjsip;
    }
    _debug("UserAgent: SIP Init -- listening on port %d\n", _localExternPort);

    // Initialize transaction layer
    status = pjsip_tsx_layer_init_module(_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Initialize UA layer module
    status = pjsip_ua_init_module(_endpt, NULL);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Initialize Replaces support. See the Replaces specification in RFC 3891
    status = pjsip_replaces_init_module(_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Initialize 100rel support 
    status = pjsip_100rel_init_module(_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Initialize and register sflphone module
    {
        const pjsip_module mod_initializer ={
            NULL, NULL, 			// prev, next.			
            { (char*)"mod-sflphone", 9}, 	// Name.				
            -1,		 			// Id				
            PJSIP_MOD_PRIORITY_APPLICATION, 	// Priority			
            NULL, 				// load()				
            NULL, 				// start()				
            NULL, 				// stop()				
            NULL, 				// unload()				
            &mod_on_rx_request, 		// on_rx_request()			
            &mod_on_rx_response, 		// on_rx_response()			
            NULL, 				// on_tx_request.			
            NULL, 				// on_tx_response()			
            NULL, 				// on_tsx_state()			
        };

        _mod = mod_initializer;

        status = pjsip_endpt_register_module(_endpt, &_mod);
    	PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
        
    }

    // Init the event subscription module.
    // It extends PJSIP by supporting SUBSCRIBE and NOTIFY methods
    status = pjsip_evsub_init_module(_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Init presence module. 
    // TODO We probably do not need that extension
    status = pjsip_pres_init_module(_endpt, pjsip_evsub_instance());
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
    
    // Init PUBLISH module 
    // Provide an implementation of SIP Extension for Event State Publication (RFC 3903)
    // TODO Check if it is necessary
    status = pjsip_publishc_init_module(_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Init xfer/REFER module
    status = pjsip_xfer_init_module(_endpt);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
        
    /*{
 	const pjsip_module handler ={
            NULL, NULL, 			// prev, next.			
            { (char*)"mod-sflphone-options", 20},//9}, 	// Name.				
            -1,		 			// Id				
            PJSIP_MOD_PRIORITY_APPLICATION, 	// Priority			
            NULL, 				// load()				
            NULL, 				// start()				
            NULL, 				// stop()				
            NULL, 				// unload()				
            &options_on_rx_request, 		// on_rx_request()			
            NULL, 				// on_tx_request.			
            NULL, 				// on_tx_response()			
            NULL, 				// on_tsx_state()			
        };

        _options_handler = handler;
    }

    // Register the OPTIONS module
    status = pjsip_endpt_register_module(_endpt, &_options_handler);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

    // Add OPTIONS in Allow header
    status = pjsip_endpt_add_capability(_endpt, 
					NULL, 
					PJSIP_H_ALLOW,
            				NULL, 1, 
					&STR_OPTIONS);
    PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );*/


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

        _debug("UserAgent: VOIP callbacks initialized\n");

        // Initialize session invite module 
        status = pjsip_inv_usage_init(_endpt, &inv_cb);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }


    // Add endpoint capabilities (INFO, OPTIONS, etc) for this UA
    {
        pj_str_t allowed[] = {
            			{(char*)"INFO", 4},
            			{(char*)"REGISTER", 8}
			      }; //  //{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6},  {"OPTIONS", 7}, 
        pj_str_t accepted = {(char*)"application/sdp", 15};

        // Register supported methods
        pjsip_endpt_add_capability(_endpt, &_mod, PJSIP_H_ALLOW, NULL, PJ_ARRAY_SIZE(allowed), allowed);

        // Register "application/sdp" in ACCEPT header
        pjsip_endpt_add_capability(_endpt, &_mod, PJSIP_H_ACCEPT, NULL, 1, &accepted);
    }

    _debug("UserAgent: sflphone version %s for %s initialized\n", pj_get_version(), PJ_OS_NAME);

    Manager::instance().setSipThreadStatus(false);
    
    // Create the secondary thread to poll sip events
    status = pj_thread_create(_pool, "sflphone", &start_thread, NULL, PJ_THREAD_DEFAULT_STACK_SIZE, 0, 
			    	&_thread);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Done! */
    return PJ_SUCCESS;

}

void UserAgent::sipDestory() {
    /* Signal threads to quit: */
    Manager::instance().setSipThreadStatus(true);

    /* Wait worker thread to quit: */
    if (_thread) {
        pj_thread_join(_thread);
        pj_thread_destroy(_thread);
        _thread = NULL;
    }

    if (_endpt) {
        /* Terminate all presence subscriptions. */
        //pjsua_pres_shutdown();

        /* Wait for some time to allow unregistration to complete: */
        _debug("UserAgent: Shutting down...\n");
        busy_sleep(1000);
    }

    /* Destroy endpoint. */
    if (_endpt) {
        pjsip_endpt_destroy(_endpt);
        _endpt = NULL;
    }

    /* Destroy mutex */
    if (_mutex) {
        pj_mutex_destroy(_mutex);
        _mutex = NULL;
    }

    /* Destroy pool and pool factory. */
    if (_pool) {
        pj_pool_release(_pool);
        _pool = NULL;
        pj_caching_pool_destroy(&_cp);

        /* Shutdown PJLIB */
        pj_shutdown();
    }

    /* Done. */    
}

void UserAgent::busy_sleep(unsigned msec)
{
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
    /* Ideally we shouldn't call pj_thread_sleep() and rather
     * CActiveScheduler::WaitForAnyRequest() here, but that will
     * drag in Symbian header and it doesn't look pretty.
     */
    pj_thread_sleep(msec);
#else
    pj_time_val timeout, now, tv;

    pj_gettimeofday(&timeout);
    timeout.msec += msec;
    pj_time_val_normalize(&timeout);

    tv.sec = 0;
    tv.msec = 10;
    pj_time_val_normalize(&tv);
    
    do {
        pjsip_endpt_handle_events(_endpt, &tv);
        pj_gettimeofday(&now);
    } while (PJ_TIME_VAL_LT(now, timeout));
#endif
}

bool UserAgent::addAccount(AccountID id, pjsip_regc **regc2, const std::string& server, const std::string& user, const std::string& passwd, const int& timeout) {
    pj_status_t status;
    AccountID *currentId = new AccountID(id);
    char contactTmp[256];
    pjsip_regc *regc;
    pj_str_t svr;
    pj_str_t aor;
    pj_str_t contact;

    pj_mutex_lock(_mutex);
    std::string tmp;

    SIPAccount *account;

    status = pjsip_regc_create(_endpt, (void *) currentId, &regc_cb, &regc);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to create regc.\n");
        return status;
    }

    tmp = "sip:" + server;
    pj_strdup2(_pool, &svr, tmp.data());
    
    tmp = "<sip:" + user + "@" + server + ">";
    pj_strdup2(_pool, &aor, tmp.data());

    sprintf(contactTmp, "<sip:%s@%s:%d>", user.data(), _localExternAddress.data(), _localExternPort);
    pj_strdup2(_pool, &contact, contactTmp);

    //_debug("UserAgent: Get in %s %d %s\n", svr.ptr, svr.slen, aor.ptr);
    _debug("UserAgent: Contact is %s\n", contact.ptr);
    status = pjsip_regc_init(regc, &svr, &aor, &aor, 1, &contact, 600); //timeout);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to initialize regc. %d\n", status); //, regc->str_srv_url.ptr);
        return status;
    }

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount(id));
    pjsip_cred_info *cred = account->getCredInfo();

    if(!cred)
        cred = new pjsip_cred_info();

    pj_bzero(cred, sizeof (pjsip_cred_info));
    pj_strdup2(_pool, &cred->username, user.data());
    cred->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    pj_strdup2(_pool, &cred->data, passwd.data());
    pj_strdup2(_pool, &cred->realm, "*");
    pj_strdup2(_pool, &cred->scheme, "digest");
    pjsip_regc_set_credentials(regc, 1, cred);

    account->setCredInfo(cred);

    pjsip_tx_data *tdata;
    status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to register regc.\n");
        return status;
    }

    status = pjsip_regc_send(regc, tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to send regc request.\n");
        return status;
    }

    account->setUserName(user);
    account->setServer(server);
    account->setContact(contactTmp);

    // associate regc with account
    *regc2 = regc;
    
    pj_mutex_unlock(_mutex);
    return true;
}

bool UserAgent::removeAccount(pjsip_regc *regc)
{
    pj_status_t status = 0;
    pjsip_tx_data *tdata = NULL;
    
    pj_mutex_lock(_mutex);

    if(regc) {
        status = pjsip_regc_unregister(regc, &tdata);
        if(status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to unregister regc.\n");
            pj_mutex_unlock(_mutex);
            return false;
        }
        
        status = pjsip_regc_send( regc, tdata );
        if(status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to send regc request.\n");
            pj_mutex_unlock(_mutex);
            return false;
        }
    } else {
        _debug("UserAgent: regc is null!\n");
        pj_mutex_unlock(_mutex);
        return false;
    }
    
    pj_mutex_unlock(_mutex);
    return true;
}

pj_str_t UserAgent::buildContact(char *userName) {
    //pj_str_t contact;
    char tmp[256];

    //FIXME: IPV6 issue!!
    _debug("In build Contact %s %s %d\n", userName, _localExternAddress.data(), _localExternPort);
    sprintf(tmp, "<sip:%s@%s:%d>", userName, _localExternAddress.data(), _localExternPort);
    //_debug("get tmp\n");
    return pj_str(tmp);
}

pj_status_t UserAgent::stunServerResolve() {
    pj_str_t stun_adr;
    pj_hostent he;
    pj_stun_config stunCfg;
    pj_status_t stun_status;
    pj_sockaddr stun_srv;

    // Initialize STUN configuration
    pj_stun_config_init(&stunCfg, &_cp.factory, 0, pjsip_endpt_get_ioqueue(_endpt), pjsip_endpt_get_timer_heap(_endpt));

    stun_status = PJ_EPENDING;

    // Init STUN socket
    pj_strdup2(_pool, &stun_adr, _stunServer.data());
    stun_status = pj_sockaddr_in_init(&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);

    if (stun_status != PJ_SUCCESS) {
        _debug("UserAgent: Unresolved stun server!\n");
        stun_status = pj_gethostbyname(&stun_adr, &he);

        if (stun_status == PJ_SUCCESS) {
            pj_sockaddr_in_init(&stun_srv.ipv4, NULL, 0);
            stun_srv.ipv4.sin_addr = *(pj_in_addr*) he.h_addr;
            stun_srv.ipv4.sin_port = pj_htons((pj_uint16_t) 3478);
        }
    }

    return stun_status;
}

int UserAgent::createUDPServer() {
    pj_status_t status;
    //pj_str_t ipAddr;
    pj_sockaddr_in bound_addr;
    pjsip_host_port a_name;
    char tmpIP[32];

    // Init bound address to ANY
    pj_memset(&bound_addr, 0, sizeof (bound_addr));
    bound_addr.sin_addr.s_addr = PJ_INADDR_ANY;

    // Create UDP server socket
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &_sock);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: (%d) UDP socket() error\n", status);
        return status;
    }

    status = pj_sock_bind_in(_sock, pj_ntohl(bound_addr.sin_addr.s_addr), (pj_uint16_t) _localPort);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: (%d) UDP bind() error\n", status);
        pj_sock_close(_sock);
        return status;
    }

    _debug("UserAgent: Use IP: %s\n", _localExternAddress.data());

    // Create UDP-Server (default port: 5060)
    strcpy(tmpIP, _localExternAddress.data());
    pj_strdup2(_pool, &a_name.host, tmpIP);
    a_name.port = (pj_uint16_t) _localExternPort;

    status = pjsip_udp_transport_attach(_endpt, _sock, &a_name, 1, NULL);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: (%d) Unable to start UDP transport!\n", status);
        return -1;
    } else {
        _debug("UserAgent: UDP server listening on port %d\n", _localExternPort);
    }

    return 0;
}

void UserAgent::setStunServer(const char *server) {
    _stunServer = std::string(server);
    _useStun = true;
}

void UserAgent::regc_cb(struct pjsip_regc_cbparam *param) {
    
    AccountID *id = static_cast<AccountID *> (param->token);
    SIPVoIPLink *voipLink;
    
    _debug("UserAgent: Account ID is %s, Register result: %d, Status: %d\n", id->data(), param->status, param->code);
    
    if (param->status == PJ_SUCCESS) {
        if (param->code < 0 || param->code >= 300) {
            /* Sometimes, the status is OK, but we still failed.
             * So checking the code for real result
             */
            Manager::instance().getAccountLink(*id)->setRegistrationState(VoIPLink::Error);
        } else
            // Registration/Unregistration is success
            voipLink = dynamic_cast<SIPVoIPLink *>(Manager::instance().getAccountLink(*id));
            if(!voipLink)
                return;
        
            if(voipLink->isRegister())
                Manager::instance().getAccountLink(*id)->setRegistrationState(VoIPLink::Registered);
            else
                Manager::instance().getAccountLink(*id)->setRegistrationState(VoIPLink::Unregistered);
    } else {
        Manager::instance().getAccountLink(*id)->setRegistrationState(VoIPLink::Error);
    }
}

bool
UserAgent::loadSIPLocalIP() {
    bool returnValue = true;
    if (_localIPAddress == "127.0.0.1") {
        pj_sockaddr ip_addr;
        if (pj_gethostip(pj_AF_INET(), &ip_addr) != PJ_SUCCESS) {
            // Update the registration state if no network capabilities found
            _debug("UserAgent: Get host ip failed!\n");
            returnValue = false;
        } else {
            _localIPAddress = std::string(pj_inet_ntoa(ip_addr.ipv4.sin_addr));
            _debug("UserAgent: Checking network, setting local IP address to: %s\n", _localIPAddress.data());
        }
    }
    return returnValue;
}

/* Thread entry point function. */
int UserAgent::start_thread(void *arg) {

    PJ_UNUSED_ARG(arg);

    // FIXME! maybe we should add a flag for exiting!
    // TODO Add the flag. We have to stop the thread when destroying the instance 
    while (!Manager::instance().getSipThreadStatus()) {
        pj_time_val timeout = {0, 10};
        pjsip_endpt_handle_events(getInstance()->getEndPoint(), &timeout);
    }

    return 0;
}

void UserAgent::set_voicemail_info( AccountID account, pjsip_msg_body *body ){

    int voicemail, pos_begin, pos_end;
    std::string voice_str = "Voice-Message: ";
    std::string delimiter = "/";
    std::string msg_body, voicemail_str;

    _debug("UserAgent: checking the voice message!\n");
    // The voicemail message is formated like that:
    // Voice-Message: 1/0  . 1 is the number we want to retrieve in this case

    // We get the notification body
    msg_body = (char*)body->data;

    // We need the position of the first character of the string voice_str
    pos_begin = msg_body.find(voice_str); 
    // We need the position of the delimiter
    pos_end = msg_body.find(delimiter); 

    // So our voicemail number between the both index
    try {

    	voicemail_str = msg_body.substr(pos_begin + voice_str.length(), pos_end - ( pos_begin + voice_str.length()));
	std::cout << "voicemail number : " << voicemail_str << std::endl;
	voicemail = atoi( voicemail_str.c_str() );
    }
    catch( std::out_of_range& e ){
	std::cerr << e.what() << std::endl;
    }

    // We need now to notify the manager 
    if( voicemail != 0 )
	Manager::instance().startVoiceMessageNotification(account, voicemail);
}


pj_bool_t UserAgent::mod_on_rx_request(pjsip_rx_data *rdata) {

    pj_status_t status;
    pj_str_t reason;
    unsigned options = 0;
    pjsip_dialog* dialog;
    pjsip_tx_data *tdata;
    //pjmedia_sdp_session *r_sdp;
    AccountID account_id;

    // voicemail part
    std::string method_name;
    std::string request;

    // Handle the incoming call invite in this function 
    _debug("UserAgent: Callback on_rx_request is involved!\n");

    /* First, let's got the username and server name from the invite.
     * We will use them to detect which account is the callee.
     */ 
    pjsip_uri *uri = rdata->msg_info.to->uri;
    pjsip_sip_uri *sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(uri);

    std::string userName = std::string(sip_uri->user.ptr, sip_uri->user.slen);
    std::string server = std::string(sip_uri->host.ptr, sip_uri->host.slen);

    // Get the account id of callee from username and server
    account_id = Manager::instance().getAccountIdFromNameAndServer(userName, server);
    if(account_id == AccountNULL) {
            _debug("UserAgent: Username %s doesn't match any account!\n",userName);
            return PJ_FALSE;
    }
    _debug("UserAgent: The receiver is : %s@%s\n", userName.data(), server.data());
    _debug("UserAgent: The callee account id is %s\n", account_id.c_str());

    /* Now, it is the time to find the information of the caller */
    uri = rdata->msg_info.from->uri;
    sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(uri);
    
    std::string caller = std::string(sip_uri->user.ptr, sip_uri->user.slen);
    std::string callerServer = std::string(sip_uri->host.ptr, sip_uri->host.slen);
    std::string peerNumber = caller + "@" + callerServer;
    
    
    // Get the server voicemail notification
    // Catch the NOTIFY message
    if( rdata->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD )
    {
	method_name = "NOTIFY";
	// Retrieve all the message. Should contains only the method name but ...
	request =  rdata->msg_info.msg->line.req.method.name.ptr;
	// Check if the message is a notification
	if( request.find( method_name ) != -1 ) {
    		set_voicemail_info( account_id, rdata->msg_info.msg->body );
	}
        pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, PJSIP_SC_OK, NULL, NULL, NULL);
	return PJ_SUCCESS;
    }

    // Respond statelessly any non-INVITE requests with 500
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
        if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
            pj_strdup2(getInstance()->getAppPool(), &reason, "user agent unable to handle this request ");
            pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, PJSIP_SC_METHOD_NOT_ALLOWED, &reason, NULL,
                    NULL);
            return PJ_TRUE;
        }
    }
    
    // Verify that we can handle the request
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL, getInstance()->getEndPoint(), NULL);
    if (status != PJ_SUCCESS) {
        pj_strdup2(getInstance()->getAppPool(), &reason, "user agent unable to handle this INVITE ");
        pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, PJSIP_SC_METHOD_NOT_ALLOWED, &reason, NULL,
                NULL);
        return PJ_TRUE;
    }

    // Generate a new call ID for the incoming call!
    CallID id = Manager::instance().getNewCallID();

    _debug("UserAgent: The call id of the incoming call is %s\n", id.c_str());
    SIPCall* call = new SIPCall(id, Call::Incoming);
    if (!call) {
        _debug("UserAgent: unable to create an incoming call");
        return PJ_FALSE;
    }

    // Set the codec map, IP, peer number and so on... for the SIPCall object
    getInstance()->setCallAudioLocal(call);
    call->setCodecMap(Manager::instance().getCodecDescriptorMap());
    call->setConnectionState(Call::Progressing);
    call->setIp(getInstance()->getLocalIP());
    call->setPeerNumber(peerNumber);
    
    /* Call the SIPCallInvite function to generate the local sdp,
     * remote sdp and negociator.
     * This function is also used to set the parameters of audio RTP, including:
     *     local IP and port number 
     *     remote IP and port number
     *     possilbe audio codec will be used in this call
     */
    if (call->SIPCallInvite(rdata, getInstance()->getAppPool())) {
                
        // Notify UI there is an incoming call
        if (Manager::instance().incomingCall(call, account_id)) {
            // Add this call to the callAccountMap in ManagerImpl
            Manager::instance().getAccountLink(account_id)->addCall(call);
            _debug("UserAgent: Notify UI success!\n");
        } else {
            // Fail to notify UI
            delete call;
            call = NULL;
            _debug("UserAgent: Fail to notify UI!\n");
            return PJ_FALSE;
        }
    } else {
        // Fail to collect call information
        delete call;
        call = NULL;
        _debug("UserAgent: Call SIPCallInvite failed!\n");
        return PJ_FALSE;
    }

    /* Create the local dialog (UAS) */
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, NULL, &dialog);
    if (status != PJ_SUCCESS) {
        pjsip_endpt_respond_stateless(getInstance()->getEndPoint(), rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, &reason, NULL,
                NULL);
        return PJ_TRUE;
    }
    
    // Specify media capability during invite session creation
    pjsip_inv_session *inv;
    status = pjsip_inv_create_uas(dialog, rdata, call->getLocalSDPSession(), 0, &inv);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Associate the call in the invite session
    inv->mod_data[getInstance()->getModId()] = call;
    
    // Send a 180/Ringing response
    status = pjsip_inv_initial_answer(inv, rdata, PJSIP_SC_RINGING, NULL, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    status = pjsip_inv_send_msg(inv, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Associate invite session to the current call
    call->setInvSession(inv);

    // Update the connection state
    call->setConnectionState(Call::Ringing);

    /* Done */
    return PJ_SUCCESS;
}

bool UserAgent::setCallAudioLocal(SIPCall* call) {
    // Firstly, we use the local IP and port number
    unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
    unsigned int callLocalExternAudioPort = callLocalAudioPort;
    
    if (_useStun) {
        // If use Stun server, modify them
        if (Manager::instance().behindNat(_stunServer, callLocalAudioPort)) {
            callLocalExternAudioPort = Manager::instance().getFirewallPort();
        }
    }
    _debug("UserAgent: Setting local audio port to: %d\n", callLocalAudioPort);
    _debug("UserAgent: Setting local audio port (external) to: %d\n", callLocalExternAudioPort);

    // Set local audio port for SIPCall(id)
    call->setLocalIp(_localIPAddress);
    call->setLocalAudioPort(callLocalAudioPort);
    call->setLocalExternAudioPort(callLocalExternAudioPort);

    return true;
}

int UserAgent::answer(SIPCall *call) {
    pj_status_t status;
    pjsip_tx_data *tdata;

    // User answered the incoming call, tell peer this news
    if (call->startNegociation(_pool)) {
        // Create and send a 200(OK) response
        _debug("UserAgent: Negociation success!\n");
        status = pjsip_inv_answer(call->getInvSession(), PJSIP_SC_OK, NULL, NULL, &tdata);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
        status = pjsip_inv_send_msg(call->getInvSession(), tdata);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

        return 0;
    }

    return 1;
}

bool UserAgent::makeOutgoingCall(const std::string& strTo, SIPCall* call, const AccountID& id) {
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_tx_data *tdata;
    pj_str_t from, to, contact;

    // Get the basic information about the callee account
    SIPAccount* account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(id));
    
    // Generate the from URI
    std::string strFrom = "sip:" + account->getUserName() + "@" + account->getServer();

    _debug("UserAgent: Make a new call from:%s to %s. Contact is %s\n", 
            strFrom.data(), strTo.data(), account->getContact().data());

    // pjsip need the from and to information in pj_str_t format
    pj_strdup2(_pool, &from, strFrom.data());
    pj_strdup2(_pool, &to, strTo.data());
    pj_strdup2(_pool, &contact, account->getContact().data());

    // create the dialog (UAC)
    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &from,
                                    &contact,
                                    &to,
                                    NULL,
                                    &dialog);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    setCallAudioLocal(call);
    call->setIp(getInstance()->getLocalIP());

    // Building the local SDP offer
    call->createInitialOffer(_pool);

    // Create the invite session for this call
    pjsip_inv_session *inv;
    status = pjsip_inv_create_uac(dialog, call->getLocalSDPSession(), 0, &inv);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Set auth information
    pjsip_auth_clt_set_credentials(&dialog->auth_sess, 1, account->getCredInfo());

    // Associate current call in the invite session
    inv->mod_data[_mod.id] = call;

    status = pjsip_inv_invite(inv, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Associate current invite session in the call
    call->setInvSession(inv);
    
    status = pjsip_inv_send_msg(inv, tdata);
    //PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    if(status != PJ_SUCCESS) {
	return false;
    }

    return true;
}

void UserAgent::call_on_forked(pjsip_inv_session *inv, pjsip_event *e) {
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}

void UserAgent::call_on_tsx_changed(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e) {

    pjsip_rx_data *rdata;
    AccountID accId;
    SIPCall *call;
    SIPVoIPLink *link;
    pjsip_msg *msg;

    _debug("UserAgent: TSX Changed! The tsx->state is %d; tsx->role is %d; code is %d; method id is %.*s.\n",
            tsx->state, tsx->role, tsx->status_code, tsx->method.name.slen, tsx->method.name.ptr);

    //Retrieve the body message
    rdata = e->body.tsx_state.src.rdata;

    if (tsx->role == PJSIP_ROLE_UAC) {
        switch (tsx->state) {
            case PJSIP_TSX_STATE_TERMINATED:
                if (tsx->status_code == 200 &&
                        pjsip_method_cmp(&tsx->method, pjsip_get_refer_method()) != 0) {
                    // Peer answered the outgoing call
                    _debug("UserAgent: Peer answered the outgoing call!\n");
                    call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
                    if (call == NULL)
                        return;

                    //_debug("UserAgent: The call id is %s\n", call->getCallId().data());

                    accId = Manager::instance().getAccountFromCall(call->getCallId());
                    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
                    if (link)
                        link->SIPCallAnswered(call, rdata);
                } else if (tsx->status_code / 100 == 5) {
		    _debug("UserAgent: 5xx error message received\n");
                }
                break;
            case PJSIP_TSX_STATE_PROCEEDING:
                // Peer is ringing for the outgoing call
                msg = rdata->msg_info.msg;

                call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
                if (call == NULL)
                    return;

                if (msg->line.status.code == 180) {
                    _debug("UserAgent: Peer is ringing!\n");

                    call->setConnectionState(Call::Ringing);
                    Manager::instance().peerRingingCall(call->getCallId());
                }
                break;
            case PJSIP_TSX_STATE_COMPLETED:
                if (tsx->status_code / 100 == 6 || tsx->status_code / 100 == 4) {
                    // We get error message of outgoing call from server
                    _debug("UserAgent: Server error message is received!\n");
                    call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
                    if (call == NULL) {
                        _debug("UserAgent: Call has been removed!\n");
                        return;
                    }
                    accId = Manager::instance().getAccountFromCall(call->getCallId());
                    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
                    if (link) {
                        link->SIPCallServerFailure(call);
                    }
                }
                break;
            default:
                break;
        } // end of switch
        
    } else {
        switch (tsx->state) {
            case PJSIP_TSX_STATE_TRYING:
                if (pjsip_method_cmp(&tsx->method, pjsip_get_refer_method()) == 0) {
                    // Peer ask me to transfer call to another number.
                    _debug("UserAgent: Incoming REFER request!\n");
                    getInstance()->onCallTransfered(inv, e->body.tsx_state.src.rdata);
                }
                break;
            case PJSIP_TSX_STATE_COMPLETED:
                if (tsx->status_code == 200 && tsx->method.id == PJSIP_BYE_METHOD) {
                    // Peer hangup the call
                    _debug("UserAgent: Peer hangup(bye) message is received!\n");
                    call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
                    if (call == NULL) {
                        _debug("UserAgent: Call has been removed!\n");
                        return;
                    }
                    accId = Manager::instance().getAccountFromCall(call->getCallId());
                    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
                    if (link) {
                        link->SIPCallClosed(call);
                    }
                } else if (tsx->status_code == 200 && tsx->method.id == PJSIP_CANCEL_METHOD) {
                    // Peer refuse the call
                    _debug("UserAgent: Cancel message is received!\n");
                    call = reinterpret_cast<SIPCall *> (inv->mod_data[getInstance()->getModId()]);
                    if (call == NULL) {
                        _debug("UserAgent: Call has been removed!\n");
                        return;
                    }

                    accId = Manager::instance().getAccountFromCall(call->getCallId());
                    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
                    if (link) {
                        link->SIPCallReleased(call);
                    }
                }
                break;
            default:
                break;
        } // end of switch
    }

}

void UserAgent::call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e) {

    PJ_UNUSED_ARG(inv);
    
    SIPCall *call = reinterpret_cast<SIPCall*> (inv->mod_data[getInstance()->getModId()]);
    if(!call)
        return;
    
    /* If this is an outgoing INVITE that was created because of
     * REFER/transfer, send NOTIFY to transferer.
     */
    if (call->getXferSub() && e->type==PJSIP_EVENT_TSX_STATE)  {
        int st_code = -1;
        pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;


        switch (call->getInvSession()->state) {
            case PJSIP_INV_STATE_NULL:
            case PJSIP_INV_STATE_CALLING:
                /* Do nothing */
                break;

            case PJSIP_INV_STATE_EARLY:
            case PJSIP_INV_STATE_CONNECTING:
                st_code = e->body.tsx_state.tsx->status_code;
                ev_state = PJSIP_EVSUB_STATE_ACTIVE;
                break;

            case PJSIP_INV_STATE_CONFIRMED:
                /* When state is confirmed, send the final 200/OK and terminate
                 * subscription.
                 */
                st_code = e->body.tsx_state.tsx->status_code;
                ev_state = PJSIP_EVSUB_STATE_TERMINATED;
                break;

            case PJSIP_INV_STATE_DISCONNECTED:
                st_code = e->body.tsx_state.tsx->status_code;
                ev_state = PJSIP_EVSUB_STATE_TERMINATED;
                break;

            case PJSIP_INV_STATE_INCOMING:
                /* Nothing to do. Just to keep gcc from complaining about
                 * unused enums.
                 */
                break;
        }

        if (st_code != -1) {
            pjsip_tx_data *tdata;
            pj_status_t status;

            status = pjsip_xfer_notify( call->getXferSub(),
                                        ev_state, st_code,
                                        NULL, &tdata);
            if (status != PJ_SUCCESS) {
                _debug("UserAgent: Unable to create NOTIFY -- %d\n", status);
            } else {
                status = pjsip_xfer_send_request(call->getXferSub(), tdata);
                if (status != PJ_SUCCESS) {
                    _debug("UserAgent: Unable to send NOTIFY -- %d\n", status);
                }
            }
        }
    }

}

bool UserAgent::onhold(SIPCall *call) {
    _debug("UserAgent: Before onhold pjsip_inv_reinite begins!\n");
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_attr *attr;


    /* Create re-INVITE with new offer */
    pjmedia_sdp_media_remove_all_attr(call->getLocalSDPSession()->media[0], "sendrecv");
    attr = pjmedia_sdp_attr_create(_pool, "sendonly", NULL);
    pjmedia_sdp_media_add_attr(call->getLocalSDPSession()->media[0], attr);

    status = pjsip_inv_reinvite( call->getInvSession(), NULL, call->getLocalSDPSession(), &tdata);
    /* Send the request */
    status = pjsip_inv_send_msg( call->getInvSession(), tdata);
 
    _debug("UserAgent: After pjsip_inv_reinite begins!\n");
    return (status == PJ_SUCCESS);
}

bool UserAgent::offhold(SIPCall *call) {
    _debug("UserAgent: Before offhold pjsip_inv_reinite begins!\n");
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_attr *attr;


    /* Create re-INVITE with new offer */
    pjmedia_sdp_media_remove_all_attr(call->getLocalSDPSession()->media[0], "sendonly");
    attr = pjmedia_sdp_attr_create(_pool, "sendrecv", NULL);
    pjmedia_sdp_media_add_attr(call->getLocalSDPSession()->media[0], attr);

    status = pjsip_inv_reinvite( call->getInvSession(), NULL, call->getLocalSDPSession(), &tdata);
    /* Send the request */
    status = pjsip_inv_send_msg( call->getInvSession(), tdata);
 
    _debug("UserAgent: After pjsip_inv_reinite begins!\n");
    return (status == PJ_SUCCESS);
}

bool UserAgent::hangup(SIPCall* call) {
    pj_status_t status;
    pjsip_tx_data *tdata;
    
    // User hangup current call. Notify peer
    status = pjsip_inv_end_session(call->getInvSession(), 404, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    status = pjsip_inv_send_msg(call->getInvSession(), tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    call->getInvSession()->mod_data[getInstance()->getModId()] = NULL;
    return true;
}

bool UserAgent::refuse(SIPCall* call)
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    
    // User refuse current call. Notify peer
    status = pjsip_inv_end_session(call->getInvSession(), PJSIP_SC_DECLINE, NULL, &tdata); //603
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    status = pjsip_inv_send_msg(call->getInvSession(), tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    call->getInvSession()->mod_data[getInstance()->getModId()] = NULL;
    return true;
}

bool UserAgent::transfer(SIPCall *call, const std::string& to)
{
    pjsip_evsub *sub;
    pjsip_tx_data *tdata;
    //pjsip_dialog *dlg;
    //pjsip_generic_string_hdr *gs_hdr;
    //const pj_str_t str_ref_by = { (char*)"Referred-By", 11 };
    struct pjsip_evsub_user xfer_cb;
    pj_status_t status;
    pj_str_t dest;
    
    pj_strdup2(_pool, &dest, to.data());

    /* Create xfer client subscription. */
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &xfer_func_cb;
    
    status = pjsip_xfer_create_uac(call->getInvSession()->dlg, &xfer_cb, &sub);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to create xfer -- %d\n", status);
        return false;
    }
    
    /* Associate this voiplink of call with the client subscription 
     * We can not just associate call with the client subscription
     * because after this function, we can not find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    AccountID accId = Manager::instance().getAccountFromCall(call->getCallId());
    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
    pjsip_evsub_set_mod_data(sub, _mod.id, link);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate(sub, &dest, &tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to create REFER request -- %d\n", status);
        return false;
    }

    /* Send. */
    status = pjsip_xfer_send_request(sub, tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to send REFER request -- %d\n", status);
        return false;
    }

    return true;
}

void UserAgent::xfer_func_cb( pjsip_evsub *sub, pjsip_event *event)
{
    PJ_UNUSED_ARG(event);

    _debug("UserAgent: Transfer callback is involved!\n");
    /*
     * When subscription is accepted (got 200/OK to REFER), check if 
     * subscription suppressed.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACCEPTED) {

        pjsip_rx_data *rdata;
        pjsip_generic_string_hdr *refer_sub;
        const pj_str_t REFER_SUB = {(char*)"Refer-Sub", 9 };

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
                // It's the time to stop the RTP
                link->transferStep2();
            }

            /* Yes, subscription is suppressed.
             * Terminate our subscription now.
             */
            _debug("UserAgent: Xfer subscription suppressed, terminating event subcription...\n");
            pjsip_evsub_terminate(sub, PJ_TRUE);

        } else {
            /* Notify application about call transfer progress. 
             * Initially notify with 100/Accepted status.
             */
            _debug("UserAgent: Xfer subscription 100/Accepted received...\n");
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
            _debug("UserAgent: Xfer client subscription terminated\n");

        }

        if (!link || !event) {
            /* Application is not interested with call progress status */
            _debug("UserAgent: Either link or event is empty!\n");
            return;
        }

        // Get current call
        SIPCall *call = dynamic_cast<SIPCall *>(link->getCall(Manager::instance().getCurrentCallId()));
        if(!call) {
            _debug("UserAgent: Call doesn't exit!\n");
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
                _debug("UserAgent: Warning! Received NOTIFY without message body\n");
                return;
            }

            /* Check for appropriate content */
            if (pj_stricmp2(&body->content_type.type, "message") != 0 ||
                pj_stricmp2(&body->content_type.subtype, "sipfrag") != 0)
            {
                _debug("UserAgent: Warning! Received NOTIFY with non message/sipfrag content\n");
                return;
            }

            /* Try to parse the content */
            status = pjsip_parse_status_line((char*)body->data, body->len,
                                             &status_line);
            if (status != PJ_SUCCESS) {
                _debug("UserAgent: Warning! Received NOTIFY with invalid message/sipfrag content\n");
                return;
            }

        } else {
            _debug("UserAgent: Set code to 500!\n");
            status_line.code = 500;
            status_line.reason = *pjsip_get_status_text(500);
        }

        /* Notify application */
        is_last = (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED);
        cont = !is_last;
        
        if(status_line.code/100 == 2) {
            _debug("UserAgent: Try to stop rtp!\n");
            pjsip_tx_data *tdata;
            
            status = pjsip_inv_end_session(call->getInvSession(), PJSIP_SC_GONE, NULL, &tdata);
            if(status != PJ_SUCCESS) {
                _debug("UserAgent: Fail to create end session msg!\n");
            } else {
                status = pjsip_inv_send_msg(call->getInvSession(), tdata);
                if(status != PJ_SUCCESS) 
                    _debug("UserAgent: Fail to send end session msg!\n");
            }
            
            link->transferStep2();
            cont = PJ_FALSE;
        }
        
        if (!cont) {
            pjsip_evsub_set_mod_data(sub, getInstance()->getModId(), NULL);
        }
    }
         
}

void UserAgent::onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    SIPCall *existing_call;
    const pj_str_t str_refer_to = { (char*)"Refer-To", 8};
    const pj_str_t str_refer_sub = { (char*)"Refer-Sub", 9 };
    const pj_str_t str_ref_by = { (char*)"Referred-By", 11 };
    pjsip_generic_string_hdr *refer_to;
    pjsip_generic_string_hdr *refer_sub;
    pjsip_hdr *ref_by_hdr;
    pj_bool_t no_refer_sub = PJ_FALSE;
    char *uri;
    std::string tmp;
    pjsip_status_code code;
    pjsip_evsub *sub;

    existing_call = (SIPCall *) inv->mod_data[getInstance()->getModId()];

    /* Find the Refer-To header */
    refer_to = (pjsip_generic_string_hdr*)
        pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL);

    if (refer_to == NULL) {
        /* Invalid Request.
         * No Refer-To header!
         */
        _debug("UserAgent: Received REFER without Refer-To header!\n");
        pjsip_dlg_respond( inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    /* Find optional Refer-Sub header */
    refer_sub = (pjsip_generic_string_hdr*)
        pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_sub, NULL);

    if (refer_sub) {
        if (!pj_strnicmp2(&refer_sub->hvalue, "true", 4)==0)
            no_refer_sub = PJ_TRUE;
    }

    /* Find optional Referred-By header (to be copied onto outgoing INVITE
     * request.
     */
    ref_by_hdr = (pjsip_hdr*)
                 pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_ref_by,
                                            NULL);

    /* Notify callback */
    code = PJSIP_SC_ACCEPTED;

    _debug("UserAgent: Call to %.*s is being transfered to %.*s\n",
              (int)inv->dlg->remote.info_str.slen,
              inv->dlg->remote.info_str.ptr,
              (int)refer_to->hvalue.slen,
              refer_to->hvalue.ptr);

    if (no_refer_sub) {
        /*
         * Always answer with 2xx.
         */
        pjsip_tx_data *tdata;
        const pj_str_t str_false = { (char*)"false", 5};
        pjsip_hdr *hdr;

        status = pjsip_dlg_create_response(inv->dlg, rdata, code, NULL,
                                           &tdata);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to create 2xx response to REFER -- %d\n", status);
            return;
        }

        /* Add Refer-Sub header */
        hdr = (pjsip_hdr*)
               pjsip_generic_string_hdr_create(tdata->pool, &str_refer_sub,
                                              &str_false);
        pjsip_msg_add_hdr(tdata->msg, hdr);

 
        /* Send answer */
        status = pjsip_dlg_send_response(inv->dlg, pjsip_rdata_get_tsx(rdata),
                                         tdata);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to create 2xx response to REFER -- %d\n", status);
            return;
        }

        /* Don't have subscription */
        sub = NULL;

    } else {
        struct pjsip_evsub_user xfer_cb;
        pjsip_hdr hdr_list;

        /* Init callback */
        pj_bzero(&xfer_cb, sizeof(xfer_cb));
        xfer_cb.on_evsub_state = &xfer_svr_cb;

        /* Init addiTHIS_FILE, THIS_FILE, tional header list to be sent with REFER response */
        pj_list_init(&hdr_list);

        /* Create transferee event subscription */
        status = pjsip_xfer_create_uas( inv->dlg, &xfer_cb, rdata, &sub);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to create xfer uas -- %d\n", status);
            pjsip_dlg_respond( inv->dlg, rdata, 500, NULL, NULL, NULL);
            return;
        }

        /* If there's Refer-Sub header and the value is "true", send back
         * Refer-Sub in the response with value "true" too.
         */
        if (refer_sub) {
            const pj_str_t str_true = { (char*)"true", 4 };
            pjsip_hdr *hdr;

            hdr = (pjsip_hdr*)
                   pjsip_generic_string_hdr_create(inv->dlg->pool,
                                                   &str_refer_sub,
                                                   &str_true);
            pj_list_push_back(&hdr_list, hdr);

        }

        /* Accept the REFER request, send 2xx. */
        pjsip_xfer_accept(sub, rdata, code, &hdr_list);

        /* Create initial NOTIFY request */
        status = pjsip_xfer_notify( sub, PJSIP_EVSUB_STATE_ACTIVE,
                                    100, NULL, &tdata);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to create NOTIFY to REFER -- %d", status);
            return;
        }

        /* Send initial NOTIFY request */
        status = pjsip_xfer_send_request( sub, tdata);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to send NOTIFY to REFER -- %d\n", status);
            return;
        }
    }

    /* We're cheating here.
     * We need to get a null terminated string from a pj_str_t.
     * So grab the pointer from the hvalue and NULL terminate it, knowing
     * that the NULL position will be occupied by a newline. 
     */
    uri = refer_to->hvalue.ptr;
    uri[refer_to->hvalue.slen] = '\0';

    /* Now make the outgoing call. */
    tmp = std::string(uri);

    if(existing_call == NULL) {
        _debug("UserAgent: Call doesn't exist!\n");
        return;
    }
    
    AccountID accId = Manager::instance().getAccountFromCall(existing_call->getCallId());
    CallID newCallId = Manager::instance().getNewCallID();
    
    if(!Manager::instance().outgoingCall(accId, newCallId, tmp)) {
        
        /* Notify xferer about the error (if we have subscription) */
        if (sub) {
            status = pjsip_xfer_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
                                       500, NULL, &tdata);
            if (status != PJ_SUCCESS) {
                _debug("UserAgent: Unable to create NOTIFY to REFER -- %d\n", status);
                return;
            }
            status = pjsip_xfer_send_request(sub, tdata);
            if (status != PJ_SUCCESS) {
                _debug("UserAgent: Unable to send NOTIFY to REFER -- %d\n", status);
                return;
            }
        }
        return;
    }

    SIPCall* newCall;
    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
    if(link) {
        newCall = dynamic_cast<SIPCall *>(link->getCall(newCallId));
        if(!newCall) {
            _debug("UserAgent: can not find the call from sipvoiplink!\n");
            return;
        }
    }

    if (sub) {
        /* Put the server subscription in inv_data.
         * Subsequent state changed in pjsua_inv_on_state_changed() will be
         * reported back to the server subscription.
         */
        newCall->setXferSub(sub);

        /* Put the invite_data in the subscription. */
        pjsip_evsub_set_mod_data(sub, _mod.id,
                                 newCall);
    }    
}

void UserAgent::xfer_svr_cb(pjsip_evsub *sub, pjsip_event *event)
{
    PJ_UNUSED_ARG(event);

    /*
     * When subscription is terminated, clear the xfer_sub member of 
     * the inv_data.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        SIPCall *call;

        call = (SIPCall*) pjsip_evsub_get_mod_data(sub, getInstance()->getModId());
        if (!call)
            return;

        pjsip_evsub_set_mod_data(sub, getInstance()->getModId(), NULL);
        call->setXferSub(NULL);

        _debug("UserAgent: Xfer server subscription terminated\n");
    }    
}
