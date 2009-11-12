/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include "sipvoiplink.h"

#include "manager.h"

#include "sip/sdp.h"
#include "sipcall.h"
#include "sipaccount.h"
#include "eventthread.h"

#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"

#include "pjsip/sip_endpoint.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_uri.h"
#include <pjnath.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <istream>

#define CAN_REINVITE        1

static char * invitationStateMap[] = {
    (char*) "PJSIP_INV_STATE_NULL",
    (char*) "PJSIP_INV_STATE_CALLING",
    (char*) "PJSIP_INV_STATE_INCOMING",
    (char*) "PJSIP_INV_STATE_EARLY",
    (char*) "PJSIP_INV_STATE_CONNECTING",
    (char*) "PJSIP_INV_STATE_CONFIRMED",
    (char*) "PJSIP_INV_STATE_DISCONNECTED"
};

static char * transactionStateMap[] = {
    (char*) "PJSIP_TSX_STATE_NULL" ,
    (char*) "PJSIP_TSX_STATE_CALLING",
    (char*) "PJSIP_TSX_STATE_TRYING",
    (char*) "PJSIP_TSX_STATE_PROCEEDING",
    (char*) "PJSIP_TSX_STATE_COMPLETED",
    (char*) "PJSIP_TSX_STATE_CONFIRMED",
    (char*) "PJSIP_TSX_STATE_TERMINATED",
    (char*) "PJSIP_TSX_STATE_DESTROYED",
    (char*) "PJSIP_TSX_STATE_MAX"
};

struct result {
    pj_status_t             status;
    pjsip_server_addresses  servers;
};

pjsip_transport *_localUDPTransport;

const pj_str_t STR_USER_AGENT = { (char*) "User-Agent", 10 };

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

/*
 * Retrieve the SDP of the peer contained in the offer
 *
 * @param rdata The request data
 * @param r_sdp The pjmedia_sdp_media to stock the remote SDP
 */
void get_remote_sdp_from_offer (pjsip_rx_data *rdata, pjmedia_sdp_session** r_sdp);

int getModId();

/**
 * Set audio (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 * @return bool True
 */
bool setCallAudioLocal (SIPCall* call, std::string localIP);

void handle_incoming_options (pjsip_rx_data *rxdata);

std::string fetch_header_value (pjsip_msg *msg, std::string field);

std::string getLocalAddressAssociatedToAccount (AccountID id);


/*
 *  The global pool factory
 */
pj_caching_pool _cp;

/*
 * The pool to allocate memory
 */
pj_pool_t *_pool;

/*
 *	The SIP endpoint
 */
pjsip_endpoint *_endpt;

/*
 *	The SIP module
 */
pjsip_module _mod_ua;

/*
 * Thread related
 */
pj_thread_t *thread;
pj_thread_desc desc;

/*
 * Url hook instance
 */
UrlHook *urlhook;

/**
 * Get the number of voicemail waiting in a SIP message
 */
void set_voicemail_info (AccountID account, pjsip_msg_body *body);

// Documentated from the PJSIP Developer's Guide, available on the pjsip website/


pj_bool_t stun_sock_on_status (pj_stun_sock *stun_sock, pj_stun_sock_op op, pj_status_t status);
pj_bool_t stun_sock_on_rx_data (pj_stun_sock *stun_sock, void *pkt, unsigned pkt_len, const pj_sockaddr_t *src_addr, unsigned addr_len);


/*
 * Session callback
 * Called when the invite session state has changed.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	e	A pointer on a pjsip_event structure
 */
void call_on_state_changed (pjsip_inv_session *inv, pjsip_event *e);

/*
 * Session callback
 * Called after SDP offer/answer session has completed.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	status	A pj_status_t structure
 */
void call_on_media_update (pjsip_inv_session *inv, pj_status_t status UNUSED);

/*
 * Called when the invite usage module has created a new dialog and invite
 * because of forked outgoing request.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	e	A pointer on a pjsip_event structure
 */
void call_on_forked (pjsip_inv_session *inv, pjsip_event *e);

/*
 * Session callback
 * Called whenever any transactions within the session has changed their state.
 * Useful to monitor the progress of an outgoing request.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	tsx	A pointer on a pjsip_transaction structure
 * @param	e	A pointer on a pjsip_event structure
 */
void call_on_tsx_changed (pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);

void on_rx_offer (pjsip_inv_session *inv, const pjmedia_sdp_session *offer);

/*
 * Registration callback
 */
void regc_cb (struct pjsip_regc_cbparam *param);

/*
 * DNS Callback used in workaround for bug #1852
 */
static void dns_cb (pj_status_t status, void *token, const struct pjsip_server_addresses *addr);

/*
 * Called to handle incoming requests outside dialogs
 * @param   rdata
 * @return  pj_bool_t
 */
pj_bool_t mod_on_rx_request (pjsip_rx_data *rdata);

/*
 * Called to handle incoming response
 * @param	rdata
 * @return	pj_bool_t
 */
pj_bool_t mod_on_rx_response (pjsip_rx_data *rdata UNUSED) ;

/*
 * Transfer callbacks
 */
void xfer_func_cb (pjsip_evsub *sub, pjsip_event *event);
void xfer_svr_cb (pjsip_evsub *sub, pjsip_event *event);
void onCallTransfered (pjsip_inv_session *inv, pjsip_rx_data *rdata);

/*************************************************************************************************/

SIPVoIPLink* SIPVoIPLink::_instance = NULL;


SIPVoIPLink::SIPVoIPLink (const AccountID& accountID)
        : VoIPLink (accountID)
        , _nbTryListenAddr (2)   // number of times to try to start SIP listener
        , _regPort (atoi (DEFAULT_SIP_PORT))
        , _clients (0)
{
    // to get random number for RANDOM_PORT
    srand (time (NULL));

    urlhook = new UrlHook ();

    /* Start pjsip initialization step */
    init();
}

SIPVoIPLink::~SIPVoIPLink()
{
    terminate();
}

SIPVoIPLink* SIPVoIPLink::instance (const AccountID& id)
{

    if (!_instance) {
	_debug ("Create new SIPVoIPLink instance\n");
        _instance = new SIPVoIPLink (id);
    }

    return _instance;
}

void SIPVoIPLink::decrementClients (void)
{
    _clients--;

    if (_clients == 0) {
        terminate();
        SIPVoIPLink::_instance=NULL;
    }
}

bool SIPVoIPLink::init()
{
    if (initDone())
        return false;

	// TODO This port should be the one configured for the IP profile
	// and not the global one
    _regPort = Manager::instance().getLocalIp2IpPort();

    /* Instanciate the C++ thread */
    _evThread = new EventThread (this);

    /* Initialize the pjsip library */
    pjsip_init();

    initDone (true);

    return true;
}

void
SIPVoIPLink::terminate()
{
    _debug("SIPVoIPLink::terminate");



    if (_evThread) {
        _debug("SIPVoIPLink:: delete eventThread");
        delete _evThread;
        _evThread = NULL;
    }



    /* Clean shutdown of pjsip library */
    if (initDone()) {
      _debug("pjsip_shutdown\n");
        pjsip_shutdown();
    }

    initDone (false);
}

void
SIPVoIPLink::terminateSIPCall()
{
    ost::MutexLock m (_callMapMutex);
    CallMap::iterator iter = _callMap.begin();
    SIPCall *call;

    while (iter != _callMap.end()) {
        call = dynamic_cast<SIPCall*> (iter->second);

        if (call) {
            // terminate the sip call
            delete call;
            call = 0;
        }

        iter++;
    }

    _callMap.clear();
}

void
SIPVoIPLink::terminateOneCall (const CallID& id)
{

    SIPCall *call = getSIPCall (id);

    if (call) {
        // terminate the sip call
        delete call;
        call = 0;
    }
}

void get_remote_sdp_from_offer (pjsip_rx_data *rdata, pjmedia_sdp_session** r_sdp)
{

    pjmedia_sdp_session *sdp;
    pjsip_msg *msg;
    pjsip_msg_body *body;

    // Get the message
    msg = rdata->msg_info.msg;
    // Get the body message
    body = msg->body;

    // Parse the remote request to get the sdp session

    if (body) {
        pjmedia_sdp_parse (rdata->tp_info.pool, (char*) body->data, body->len, &sdp);
        *r_sdp = sdp;
    }

    else
        *r_sdp = NULL;
}


std::string SIPVoIPLink::get_useragent_name (void)
{
    std::ostringstream  useragent;
    useragent << PROGNAME << "/" << SFLPHONED_VERSION;
    return useragent.str();
}

void
SIPVoIPLink::getEvent()
{
    // We have to register the external thread so it could access the pjsip frameworks
    if (!pj_thread_is_registered())
        pj_thread_register (NULL, desc, &thread);

    // PJSIP polling
    pj_time_val timeout = {0, 10};

    pjsip_endpt_handle_events (_endpt, &timeout);

}

int SIPVoIPLink::sendRegister (AccountID id)
{
    int expire_value;

    pj_status_t status;
    pj_str_t useragent;
    pjsip_tx_data *tdata;
    pjsip_host_info destination;

    std::string tmp, hostname, username, password;
    SIPAccount *account = NULL;
    pjsip_regc *regc;
    pjsip_generic_string_hdr *h;
    pjsip_hdr hdr_list;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _debug ("In sendRegister: account is null");
        return false;
    }

    // Resolve hostname here and keep its
    // IP address for the whole time the
    // account is connected. This was a
    // workaround meant to help issue
    // #1852 that we hope should be fixed
    // soon.
    if (account->isResolveOnce()) {

        struct result result;
        destination.type = PJSIP_TRANSPORT_UNSPECIFIED;
        destination.flag = pjsip_transport_get_flag_from_type (PJSIP_TRANSPORT_UNSPECIFIED);
        destination.addr.host = pj_str (const_cast<char*> ( (account->getHostname()).c_str()));
        destination.addr.port = 0;

        result.status = 0x12345678;

        pjsip_endpt_resolve (_endpt, _pool, &destination, &result, &dns_cb);

        /* The following magic number and construct are inspired from dns_test.c
         * in test-pjsip directory.
         */

        while (result.status == 0x12345678) {
            pj_time_val timeout = { 1, 0 };
            pjsip_endpt_handle_events (_endpt, &timeout);
            _debug ("status : %d\n", result.status);
        }

        if (result.status != PJ_SUCCESS) {
            _debug ("Failed to resolve hostname only once."
                    " Default resolver will be used on"
                    " hostname for all requests.\n");
        } else {
            _debug ("%d servers where obtained from name resolution.\n", result.servers.count);
            char addr_buf[80];

            pj_sockaddr_print ( (pj_sockaddr_t*) &result.servers.entry[0].addr, addr_buf, sizeof (addr_buf), 3);
            account->setHostname (addr_buf);
        }
    }

    // Launch a new TLS listener/transport
    // if the user did choose it.
    if (account->isTlsEnabled()) {
        pj_status_t status;

        _debug ("    sendRegister: createTlsTransport\n");
        status = createTlsTransportRetryOnFailure (id);

        if (status != PJ_SUCCESS) {
            _debug ("Failed to initialize TLS transport for account %s\n", id.c_str());
        }
    }

    else {
        // Launch a new UDP listener/transport, using the published address
        if (account->isStunEnabled ()) {
            pj_status_t status;

            _debug ("    sendRegister: createAlternateUdpTransport\n");
            status = createAlternateUdpTransport (id);

            if (status != PJ_SUCCESS) {
                _debug ("Failed to initialize UDP transport with an extern published address for account %s\n", id.c_str());
            }
        } else {

            status = createUDPServer (id);

            if (status != PJ_SUCCESS) {
                _debug ("Use the local UDP transport");
                account->setAccountTransport (_localUDPTransport);
            }
        }
    }

    _mutexSIP.enterMutex();

    // Get the client registration information for this particular account
    regc = account->getRegistrationInfo();
    account->setRegister (true);

    // Set the expire value of the message from the config file
    istringstream stream (account->getRegistrationExpire());
    stream >> expire_value;

    if (!expire_value) {
        expire_value = PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;
    }

    // Update the state of the voip link
    account->setRegistrationState (Trying);

    // Create the registration according to the account ID
    status = pjsip_regc_create (_endpt, (void*) account, &regc_cb, &regc);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to create regc.\n");
        _mutexSIP.leaveMutex();
        return false;
    }

    // Creates URI
    std::string fromUri;

    std::string contactUri;

    std::string srvUri;

    std::string address;

    fromUri = account->getFromUri();

    srvUri = account->getServerUri();

    address = findLocalAddressFromUri (srvUri, account->getAccountTransport ());

    int port = findLocalPortFromUri (srvUri, account->getAccountTransport ());

    std::stringstream ss;

    std::string portStr;

    ss << port;

    ss >> portStr;

    contactUri = account->getContactHeader (address, portStr);

    _debug ("sendRegister: fromUri: %s serverUri: %s contactUri: %s\n",
            fromUri.c_str(),
            srvUri.c_str(),
            contactUri.c_str());

    pj_str_t pjFrom;

    pj_cstr (&pjFrom, fromUri.c_str());

    pj_str_t pjContact;

    pj_cstr (&pjContact, contactUri.c_str());

    pj_str_t pjSrv;

    pj_cstr (&pjSrv, srvUri.c_str());

    // Initializes registration
    status = pjsip_regc_init (regc, &pjSrv, &pjFrom, &pjFrom, 1, &pjContact, expire_value);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to initialize account %d in sendRegister\n", status);
        _mutexSIP.leaveMutex();
        return false;
    }

    pjsip_cred_info *cred = account->getCredInfo();

    int credential_count = account->getCredentialCount();
    _debug ("setting %d credentials in sendRegister\n", credential_count);
    pjsip_regc_set_credentials (regc, credential_count, cred);

    // Add User-Agent Header
    pj_list_init (&hdr_list);

    useragent = pj_str ( (char*) get_useragent_name ().c_str());

    h = pjsip_generic_string_hdr_create (_pool, &STR_USER_AGENT, &useragent);

    pj_list_push_back (&hdr_list, (pjsip_hdr*) h);

    pjsip_regc_add_headers (regc, &hdr_list);

    status = pjsip_regc_register (regc, PJ_TRUE, &tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to register regc.\n");
        _mutexSIP.leaveMutex();
        return false;
    }

    pjsip_tpselector *tp;

    init_transport_selector (account->getAccountTransport (), &tp);
    status = pjsip_regc_set_transport (regc, tp);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to set transport.\n");
        _mutexSIP.leaveMutex ();
        return false;
    }

    // Send registration request
    status = pjsip_regc_send (regc, tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to send regc request.\n");
        _mutexSIP.leaveMutex();
        return false;
    }

    _mutexSIP.leaveMutex();

    account->setRegistrationInfo (regc);
    _debug ("ok\n");
    return true;
}

int
SIPVoIPLink::sendUnregister (AccountID id)
{
    pj_status_t status = 0;
    pjsip_tx_data *tdata = NULL;
    SIPAccount *account;
    pjsip_regc *regc;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));
    regc = account->getRegistrationInfo();

    if (!account->isRegister()) {
        account->setRegistrationState (Unregistered);
        return true;
    }

    if (regc) {
        status = pjsip_regc_unregister (regc, &tdata);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to unregister regc.\n");
            return false;
        }

        status = pjsip_regc_send (regc, tdata);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to send regc request.\n");
            return false;
        }
    } else {
        _debug ("UserAgent: regc is null!\n");
        return false;
    }

    //account->setRegistrationInfo(regc);
    account->setRegister (false);

    return true;
}

Call*
SIPVoIPLink::newOutgoingCall (const CallID& id, const std::string& toUrl)
{
    SIPAccount * account = NULL;
    pj_status_t status;
    std::string localAddr;

    SIPCall* call = new SIPCall (id, Call::Outgoing, _pool);

    if (call) {
        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (Manager::instance().getAccountFromCall (id)));

        if (account == NULL) {
            _debug ("Error retrieving the account to the make the call with\n");
            call->setConnectionState (Call::Disconnected);
            call->setState (Call::Error);
            delete call;
            call=0;
            return call;
        }

        std::string toUri = account->getToUri (toUrl);

        call->setPeerNumber (toUri);

		// TODO May use the published address as well
        localAddr = account->getLocalAddress ();
        setCallAudioLocal (call, localAddr);

        try {
            _debug ("Creating new rtp session in newOutgoingCall\n");
            call->getAudioRtp()->initAudioRtpSession (call);
        } catch (...) {
            _debug ("Failed to create rtp thread from newOutGoingCall\n");
        }

        call->initRecFileName();

        _debug ("Try to make a call to: %s with call ID: %s\n", toUrl.data(), id.data());
        // Building the local SDP offer
        // localAddr = getLocalAddressAssociatedToAccount (account->getAccountID());
        call->getLocalSDP()->set_ip_address (localAddr);
        status = call->getLocalSDP()->create_initial_offer();

        if (status != PJ_SUCCESS) {
            delete call;
            call=0;
            return call;
        }

        if (SIPOutgoingInvite (call)) {
            call->setConnectionState (Call::Progressing);
            call->setState (Call::Active);
            addCall (call);
        } else {
            delete call;
            call = 0;
        }
    }

    return call;
}

bool
SIPVoIPLink::answer (const CallID& id)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_tx_data *tdata;
    Sdp *local_sdp;
    pjsip_inv_session *inv_session;

    _debug ("SIPVoIPLink::answer: start answering \n");

    call = getSIPCall (id);

    if (call==0) {
        _debug ("! SIP Failure: SIPCall doesn't exists\n");
        return false;
    }

    local_sdp = call->getLocalSDP();

    try {
        call->getAudioRtp()->initAudioRtpSession (call);
    } catch (...) {
        _debug ("Failed to create rtp thread from answer\n");
    }

    inv_session = call->getInvSession();

    status = local_sdp->start_negociation ();

    if (status == PJ_SUCCESS) {

        _debug ("SIPVoIPLink::answer:UserAgent: Negociation success! : call %s \n", call->getCallId().c_str());
        // Create and send a 200(OK) response
        status = pjsip_inv_answer (inv_session, PJSIP_SC_OK, NULL, NULL, &tdata);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);
        status = pjsip_inv_send_msg (inv_session, tdata);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

        call->setConnectionState (Call::Connected);
        call->setState (Call::Active);

        return true;
    } else {
        // Create and send a 488/Not acceptable here
        // because the SDP negociation failed
        status = pjsip_inv_answer (inv_session, PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, NULL,
                                   &tdata);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);
        status = pjsip_inv_send_msg (inv_session, tdata);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

        // Terminate the call
        _debug ("SIPVoIPLink::answer: fail terminate call %s \n",call->getCallId().c_str());

        if (call->getAudioRtp())
            call->getAudioRtp()->stop ();

        terminateOneCall (call->getCallId());

        removeCall (call->getCallId());

        return false;
    }
}

bool
SIPVoIPLink::hangup (const CallID& id)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    SIPCall* call;

    call = getSIPCall (id);

    if (call==0) {
        _debug ("! SIP Error: Call doesn't exist\n");
        return false;
    }

    // User hangup current call. Notify peer
    status = pjsip_inv_end_session (call->getInvSession(), 404, NULL, &tdata);

    if (status != PJ_SUCCESS)
        return false;


    if (tdata == NULL)
        return true;

    // _debug("Some tdata info: %",);

    status = pjsip_inv_send_msg (call->getInvSession(), tdata);

    if (status != PJ_SUCCESS)
        return false;

    call->getInvSession()->mod_data[getModId() ] = NULL;

    // Release RTP thread
    if (Manager::instance().isCurrentCall (id)) {
        _debug ("* SIP Info: Stopping AudioRTP for hangup\n");
        call->getAudioRtp()->stop();
    }

    terminateOneCall (id);

    removeCall (id);

    return true;
}

bool
SIPVoIPLink::peerHungup (const CallID& id)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    SIPCall* call;

    call = getSIPCall (id);

    if (call==0) {
        _debug ("! SIP Error: Call doesn't exist\n");
        return false;
    }

    // User hangup current call. Notify peer
    status = pjsip_inv_end_session (call->getInvSession(), 404, NULL, &tdata);

    if (status != PJ_SUCCESS)
        return false;

    if (tdata == NULL)
        return true;

    status = pjsip_inv_send_msg (call->getInvSession(), tdata);

    if (status != PJ_SUCCESS)
        return false;

    call->getInvSession()->mod_data[getModId() ] = NULL;

    // Release RTP thread
    if (Manager::instance().isCurrentCall (id)) {
        _debug ("* SIP Info: Stopping AudioRTP for hangup\n");
        call->getAudioRtp()->stop();
    }

    terminateOneCall (id);

    removeCall (id);

    return true;
}

bool
SIPVoIPLink::cancel (const CallID& id)
{
    SIPCall* call = getSIPCall (id);

    if (call==0) {
        _debug ("! SIP Error: Call doesn't exist\n");
        return false;
    }

    _debug ("- SIP Action: Cancel call %s [cid: %3d]\n", id.data(), call->getCid());

    terminateOneCall (id);
    removeCall (id);

    return true;
}

bool
SIPVoIPLink::onhold (const CallID& id)
{

    pj_status_t status;
    SIPCall* call;

    call = getSIPCall (id);

    if (call==0) {
        _debug ("! SIP Error: call doesn't exist\n");
        return false;
    }


    // Stop sound
    call->setAudioStart (false);

    call->setState (Call::Hold);

    _debug ("* SIP Info: Stopping AudioRTP for onhold action\n");

    call->getAudioRtp()->stop();

    /* Create re-INVITE with new offer */
    status = inv_session_reinvite (call, "sendonly");

    if (status != PJ_SUCCESS)
        return false;

    return true;
}

int SIPVoIPLink::inv_session_reinvite (SIPCall *call, std::string direction)
{

    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_session *local_sdp;
    pjmedia_sdp_attr *attr;

    local_sdp = call->getLocalSDP()->get_local_sdp_session();

    if (local_sdp == NULL) {
        _debug ("! SIP Failure: unable to find local_sdp\n");
        return !PJ_SUCCESS;
    }

    // reinvite only if connected
    // Build the local SDP offer
    status = call->getLocalSDP()->create_initial_offer();

    if (status != PJ_SUCCESS)
        return 1;   // !PJ_SUCCESS

    pjmedia_sdp_media_remove_all_attr (local_sdp->media[0], "sendrecv");

    attr = pjmedia_sdp_attr_create (_pool, direction.c_str(), NULL);

    pjmedia_sdp_media_add_attr (local_sdp->media[0], attr);

    // Build the reinvite request

    status = pjsip_inv_reinvite (call->getInvSession(), NULL,
                                 local_sdp, &tdata);

    if (status != PJ_SUCCESS)
        return 1;   // !PJ_SUCCESS

    // Send it
    status = pjsip_inv_send_msg (call->getInvSession(), tdata);

    if (status != PJ_SUCCESS)
        return 1;   // !PJ_SUCCESS

    return PJ_SUCCESS;
}


bool
SIPVoIPLink::offhold (const CallID& id)
{
    SIPCall *call;
    pj_status_t status;

    call = getSIPCall (id);

    if (call==0) {
        _debug ("! SIP Error: Call doesn't exist\n");
        return false;
    }

    try {
        call->getAudioRtp()->initAudioRtpSession (call);
    } catch (...) {
        _debug ("! SIP Failure: Unable to create RTP Session (%s:%d)\n", __FILE__, __LINE__);
    }

    /* Create re-INVITE with new offer */
    status = inv_session_reinvite (call, "sendrecv");

    if (status != PJ_SUCCESS)
        return false;

    call->setState (Call::Active);

    return true;
}

bool
SIPVoIPLink::transfer (const CallID& id, const std::string& to)
{

    SIPCall *call;
    std::string tmp_to;
    pjsip_evsub *sub;
    pjsip_tx_data *tdata;

    struct pjsip_evsub_user xfer_cb;
    pj_status_t status;
    AccountID account_id;
    SIPAccount * account = NULL;

    call = getSIPCall (id);
    call->stopRecording();
    account_id = Manager::instance().getAccountFromCall (id);
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));

    if (account == NULL) {
        _debug ("SIPVoIPLink::transfer account is null. Returning.\n");
        return false;
    }

    if (call==0) {
        _debug ("! SIP Failure: Call doesn't exist\n");
        return false;
    }

    std::string dest;

    pj_str_t pjDest;

    if (to.find ("@") == std::string::npos) {
        dest = account->getToUri (to);
        pj_cstr (&pjDest, dest.c_str());
    }

    _debug ("Transfering to %s\n", dest.c_str());

    /* Create xfer client subscription. */
    pj_bzero (&xfer_cb, sizeof (xfer_cb));
    xfer_cb.on_evsub_state = &xfer_func_cb;

    status = pjsip_xfer_create_uac (call->getInvSession()->dlg, &xfer_cb, &sub);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to create xfer -- %d\n", status);
        return false;
    }

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can not find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data (sub, getModId(), this);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate (sub, &pjDest, &tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to create REFER request -- %d\n", status);
        return false;
    }

    /* Send. */
    status = pjsip_xfer_send_request (sub, tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to send REFER request -- %d\n", status);
        return false;
    }

    return true;
}

bool SIPVoIPLink::transferStep2 (SIPCall* call)
{
    call->getAudioRtp()->stop();
    return true;
}

bool
SIPVoIPLink::refuse (const CallID& id)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_tx_data *tdata;


    call = getSIPCall (id);

    if (call==0) {
        _debug ("Call doesn't exist\n");
        return false;
    }

    // can't refuse outgoing call or connected
    if (!call->isIncoming() || call->getConnectionState() == Call::Connected) {
        _debug ("It's not an incoming call, or it's already answered\n");
        return false;
    }

    // User refuse current call. Notify peer
    status = pjsip_inv_end_session (call->getInvSession(), PJSIP_SC_DECLINE, NULL, &tdata);   //603

    if (status != PJ_SUCCESS)
        return false;

    status = pjsip_inv_send_msg (call->getInvSession(), tdata);

    if (status != PJ_SUCCESS)
        return false;

    call->getInvSession()->mod_data[getModId() ] = NULL;

    terminateOneCall (id);

    return true;
}


std::string
SIPVoIPLink::getCurrentCodecName()
{

    SIPCall *call;
    AudioCodec *ac = NULL;
    std::string name = "";

    call = getSIPCall (Manager::instance().getCurrentCallId());

    if (call)
        ac = call->getLocalSDP()->get_session_media();

    if (ac)
        name = ac->getCodecName();

    return name;
}

bool
SIPVoIPLink::carryingDTMFdigits (const CallID& id, char code)
{

    SIPCall *call;
    int duration;
    const int body_len = 1000;
    char *dtmf_body;
    pj_status_t status;
    pjsip_tx_data *tdata;
    pj_str_t methodName, content;
    pjsip_method method;
    pjsip_media_type ctype;

    call = getSIPCall (id);

    if (call==0) {
        _debug ("Call doesn't exist\n");
        return false;
    }

    duration = Manager::instance().getConfigInt (SIGNALISATION, PULSE_LENGTH);

    dtmf_body = new char[body_len];

    snprintf (dtmf_body, body_len - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);

    pj_strdup2 (_pool, &methodName, "INFO");
    pjsip_method_init_np (&method, &methodName);

    /* Create request message. */
    status = pjsip_dlg_create_request (call->getInvSession()->dlg, &method, -1, &tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to create INFO request -- %d\n", status);
        return false;
    }

    /* Get MIME type */
    pj_strdup2 (_pool, &ctype.type, "application");

    pj_strdup2 (_pool, &ctype.subtype, "dtmf-relay");

    /* Create "application/dtmf-relay" message body. */
    pj_strdup2 (_pool, &content, dtmf_body);

    tdata->msg->body = pjsip_msg_body_create (tdata->pool, &ctype.type, &ctype.subtype, &content);

    if (tdata->msg->body == NULL) {
        _debug ("UserAgent: Unable to create msg body!\n");
        pjsip_tx_data_dec_ref (tdata);
        return false;
    }

    /* Send the request. */
    status = pjsip_dlg_send_request (call->getInvSession()->dlg, tdata, getModId(), NULL);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to send MESSAGE request -- %d\n", status);
        return false;
    }

    return true;
}

bool
SIPVoIPLink::SIPOutgoingInvite (SIPCall* call)
{
    // If no SIP proxy setting for direct call with only IP address
    if (!SIPStartCall (call, "")) {
        _debug ("! SIP Failure: call not started\n");
        return false;
    }

    return true;
}

bool
SIPVoIPLink::SIPStartCall (SIPCall* call, const std::string& subject UNUSED)
{
    pj_status_t status;
    pjsip_inv_session *inv;
    pjsip_dialog *dialog;
    pjsip_tx_data *tdata;

    AccountID id;

    if (call == NULL)
        return false;

    id = Manager::instance().getAccountFromCall (call->getCallId());

    // Get the basic information about the callee account
    SIPAccount * account = NULL;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _debug ("Account is null in SIPStartCall\n");
        return false;
    }

    // Creates URI
    std::string fromUri;

    std::string toUri;

    std::string contactUri;

    fromUri = account->getFromUri();

    toUri = call->getPeerNumber(); // expecting a fully well formed sip uri

    std::string address = findLocalAddressFromUri (toUri, account->getAccountTransport ());

    int port = findLocalPortFromUri (toUri, account->getAccountTransport ());

    std::stringstream ss;

    std::string portStr;

    ss << port;

    ss >> portStr;

    contactUri = account->getContactHeader (address, portStr);

    _debug ("SIPStartCall: fromUri: %s toUri: %s contactUri: %s\n",
            fromUri.c_str(),
            toUri.c_str(),
            contactUri.c_str());

    pj_str_t pjFrom;

    pj_cstr (&pjFrom, fromUri.c_str());

    pj_str_t pjContact;

    pj_cstr (&pjContact, contactUri.c_str());

    pj_str_t pjTo;

    pj_cstr (&pjTo, toUri.c_str());

    // Create the dialog (UAC)
    status = pjsip_dlg_create_uac (pjsip_ua_instance(), &pjFrom,
                                   &pjContact,
                                   &pjTo,
                                   NULL,
                                   &dialog);

    if (status != PJ_SUCCESS) {
        _debug ("UAC creation failed\n");
        return false;
    }

    // Create the invite session for this call
    status = pjsip_inv_create_uac (dialog, call->getLocalSDP()->get_local_sdp_session(), 0, &inv);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

    // Set auth information
    pjsip_auth_clt_set_credentials (&dialog->auth_sess, 1, account->getCredInfo());

    // Associate current call in the invite session
    inv->mod_data[getModId() ] = call;

    status = pjsip_inv_invite (inv, &tdata);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

    // Associate current invite session in the call
    call->setInvSession (inv);

    // Set the appropriate transport
    pjsip_tpselector *tp;
    init_transport_selector (account->getAccountTransport (), &tp);
    status = pjsip_dlg_set_transport (dialog, tp);

    status = pjsip_inv_send_msg (inv, tdata);

    if (status != PJ_SUCCESS) {
        _debug ("    SIPStartCall: failed to send invite\n");
        return false;
    }

    return true;
}

void
SIPVoIPLink::SIPCallServerFailure (SIPCall *call)
{
    if (call != 0) {
        _debug ("Server error!\n");
        CallID id = call->getCallId();
        Manager::instance().callFailure (id);
        terminateOneCall (id);
        removeCall (id);

        if (call->getAudioRtp ())
            call->getAudioRtp()->stop();
    }
}

void
SIPVoIPLink::SIPCallClosed (SIPCall *call)
{
    if (!call) {
        return;
    }

    CallID id = call->getCallId();

    if (Manager::instance().isCurrentCall (id)) {
        call->setAudioStart (false);
        _debug ("* SIP Info: Stopping AudioRTP when closing\n");
        call->getAudioRtp()->stop();
    }

    _debug ("After close RTP\n");

    Manager::instance().peerHungupCall (id);
    terminateOneCall (id);
    removeCall (id);
    _debug ("After remove call ID\n");
}

void
SIPVoIPLink::SIPCallReleased (SIPCall *call)
{
    if (!call) {
        return;
    }

    // if we are here.. something when wrong before...
    _debug ("SIP call release\n");

    CallID id = call->getCallId();

    Manager::instance().callFailure (id);

    terminateOneCall (id);

    removeCall (id);
}


void
SIPVoIPLink::SIPCallAnswered (SIPCall *call, pjsip_rx_data *rdata)
{

    _debug ("SIPCallAnswered\n");

    if (!call) {
        _debug ("! SIP Failure: unknown call\n");
        return;
    }

    if (call->getConnectionState() != Call::Connected) {
        _debug ("Update call state , id = %s\n", call->getCallId().c_str());
        call->setConnectionState (Call::Connected);
        call->setState (Call::Active);
        Manager::instance().peerAnsweredCall (call->getCallId());
    } else {
        _debug ("* SIP Info: Answering call (on/off hold to send ACK)\n");
    }
}


SIPCall*
SIPVoIPLink::getSIPCall (const CallID& id)
{
    Call* call = getCall (id);

    if (call) {
        return dynamic_cast<SIPCall*> (call);
    }

    return NULL;
}

bool SIPVoIPLink::new_ip_to_ip_call (const CallID& id, const std::string& to)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;
	std::string localAddress;

    /* Create the call */
    call = new SIPCall (id, Call::Outgoing, _pool);

    if (call) {
        call->setCallConfiguration (Call::IPtoIP);
        call->initRecFileName();

        AccountID accountId = Manager::instance().getAccountFromCall (id);
        SIPAccount * account = NULL;

        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));

        if (account == NULL) {
            _debug ("Account is null. Returning\n");
            return !PJ_SUCCESS;
        }
        
		// Set SDP parameters
		localAddress = account->getLocalAddress ();
		_debug("new_ip_to_ip_call localAddress: %s\n", localAddress.c_str());
		if (localAddress == "0.0.0.0"){
			_debug ("Here is the local address: %s\n", localAddress.c_str ());
			loadSIPLocalIP (&localAddress);	
		}
		setCallAudioLocal (call, localAddress);

        _debug ("toUri received in new_ip_to_ip call %s\n", to.c_str());

        std::string toUri = account->getToUri (to);
        call->setPeerNumber (toUri);
        _debug ("toUri in new_ip_to_ip call %s\n", toUri.c_str());
        // Building the local SDP offer
        call->getLocalSDP()->set_ip_address (localAddress);
        call->getLocalSDP()->create_initial_offer();

        try {
            call->getAudioRtp()->initAudioRtpSession (call);
        } catch (...) {
            _debug ("! SIP Failure: Unable to create RTP Session  in SIPVoIPLink::new_ip_to_ip_call (%s:%d)\n", __FILE__, __LINE__);
        }

	// If no account already set, use the default one created at pjsip initialization
	if(account->getAccountTransport() == NULL) {
	    _debug("No transport for this account, using the default one\n");
	    account->setAccountTransport(_localUDPTransport);
	}

	_debug("IptoIP local port %i\n", account->getLocalPort());
	_debug("IptoIP local address %s\n", account->getLocalAddress().c_str());

        // Create URI
        std::string fromUri;

        std::string contactUri;

        fromUri = account->getFromUri();

        std::string address = findLocalAddressFromUri (toUri, account->getAccountTransport());

        int port = findLocalPortFromUri (toUri, account->getAccountTransport());

        std::stringstream ss;

        std::string portStr;

        ss << port;

        ss >> portStr;

        contactUri = account->getContactHeader (address, portStr);

        _debug ("new_ip_to_ip_call: fromUri: %s toUri: %s contactUri: %s\n",
                fromUri.c_str(),
                toUri.c_str(),
                contactUri.c_str());

        pj_str_t pjFrom;

        pj_cstr (&pjFrom, fromUri.c_str());

        pj_str_t pjTo;

        pj_cstr (&pjTo, toUri.c_str());

        pj_str_t pjContact;

        pj_cstr (&pjContact, contactUri.c_str());

        // Create the dialog (UAC)
        // (Parameters are "strduped" inside this function)
        status = pjsip_dlg_create_uac (pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog);

        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

        // Create the invite session for this call
        status = pjsip_inv_create_uac (dialog, call->getLocalSDP()->get_local_sdp_session(), 0, &inv);

        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

        // Set the appropriate transport
        pjsip_tpselector *tp;

        init_transport_selector (account->getAccountTransport(), &tp);

        status = pjsip_dlg_set_transport (dialog, tp);

        if (status != PJ_SUCCESS) {
            _debug ("Failed to set the transport for an IP call\n");
            return status;
        }

        // Associate current call in the invite session
        inv->mod_data[getModId() ] = call;

        status = pjsip_inv_invite (inv, &tdata);

        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

        // Associate current invite session in the call
        call->setInvSession (inv);

        status = pjsip_inv_send_msg (inv, tdata);

        if (status != PJ_SUCCESS) {
            delete call;
            call = 0;
            return false;
        }

        call->setConnectionState (Call::Progressing);

        call->setState (Call::Active);
        addCall (call);

        return true;
    } else
        return false;
}


///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////


bool get_dns_server_addresses (std::vector<std::string> *servers)
{

    int server_count, i;
    std::vector<std::string> nameservers;

    struct  sockaddr_in current_server;
    in_addr address;

    // Read configuration files

    if (res_init () != 0) {
        _debug ("Resolver initialization failed\n");
        return false;
    }

    server_count = _res.nscount;

    for (i=0; i<server_count; i++) {
        current_server = (struct  sockaddr_in) _res.nsaddr_list[i];
        address = current_server.sin_addr;
        nameservers.push_back (inet_ntoa (address));
    }

    *servers = nameservers;

    return true;
}

pj_status_t SIPVoIPLink::enable_dns_srv_resolver (pjsip_endpoint *endpt, pj_dns_resolver **p_resv)
{

    pj_status_t status;
    pj_dns_resolver *resv;
    std::vector <std::string> dns_servers;
    int scount, i;

    // Create the DNS resolver instance
    status = pjsip_endpt_create_resolver (endpt, &resv);

    if (status != PJ_SUCCESS) {
        _debug ("Error creating the DNS resolver instance\n");
        return status;
    }

    if (!get_dns_server_addresses (&dns_servers)) {
        _debug ("Error  while fetching DNS information\n");
        return -1;
    }

    // Build the nameservers list needed by pjsip
    scount = dns_servers.size ();

    pj_str_t nameservers[scount];

    for (i = 0; i<scount; i++) {
        nameservers[i] = pj_str ( (char*) dns_servers[i].c_str());
    }

    // Update the name servers for the DNS resolver
    status = pj_dns_resolver_set_ns (resv, scount, nameservers, NULL);

    if (status != PJ_SUCCESS) {
        _debug ("Error updating the name servers for the DNS resolver\n");
        return status;
    }

    // Set the DNS resolver instance of the SIP resolver engine
    status = pjsip_endpt_set_resolver (endpt, resv);

    if (status != PJ_SUCCESS) {
        _debug ("Error setting the DNS resolver instance of the SIP resolver engine\n");
        return status;
    }

    *p_resv = resv;

    return PJ_SUCCESS;

}

bool SIPVoIPLink::pjsip_init()
{
    pj_status_t status;
    int errPjsip = 0;
    int port;
    pjsip_inv_callback inv_cb;
    pj_str_t accepted;
    std::string name_mod;
    pj_dns_resolver *p_resv;
	std::string addr;

    name_mod = "sflphone";

    _debug("pjsip_init\n");

    // Init PJLIB: must be called before any call to the pjsip library
    status = pj_init();
    // Use pjsip macros for sanity check
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init PJLIB-UTIL library
    status = pjlib_util_init();
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Set the pjsip log level
    pj_log_set_level (PJ_LOG_LEVEL);

    // Init PJNATH
    status = pjnath_init();
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Create a pool factory to allocate memory
    pj_caching_pool_init (&_cp, &pj_pool_factory_default_policy, 0);

    // Create memory pool for application.
    _pool = pj_pool_create (&_cp.factory, "sflphone", 4000, 4000, NULL);

    if (!_pool) {
        _debug ("UserAgent: Could not initialize memory pool\n");
        return PJ_ENOMEM;
    }

    // Create the SIP endpoint
    status = pjsip_endpt_create (&_cp.factory, pj_gethostname()->ptr, &_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    if (!loadSIPLocalIP (&addr)) {
        _debug ("UserAgent: Unable to determine network capabilities\n");
        return false;
    }

    // Retrieve Direct IP Calls settings.
    // This corresponds to the accountID set to
    // AccountNULL
    SIPAccount * account = NULL;
    bool directIpCallsTlsEnabled = false;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (AccountNULL));

    if (account == NULL) {
        _debug ("Account is null in pjsip init\n");
	port = _regPort;
    } else {
        directIpCallsTlsEnabled = account->isTlsEnabled();
	port = account->getLocalPort ();
    }

    // Create a UDP listener meant for all accounts
    // for which TLS was not enabled
    errPjsip = createUDPServer();

    // If the above UDP server
    // could not be created, then give it another try
    // on a random sip port
    if (errPjsip != PJ_SUCCESS) {
        _debug ("UserAgent: Could not initialize SIP listener on port %d\n", port);
        port = RANDOM_SIP_PORT;

        _debug ("UserAgent: Trying to initialize SIP listener on port %d\n", port);
        errPjsip = createUDPServer();

        if (errPjsip != PJ_SUCCESS) {
            _debug ("UserAgent: Fail to initialize SIP listener on port %d\n", port);
            return errPjsip;
        }
    }

    // Bind the newly created transport to the ip to ip account
    // setAccountTransport

    _debug ("pjsip_init -- listening on port %d\n", port);

    // Create a TLS listener meant for Direct IP calls
    // if the user did enabled it.

    if (directIpCallsTlsEnabled) {
        errPjsip = createTlsTransportRetryOnFailure (AccountNULL);
    }

    if (errPjsip != PJ_SUCCESS) {
        _debug ("pj_init(): could not start TLS transport for Direct Calls");
    }

    // TODO: For TLS, retry on random port, just we already do above
    // for UDP transport.

    // Initialize transaction layer
    status = pjsip_tsx_layer_init_module (_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Initialize UA layer module
    status = pjsip_ua_init_module (_endpt, NULL);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Initialize Replaces support. See the Replaces specification in RFC 3891
    status = pjsip_replaces_init_module (_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Initialize 100rel support
    status = pjsip_100rel_init_module (_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Initialize and register sflphone module
    _mod_ua.name = pj_str ( (char*) name_mod.c_str());

    _mod_ua.id = -1;

    _mod_ua.priority = PJSIP_MOD_PRIORITY_APPLICATION;

    _mod_ua.on_rx_request = &mod_on_rx_request;

    _mod_ua.on_rx_response = &mod_on_rx_response;

    status = pjsip_endpt_register_module (_endpt, &_mod_ua);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init the event subscription module.
    // It extends PJSIP by supporting SUBSCRIBE and NOTIFY methods
    status = pjsip_evsub_init_module (_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init xfer/REFER module
    status = pjsip_xfer_init_module (_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    status = enable_dns_srv_resolver (_endpt, &p_resv);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init the callback for INVITE session:
    pj_bzero (&inv_cb, sizeof (inv_cb));

    inv_cb.on_state_changed = &call_on_state_changed;

    inv_cb.on_new_session = &call_on_forked;

    inv_cb.on_media_update = &call_on_media_update;

    inv_cb.on_tsx_state_changed = &call_on_tsx_changed;

    inv_cb.on_rx_offer = &on_rx_offer;

    // Initialize session invite module
    status = pjsip_inv_usage_init (_endpt, &inv_cb);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    _debug ("UserAgent: VOIP callbacks initialized\n");

    // Add endpoint capabilities (INFO, OPTIONS, etc) for this UA
    pj_str_t allowed[] = { { (char*) "INFO", 4}, { (char*) "REGISTER", 8}, { (char*) "OPTIONS", 7} };       //  //{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6}

    accepted = pj_str ( (char*) "application/sdp");

    // Register supported methods
    pjsip_endpt_add_capability (_endpt, &_mod_ua, PJSIP_H_ALLOW, NULL, PJ_ARRAY_SIZE (allowed), allowed);

    // Register "application/sdp" in ACCEPT header
    pjsip_endpt_add_capability (_endpt, &_mod_ua, PJSIP_H_ACCEPT, NULL, 1, &accepted);

    _debug ("UserAgent: pjsip version %s for %s initialized\n", pj_get_version(), PJ_OS_NAME);

    // Create the secondary thread to poll sip events
    _evThread->start();

    /* Done! */
    return PJ_SUCCESS;
}

pj_status_t SIPVoIPLink::stunServerResolve (AccountID id)
{
    pj_str_t stunServer;
    pj_uint16_t stunPort;
    pj_stun_sock_cb stun_sock_cb;
    pj_stun_sock *stun_sock;
    pj_stun_config stunCfg;
    pj_status_t status;

    // Fetch the account information from the config file
    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _debug ("stunServerResolve: Account is null. Returning\n");
        return !PJ_SUCCESS;
    }

    // Get the STUN server name and port
    stunServer = account->getStunServerName ();

    stunPort = account->getStunPort ();

    // Initialize STUN configuration
    pj_stun_config_init (&stunCfg, &_cp.factory, 0, pjsip_endpt_get_ioqueue (_endpt), pjsip_endpt_get_timer_heap (_endpt));

    status = PJ_EPENDING;

    pj_bzero (&stun_sock_cb, sizeof (stun_sock_cb));

    stun_sock_cb.on_rx_data = &stun_sock_on_rx_data;

    stun_sock_cb.on_status = &stun_sock_on_status;

    status = pj_stun_sock_create (&stunCfg, "stunresolve", pj_AF_INET(), &stun_sock_cb, NULL, NULL, &stun_sock);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror (status, errmsg, sizeof (errmsg));
        _debug ("Error creating STUN socket for %.*s: %s", (int) stunServer.slen, stunServer.ptr, errmsg);
        return status;
    }

    status = pj_stun_sock_start (stun_sock, &stunServer, stunPort, NULL);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror (status, errmsg, sizeof (errmsg));
        _debug ("Error starting STUN socket for %.*s: %s", (int) stunServer.slen, stunServer.ptr, errmsg);
        pj_stun_sock_destroy (stun_sock);
        stun_sock = NULL;
        return status;
    }

    return status;
}

int SIPVoIPLink::createUDPServer (AccountID id)
{

    pj_status_t status;
    pj_sockaddr_in bound_addr;
    pjsip_host_port a_name;
    char tmpIP[32];
    pjsip_transport *transport;
    std::string listeningAddress = "127.0.0.1";
    int listeningPort = _regPort;

    /* Use my local address as default value */
    if (!loadSIPLocalIP (&listeningAddress))
        return !PJ_SUCCESS;

    _debug("SIPVoIPLink::createUDPServer\n");
    /*
     * Retrieve the account information
     */
    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    // Set information to the local address and port

    if (account == NULL) {
        _debug ("Account with id \"%s\" is null in createUDPServer.\n", id.c_str());
	// account = Manager::instance()->getAccount(IP2IP_PROFILE);
    } else {
        // We are trying to initialize a UDP transport available for all local accounts and direct IP calls
		if (account->getLocalAddress () != "0.0.0.0"){
			listeningAddress = account->getLocalAddress ();
		}
		listeningPort = account->getLocalPort ();
    }

    // Init bound address to ANY
    pj_memset (&bound_addr, 0, sizeof (bound_addr));

    bound_addr.sin_addr.s_addr = pj_htonl (PJ_INADDR_ANY);

    bound_addr.sin_port = pj_htons ( (pj_uint16_t) listeningPort);

    bound_addr.sin_family = PJ_AF_INET;

    pj_bzero (bound_addr.sin_zero, sizeof (bound_addr.sin_zero));

    // Create UDP-Server (default port: 5060)
    strcpy (tmpIP, listeningAddress.data());

    pj_strdup2 (_pool, &a_name.host, tmpIP);

    a_name.port = (pj_uint16_t) listeningPort;

    status = pjsip_udp_transport_start (_endpt, &bound_addr, &a_name, 1, &transport);

    // Get the transport manager associated with
    // this endpoint
    pjsip_tpmgr * tpmgr = NULL;

    tpmgr = pjsip_endpt_get_tpmgr (_endpt);

    _debug ("number of transport: %i\n", pjsip_tpmgr_get_transport_count (tpmgr));

    pjsip_tpmgr_dump_transports (tpmgr);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: (%d) Unable to start UDP transport on %s:%d\n", status, listeningAddress.data(), listeningPort);
        // Try to acquire an existing one
        // pjsip_tpmgr_acquire_transport ()
        return status;
    } else {
        _debug ("UserAgent: UDP server listening on port %d\n", listeningPort);

        if (account == NULL)
            _localUDPTransport = transport;
        else
			account->setAccountTransport (transport);
    }

    _debug ("Transport initialized successfully on %s:%i\n", listeningAddress.c_str (), listeningPort);

    return PJ_SUCCESS;
}

std::string SIPVoIPLink::findLocalAddressFromUri (const std::string& uri, pjsip_transport *transport)
{
    pj_str_t localAddress;
    pjsip_transport_type_e transportType;
    pjsip_tpselector *tp_sel;

    _debug ("SIPVoIPLink::findLocalAddressFromUri\n");

    // Find the transport that must be used with the given uri
    pj_str_t tmp;
    pj_strdup2_with_null (_pool, &tmp, uri.c_str());
    pjsip_uri * genericUri = NULL;
    genericUri = pjsip_parse_uri (_pool, tmp.ptr, tmp.slen, 0);

    pj_str_t pjMachineName;
    pj_strdup (_pool, &pjMachineName, pj_gethostname());
    std::string machineName (pjMachineName.ptr, pjMachineName.slen);

    if (genericUri == NULL) {
        _debug ("genericUri is NULL in findLocalAddressFromUri\n");
        return machineName;
    }

    pjsip_sip_uri * sip_uri = NULL;

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri (genericUri);

    if (sip_uri == NULL) {
        _debug ("Invalid uri in findLocalAddressFromTransport\n");
        return machineName;
    }

    if (PJSIP_URI_SCHEME_IS_SIPS (sip_uri)) {
        transportType = PJSIP_TRANSPORT_TLS;
    } else {
        if (transport == NULL) {
            _debug ("transport is NULL in findLocalAddressFromUri\n. Try the local UDP transport");
            transport = _localUDPTransport;
        }

        transportType = PJSIP_TRANSPORT_UDP;
    }

    // Get the transport manager associated with
    // this endpoint
    pjsip_tpmgr * tpmgr = NULL;

    tpmgr = pjsip_endpt_get_tpmgr (_endpt);

    if (tpmgr == NULL) {
        _debug ("Unexpected: Cannot get tpmgr from endpoint.\n");
        return machineName;
    }

    // Find the local address (and port) based on the registered
    // transports and the transport type
    int port;

    pj_status_t status;

    /* Init the transport selector */
    //_debug ("Transport ID: %s\n", transport->obj_name);
    if (transportType == PJSIP_TRANSPORT_UDP) {
        status = init_transport_selector (transport, &tp_sel);

        if (status == PJ_SUCCESS)
            status = pjsip_tpmgr_find_local_addr (tpmgr, _pool, transportType, tp_sel, &localAddress, &port);
        else
            status = pjsip_tpmgr_find_local_addr (tpmgr, _pool, transportType, NULL, &localAddress, &port);
    } else
        status = pjsip_tpmgr_find_local_addr (tpmgr, _pool, transportType, NULL, &localAddress, &port);

    if (status != PJ_SUCCESS) {
        _debug ("Failed to find local address from transport\n");
        return machineName;
    }

	_debug ("Local address discovered from attached transport: %s\n", localAddress.ptr);
    return std::string (localAddress.ptr, localAddress.slen);
}

pj_status_t SIPVoIPLink::init_transport_selector (pjsip_transport *transport, pjsip_tpselector **tp_sel)
{
    pjsip_tpselector *tp;

    if (transport != NULL) {
        tp = (pjsip_tpselector *) pj_pool_zalloc (_pool, sizeof (pjsip_tpselector));
        tp->type = PJSIP_TPSELECTOR_TRANSPORT;
        tp->u.transport = transport;

        *tp_sel = tp;

        return PJ_SUCCESS;
    }

    return !PJ_SUCCESS;
}

int SIPVoIPLink::findLocalPortFromUri (const std::string& uri, pjsip_transport *transport)
{
    pj_str_t localAddress;
    pjsip_transport_type_e transportType;
    int port;
    pjsip_tpselector *tp_sel;

    // Find the transport that must be used with the given uri
    pj_str_t tmp;
    pj_strdup2_with_null (_pool, &tmp, uri.c_str());
    pjsip_uri * genericUri = NULL;
    genericUri = pjsip_parse_uri (_pool, tmp.ptr, tmp.slen, 0);

    if (genericUri == NULL) {
        _debug ("genericUri is NULL in findLocalPortFromUri\n");
        return atoi (DEFAULT_SIP_PORT);
    }

    pjsip_sip_uri * sip_uri = NULL;

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri (genericUri);

    if (sip_uri == NULL) {
        _debug ("Invalid uri in findLocalAddressFromTransport\n");
        return atoi (DEFAULT_SIP_PORT);
    }

    if (PJSIP_URI_SCHEME_IS_SIPS (sip_uri)) {
        transportType = PJSIP_TRANSPORT_TLS;
        port = atoi (DEFAULT_SIP_TLS_PORT);
    } else {
        if (transport == NULL) {
            _debug ("transport is NULL in findLocalPortFromUri - Try the local UDP transport\n");
            transport = _localUDPTransport;
        }

        transportType = PJSIP_TRANSPORT_UDP;

        port = atoi (DEFAULT_SIP_PORT);
    }

    // Get the transport manager associated with
    // this endpoint
    pjsip_tpmgr * tpmgr = NULL;

    tpmgr = pjsip_endpt_get_tpmgr (_endpt);

    if (tpmgr == NULL) {
        _debug ("Unexpected: Cannot get tpmgr from endpoint.\n");
        return port;
    }

    // Find the local address (and port) based on the registered
    // transports and the transport type

    /* Init the transport selector */
    pj_status_t status;

    if (transportType == PJSIP_TRANSPORT_UDP) {
        _debug ("Transport ID: %s\n", transport->obj_name);

        status = init_transport_selector (transport, &tp_sel);

        if (status == PJ_SUCCESS)
            status = pjsip_tpmgr_find_local_addr (tpmgr, _pool, transportType, tp_sel, &localAddress, &port);
        else
            status = pjsip_tpmgr_find_local_addr (tpmgr, _pool, transportType, NULL, &localAddress, &port);
    } else
        status = pjsip_tpmgr_find_local_addr (tpmgr, _pool, transportType, NULL, &localAddress, &port);


    if (status != PJ_SUCCESS) {
        _debug ("Failed to find local address from transport\n");
    }

	_debug ("Local port discovered from attached transport: %i\n", port);
    return port;
}

pj_status_t SIPVoIPLink::createTlsTransportRetryOnFailure (AccountID id)
{
    pj_status_t success;

    // Create a TLS listener.
    // Note that STUN cannot be used for
    // TCP NAT traversal. At the moment (20/08/09)
    // user must supply the public address/port
    // manually.
    success = createTlsTransport (id);

    if (success != PJ_SUCCESS) {
        unsigned int randomPort = RANDOM_SIP_PORT;

        // Update new port in the corresponding SIPAccount
        SIPAccount * account = NULL;
        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

        if (account == NULL) {
            _debug ("createTlsTransportRetryOnFailure: Account is null. Returning\n");
            return !PJ_SUCCESS;
        }

        account->setLocalPort ( (pj_uint16_t) randomPort);

        // Try to start the transport again on
        // the new port.
        success = createTlsTransport (id);

        if (success != PJ_SUCCESS) {
            _debug ("createTlsTransportRetryOnFailure: failed to retry on random port %d\n", randomPort);
            return success;
        }

        _debug ("createTlsTransportRetryOnFailure: TLS transport listening on port %d\n", randomPort);
    }

    return PJ_SUCCESS;
}

pj_status_t SIPVoIPLink::createAlternateUdpTransport (AccountID id)
{
    pj_sockaddr_in boundAddr;
    pjsip_host_port a_name;
    pj_status_t status;
    pj_str_t stunServer;
    pj_uint16_t stunPort;
    pj_sockaddr_in pub_addr;
    pj_sock_t sock;
	std::string listeningAddress = "";
	int listeningPort;

    /*
     * Retrieve the account information
     */
    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _debug ("Account is null. Returning\n");
        return !PJ_SUCCESS;
    }

    stunServer = account->getStunServerName ();

    stunPort = account->getStunPort ();

    status = stunServerResolve (id);

    if (status != PJ_SUCCESS) {
        _debug ("Error resolving STUN server: %i\n", status);
        return status;
    }

    // Init socket
    sock = PJ_INVALID_SOCKET;

    _debug ("Initializing IPv4 socket on %s:%i\n", stunServer.ptr, stunPort);

    status = pj_sockaddr_in_init (&boundAddr, &stunServer, 0);

    if (status != PJ_SUCCESS) {
        _debug ("Error when initializing IPv4 socket on %s:%i\n", stunServer.ptr, stunPort);
        return status;
    }

    // Create and bind the socket
    status = pj_sock_socket (pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock);

    if (status != PJ_SUCCESS) {
        _debug ("Socket() error (%d)\n", status);
        return status;
    }

    // Query the mapped IP address and port on the 'outside' of the NAT
    status = pjstun_get_mapped_addr (&_cp.factory, 1, &sock, &stunServer, stunPort, &stunServer, stunPort, &pub_addr);

    if (status != PJ_SUCCESS) {
        _debug ("Error contacting STUN server (%d)", status);
        pj_sock_close (sock);
        return status;
    }

    _debug ("Firewall address : %s:%d",

            pj_inet_ntoa (pub_addr.sin_addr),
            pj_ntohs (pub_addr.sin_port));

    a_name.host = pj_str (pj_inet_ntoa (pub_addr.sin_addr));
    a_name.port = pj_ntohs (pub_addr.sin_port);

    listeningAddress = std::string (a_name.host.ptr);
    listeningPort = (int) a_name.port;

    // Set the address to be used in SDP
    account->setPublishedAddress (listeningAddress);
    account->setPublishedPort (listeningPort);

    // Create the UDP transport
    pjsip_transport *transport;
    status = pjsip_udp_transport_attach2 (_endpt, PJSIP_TRANSPORT_UDP, sock, &a_name, 1, &transport);

    if (status != PJ_SUCCESS) {
        _debug ("Error creating alternate SIP UDP listener (%d)\n", status);
		return status;
    }

	_debug ("UDP Transport successfully created on %s:%i\n", listeningAddress.c_str (), listeningPort);
    account->setAccountTransport (transport);

    return PJ_SUCCESS;
}



pj_status_t SIPVoIPLink::createTlsTransport (AccountID id)
{
    pjsip_tpfactory *tls;
    pj_sockaddr_in local_addr;
    pjsip_host_port a_name;
    pj_status_t status;

    /* Grab the tls settings, populated
     * from configuration file.
     */
    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _debug ("Account is null. Returning\n");
        return !PJ_SUCCESS;
    }

    /**
     * Init local address.
     * If IP interface address is not specified,
     * socket will be bound to PJ_INADDR_ANY.
     * If user specified port is an empty string
     * or if it is equal to 0, then the port will
     * be chosen automatically by the OS.
     */
    pj_sockaddr_in_init (&local_addr, 0, 0);

    pj_uint16_t localTlsPort = account->getLocalPort();

    if (localTlsPort != 0) {
        local_addr.sin_port = pj_htons (localTlsPort);
    }

    std::string localAddress = account->getLocalAddress();

    if (!localAddress.empty()) {
        pj_str_t pjAddress;
        pj_cstr (&pjAddress, (account->getLocalAddress()).c_str());

        pj_status_t success;
        success = pj_sockaddr_in_set_str_addr (&local_addr, &pjAddress);

        if (success != PJ_SUCCESS) {
            _debug ("Failed to set local address in %d\n", __LINE__);
        }
    }

    /* Init published name */
    pj_bzero (&a_name, sizeof (pjsip_host_port));

    pj_cstr (&a_name.host, (account->getPublishedAddress()).c_str());

    a_name.port = account->getPublishedPort();

    /* Get TLS settings. Expected to be filled */
    pjsip_tls_setting * tls_setting = account->getTlsSetting();

    _debug ("TLS transport to be initialized with published address %.*s,"
            " published port %d, local address %s, local port %d\n",
            (int) a_name.host.slen, a_name.host.ptr,
            (int) a_name.port, localAddress.c_str(), (int) localTlsPort);



    status = pjsip_tls_transport_start (_endpt, tls_setting, &local_addr, &a_name, 1, &tls);

    if (status != PJ_SUCCESS) {
        _debug ("Error creating SIP TLS listener (%d)\n", status);
    }

    return PJ_SUCCESS;
}


void SIPVoIPLink::updateAccountInfo(const AccountID& accountID)
{

    createUDPServer(accountID);

}


bool SIPVoIPLink::loadSIPLocalIP (std::string *addr)
{

    bool returnValue = true;
	std::string localAddress = "127.0.0.1";
    pj_sockaddr ip_addr;

    if (pj_gethostip (pj_AF_INET(), &ip_addr) != PJ_SUCCESS) {
		// Update the registration state if no network capabilities found
        _debug ("UserAgent: Get host ip failed!\n");
        returnValue = false;
    } else {
        localAddress = std::string (pj_inet_ntoa (ip_addr.ipv4.sin_addr));
        _debug ("UserAgent: Checking network, setting local IP address to: %s\n", localAddress.data());
    }

	*addr = localAddress;
    return returnValue;
}

void SIPVoIPLink::busy_sleep (unsigned msec)
{

    _debug("SIPVoIPLink::busy_sleep\n");
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
    /* Ideally we shouldn't call pj_thread_sleep() and rather
     * CActiveScheduler::WaitForAnyRequest() here, but that will
     * drag in Symbian header and it doesn't look pretty.
     */
    pj_thread_sleep (msec);
#else
    pj_time_val timeout, now, tv;

    pj_gettimeofday (&timeout);
    timeout.msec += msec;
    pj_time_val_normalize (&timeout);

    tv.sec = 0;
    tv.msec = 10;
    pj_time_val_normalize (&tv);

    do {
        pjsip_endpt_handle_events (_endpt, &tv);
        pj_gettimeofday (&now);
    } while (PJ_TIME_VAL_LT (now, timeout));

#endif
}

bool SIPVoIPLink::pjsip_shutdown (void)
{
    if (_endpt) {
        _debug ("UserAgent: Shutting down...\n");
        busy_sleep (1000);
    }

    pj_thread_join (thread);

    pj_thread_destroy (thread);
    thread = NULL;

    /* Destroy endpoint. */

    if (_endpt) {
        pjsip_endpt_destroy (_endpt);
        _endpt = NULL;
    }

    /* Destroy pool and pool factory. */
    if (_pool) {
        pj_pool_release (_pool);
        _pool = NULL;
        pj_caching_pool_destroy (&_cp);
    }

    /* Shutdown PJLIB */
    pj_shutdown();

    _debug ("UserAgent: Shutted down succesfully\n");

    /* Done. */
    return true;
}

int getModId()
{
    return _mod_ua.id;
}

static void dns_cb (pj_status_t status, void *token, const struct pjsip_server_addresses *addr)
{

    struct result * result = (struct result*) token;

    result->status = status;

    if (status == PJ_SUCCESS) {
        pj_memcpy (&result->servers, addr, sizeof (*addr));
    }
}

void set_voicemail_info (AccountID account, pjsip_msg_body *body)
{

    int voicemail = 0, pos_begin, pos_end;
    std::string voice_str = "Voice-Message: ";
    std::string delimiter = "/";
    std::string msg_body, voicemail_str;

    _debug ("UserAgent: checking the voice message!\n");
    // The voicemail message is formated like that:
    // Voice-Message: 1/0  . 1 is the number we want to retrieve in this case

    // We get the notification body
    msg_body = (char*) body->data;

    // We need the position of the first character of the string voice_str
    pos_begin = msg_body.find (voice_str);
    // We need the position of the delimiter
    pos_end = msg_body.find (delimiter);

    // So our voicemail number between the both index

    try {

        voicemail_str = msg_body.substr (pos_begin + voice_str.length(), pos_end - (pos_begin + voice_str.length()));
        std::cout << "voicemail number : " << voicemail_str << std::endl;
        voicemail = atoi (voicemail_str.c_str());
    } catch (std::out_of_range& e) {
        std::cerr << e.what() << std::endl;
    }

    // We need now to notify the manager
    if (voicemail != 0)
        Manager::instance().startVoiceMessageNotification (account, voicemail);
}

void SIPVoIPLink::handle_reinvite (SIPCall *call)
{
    // Close the previous RTP session
    call->getAudioRtp()->stop ();
    call->setAudioStart (false);

    _debug ("Create new rtp session from handle_reinvite : %s:%i\n", call->getLocalIp().c_str(), call->getLocalAudioPort());

    try {
        call->getAudioRtp()->initAudioRtpSession (call);
    } catch (...) {
        _debug ("! SIP Failure: Unable to create RTP Session (%s:%d)\n", __FILE__, __LINE__);
    }
}

// This callback is called when the invite session state has changed
void call_on_state_changed (pjsip_inv_session *inv, pjsip_event *e)
{
    _debug ("call_on_state_changed to state %s\n", invitationStateMap[inv->state]);

    pjsip_rx_data *rdata;
    pj_status_t status;

    /* Retrieve the call information */
    SIPCall * call = NULL;
    call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);

    if (call == NULL) {
        _debug ("Call is NULL in call_on_state_changed");
        return;
    } else {
        // _debug("    call_on_state_changed: call id %s\n", call->getCallId().c_str());
        // _debug("    call_on_state_changed: call state %s\n", invitationStateMap[call->getInvSession()->state]);
    }

    //Retrieve the body message
    rdata = e->body.tsx_state.src.rdata;

    // If the call is a direct IP-to-IP call
    AccountID accId;

    SIPVoIPLink * link = NULL;

    if (call->getCallConfiguration () == Call::IPtoIP) {
        link = SIPVoIPLink::instance ("");
    } else {
        accId = Manager::instance().getAccountFromCall (call->getCallId());
        link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));
    }

    if (link == NULL) {
        _debug ("Link is NULL in call_on_state_changed");
        return;
    }

    // If this is an outgoing INVITE that was created because of
    // REFER/transfer, send NOTIFY to transferer.
    if (call->getXferSub() && e->type==PJSIP_EVENT_TSX_STATE) {

        int st_code = -1;
        pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;

        switch (call->getInvSession()->state) {
                // switch (inv->state) {

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

            status = pjsip_xfer_notify (call->getXferSub(),
                                        ev_state, st_code,
                                        NULL, &tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Unable to create NOTIFY -- %d\n", status);
            } else {
                status = pjsip_xfer_send_request (call->getXferSub(), tdata);

                if (status != PJ_SUCCESS) {
                    _debug ("UserAgent: Unable to send NOTIFY -- %d\n", status);
                }
            }
        }

        return;
    }

    if (inv->state != PJSIP_INV_STATE_CONFIRMED) {
        // Update UI with the current status code and description
        //pjsip_transaction * tsx
        pjsip_transaction * tsx = NULL;
        tsx = e->body.tsx_state.tsx;
        int statusCode;

        if (tsx != NULL) {
            statusCode = tsx->status_code;
        }

        const pj_str_t * description = pjsip_get_status_text (statusCode);

        if (statusCode) {
            DBusManager::instance().getCallManager()->sipCallStateChanged (call->getCallId(), std::string (description->ptr, description->slen), statusCode);
        }
    }

    // The call is ringing - We need to handle this case only on outgoing call
    if (inv->state == PJSIP_INV_STATE_EARLY && e->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
        call->setConnectionState (Call::Ringing);
        Manager::instance().peerRingingCall (call->getCallId());
    }

    // After 2xx is sent/received.
    else if (inv->state == PJSIP_INV_STATE_CONNECTING) {
        status = call->getLocalSDP()->check_sdp_answer (inv, rdata);

        if (status != PJ_SUCCESS) {
            _debug ("Failed to check_incoming_sdp in call_on_state_changed\n");
            return;
        }
    }

    // After we sent or received a ACK - The connection is established
    else if (inv->state == PJSIP_INV_STATE_CONFIRMED) {

        link->SIPCallAnswered (call, rdata);

    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

        _debug ("State: %s. Cause: %.*s\n", invitationStateMap[inv->state], (int) inv->cause_text.slen, inv->cause_text.ptr);

        switch (inv->cause) {
                /* The call terminates normally - BYE / CANCEL */

            case PJSIP_SC_OK:

            case PJSIP_SC_REQUEST_TERMINATED:
                accId = Manager::instance().getAccountFromCall (call->getCallId());
                link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));

                if (link) {
                    link->SIPCallClosed (call);
                }

                break;

            case PJSIP_SC_NOT_FOUND:            /* peer not found */

            case PJSIP_SC_DECLINE:

            case PJSIP_SC_REQUEST_TIMEOUT:      /* request timeout */

            case PJSIP_SC_NOT_ACCEPTABLE_HERE:  /* no compatible codecs */

            case PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE:

            case PJSIP_SC_UNSUPPORTED_MEDIA_TYPE:

            case PJSIP_SC_UNAUTHORIZED:

            case PJSIP_SC_REQUEST_PENDING:
                accId = Manager::instance().getAccountFromCall (call->getCallId());
                link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));

                if (link) {
                    link->SIPCallServerFailure (call);
                }

                break;

            default:
                _debug ("sipvoiplink.cpp - line %d : Unhandled call state. This is probably a bug.\n", __LINE__);
                break;
        }
    }

}

// This callback is called after SDP offer/answer session has completed.
void call_on_media_update (pjsip_inv_session *inv, pj_status_t status)
{
    _debug ("call_on_media_update\n");

    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;

    SIPVoIPLink * link = NULL;
    SIPCall * call;

    call = reinterpret_cast<SIPCall *> (inv->mod_data[getModId() ]);

    if (!call) {
        _debug ("Call declined by peer, SDP negociation stopped\n");
        return;
    }

    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (AccountNULL));

    if (link == NULL) {
        _debug ("Failed to get sip link\n");
        return;
    }

    if (status != PJ_SUCCESS) {
        _debug ("Error while negotiating the offer\n");
        link->hangup (call->getCallId());
        Manager::instance().callFailure (call->getCallId());
        return;
    }

    if (!inv->neg) {
        return;
    }

    // Get the new sdp, result of the negotiation
    pjmedia_sdp_neg_get_active_local (inv->neg, &local_sdp);

    pjmedia_sdp_neg_get_active_remote (inv->neg, &remote_sdp);

    // Clean the resulting sdp offer to create a new one (in case of a reinvite)
    call->getLocalSDP()->clean_session_media();

    // Set the fresh negotiated one, no matter if that was an offer or answer.
    // The local sdp is updated in case of an answer, even if the remote sdp
    // is kept internally.
    call->getLocalSDP()->set_negotiated_sdp (local_sdp);

    // Set remote ip / port
    call->getLocalSDP()->set_media_transport_info_from_remote_sdp (remote_sdp);

    try {
        call->setAudioStart (true);
        call->getAudioRtp()->start();
    } catch (exception& rtpException) {
        _debug ("%s\n", rtpException.what());
    }

}

void call_on_forked (pjsip_inv_session *inv, pjsip_event *e)
{
}

void call_on_tsx_changed (pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e)
{
    _debug ("call_on_tsx_changed to state %s\n", transactionStateMap[tsx->state]);

    if (tsx->role==PJSIP_ROLE_UAS && tsx->state==PJSIP_TSX_STATE_TRYING &&
            pjsip_method_cmp (&tsx->method, &pjsip_refer_method) ==0) {
        /** Handle the refer method **/
        onCallTransfered (inv, e->body.tsx_state.src.rdata);
    }
}

void regc_cb (struct pjsip_regc_cbparam *param)
{
    SIPAccount * account = NULL;
    account = static_cast<SIPAccount *> (param->token);

    if (account == NULL) {
        _debug ("Account is NULL in regc_cb.\n");
        return;
    }

    assert (param);

    const pj_str_t * description = pjsip_get_status_text (param->code);

    if (param->code) {
        DBusManager::instance().getCallManager()->registrationStateChanged (account->getAccountID(), std::string (description->ptr, description->slen), param->code);
        std::pair<int, std::string> details (param->code, std::string (description->ptr, description->slen));
        account->setRegistrationStateDetailed (details);
    }

    if (param->status == PJ_SUCCESS) {
        if (param->code < 0 || param->code >= 300) {
            /* Sometimes, the status is OK, but we still failed.
             * So checking the code for real result
             */
            _debug ("UserAgent: The error is: %d\n", param->code);

            switch (param->code) {

                case 606:
                    account->setRegistrationState (ErrorConfStun);
                    break;

                case 503:

                case 408:
                    account->setRegistrationState (ErrorHost);
                    break;

                case 401:

                case 403:

                case 404:
                    account->setRegistrationState (ErrorAuth);
                    break;

                default:
                    account->setRegistrationState (Error);
                    break;
            }

            account->setRegister (false);
        } else {
            // Registration/Unregistration is success
            if (account->isRegister())
                account->setRegistrationState (Registered);
            else {
                account->setRegistrationState (Unregistered);
                account->setRegister (false);
            }
        }
    } else {
        account->setRegistrationState (ErrorAuth);
        account->setRegister (false);
    }

}

// Optional function to be called to process incoming request message.
pj_bool_t
mod_on_rx_request (pjsip_rx_data *rdata)
{
    pj_status_t status;
    pj_str_t reason;
    unsigned options = 0;
    pjsip_dialog* dialog;
    pjsip_tx_data *tdata;
    AccountID account_id;
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    std::string userName, server, displayName;
    SIPVoIPLink *link;
    CallID id;
    SIPCall* call;
    pjsip_inv_session *inv;
    SIPAccount *account;
    pjmedia_sdp_session *r_sdp;

    // pjsip_generic_string_hdr* hdr;

    // voicemail part
    std::string method_name;
    std::string request;

    // Handle the incoming call invite in this function
    _debug ("UserAgent: Callback on_rx_request is involved! \n");

    /* First, let's got the username and server name from the invite.
     * We will use them to detect which account is the callee.
     */
    uri = rdata->msg_info.to->uri;
    sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (uri);

    userName = std::string (sip_uri->user.ptr, sip_uri->user.slen);
    server = std::string (sip_uri->host.ptr, sip_uri->host.slen);

    _debug ("mod_on_rx_request: %s@%s\n", userName.c_str(), server.c_str());

    // Get the account id of callee from username and server
    account_id = Manager::instance().getAccountIdFromNameAndServer (userName, server);

    /* If we don't find any account to receive the call */

    if (account_id == AccountNULL) {
        _debug ("UserAgent: Username %s doesn't match any account!\n",userName.c_str());
        //return false;
    }

    /* Get the voip link associated to the incoming call */
    /* The account must before have been associated to the call in ManagerImpl */
    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (account_id));

    /* If we can't find any voIP link to handle the incoming call */
    if (link == 0) {
        _debug ("ERROR: can not retrieve the voiplink from the account ID...\n");
        return false;
    }


    char* from_header = strstr (rdata->msg_info.msg_buf, "From: ");

    if (from_header) {

        std::string temp (from_header);
        int begin_displayName = temp.find ("\"") + 1;
        int end_displayName = temp.rfind ("\"");
        // _debug("The display name start at %i, end at %i\n", begin_displayName, end_displayName);
        displayName = temp.substr (begin_displayName, end_displayName - begin_displayName);//display_name);
    } else {
        displayName = std::string ("");
    }

    _debug ("UserAgent: The receiver is : %s@%s\n", userName.data(), server.data());

    _debug ("UserAgent: The callee account id is %s\n", account_id.c_str());

    /* Now, it is the time to find the information of the caller */
    uri = rdata->msg_info.from->uri;


    // display_name = rdata->msg_info.from->name;

    // std::string temp("");///(char*)(&display_name));

    sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (uri);

    // Store the peer number
    char tmp[PJSIP_MAX_URL_SIZE];
    int length = pjsip_uri_print (PJSIP_URI_IN_FROMTO_HDR,
                                  sip_uri, tmp, PJSIP_MAX_URL_SIZE);

    std::string peerNumber (tmp, length);

    // Get the server voicemail notification
    // Catch the NOTIFY message

    if (rdata->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {
        method_name = "NOTIFY";
        // Retrieve all the message. Should contains only the method name but ...
        request =  rdata->msg_info.msg->line.req.method.name.ptr;
        // Check if the message is a notification

        if (request.find (method_name) != (size_t)-1) {
            /* Notify the right account */
            set_voicemail_info (account_id, rdata->msg_info.msg->body);
            request.find (method_name);
        }

        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_OK, NULL, NULL, NULL);

        return true;
    }

    // Handle an OPTIONS message
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_OPTIONS_METHOD) {
        handle_incoming_options (rdata);
        return true;
    }

    // Respond statelessly any non-INVITE requests with 500
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
        if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
            pj_strdup2 (_pool, &reason, "user agent unable to handle this request ");
            pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, &reason, NULL,
                                           NULL);
            return true;
        }
    }

    // Verify that we can handle the request
    status = pjsip_inv_verify_request (rdata, &options, NULL, NULL, _endpt, NULL);

    if (status != PJ_SUCCESS) {
        pj_strdup2 (_pool, &reason, "user agent unable to handle this INVITE ");
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, &reason, NULL,
                                       NULL);
        return true;
    }

    /******************************************* URL HOOK *********************************************/

    if (Manager::instance().getConfigString (HOOKS, URLHOOK_SIP_ENABLED) == "1") {

        std::string header_value;

        header_value = fetch_header_value (rdata->msg_info.msg, Manager::instance().getConfigString (HOOKS, URLHOOK_SIP_FIELD));

        if (header_value.size () < header_value.max_size()) {
            if (header_value!="") {
                urlhook->addAction (header_value,
                                    Manager::instance().getConfigString (HOOKS, URLHOOK_COMMAND));
            }
        } else
            throw length_error ("Url exceeds std::string max_size\n");

    }

    /************************************************************************************************/

    // Generate a new call ID for the incoming call!
    id = Manager::instance().getNewCallID();

    call = new SIPCall (id, Call::Incoming, _pool);

    /* If an error occured at the call creation */
    if (!call) {
        _debug ("UserAgent: unable to create an incoming call");
        return false;
    }

    std::string addrToUse;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));

    if (account != NULL) {
		// TODO May use the published address as well
		addrToUse = account->getLocalAddress ();
    }

	if (addrToUse == "0.0.0.0")
	{
		link->loadSIPLocalIP (&addrToUse);
	}

	// Have to do some stuff with the SDP
	// Set the codec map, IP, peer number and so on... for the SIPCall object
	setCallAudioLocal (call, addrToUse);

    // We retrieve the remote sdp offer in the rdata struct to begin the negociation
    call->getLocalSDP()->set_ip_address (addrToUse);

    get_remote_sdp_from_offer (rdata, &r_sdp);

    status = call->getLocalSDP()->receiving_initial_offer (r_sdp);

    if (status!=PJ_SUCCESS) {
        delete call;
        call=0;
        return false;
    }

    call->setConnectionState (Call::Progressing);

    call->setPeerNumber (peerNumber);

    call->setDisplayName (displayName);

    call->initRecFileName();

    // Notify UI there is an incoming call

    if (Manager::instance().incomingCall (call, account_id)) {
        // Add this call to the callAccountMap in ManagerImpl
        Manager::instance().getAccountLink (account_id)->addCall (call);
    } else {
        // Fail to notify UI
        delete call;
        call = NULL;
        _debug ("UserAgent: Fail to notify UI!\n");
        return false;
    }


    /* Create the local dialog (UAS) */
    status = pjsip_dlg_create_uas (pjsip_ua_instance(), rdata, NULL, &dialog);

    if (status != PJ_SUCCESS) {
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, &reason, NULL,
                                       NULL);
        return true;
    }

    // Specify media capability during invite session creation
    status = pjsip_inv_create_uas (dialog, rdata, call->getLocalSDP()->get_local_sdp_session(), 0, &inv);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Associate the call in the invite session
    inv->mod_data[_mod_ua.id] = call;

    // Send a 180/Ringing response
    status = pjsip_inv_initial_answer (inv, rdata, PJSIP_SC_RINGING, NULL, NULL, &tdata);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    status = pjsip_inv_send_msg (inv, tdata);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Associate invite session to the current call
    call->setInvSession (inv);

    // Update the connection state
    call->setConnectionState (Call::Ringing);

    /* Done */
    return true;

}

pj_bool_t mod_on_rx_response (pjsip_rx_data *rdata UNUSED)
{
    _debug ("Mod on rx response");
    return PJ_SUCCESS;
}

void onCallTransfered (pjsip_inv_session *inv, pjsip_rx_data *rdata)
{

    pj_status_t status;
    pjsip_tx_data *tdata;
    SIPCall *existing_call;
    const pj_str_t str_refer_to = { (char*) "Refer-To", 8};
    const pj_str_t str_refer_sub = { (char*) "Refer-Sub", 9 };
    const pj_str_t str_ref_by = { (char*) "Referred-By", 11 };
    pjsip_generic_string_hdr *refer_to;
    pjsip_generic_string_hdr *refer_sub;
    pjsip_hdr *ref_by_hdr;
    pj_bool_t no_refer_sub = PJ_FALSE;
    char *uri;
    std::string tmp;
    pjsip_status_code code;
    pjsip_evsub *sub;

    existing_call = (SIPCall *) inv->mod_data[_mod_ua.id];

    /* Find the Refer-To header */
    refer_to = (pjsip_generic_string_hdr*)
               pjsip_msg_find_hdr_by_name (rdata->msg_info.msg, &str_refer_to, NULL);

    if (refer_to == NULL) {
        /* Invalid Request.
         * No Refer-To header!
         */
        _debug ("UserAgent: Received REFER without Refer-To header!\n");
        pjsip_dlg_respond (inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    /* Find optional Refer-Sub header */
    refer_sub = (pjsip_generic_string_hdr*)
                pjsip_msg_find_hdr_by_name (rdata->msg_info.msg, &str_refer_sub, NULL);

    if (refer_sub) {
        if (!pj_strnicmp2 (&refer_sub->hvalue, "true", 4) ==0)
            no_refer_sub = PJ_TRUE;
    }

    /* Find optional Referred-By header (to be copied onto outgoing INVITE
     * request.
     */
    ref_by_hdr = (pjsip_hdr*)
                 pjsip_msg_find_hdr_by_name (rdata->msg_info.msg, &str_ref_by,
                                             NULL);

    /* Notify callback */
    code = PJSIP_SC_ACCEPTED;

    _debug ("UserAgent: Call to %.*s is being transfered to %.*s\n",
            (int) inv->dlg->remote.info_str.slen,
            inv->dlg->remote.info_str.ptr,
            (int) refer_to->hvalue.slen,
            refer_to->hvalue.ptr);

    if (no_refer_sub) {
        /*
         * Always answer with 2xx.
         */
        pjsip_tx_data *tdata;
        const pj_str_t str_false = { (char*) "false", 5};
        pjsip_hdr *hdr;

        status = pjsip_dlg_create_response (inv->dlg, rdata, code, NULL,
                                            &tdata);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to create 2xx response to REFER -- %d\n", status);
            return;
        }

        /* Add Refer-Sub header */
        hdr = (pjsip_hdr*)
              pjsip_generic_string_hdr_create (tdata->pool, &str_refer_sub,
                                               &str_false);

        pjsip_msg_add_hdr (tdata->msg, hdr);


        /* Send answer */
        status = pjsip_dlg_send_response (inv->dlg, pjsip_rdata_get_tsx (rdata),
                                          tdata);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to create 2xx response to REFER -- %d\n", status);
            return;
        }

        /* Don't have subscription */
        sub = NULL;

    } else {

        struct pjsip_evsub_user xfer_cb;
        pjsip_hdr hdr_list;

        /* Init callback */
        pj_bzero (&xfer_cb, sizeof (xfer_cb));
        xfer_cb.on_evsub_state = &xfer_svr_cb;

        /* Init addiTHIS_FILE, THIS_FILE, tional header list to be sent with REFER response */
        pj_list_init (&hdr_list);

        /* Create transferee event subscription */
        status = pjsip_xfer_create_uas (inv->dlg, &xfer_cb, rdata, &sub);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to create xfer uas -- %d\n", status);
            pjsip_dlg_respond (inv->dlg, rdata, 500, NULL, NULL, NULL);
            return;
        }

        /* If there's Refer-Sub header and the value is "true", send back
         * Refer-Sub in the response with value "true" too.
         */
        if (refer_sub) {
            const pj_str_t str_true = { (char*) "true", 4 };
            pjsip_hdr *hdr;

            hdr = (pjsip_hdr*)
                  pjsip_generic_string_hdr_create (inv->dlg->pool,
                                                   &str_refer_sub,
                                                   &str_true);
            pj_list_push_back (&hdr_list, hdr);

        }

        /* Accept the REFER request, send 2xx. */
        pjsip_xfer_accept (sub, rdata, code, &hdr_list);

        /* Create initial NOTIFY request */
        status = pjsip_xfer_notify (sub, PJSIP_EVSUB_STATE_ACTIVE,
                                    100, NULL, &tdata);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to create NOTIFY to REFER -- %d", status);
            return;
        }

        /* Send initial NOTIFY request */
        status = pjsip_xfer_send_request (sub, tdata);

        if (status != PJ_SUCCESS) {
            _debug ("UserAgent: Unable to send NOTIFY to REFER -- %d\n", status);
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
    tmp = std::string (uri);

    if (existing_call == NULL) {
        _debug ("UserAgent: Call doesn't exist!\n");
        return;
    }

    AccountID accId = Manager::instance().getAccountFromCall (existing_call->getCallId());

    CallID newCallId = Manager::instance().getNewCallID();

    if (!Manager::instance().outgoingCall (accId, newCallId, tmp)) {

        /* Notify xferer about the error (if we have subscription) */
        if (sub) {
            status = pjsip_xfer_notify (sub, PJSIP_EVSUB_STATE_TERMINATED,
                                        500, NULL, &tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Unable to create NOTIFY to REFER -- %d\n", status);
                return;
            }

            status = pjsip_xfer_send_request (sub, tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Unable to send NOTIFY to REFER -- %d\n", status);
                return;
            }
        }

        return;
    }

    SIPCall* newCall = 0;

    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));

    if (link) {
        newCall = dynamic_cast<SIPCall *> (link->getCall (newCallId));

        if (!newCall) {
            _debug ("UserAgent: can not find the call from sipvoiplink!\n");
            return;
        }
    }

    if (sub) {
        /* Put the server subscription in inv_data.
         * Subsequent state changed in pjsua_inv_on_state_changed() will be
         * reported back to the server subscription.
         */
        newCall->setXferSub (sub);

        /* Put the invite_data in the subscription. */
        pjsip_evsub_set_mod_data (sub, _mod_ua.id,
                                  newCall);
    }
}



void xfer_func_cb (pjsip_evsub *sub, pjsip_event *event)
{


    PJ_UNUSED_ARG (event);

    /*
     * When subscription is accepted (got 200/OK to REFER), check if
     * subscription suppressed.
     */

    if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_ACCEPTED) {

        _debug ("Transfer accepted! Waiting for notifications. \n");

    }

    /*
     * On incoming NOTIFY, notify application about call transfer progress.
     */
    else if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_ACTIVE ||
             pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        pjsip_msg *msg;
        pjsip_msg_body *body;
        pjsip_status_line status_line;
        pj_bool_t is_last;
        pj_bool_t cont;
        pj_status_t status;

        std::string noresource;
        std::string ringing;
        std::string request;

        noresource = "noresource";
        ringing = "Ringing";


        SIPVoIPLink *link = reinterpret_cast<SIPVoIPLink *> (pjsip_evsub_get_mod_data (sub, _mod_ua.id));

        /* When subscription is terminated, clear the xfer_sub member of
         * the inv_data.
         */

        if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
            _debug ("UserAgent: Xfer client subscription terminated\n");

        }

        if (!link || !event) {
            /* Application is not interested with call progress status */
            _debug ("UserAgent: Either link or event is empty!\n");
            return;
        }



        /* This better be a NOTIFY request */
        if (event->type == PJSIP_EVENT_TSX_STATE &&
                event->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {

            pjsip_rx_data *rdata;

            rdata = event->body.tsx_state.src.rdata;


            /* Check if there's body */
            msg = rdata->msg_info.msg;
            body = msg->body;

            if (!body) {
                // if (call->getCallConfiguration () == Call::IPtoIP) {
                //   _debug("UserAgent: IptoIp NOTIFY without message body\n");
                // }
                // else{
                _debug ("UserAgent: Warning! Received NOTIFY without message body\n");
                return;
                // }
            }



            /* Check for appropriate content */
            if (pj_stricmp2 (&body->content_type.type, "message") != 0 ||
                    pj_stricmp2 (&body->content_type.subtype, "sipfrag") != 0) {
                _debug ("UserAgent: Warning! Received NOTIFY with non message/sipfrag content\n");
                return;
            }

            /* Try to parse the content */
            status = pjsip_parse_status_line ( (char*) body->data, body->len,
                                               &status_line);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Warning! Received NOTIFY with invalid message/sipfrag content\n");
                return;
            }

        } else {
            _debug ("UserAgent: Set code to 500!\n");
            status_line.code = 500;
            status_line.reason = *pjsip_get_status_text (500);
        }

        // Get current call
        SIPCall *call = dynamic_cast<SIPCall *> (link->getCall (Manager::instance().getCurrentCallId()));

        if (!call) {
            _debug ("UserAgent: Call doesn't exit!\n");
            return;
        }


        if (event->body.rx_msg.rdata->msg_info.msg_buf != NULL) {
            request = event->body.rx_msg.rdata->msg_info.msg_buf;

            if ( (int) request.find (noresource) != -1) {
                _debug ("UserAgent: NORESOURCE for transfer!\n");
                link->transferStep2 (call);
                pjsip_evsub_terminate (sub, PJ_TRUE);

                Manager::instance().transferFailed();
                return;
            }

            if ( (int) request.find (ringing) != -1) {
                _debug ("UserAgent: transfered call RINGING!\n");
                link->transferStep2 (call);
                pjsip_evsub_terminate (sub, PJ_TRUE);

                Manager::instance().transferSucceded();
                return;
            }
        }


        /* Notify application */
        is_last = (pjsip_evsub_get_state (sub) ==PJSIP_EVSUB_STATE_TERMINATED);

        cont = !is_last;

        if (status_line.code/100 == 2) {

            _debug ("UserAgent: Try to stop rtp!\n");
            pjsip_tx_data *tdata;

            status = pjsip_inv_end_session (call->getInvSession(), PJSIP_SC_GONE, NULL, &tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Fail to create end session msg!\n");
            } else {
                status = pjsip_inv_send_msg (call->getInvSession(), tdata);

                if (status != PJ_SUCCESS)
                    _debug ("UserAgent: Fail to send end session msg!\n");
            }

            link->transferStep2 (call);

            cont = PJ_FALSE;
        }

        if (!cont) {
            pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
        }

    }

}


void xfer_svr_cb (pjsip_evsub *sub, pjsip_event *event)
{


    PJ_UNUSED_ARG (event);

    /*
     * When subscription is terminated, clear the xfer_sub member of
     * the inv_data.
     */

    if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        SIPCall *call;

        call = (SIPCall*) pjsip_evsub_get_mod_data (sub, _mod_ua.id);

        if (!call)
            return;

        pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);

        call->setXferSub (NULL);

        _debug ("UserAgent: Xfer server subscription terminated\n");
    }
}

void on_rx_offer (pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{


#ifdef CAN_REINVITE
    _debug ("%s (%d): on_rx_offer REINVITE\n", __FILE__, __LINE__);

    SIPCall *call;
    pj_status_t status;
    AccountID accId;
    SIPVoIPLink *link;

    call = (SIPCall*) inv->mod_data[getModId() ];

    if (!call)
        return;

    accId = Manager::instance().getAccountFromCall (call->getCallId());

    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));

    call->getLocalSDP()->receiving_initial_offer ( (pjmedia_sdp_session*) offer);

    status=pjsip_inv_set_sdp_answer (call->getInvSession(), call->getLocalSDP()->get_local_sdp_session());

    if (link)
        link->handle_reinvite (call);

#endif

}

void handle_incoming_options (pjsip_rx_data *rdata)
{


    pjsip_tx_data *tdata;
    pjsip_response_addr res_addr;
    const pjsip_hdr *cap_hdr;
    pj_status_t status;

    /* Create basic response. */
    status = pjsip_endpt_create_response (_endpt, rdata, PJSIP_SC_OK, NULL, &tdata);

    if (status != PJ_SUCCESS) {
        return;
    }

    /* Add Allow header */
    cap_hdr = pjsip_endpt_get_capability (_endpt, PJSIP_H_ALLOW, NULL);

    if (cap_hdr) {
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));
    }

    /* Add Accept header */
    cap_hdr = pjsip_endpt_get_capability (_endpt, PJSIP_H_ACCEPT, NULL);

    if (cap_hdr) {
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));
    }

    /* Add Supported header */
    cap_hdr = pjsip_endpt_get_capability (_endpt, PJSIP_H_SUPPORTED, NULL);

    if (cap_hdr) {
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));
    }

    /* Add Allow-Events header from the evsub module */
    cap_hdr = pjsip_evsub_get_allow_events_hdr (NULL);

    if (cap_hdr) {
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));
    }

    /* Send response statelessly */
    pjsip_get_response_addr (tdata->pool, rdata, &res_addr);

    status = pjsip_endpt_send_response (_endpt, &res_addr, tdata, NULL, NULL);

    if (status != PJ_SUCCESS)
        pjsip_tx_data_dec_ref (tdata);
}

/*****************************************************************************************************************/


bool setCallAudioLocal (SIPCall* call, std::string localIP)
{
    SIPAccount *account = NULL;

    if (call) {
        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (Manager::instance().getAccountFromCall (call->getCallId ())));

        // Setting Audio
        unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
        unsigned int callLocalExternAudioPort = callLocalAudioPort;

        if (account->isStunEnabled ()) {
            // If use Stun server
            callLocalExternAudioPort = account->getStunPort ();
        }

        _debug ("            Setting local ip address: %s\n", localIP.c_str());

        _debug ("            Setting local audio port to: %d\n", callLocalAudioPort);
        _debug ("            Setting local audio port (external) to: %d\n", callLocalExternAudioPort);

        // Set local audio port for SIPCall(id)
        call->setLocalIp (localIP);
        call->setLocalAudioPort (callLocalAudioPort);
        call->setLocalExternAudioPort (callLocalExternAudioPort);

        call->getLocalSDP()->attribute_port_to_all_media (callLocalExternAudioPort);

        return true;
    }

    return false;
}

std::string fetch_header_value (pjsip_msg *msg, std::string field)
{


    pj_str_t name;
    pjsip_generic_string_hdr * hdr;
    std::string value, url;
    size_t pos;

    std::cout << "fetch header value" << std::endl;

    /* Convert the field name into pjsip type */
    name = pj_str ( (char*) field.c_str());

    /* Get the header value and convert into string*/
    hdr = (pjsip_generic_string_hdr*) pjsip_msg_find_hdr_by_name (msg, &name, NULL);

    if (!hdr)
        return "";

    value = hdr->hvalue.ptr;

    if ( (pos=value.find ("\n")) == std::string::npos) {
        return "";
    }

    url = value.substr (0, pos);

    return url;
}

std::vector<std::string> SIPVoIPLink::getAllIpInterface (void)
{
    pj_sockaddr addrList[16];
    unsigned int addrCnt = PJ_ARRAY_SIZE (addrList);

    pj_status_t success;
    success = pj_enum_ip_interface (pj_AF_INET(), &addrCnt, addrList);

    std::vector<std::string> ifaceList;

    if (success != PJ_SUCCESS) {
        return ifaceList;
    }

    _debug ("Detecting available interfaces...\n");

    int i;

    for (i = 0; i < (int) addrCnt; i++) {
        char tmpAddr[PJ_INET_ADDRSTRLEN];
        pj_sockaddr_print (&addrList[i], tmpAddr, sizeof (tmpAddr), 0);
        ifaceList.push_back (std::string (tmpAddr));
        _debug ("Local interface %s\n", tmpAddr);
    }

    return ifaceList;
}


pj_bool_t stun_sock_on_status (pj_stun_sock *stun_sock, pj_stun_sock_op op, pj_status_t status)
{
    if (status == PJ_SUCCESS)
        return PJ_TRUE;
    else
        return PJ_FALSE;
}

pj_bool_t stun_sock_on_rx_data (pj_stun_sock *stun_sock, void *pkt, unsigned pkt_len, const pj_sockaddr_t *src_addr, unsigned addr_len)
{
    return PJ_TRUE;
}


std::string getLocalAddressAssociatedToAccount (AccountID id)
{
    SIPAccount *account = NULL;
    pj_sockaddr_in local_addr_ipv4;
    pjsip_transport *tspt;
    std::string localAddr;
    pj_str_t tmp;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    // Set the local address

    if (account != NULL) {
        tspt = account->getAccountTransport ();

        if (tspt != NULL) {
            local_addr_ipv4 = tspt->local_addr.ipv4;
        } else {
            _debug ("In getLocalAddressAssociatedToAccount: transport is null");
            local_addr_ipv4 = _localUDPTransport->local_addr.ipv4;
        }
    } else {
        _debug ("In getLocalAddressAssociatedToAccount: account is null");
        local_addr_ipv4 = _localUDPTransport->local_addr.ipv4;
    }

    _debug ("slbvasjklbvaskbvaskvbaskvaskvbsdfk: %i\n", local_addr_ipv4.sin_addr.s_addr);

    tmp = pj_str (pj_inet_ntoa (local_addr_ipv4.sin_addr));
    localAddr = std::string (tmp.ptr);

    _debug ("slbvasjklbvaskbvaskvbaskvaskvbsdfk: %s\n", localAddr.c_str());

    return localAddr;

}
