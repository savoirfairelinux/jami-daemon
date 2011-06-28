/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yun Liu <yun.liu@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "sipvoiplink.h"

#include "manager.h"

#include "sip/sdp.h"
#include "sipcall.h"
#include "sipaccount.h"
#include "eventthread.h"
#include "SdesNegotiator.h"

#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"

#include "hooks/urlhook.h"
#include "im/InstantMessaging.h"

#include "audio/audiolayer.h"
#include "audio/audiortp/AudioRtpFactory.h"

#include "video/video_rtp_factory.h"

#include "pjsip/sip_endpoint.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_transport_tls.h"
#include "pjsip/sip_uri.h"
#include <pjnath.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <istream>
#include <utility> // for std::pair

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>

#include <map>

#define CAN_REINVITE        1

using namespace sfl;

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

/** The default transport (5060) */
pjsip_transport *_localUDPTransport = NULL;

/** The local tls listener */
pjsip_tpfactory *_localTlsListener = NULL;

/** A map to retreive SFLphone internal call id
 *  Given a SIP call ID (usefull for transaction sucha as transfer)*/
std::map<std::string, CallID> transferCallID;


const pj_str_t STR_USER_AGENT = { (char*) "User-Agent", 10 };

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

/*
 * Retrieve the SDP of the peer contained in the offer
 *
 * @param rdata The request data
 * @param r_sdp The pjmedia_sdp_media to stock the remote SDP
 */
void getRemoteSdpFromOffer (pjsip_rx_data *rdata, pjmedia_sdp_session** r_sdp);

int getModId();

/**
 * Set audio (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 * @return bool True
 */
bool setCallMediaLocal (SIPCall* call, const std::string &localIP);

/**
 * Helper function to parse incoming OPTION message
 */
void handleIncomingOptions (pjsip_rx_data *rxdata);

/**
 * Helper function to parser header from incoming sip messages
 */
std::string fetchHeaderValue (pjsip_msg *msg, std::string field);

/**
 * Helper function that retreive IP address from local udp transport
 */
std::string getLocalAddressAssociatedToAccount (AccountID id);

/*
 *  The global pool factory
 */
pj_caching_pool pool_cache, *_cp = &pool_cache;

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
void setVoicemailInfo (AccountID account, pjsip_msg_body *body);

pj_bool_t stun_sock_on_status_cb (pj_stun_sock *stun_sock, pj_stun_sock_op op, pj_status_t status);
pj_bool_t stun_sock_on_rx_data_cb (pj_stun_sock *stun_sock, void *pkt, unsigned pkt_len, const pj_sockaddr_t *src_addr, unsigned addr_len);

/*
 * Session callback
 * Called after SDP offer/answer session has completed.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	status	A pj_status_t structure
 */
void sdp_media_update_cb (pjsip_inv_session *inv, pj_status_t status UNUSED);


void sdp_request_offer_cb (pjsip_inv_session *inv, const pjmedia_sdp_session *offer);

void sdp_create_offer_cb (pjsip_inv_session *inv, pjmedia_sdp_session **p_offer);

/*
 * Session callback
 * Called when the invite session state has changed.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	e	A pointer on a pjsip_event structure
 */
void invite_session_state_changed_cb (pjsip_inv_session *inv, pjsip_event *e);

/*
 * Called when the invite usage module has created a new dialog and invite
 * because of forked outgoing request.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	e	A pointer on a pjsip_event structure
 */
void outgoing_request_forked_cb (pjsip_inv_session *inv, pjsip_event *e);

/*
 * Session callback
 * Called whenever any transactions within the session has changed their state.
 * Useful to monitor the progress of an outgoing request.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	tsx	A pointer on a pjsip_transaction structure
 * @param	e	A pointer on a pjsip_event structure
 */
void transaction_state_changed_cb (pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);


/*
 * Registration callback
 */
void registration_cb (struct pjsip_regc_cbparam *param);

/*
 * DNS Callback used in workaround for bug #1852
 */
static void dns_cb (pj_status_t status, void *token, const struct pjsip_server_addresses *addr);

/*
 * Called to handle incoming requests outside dialogs
 * @param   rdata
 * @return  pj_bool_t
 */
pj_bool_t transaction_request_cb (pjsip_rx_data *rdata);

/*
 * Called to handle incoming response
 * @param	rdata
 * @return	pj_bool_t
 */
pj_bool_t transaction_response_cb (pjsip_rx_data *rdata UNUSED) ;

/**
 * Send an ACK message inside a transaction. PJSIP send automatically, non-2xx ACK response.
 * ACK for a 2xx response must be send using this method.
 */
static void sendAck (pjsip_dialog *dlg, pjsip_rx_data *rdata);

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 * @param sip call
 */
int SIPSessionReinvite(SIPCall *);

/*
 * Transfer callbacks
 */
void transfer_client_cb (pjsip_evsub *sub, pjsip_event *event);
void transfer_server_cb (pjsip_evsub *sub, pjsip_event *event);

/**
 * Helper function to process refer function on call transfer
 */
void onCallTransfered (pjsip_inv_session *inv, pjsip_rx_data *rdata);

/*************************************************************************************************/

SIPVoIPLink* SIPVoIPLink::_instance = NULL;


SIPVoIPLink::SIPVoIPLink (const AccountID& accountID)
    : VoIPLink (accountID)
    , _nbTryListenAddr (2)   // number of times to try to start SIP listener
    , _regPort (atoi (DEFAULT_SIP_PORT))
    , _clients (0)
{

    _debug ("SIPVOIPLINK");
    // to get random number for RANDOM_PORT
    srand (time (NULL));

    urlhook = new UrlHook ();

    /* Start pjsip initialization step */
    init();
}

SIPVoIPLink::~SIPVoIPLink()
{
    _debug ("UserAgent: SIPVoIPLink destructor called");

    terminate();

}

SIPVoIPLink* SIPVoIPLink::instance (const AccountID& id)
{

    if (!_instance) {
        _debug ("UserAgent: Create new SIPVoIPLink instance");
        _instance = new SIPVoIPLink (id);
    }

    return _instance;
}

void SIPVoIPLink::decrementClients (void)
{
    _clients--;

    if (_clients == 0) {

        _debug ("UserAgent: No SIP account anymore, terminate SIPVoIPLink");
        // terminate();
        delete SIPVoIPLink::_instance;
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
    pjsipInit();

    initDone (true);

    return true;
}

void
SIPVoIPLink::terminate()
{
    _debug ("UserAgent: Terminating SIPVoIPLink");

    if (_evThread) {
        _debug ("UserAgent: Deleting sip eventThread");
        delete _evThread;
        _evThread = NULL;
    }


    /* Clean shutdown of pjsip library */
    if (initDone()) {
        _debug ("UserAgent: Shutting down PJSIP");
        pjsipShutdown();
    }

    initDone (false);

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

void SIPVoIPLink::sendRegister (AccountID id) throw(VoipLinkException)
{

    int expire_value;

    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsip_host_info destination;

    std::string tmp, hostname, username, password;
    SIPAccount *account = NULL;
    pjsip_regc *regc;
    pjsip_hdr hdr_list;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));
    if (account == NULL) {
        throw VoipLinkException("Account pointer is NULL in send register");
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
            _debug ("status : %d", result.status);
        }

        if (result.status != PJ_SUCCESS) {
            _debug ("Failed to resolve hostname only once."
                    " Default resolver will be used on"
                    " hostname for all requests.");
        } else {
            _debug ("%d servers where obtained from name resolution.", result.servers.count);
            char addr_buf[80];

            pj_sockaddr_print ( (pj_sockaddr_t*) &result.servers.entry[0].addr, addr_buf, sizeof (addr_buf), 3);
            account->setHostname (addr_buf);
        }
    }


    // Create SIP transport or get existent SIP transport from internal map
    // according to account settings, if the transport could not be created but
    // one is already set in account, use this one (most likely this is the
    // transport we tried to create)
    acquireTransport (account->getAccountID());

    if (account->getAccountTransport()) {
        _debug ("Acquire transport in account registration: %s %s (refcnt=%d)",
                account->getAccountTransport()->obj_name,
                account->getAccountTransport()->info,
                (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));
    }

    _mutexSIP.enterMutex();

    // Get the client registration information for this particular account
    regc = account->getRegistrationInfo();
    account->setRegister (true);

    // Set the expire value of the message from the config file
    std::istringstream stream (account->getRegistrationExpire());
    stream >> expire_value;

    if (!expire_value) {
        expire_value = PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;
    }

    // Update the state of the voip link
    account->setRegistrationState (Trying);

    // Create the registration according to the account ID
    // status = pjsip_regc_create (_endpt, (void*) account, &registration_cb, &regc);
    status = pjsip_regc_create (_endpt, (void *) &account->getAccountID(), &registration_cb, &regc);

    if (status != PJ_SUCCESS) {
        _mutexSIP.leaveMutex();
        throw VoipLinkException("UserAgent: Unable to create regc structure.");
    }

    // Creates URI
    std::string fromUri = account->getFromUri();
    std::string srvUri = account->getServerUri();

    std::string address = findLocalAddressFromUri (srvUri, account->getAccountTransport ());
    int port = findLocalPortFromUri (srvUri, account->getAccountTransport ());

    std::stringstream ss;
    std::string portStr;
    ss << port;
    ss >> portStr;

    std::string contactUri = account->getContactHeader (address, portStr);

    _debug ("UserAgent: sendRegister: fromUri: %s serverUri: %s contactUri: %s",
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
        _mutexSIP.leaveMutex();
        throw VoipLinkException("Unable to initialize account registration structure");
    }

    // Fill route set
    if (! (account->getServiceRoute().empty())) {
        pjsip_route_hdr *route_set = createRouteSet(account, _pool);
        pjsip_regc_set_route_set (regc, route_set);
    }

    pjsip_cred_info *cred = account->getCredInfo();
    int credential_count = account->getCredentialCount();
    _debug ("UserAgent: setting %d credentials in sendRegister", credential_count);
    pjsip_regc_set_credentials (regc, credential_count, cred);

    // Add User-Agent Header
    pj_list_init (&hdr_list);

    const char *useragent_name = getUseragentName (id).c_str();
    pj_str_t useragent = pj_str ( (char *) useragent_name);
    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create (_pool, &STR_USER_AGENT, &useragent);

    pj_list_push_back (&hdr_list, (pjsip_hdr*) h);
    pjsip_regc_add_headers (regc, &hdr_list);


    if ((status = pjsip_regc_register (regc, PJ_TRUE, &tdata)) != PJ_SUCCESS) {
        _mutexSIP.leaveMutex();
        throw VoipLinkException("Unable to initialize transaction data for account registration");
    }

    pjsip_tpselector *tp;

    initTransportSelector (account->getAccountTransport (), &tp, _pool);

    // pjsip_regc_set_transport increments transport ref count by one
    status = pjsip_regc_set_transport (regc, tp);

    if (account->getAccountTransport()) {
        // decrease transport's ref count, counter icrementation is
        // managed when acquiring transport
        pjsip_transport_dec_ref (account->getAccountTransport ());

        _debug ("UserAgent: After setting the transport in account registration using transport: %s %s (refcnt=%d)",
                account->getAccountTransport()->obj_name,
                account->getAccountTransport()->info,
                (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));
    }

    if (status != PJ_SUCCESS) {
        _mutexSIP.leaveMutex ();
        throw VoipLinkException("Unable to set transport");
    }

    // Send registration request
    // pjsip_regc_send increment the transport ref count by one,
    status = pjsip_regc_send (regc, tdata);

    if (account->getAccountTransport()) {
        // Decrease transport's ref count, since coresponding reference counter decrementation
        // is performed in pjsip_regc_destroy. This function is never called in SFLphone as the
        // regc data structure is permanently associated to the account at first registration.
        pjsip_transport_dec_ref (account->getAccountTransport ());
    }

    if (status != PJ_SUCCESS) {
        _mutexSIP.leaveMutex();
        throw VoipLinkException("Unable to send account registration request");
    }

    _mutexSIP.leaveMutex();

    account->setRegistrationInfo (regc);

    if (account->getAccountTransport()) {

        _debug ("Sent account registration using transport: %s %s (refcnt=%d)",
                account->getAccountTransport()->obj_name,
                account->getAccountTransport()->info,
                (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));
    }
}

void SIPVoIPLink::sendUnregister (AccountID id) throw(VoipLinkException)
{

    pj_status_t status = 0;
    pjsip_tx_data *tdata = NULL;
    SIPAccount *account;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    // If an transport is attached to this account, detach it and decrease reference counter
    if (account->getAccountTransport()) {

        _debug ("Sent account unregistration using transport: %s %s (refcnt=%d)",
                account->getAccountTransport()->obj_name,
                account->getAccountTransport()->info,
                (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));

    }

    // This may occurs if account failed to register and is in state INVALID
    if (!account->isRegister()) {
        account->setRegistrationState (Unregistered);
        return;
    }

    pjsip_regc *regc = account->getRegistrationInfo();
    if(regc == NULL) {
    	throw VoipLinkException("Registration structure is NULL");
    }

    status = pjsip_regc_unregister (regc, &tdata);
    if (status != PJ_SUCCESS) {
    	throw VoipLinkException("Unable to unregister sip account");
    }

    status = pjsip_regc_send (regc, tdata);

    if (status != PJ_SUCCESS) {
    	throw VoipLinkException("Unable to send request to unregister sip account");
    }

    account->setRegister (false);
}

Call *SIPVoIPLink::newOutgoingCall (const CallID& id, const std::string& toUrl) throw (VoipLinkException)
{
    SIPAccount * account = NULL;
    pj_status_t status;
    std::string localAddr, addrSdp;

    // Create a new SIP call
    SIPCall* call = new SIPCall (id, Call::Outgoing, _cp);
    if(call == NULL) {
    	throw VoipLinkException("Could not create new SIP call");
    }

    // Find the account associated to this call
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (Manager::instance().getAccountFromCall (id)));
    if (account == NULL) {
    	_error ("UserAgent: Error: Could not retrieving account to make call with");
    	call->setConnectionState (Call::Disconnected);
    	call->setState (Call::Error);
    	delete call;
    	call = NULL;
    	// TODO: We should investigate how we could get rid of this error and create a IP2IP call instead
    	throw VoipLinkException("Could not get account for this call");
    }

    // If toUri is not a well formated sip URI, use account information to process it
    std::string toUri;
    if((toUrl.find("sip:") != std::string::npos) ||
    		toUrl.find("sips:") != std::string::npos) {
    	toUri = toUrl;
    }
    else {
        toUri = account->getToUri (toUrl);
    }
    call->setPeerNumber (toUri);
    _debug ("UserAgent: New outgoing call %s to %s", id.c_str(), toUri.c_str());

    localAddr = getInterfaceAddrFromName (account->getLocalInterface ());
    _debug ("UserAgent: Local address for thi call: %s", localAddr.c_str());

    if (localAddr == "0.0.0.0") {
    	loadSIPLocalIP (&localAddr);
    }

    setCallMediaLocal (call, localAddr);

    // May use the published address as well
    account->isStunEnabled () ? addrSdp = account->getPublishedAddress () : addrSdp = getInterfaceAddrFromName (account->getLocalInterface ());

    if (addrSdp == "0.0.0.0") {
				loadSIPLocalIP (&addrSdp);
    }

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (PAYLOAD_CODEC_ULAW);
    if (audiocodec == NULL) {
    	_error ("UserAgent: Could not instantiate codec");
    	delete call;
    	call = NULL;
    	throw VoipLinkException ("Could not instantiate codec for early media");
    }

	try {
		_info ("UserAgent: Creating new rtp session");
		call->getAudioRtp()->initAudioRtpConfig (call);
		call->getAudioRtp()->initAudioRtpSession (call);
		call->getAudioRtp()->initLocalCryptoInfo (call);
		_info ("UserAgent: Start audio rtp session");
		call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
		_info ("UserAgent: Start video rtp session");
		call->getVideoRtp()->start();
	} catch (...) {
		throw VoipLinkException ("Could not start rtp session for early media");
	}

	// init file name according to peer phone number
	call->initRecFileName (toUrl);

	// Building the local SDP offer
	call->getLocalSDP()->setLocalIP (addrSdp);
	status = call->getLocalSDP()->createOffer (account->getActiveCodecs ());
	if (status != PJ_SUCCESS) {
		delete call;
		call = NULL;
		throw VoipLinkException ("Could not create local sdp offer for new call");
	}

	if (SIPOutgoingInvite (call)) {
		call->setConnectionState (Call::Progressing);
		call->setState (Call::Active);
		addCall (call);
	} else {
		delete call;
		call = NULL;
		throw VoipLinkException("Could not send outgoing INVITE request for new call");
	}

	return call;
}

bool
SIPVoIPLink::answer (const CallID& id) throw (VoipLinkException)
{
    pj_status_t status = PJ_SUCCESS;
    pjsip_tx_data *tdata;
    pjsip_inv_session *inv_session;

    _debug ("UserAgent: Answering call %s", id.c_str());

    SIPCall *call = getSIPCall (id);
    if (call==NULL) {
        throw VoipLinkException("Call is NULL while answering");
    }

    inv_session = call->getInvSession();

    if (status == PJ_SUCCESS) {

        _debug ("UserAgent: SDP negotiation success! : call %s ", call->getCallId().c_str());
        // Create and send a 200(OK) response
        if((status = pjsip_inv_answer (inv_session, PJSIP_SC_OK, NULL, NULL, &tdata)) != PJ_SUCCESS) {
        	throw VoipLinkException("Could not init invite request answer (200 OK)");
        }
        if((status = pjsip_inv_send_msg (inv_session, tdata)) != PJ_SUCCESS) {
        	throw VoipLinkException("Could not send invite request answer (200 OK)");
        }

        call->setConnectionState (Call::Connected);
        call->setState (Call::Active);

        return true;
    } else {
        // Create and send a 488/Not acceptable because the SDP negotiation failed
        if((status = pjsip_inv_answer (inv_session, PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, NULL, &tdata)) != PJ_SUCCESS) {
        	throw VoipLinkException("Could not init invite answer (488 not acceptable here)");
        }
        if((status = pjsip_inv_send_msg (inv_session, tdata)) != PJ_SUCCESS) {
        	throw VoipLinkException("Could not init invite request answer (488 NOT ACCEPTABLE HERE)");
        }
        // Terminate the call
        _debug ("UserAgent: SDP negotiation failed, terminate call %s ", call->getCallId().c_str());

        if(call->getAudioRtp()) {
        	throw VoipLinkException("No audio rtp session for this call");
        }

        try {
            call->getAudioRtp()->stop ();
        }
        catch(...) {
        	throw VoipLinkException("Could not stop rtp session");
        }

        removeCall (call->getCallId());

        return false;
    }
}

bool
SIPVoIPLink::hangup (const CallID& id) throw (VoipLinkException)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;

    SIPCall* call = getSIPCall (id);
    if (call == NULL) {
        throw VoipLinkException("Call is NULL while hanging up");
    }

    AccountID account_id = Manager::instance().getAccountFromCall (id);
    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));
    if(account == NULL) {
    	throw VoipLinkException("Could not find account for this call");
    }


    pjsip_inv_session *inv = call->getInvSession();
    if(inv == NULL) {
    	throw VoipLinkException("No invite session for this call");
    }

    // Looks for sip routes
    if (! (account->getServiceRoute().empty())) {
        pjsip_route_hdr *route_set = createRouteSet(account, inv->pool);
        pjsip_dlg_set_route_set (inv->dlg, route_set);
    }

    // User hangup current call. Notify peer
    status = pjsip_inv_end_session (inv, 404, NULL, &tdata);
    if (status != PJ_SUCCESS) {
        return false;
    }

    if (tdata == NULL) {
        return true;
    }

    status = pjsip_inv_send_msg (inv, tdata);
    if (status != PJ_SUCCESS)
        return false;

    // Make sure user data is NULL in callbacks
    inv->mod_data[getModId()] = NULL;

    // Release RTP thread
    try {
        if (Manager::instance().isCurrentCall (id)) {
            call->getAudioRtp()->stop();
            call->getVideoRtp()->stop();
        }
    }
    catch(...) {
    	throw VoipLinkException("Could not stop audio rtp session");
    }

    removeCall (id);

    return true;
}

bool
SIPVoIPLink::peerHungup (const CallID& id) throw (VoipLinkException)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    SIPCall* call;

    _info ("UserAgent: Peer hungup");

    call = getSIPCall (id);
    if (call == NULL) {
        throw VoipLinkException("Call does not exist");
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

    // Make sure user data is NULL in callbacks
    call->getInvSession()->mod_data[getModId() ] = NULL;

    // Release RTP thread
    try {
        if (Manager::instance().isCurrentCall (id)) {
            _debug ("UserAgent: Stopping AudioRTP for hangup");
            call->getAudioRtp()->stop();
            call->getVideoRtp()->stop();
        }
    }
    catch(...) {
    	throw VoipLinkException("Could not stop audio rtp session");
    }

    removeCall (id);

    return true;
}

bool
SIPVoIPLink::cancel (const CallID& id) throw (VoipLinkException)
{
    _info ("UserAgent: Cancel call %s", id.c_str());

    SIPCall* call = getSIPCall (id);
    if (!call) {
    	throw VoipLinkException("Call does not exist");
    }

    removeCall (id);

    return true;
}


bool
SIPVoIPLink::onhold (const CallID& id) throw (VoipLinkException)
{
	Sdp *sdpSession;
    pj_status_t status;
    SIPCall* call;

    call = getSIPCall (id);

    if (call == NULL) {
    	throw VoipLinkException("Could not find call");
    }

    // Stop sound
    call->setState (Call::Hold);

    try {
        call->getAudioRtp()->stop();
        call->getVideoRtp()->stop();
    }
    catch (...) {
    	throw VoipLinkException("Could not stop audio rtp session");
    }

    _debug ("UserAgent: Stopping RTP session for on hold action");

    sdpSession = call->getLocalSDP();
    if (sdpSession == NULL) {
    	throw VoipLinkException("Could not find sdp session");
    }

    sdpSession->removeAttributeFromLocalAudioMedia("sendrecv");
    sdpSession->removeAttributeFromLocalAudioMedia("sendonly");

    sdpSession->addAttributeToLocalAudioMedia("sendonly");

    // Create re-INVITE with new offer
    status = SIPSessionReinvite (call);

    if (status != PJ_SUCCESS) {
        return false;
    }

    return true;
}

bool
SIPVoIPLink::offhold (const CallID& id) throw (VoipLinkException)
{
	Sdp *sdpSession;
    pj_status_t status;
    SIPCall *call;

    _debug ("UserAgent: retrive call from hold status");

    call = getSIPCall (id);

    if (call == NULL) {
    	throw VoipLinkException("Could not find call");
    }

    sdpSession = call->getLocalSDP();
    if (sdpSession == NULL) {
    	throw VoipLinkException("Could not find sdp session");
    }

    try {
        // Retreive previously selected codec
        AudioCodecType pl;
        sfl::Codec *sessionMedia = sdpSession->getSessionMedia();
        if (sessionMedia == NULL) {
            // throw VoipLinkException("Could not find session media");
    	    _warn("UserAgent: Session media not yet initialized, using default (ULAW)");
    	    pl = PAYLOAD_CODEC_ULAW;
        }
        else {
    	    // Get PayloadType for this codec
    	    pl = (AudioCodecType) sessionMedia->getPayloadType();
        }

        _debug ("UserAgent: Payload from session media %d", pl);


        // Create a new instance for this codec
        sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (pl);
        if (audiocodec == NULL) {
    	    throw VoipLinkException("Could not instantiate codec");
        }

        call->getAudioRtp()->initAudioRtpConfig (call);
        call->getAudioRtp()->initAudioRtpSession (call);
        call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
        call->getVideoRtp()->start();

    }
    catch (SdpException &e) {
    	_error("UserAgent: Exception: %s", e.what());
    } 
    catch (...) {
    	throw VoipLinkException("Could not create audio rtp session");
    }

    sdpSession->removeAttributeFromLocalAudioMedia("sendrecv");
    sdpSession->removeAttributeFromLocalAudioMedia("sendonly");

    sdpSession->addAttributeToLocalAudioMedia("sendrecv");

    /* Create re-INVITE with new offer */
    status = SIPSessionReinvite (call);
    if (status != PJ_SUCCESS) {
        return false;
    }

    call->setState (Call::Active);

    return true;
}

bool
SIPVoIPLink::sendTextMessage (sfl::InstantMessaging *module, const std::string& callID, const std::string& message, const std::string& from)
{
    _debug ("SipVoipLink: Send text message to %s, from %s", callID.c_str(), from.c_str());

    SIPCall *call = getSIPCall (callID);
    pj_status_t status = !PJ_SUCCESS;


    if (call) {
        std::string formatedFrom = from;

        // add double quotes for xml formating
        formatedFrom.insert (0,"\"");
        formatedFrom.append ("\"");

        /* Send IM message */
        sfl::InstantMessaging::UriList list;

        sfl::InstantMessaging::UriEntry entry;
        entry[sfl::IM_XML_URI] = std::string (formatedFrom);

        list.push_front (entry);

        std::string formatedMessage = module->appendUriList (message, list);

        status = module->send_sip_message (call->getInvSession (), (CallID&) callID, formatedMessage);

    } else {
        /* Notify the client of an error */
        /*Manager::instance ().incomingMessage (	"",
        										"sflphoned",
        										"Unable to send a message outside a call.");*/
    }

    return status;
}

int SIPSessionReinvite (SIPCall *call)
{

    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_session *local_sdp;

    _debug("UserAgent: Sending re-INVITE request");

    if (call == NULL) {
        _error ("UserAgent: Error: Call is NULL in session reinvite");
        return !PJ_SUCCESS;
    }

    if ( (local_sdp = call->getLocalSDP()->getLocalSdpSession()) == NULL) {
        _debug ("UserAgent: Error: Unable to find local sdp");
        return !PJ_SUCCESS;
    }

    // Build the reinvite request
    status = pjsip_inv_reinvite (call->getInvSession(), NULL, local_sdp, &tdata);

    if (status != PJ_SUCCESS)
        return 1;   // !PJ_SUCCESS

    // Send it
    status = pjsip_inv_send_msg (call->getInvSession(), tdata);

    if (status != PJ_SUCCESS)
        return 1;   // !PJ_SUCCESS

    return PJ_SUCCESS;
}

bool
SIPVoIPLink::transfer (const CallID& id, const std::string& to) throw (VoipLinkException)
{

    std::string tmp_to;
    pjsip_evsub *sub;
    pjsip_tx_data *tdata;

    struct pjsip_evsub_user xfer_cb;
    pj_status_t status;

    SIPCall *call = getSIPCall (id);
    if (call == NULL) {
    	throw VoipLinkException("Could not find call");
    }

    call->stopRecording();

    AccountID account_id = Manager::instance().getAccountFromCall (id);
    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));
    if (account == NULL) {
    	throw VoipLinkException("Could not find account");
    }

    std::string dest;
    pj_str_t pjDest;

    if (to.find ("@") == std::string::npos) {
        dest = account->getToUri (to);
        pj_cstr (&pjDest, dest.c_str());
    }

    _info ("UserAgent: Transfering to %s", dest.c_str());

    /* Create xfer client subscription. */
    pj_bzero (&xfer_cb, sizeof (xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    status = pjsip_xfer_create_uac (call->getInvSession()->dlg, &xfer_cb, &sub);
    if (status != PJ_SUCCESS) {
    	throw VoipLinkException("Could not create xfer request");
    }

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data (sub, getModId(), this);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate (sub, &pjDest, &tdata);
    if (status != PJ_SUCCESS) {
    	throw VoipLinkException("Could not create REFER request");
    }

    // Put SIP call id in map in order to retrieve call during transfer callback
    std::string callidtransfer (call->getInvSession()->dlg->call_id->id.ptr, call->getInvSession()->dlg->call_id->id.slen);
    transferCallID.insert (std::pair<std::string, CallID> (callidtransfer, call->getCallId()));

    /* Send. */
    status = pjsip_xfer_send_request (sub, tdata);
    if (status != PJ_SUCCESS) {
    	throw VoipLinkException("Could not send xfer request");
    }

    return true;
}

bool SIPVoIPLink::attendedTransfer(const CallID& transferId, const CallID& targetId)
{
	char str_dest_buf[PJSIP_MAX_URL_SIZE*2];
	pj_str_t str_dest;
	pjsip_dialog *target_dlg;
	pjsip_uri *uri;
	pjsip_evsub *sub;
	pjsip_tx_data *tdata;

	struct pjsip_evsub_user xfer_cb;
	pj_status_t status;

	_debug("UserAgent: Attended transfer");

	str_dest.ptr = NULL;
	str_dest.slen = 0;

    SIPCall *targetCall = getSIPCall (targetId);
    target_dlg = targetCall->getInvSession()->dlg;

    /* Print URI */
    str_dest_buf[0] = '<';
    str_dest.slen = 1;

    uri = (pjsip_uri*) pjsip_uri_get_uri(target_dlg->remote.info->uri);
    int len = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri,
                              str_dest_buf+1, sizeof(str_dest_buf)-1);
    str_dest.slen += len;

    len = pj_ansi_snprintf(str_dest_buf + str_dest.slen,
    		               sizeof(str_dest_buf) - str_dest.slen,
    			           "?"
    		               "Replaces=%.*s"
    			           "%%3Bto-tag%%3D%.*s"
    			           "%%3Bfrom-tag%%3D%.*s>",
    			           (int)target_dlg->call_id->id.slen,
                           target_dlg->call_id->id.ptr,
                           (int)target_dlg->remote.info->tag.slen,
                           target_dlg->remote.info->tag.ptr,
                           (int)target_dlg->local.info->tag.slen,
                           target_dlg->local.info->tag.ptr);

    str_dest.ptr = str_dest_buf;
    str_dest.slen += len;

    SIPCall *transferCall = getSIPCall (transferId);

    /* Create xfer client subscription. */
    pj_bzero (&xfer_cb, sizeof (xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;

    status = pjsip_xfer_create_uac (transferCall->getInvSession()->dlg, &xfer_cb, &sub);

    if (status != PJ_SUCCESS) {
    	_warn ("UserAgent: Unable to create xfer -- %d", status);
    	return false;
    }

    /* Associate this voiplink of call with the client subscription
     * We can not just associate call with the client subscription
     * because after this function, we can no find the cooresponding
     * voiplink from the call any more. But the voiplink is useful!
     */
    pjsip_evsub_set_mod_data (sub, getModId(), this);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate (sub, &str_dest, &tdata);

    if (status != PJ_SUCCESS) {
    	_error ("UserAgent: Unable to create REFER request -- %d", status);
    	return false;
    }

    // Put SIP call id in map in order to retrieve call during transfer callback
    std::string callidtransfer (transferCall->getInvSession()->dlg->call_id->id.ptr,
    							transferCall->getInvSession()->dlg->call_id->id.slen);
    _debug ("%s", callidtransfer.c_str());
    transferCallID.insert (std::pair<std::string, CallID> (callidtransfer, transferCall->getCallId()));


    /* Send. */
    status = pjsip_xfer_send_request (sub, tdata);

    if (status != PJ_SUCCESS) {
    	_error ("UserAgent: Unable to send REFER request -- %d", status);
    	return false;
    }

	return true;
}

bool SIPVoIPLink::transferStep2 (SIPCall* call)
{

    // TODO is this the best way to proceed?
    Manager::instance().peerHungupCall (call->getCallId());

    return true;
}

bool
SIPVoIPLink::refuse (const CallID& id)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_tx_data *tdata;

    _debug ("UserAgent: Refuse call %s", id.c_str());

    call = getSIPCall (id);

    if (call==0) {
        _error ("UserAgent: Error: Call doesn't exist");
        return false;
    }

    // can't refuse outgoing call or connected
    if (!call->isIncoming() || call->getConnectionState() == Call::Connected) {
        _debug ("UserAgent: Call %s is not in state incoming, or is already answered");
        return false;
    }

    // Stop Audio RTP session
    call->getAudioRtp()->stop();
    call->getVideoRtp()->stop();

    // User refuse current call. Notify peer
    status = pjsip_inv_end_session (call->getInvSession(), PJSIP_SC_DECLINE, NULL, &tdata);   //603

    if (status != PJ_SUCCESS)
        return false;

    status = pjsip_inv_send_msg (call->getInvSession(), tdata);

    if (status != PJ_SUCCESS)
        return false;

    // Make sure the pointer is NULL in callbacks
    call->getInvSession()->mod_data[getModId() ] = NULL;

    removeCall (id);

    _debug ("UserAgent: Refuse call completed");

    return true;
}

void
SIPVoIPLink::terminateCall (const CallID& id)
{
    _debug ("UserAgent: Terminate call %s", id.c_str());

    SIPCall *call = getSIPCall (id);

    if (call) {
        // terminate the sip call
        delete call;
        call = 0;
    }
}

std::string
SIPVoIPLink::getCurrentCodecName(const CallID& id)
{

    SIPCall *call = NULL;
    sfl::Codec *ac = NULL;
    std::string name = "";

    try {
        // call = getSIPCall (Manager::instance().getCurrentCallId());
        call = getSIPCall (id);
        if(call == NULL) {
            _error("UserAgent: Error: No current call");
   	    // return empty string
            return name;
        }
    
        if(call->getLocalSDP()->hasSessionMedia()) {
            ac = call->getLocalSDP()->getSessionMedia();
        }
        else {
	    return name;
        }
    }
    catch (SdpException &e) {
	_error("UserAgent: Exception: %s", e.what());
    }

    if (ac == NULL) {
	_error("UserAgent: Error: No codec initialized for this session");
    }

    name = ac->getMimeSubtype();

    return name;
}

std::string SIPVoIPLink::getUseragentName (const AccountID& id)
{
    /*
    useragent << PROGNAME << "/" << SFLPHONED_VERSION;
    return useragent.str();
    */

    SIPAccount *account = (SIPAccount *) Manager::instance().getAccount (id);

    std::ostringstream  useragent;

    useragent << account->getUseragent();

    if (useragent.str() == "sflphone" || useragent.str() == "")
        useragent << "/" << SFLPHONED_VERSION;

    return useragent.str ();
}

bool
SIPVoIPLink::carryingDTMFdigits (const CallID& id, char code)
{
    SIPCall *call = getSIPCall (id);

    if (!call) {
        //_error ("UserAgent: Error: Call doesn't exist while sending DTMF");
        return false;
    }

    AccountID accountID = Manager::instance().getAccountFromCall (id);
    SIPAccount *account = static_cast<SIPAccount *> (Manager::instance().getAccount (accountID));

    if (!account) {
        _error ("UserAgent: Error: Account not found while sending DTMF");
        return false;
    }

    DtmfType type = account->getDtmfType();

    if (type == OVERRTP)
        dtmfOverRtp (call, code);
    else if (type == SIPINFO)
        dtmfSipInfo (call, code);
    else {
        _error ("UserAgent: Error: Dtmf type does not exist");
        return false;
    }

    return true;
}


bool
SIPVoIPLink::dtmfSipInfo (SIPCall *call, char code)
{

    int duration;
    const int body_len = 1000;
    char *dtmf_body;
    pj_status_t status;
    pjsip_tx_data *tdata;
    pj_str_t methodName, content;
    pjsip_method method;
    pjsip_media_type ctype;
    pj_pool_t *tmp_pool;

    _debug ("UserAgent: Send DTMF %c", code);

    // Create a temporary memory pool
    tmp_pool = pj_pool_create (&_cp->factory, "tmpdtmf10", 1000, 1000, NULL);
    if (tmp_pool == NULL) {
    	_debug ("UserAgent: Could not initialize memory pool while sending DTMF");
    	return false;
    }

    duration = Manager::instance().voipPreferences.getPulseLength();

    dtmf_body = new char[body_len];

    snprintf (dtmf_body, body_len - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);

    pj_strdup2 (tmp_pool, &methodName, "INFO");
    pjsip_method_init_np (&method, &methodName);

    /* Create request message. */
    status = pjsip_dlg_create_request (call->getInvSession()->dlg, &method, -1, &tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to create INFO request -- %d", status);
        return false;
    }

    /* Get MIME type */
    pj_strdup2 (tmp_pool, &ctype.type, "application");

    pj_strdup2 (tmp_pool, &ctype.subtype, "dtmf-relay");

    /* Create "application/dtmf-relay" message body. */
    pj_strdup2 (tmp_pool, &content, dtmf_body);

    tdata->msg->body = pjsip_msg_body_create (tdata->pool, &ctype.type, &ctype.subtype, &content);

    if (tdata->msg->body == NULL) {
        _debug ("UserAgent: Unable to create msg body!");
        pjsip_tx_data_dec_ref (tdata);
        return false;
    }

    /* Send the request. */
    status = pjsip_dlg_send_request (call->getInvSession()->dlg, tdata, getModId(), NULL);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to send MESSAGE request -- %d", status);
        return false;
    }

    pj_pool_release(tmp_pool);

    return true;
}

bool
SIPVoIPLink::dtmfOverRtp (SIPCall* call, char code)
{
    call->getAudioRtp()->sendDtmfDigit (atoi (&code));

    return true;
}



bool
SIPVoIPLink::SIPOutgoingInvite (SIPCall* call)
{
    // If no SIP proxy setting for direct call with only IP address
    if (!SIPStartCall (call, "")) {
        _debug ("! SIP Failure: call not started");
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

    _debug ("UserAgent: Start sip call");

    if (call == NULL)
        return false;

    _error ("UserAgent: pool capacity %d", pj_pool_get_capacity (_pool));
    _error ("UserAgent: pool size %d", pj_pool_get_used_size (_pool));

    AccountID id = Manager::instance().getAccountFromCall (call->getCallId());

    // Get the basic information about the callee account
    SIPAccount * account = NULL;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _debug ("UserAgent: Error: Account is null in SIPStartCall");
        return false;
    }


    // Creates URI
    std::string fromUri = account->getFromUri();
    std::string toUri = call->getPeerNumber(); // expecting a fully well formed sip uri

    std::string address = findLocalAddressFromUri (toUri, account->getAccountTransport ());
    int port = findLocalPortFromUri (toUri, account->getAccountTransport ());

    std::stringstream ss;
    std::string portStr;
    ss << port;
    ss >> portStr;

    std::string contactUri = account->getContactHeader (address, portStr);

    _debug ("UserAgent: FROM uri: %s, TO uri: %s, CONTACT uri: %s",
            fromUri.c_str(), toUri.c_str(), contactUri.c_str());

    pj_str_t pjFrom;
    pj_cstr (&pjFrom, fromUri.c_str());

    pj_str_t pjContact;
    pj_cstr (&pjContact, contactUri.c_str());

    pj_str_t pjTo;
    pj_cstr (&pjTo, toUri.c_str());

    // Create the dialog (UAC)
    status = pjsip_dlg_create_uac (pjsip_ua_instance(), &pjFrom,
                                   &pjContact, &pjTo, NULL, &dialog);
    if (status != PJ_SUCCESS) {
        _error ("UserAgent: Error: UAC creation failed");
        return false;
    }

    // Create the invite session for this call
    status = pjsip_inv_create_uac (dialog, call->getLocalSDP()->getLocalSdpSession(), 0, &inv);

    if (! (account->getServiceRoute().empty())) {
        pjsip_route_hdr *route_set = createRouteSet(account, inv->pool);
        pjsip_dlg_set_route_set (dialog, route_set);
    }
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

    initTransportSelector (account->getAccountTransport (), &tp, inv->pool);

    // increment transport's ref count by one
    status = pjsip_dlg_set_transport (dialog, tp);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

    status = pjsip_inv_send_msg (inv, tdata);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

    if (account->getAccountTransport()) {

        _debug ("UserAgent: Sent invite request using transport: %s %s (refcnt=%d)",
                account->getAccountTransport()->obj_name,
                account->getAccountTransport()->info,
                (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));
    }

    _error ("UserAgent: pool capacity %d", pj_pool_get_capacity (_pool));
    _error ("UserAgent: pool size %d", pj_pool_get_used_size (_pool));

    return true;
}

void
SIPVoIPLink::SIPCallServerFailure (SIPCall *call)
{
    if (call != 0) {
        _error ("UserAgent: Error: Server error!");
        CallID id = call->getCallId();
        Manager::instance().callFailure (id);
        removeCall (id);

        if (call->getAudioRtp ()) {
            call->getAudioRtp()->stop();
        }

        if (call->getVideoRtp ()) {
            call->getVideoRtp()->stop();
        }
    }
}

void
SIPVoIPLink::SIPCallClosed (SIPCall *call)
{
    _info ("UserAgent: Closing call");

    if (!call) {
        _warn ("UserAgent: Error: CAll pointer is NULL\n");
        return;
    }

    CallID id = call->getCallId();

    if (Manager::instance().isCurrentCall (id)) {
        _debug ("UserAgent: Stopping AudioRTP when closing");
        call->getAudioRtp()->stop();
        call->getVideoRtp()->stop();
    }

    Manager::instance().peerHungupCall (id);
    removeCall (id);

}

void
SIPVoIPLink::SIPCallReleased (SIPCall *call)
{
    if (!call) {
        return;
    }

    // if we are here.. something when wrong before...
    _debug ("UserAgent: SIP call release");

    CallID id = call->getCallId();

    Manager::instance().callFailure (id);

    removeCall (id);
}


void
SIPVoIPLink::SIPCallAnswered (SIPCall *call, pjsip_rx_data *rdata UNUSED)
{

    _info ("UserAgent: SIP call answered");

    if (!call) {
        _warn ("UserAgent: Error: SIP failure, unknown call");
        return;
    }

    if (call->getConnectionState() != Call::Connected) {
        _debug ("UserAgent: Update call state , id = %s", call->getCallId().c_str());
        call->setConnectionState (Call::Connected);
        call->setState (Call::Active);
        Manager::instance().peerAnsweredCall (call->getCallId());
    } else {
        _debug ("UserAgent: Answering call (on/off hold to send ACK)");
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

bool SIPVoIPLink::SIPNewIpToIpCall (const CallID& id, const std::string& to)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;
    std::string localAddress, addrSdp;

    _debug ("UserAgent: New IP2IP call %s to %s", id.c_str(), to.c_str());

    /* Create the call */
    call = new SIPCall (id, Call::Outgoing, _cp);

    if (call) {

        call->setCallConfiguration (Call::IPtoIP);

        // Init recfile name using to uri
        call->initRecFileName (to);

        SIPAccount * account = NULL;
        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));

        if (account == NULL) {
            _error ("UserAgent: Error: Account %s is null. Returning", IP2IP_PROFILE);
            return !PJ_SUCCESS;
        }

        // Set the local address
        localAddress = getInterfaceAddrFromName (account->getLocalInterface ());
        // Set SDP parameters - Set to local
        addrSdp = localAddress;

        // If local address bound to ANY, reslove it using PJSIP
        if (localAddress == "0.0.0.0") {
            loadSIPLocalIP (&localAddress);
        }

        _debug ("UserAgent: Local Address for IP2IP call: %s", localAddress.c_str());

        // Local address to appear in SDP
        if (addrSdp == "0.0.0.0") {
            addrSdp = localAddress;
        }

        _debug ("UserAgent: Media Address for IP2IP call: %s", localAddress.c_str());

        // Set local address for RTP media
        setCallMediaLocal (call, localAddress);

        std::string toUri = account->getToUri (to);
        call->setPeerNumber (toUri);

        _debug ("UserAgent: TO uri for IP2IP call: %s", toUri.c_str());

        sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (PAYLOAD_CODEC_ULAW);

        // Audio Rtp Session must be initialized before creating initial offer in SDP session
        // since SDES require crypto attribute.
        try {
            call->getAudioRtp()->initAudioRtpConfig (call);
            call->getAudioRtp()->initAudioRtpSession (call);
            call->getAudioRtp()->initLocalCryptoInfo (call);
            call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
            call->getVideoRtp()->start ();
        } catch (...) {
            _debug ("UserAgent: Unable to create RTP Session in new IP2IP call (%s:%d)", __FILE__, __LINE__);
        }

        // Building the local SDP offer
        call->getLocalSDP()->setLocalIP (addrSdp);
        status = call->getLocalSDP()->createOffer (account->getActiveCodecs ());
        if(status != PJ_SUCCESS) {
        	_error("UserAgent: Failed to create local offer\n");
        }

        // Init TLS transport if enabled
        if (account->isTlsEnabled()) {

            _debug ("UserAgent: TLS enabled for IP2IP calls");
            int at = toUri.find ("@");
            int trns = toUri.find (";transport");
            std::string remoteAddr = toUri.substr (at+1, trns-at-1);

            if (toUri.find ("sips:") != 1) {
                _debug ("UserAgent: Error \"sips\" scheme required for TLS call");
                return false;
            }

            if (createTlsTransport (account->getAccountID(), remoteAddr) != PJ_SUCCESS)
                return false;
        }

        // If no transport already set, use the default one created at pjsip initialization
        if (account->getAccountTransport() == NULL) {
            _debug ("UserAgent: No transport for this account, using the default one");
            account->setAccountTransport (_localUDPTransport);
        }

        _debug ("UserAgent: Local port %i for IP2IP call", account->getLocalPort());

        _debug ("UserAgent: Local address in sdp %s for IP2IP call", localAddress.c_str());

        // Create URI
        std::string fromUri = account->getFromUri();
        std::string address = findLocalAddressFromUri (toUri, account->getAccountTransport());

        int port = findLocalPortFromUri (toUri, account->getAccountTransport());

        std::stringstream ss;
        std::string portStr;
        ss << port;
        ss >> portStr;

        std::string contactUri = account->getContactHeader (address, portStr);

        _debug ("UserAgent:  FROM uri: %s, TO uri: %s, CONTACT uri: %s",
                fromUri.c_str(), toUri.c_str(), contactUri.c_str());

        pj_str_t pjFrom;
        pj_cstr (&pjFrom, fromUri.c_str());

        pj_str_t pjTo;
        pj_cstr (&pjTo, toUri.c_str());

        pj_str_t pjContact;
        pj_cstr (&pjContact, contactUri.c_str());

        // Create the dialog (UAC)
        // (Parameters are "strduped" inside this function)
        _debug ("UserAgent: Creating dialog for this call");
        status = pjsip_dlg_create_uac (pjsip_ua_instance(), &pjFrom, &pjContact, &pjTo, NULL, &dialog);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

        // Create the invite session for this call
        _debug ("UserAgent: Creating invite session for this call");
        status = pjsip_inv_create_uac (dialog, call->getLocalSDP()->getLocalSdpSession(), 0, &inv);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

        if (! (account->getServiceRoute().empty())) {
            pjsip_route_hdr *route_set = createRouteSet(account, inv->pool);
            pjsip_dlg_set_route_set (dialog, route_set);
        }

        // Set the appropriate transport
        pjsip_tpselector *tp;

        initTransportSelector (account->getAccountTransport(), &tp, inv->pool);

        if (!account->getAccountTransport()) {
            _error ("UserAgent: Error: Transport is NULL in IP2IP call");
        }

        // set_transport methods increment transport's ref_count
        status = pjsip_dlg_set_transport (dialog, tp);

        // decrement transport's ref count
        // pjsip_transport_dec_ref(account->getAccountTransport());

        if (status != PJ_SUCCESS) {
            _error ("UserAgent: Error: Failed to set the transport for an IP2IP call");
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

bool SIPVoIPLink::pjsipInit()
{
    pj_status_t status;
    pjsip_inv_callback inv_cb;
    pj_str_t accepted;
    std::string name_mod;
    std::string addr;

    name_mod = "sflphone";

    _debug ("pjsip_init");

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
    pj_caching_pool_init (_cp, &pj_pool_factory_default_policy, 0);

    // Create memory pool for application.
    _pool = pj_pool_create (&_cp->factory, "sflphone", 4000, 4000, NULL);

    if (!_pool) {
        _debug ("UserAgent: Could not initialize memory pool");
        return PJ_ENOMEM;
    }

    // Create the SIP endpoint
    status = pjsip_endpt_create (&_cp->factory, pj_gethostname()->ptr, &_endpt);

    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    if (!loadSIPLocalIP (&addr)) {
        _debug ("UserAgent: Unable to determine network capabilities");
        return false;
    }

    // Initialize default UDP transport according to
    // IP to IP settings (most likely using port 5060)
    createDefaultSipUdpTransport();

    // Call this method to create TLS listener
    createDefaultSipTlsListener();

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
    _mod_ua.on_rx_request = &transaction_request_cb;
    _mod_ua.on_rx_response = &transaction_response_cb;
    status = pjsip_endpt_register_module (_endpt, &_mod_ua);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init the event subscription module.
    // It extends PJSIP by supporting SUBSCRIBE and NOTIFY methods
    status = pjsip_evsub_init_module (_endpt);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init xfer/REFER module
    status = pjsip_xfer_init_module (_endpt);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Init the callback for INVITE session:
    pj_bzero (&inv_cb, sizeof (inv_cb));
    inv_cb.on_state_changed = &invite_session_state_changed_cb;
    inv_cb.on_new_session = &outgoing_request_forked_cb;
    inv_cb.on_media_update = &sdp_media_update_cb;
    inv_cb.on_tsx_state_changed = &transaction_state_changed_cb;
    inv_cb.on_rx_offer = &sdp_request_offer_cb;
    inv_cb.on_create_offer = &sdp_create_offer_cb;

    // Initialize session invite module
    status = pjsip_inv_usage_init (_endpt, &inv_cb);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    _debug ("UserAgent: VOIP callbacks initialized");

    // Add endpoint capabilities (INFO, OPTIONS, etc) for this UA
    pj_str_t allowed[] = { { (char*) "INFO", 4}, { (char*) "REGISTER", 8}, { (char*) "OPTIONS", 7}, { (char*) "MESSAGE", 7 } };       //  //{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6}
    accepted = pj_str ( (char*) "application/sdp");

    // Register supported methods
    pjsip_endpt_add_capability (_endpt, &_mod_ua, PJSIP_H_ALLOW, NULL, PJ_ARRAY_SIZE (allowed), allowed);

    const pj_str_t STR_MIME_TEXT_PLAIN = { (char*) "text/plain", 10 };
    pjsip_endpt_add_capability (_endpt, &_mod_ua, PJSIP_H_ACCEPT, NULL, 1, &STR_MIME_TEXT_PLAIN);

    // Register "application/sdp" in ACCEPT header
    pjsip_endpt_add_capability (_endpt, &_mod_ua, PJSIP_H_ACCEPT, NULL, 1, &accepted);

    _debug ("UserAgent: pjsip version %s for %s initialized", pj_get_version(), PJ_OS_NAME);

    status = pjsip_replaces_init_module	(_endpt);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

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
        _debug ("stunServerResolve: Account is null. Returning");
        return !PJ_SUCCESS;
    }

    // Get the STUN server name and port
    stunServer = account->getStunServerName ();

    stunPort = account->getStunPort ();

    // Initialize STUN configuration
    pj_stun_config_init (&stunCfg, &_cp->factory, 0, pjsip_endpt_get_ioqueue (_endpt), pjsip_endpt_get_timer_heap (_endpt));

    status = PJ_EPENDING;

    pj_bzero (&stun_sock_cb, sizeof (stun_sock_cb));

    stun_sock_cb.on_rx_data = &stun_sock_on_rx_data_cb;

    stun_sock_cb.on_status = &stun_sock_on_status_cb;

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



bool SIPVoIPLink::acquireTransport (const AccountID& accountID)
{

    SIPAccount* account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accountID));

    if (!account)
        return false;


    // If an account is already bound to this account, decrease its reference
    // as it is going to change. If the same transport is selected, reference
    // counter will be increased
    if (account->getAccountTransport()) {

        _debug ("pjsip_transport_dec_ref in acquireTransport");
        pjsip_transport_dec_ref (account->getAccountTransport());
    }

    // Try to create a new transport in case the settings for this account
    // are different than one defined for already created ones
    // If TLS is enabled, TLS connection is automatically handled when sending account registration
    // However, for any other sip transaction, we must create TLS connection
    if (createSipTransport (accountID)) {
        return true;
    }
    // A transport is already created on this port, use it
    else {

        _debug ("Could not create a new transport (%s)", account->getTransportMapKey().c_str());
        _debug ("Searching transport (%s) in transport map", account->getTransportMapKey().c_str());

        // Could not create new transport, this transport may already exists
        SipTransportMap::iterator transport;
        transport = _transportMap.find (account->getTransportMapKey());

        if (transport != _transportMap.end()) {

            // Transport already exist, use it for this account
            _debug ("Found transport (%s) in transport map", account->getTransportMapKey().c_str());

            pjsip_transport* tr = transport->second;

            // Set transport to be used for transaction involving this account
            account->setAccountTransport (tr);

            // Increment newly associated transport reference counter
            // If the account is shutdowning, time is automatically canceled
            pjsip_transport_add_ref (tr);

            return true;
        } else {

            // Transport could not either be created, socket not available
            _debug ("Did not find transport (%s) in transport map", account->getTransportMapKey().c_str());

            account->setAccountTransport (_localUDPTransport);

            std::string localHostName (_localUDPTransport->local_name.host.ptr, _localUDPTransport->local_name.host.slen);

            _debug ("Use default one instead (%s:%i)", localHostName.c_str(), _localUDPTransport->local_name.port);

            // account->setLocalAddress(localHostName);
            account->setLocalPort (_localUDPTransport->local_name.port);

            // Transport could not either be created or found in the map, socket not available
            return false;
        }
    }
}


bool SIPVoIPLink::createDefaultSipUdpTransport()
{

    int errPjsip = 0;

    // Retrieve Direct IP Calls settings.
    SIPAccount * account = NULL;

    // Use IP2IP_PROFILE to init default udp transport settings
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));

    // Create a UDP listener meant for all accounts for which TLS was not enabled
    // Cannot acquireTransport since default UDP transport must be created regardless of TLS
    errPjsip = createUdpTransport (IP2IP_PROFILE);

    if (account && (errPjsip == PJ_SUCCESS)) {

        // Store transport in transport map
        addTransportToMap (account->getTransportMapKey(), account->getAccountTransport());

        // if account is not NULL, use IP2IP trasport as default one
        _localUDPTransport = account->getAccountTransport();

    }
    // If the above UDP server
    // could not be created, then give it another try
    // on a random sip port
    else if (errPjsip != PJ_SUCCESS) {
        _debug ("UserAgent: Could not initialize SIP listener on port %d", _regPort);
        _regPort = RANDOM_SIP_PORT;

        _debug ("UserAgent: Trying to initialize SIP listener on port %d", _regPort);
        // If no AccountID specified, pointer to transport is stored in _localUDPTransport
        errPjsip = createUdpTransport();

        if (errPjsip != PJ_SUCCESS) {
            _debug ("UserAgent: Fail to initialize SIP listener on port %d", _regPort);
            return false;
        }
    }

    return true;

}


void SIPVoIPLink::createDefaultSipTlsListener()
{

    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));

    if (account->isTlsEnabled()) {
        createTlsListener (IP2IP_PROFILE);
    }
}


void SIPVoIPLink::createTlsListener (const AccountID& accountID)
{

    pjsip_tpfactory *tls;
    pj_sockaddr_in local_addr;
    pjsip_host_port a_name;
    pj_status_t status;
    pj_status_t success;

    _debug ("Create TLS listener");

    /* Grab the tls settings, populated
     * from configuration file.
     */
    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accountID));

    if (account == NULL) {
        _debug ("UserAgent: Account is null while creating TLS default listener. Returning");
        // return !PJ_SUCCESS;
    }


    // Init local address for this listener to be bound (ADDR_ANY on port 5061).
    pj_sockaddr_in_init (&local_addr, 0, 0);
    pj_uint16_t localTlsPort = account->getTlsListenerPort();
    local_addr.sin_port = pj_htons (localTlsPort);

    pj_str_t pjAddress;
    pj_cstr (&pjAddress, PJ_INADDR_ANY);
    success = pj_sockaddr_in_set_str_addr (&local_addr, &pjAddress);


    // Init published address for this listener (Local IP address on port 5061)
    std::string publishedAddress;
    loadSIPLocalIP (&publishedAddress);

    pj_bzero (&a_name, sizeof (pjsip_host_port));
    pj_cstr (&a_name.host, publishedAddress.c_str());
    a_name.port = account->getTlsListenerPort();

    /* Get TLS settings. Expected to be filled */
    pjsip_tls_setting * tls_setting = account->getTlsSetting();


    _debug ("UserAgent: TLS transport to be initialized with published address %.*s,"
    " published port %d,\n                  local address %.*s, local port %d",
    (int) a_name.host.slen, a_name.host.ptr,
    (int) a_name.port, pjAddress.slen, pjAddress.ptr, (int) localTlsPort);


    status = pjsip_tls_transport_start (_endpt, tls_setting, &local_addr, &a_name, 1, &tls);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Error creating SIP TLS listener (%d)", status);
    } else {
        _localTlsListener = tls;
    }

    // return PJ_SUCCESS;

}


bool SIPVoIPLink::createSipTransport (AccountID id)
{

    SIPAccount* account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (!account)
        return false;

    pj_status_t status;

    if (account->isTlsEnabled()) {

        if (_localTlsListener == NULL)
            createTlsListener (id);

        // Parse remote address to establish connection
        std::string remoteSipUri = account->getServerUri();
        int sips = remoteSipUri.find ("<sips:") + 6;
        int trns = remoteSipUri.find (";transport");
        std::string remoteAddr = remoteSipUri.substr (sips, trns-sips);

        // Nothing to do, TLS listener already created at pjsip's startup and TLS connection
        // is automatically handled in pjsip when sending registration messages.
        if (createTlsTransport (id, remoteAddr) != PJ_SUCCESS)
            return false;

        return true;
    } else {

        // Launch a new UDP listener/transport, using the published address
        if (account->isStunEnabled ()) {

            status = createAlternateUdpTransport (id);

            if (status != PJ_SUCCESS) {
                _debug ("Failed to init UDP transport with STUN published address for account %s", id.c_str());
                return false;
            }

        } else {

            status = createUdpTransport (id);

            if (status != PJ_SUCCESS) {
                _debug ("Failed to initialize UDP transport for account %s", id.c_str());
                return false;
            } else {

                // If transport successfully created, store it in the internal map.
                // STUN aware transport are account specific and should not be stored in map.
                // TLS transport is ephemeral and is managed by PJSIP, should not be stored either.
                addTransportToMap (account->getTransportMapKey(), account->getAccountTransport());
            }
        }
    }

    return true;
}



bool SIPVoIPLink::addTransportToMap (std::string key, pjsip_transport* transport)
{

    SipTransportMap::iterator iter_transport;
    iter_transport = _transportMap.find (key);

    // old transport in transport map, erase it
    if (iter_transport != _transportMap.end()) {
        _transportMap.erase (iter_transport);
    }

    _debug ("UserAgent: Storing newly created transport in map using key %s", key.c_str());
    _transportMap.insert (std::pair<std::string, pjsip_transport*> (key, transport));

    return true;

}


int SIPVoIPLink::createUdpTransport (AccountID id)
{

    pj_status_t status;
    pj_sockaddr_in bound_addr;
    pjsip_host_port a_name;
    // char tmpIP[32];
    pjsip_transport *transport;
    std::string listeningAddress = "0.0.0.0";
    int listeningPort = _regPort;

    /* Use my local address as default value */
    if (!loadSIPLocalIP (&listeningAddress))
        return !PJ_SUCCESS;

    _debug ("UserAgent: Create UDP transport for account \"%s\"", id.c_str());

    /*
     * Retrieve the account information
     */
    SIPAccount * account = NULL;

    // if account id is not specified, init _localUDPTransport
    if (id != "") {

        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));
    }

    // Set information to the local address and port
    if (account == NULL) {

        _debug ("UserAgent: Account with id \"%s\" is null in createUdpTransport.", id.c_str());

    } else {

        // We are trying to initialize a UDP transport available for all local accounts and direct IP calls
        _debug ("UserAgent: found account %s in map", account->getAccountID().c_str());

        if (account->getLocalInterface () != "default") {
            listeningAddress = getInterfaceAddrFromName (account->getLocalInterface());
        }

        listeningPort = account->getLocalPort ();
    }

    pj_memset (&bound_addr, 0, sizeof (bound_addr));

    pj_str_t temporary_address;

    if (account && account->getLocalInterface () == "default") {

        // Init bound address to ANY
        bound_addr.sin_addr.s_addr = pj_htonl (PJ_INADDR_ANY);
        loadSIPLocalIP (&listeningAddress);
    } else {

        // bind this account to a specific interface
        pj_strdup2 (_pool, &temporary_address, listeningAddress.c_str());
        bound_addr.sin_addr = pj_inet_addr (&temporary_address);
    }

    bound_addr.sin_port = pj_htons ( (pj_uint16_t) listeningPort);
    bound_addr.sin_family = PJ_AF_INET;
    pj_bzero (bound_addr.sin_zero, sizeof (bound_addr.sin_zero));

    // Create UDP-Server (default port: 5060)
    // Use here either the local information or the published address
    if (account && !account->getPublishedSameasLocal ()) {

        // Set the listening address to the published address
        listeningAddress = account->getPublishedAddress ();

        // Set the listening port to the published port
        listeningPort = account->getPublishedPort ();
        _debug ("UserAgent: Creating UDP transport published %s:%i", listeningAddress.c_str (), listeningPort);

    }

    // We must specify this here to avoid the IP2IP_PROFILE
    // to create a transport with name 0.0.0.0 to appear in the via header
    if (id == IP2IP_PROFILE)
        loadSIPLocalIP (&listeningAddress);

    if (listeningAddress == "" || listeningPort == 0) {
        _error ("UserAgent: Error invalid address for new udp transport");
        return !PJ_SUCCESS;
    }

    /* Init published name */
    pj_bzero (&a_name, sizeof (pjsip_host_port));
    pj_cstr (&a_name.host, listeningAddress.c_str());
    a_name.port = listeningPort;

    status = pjsip_udp_transport_start (_endpt, &bound_addr, &a_name, 1, &transport);

    // Print info from transport manager associated to endpoint
    pjsip_tpmgr * tpmgr = pjsip_endpt_get_tpmgr (_endpt);
    pjsip_tpmgr_dump_transports (tpmgr);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: (%d) Unable to start UDP transport on %s:%d", status, listeningAddress.data(), listeningPort);
        return status;
    } else {
        _debug ("UserAgent: UDP transport initialized successfully on %s:%d", listeningAddress.c_str (), listeningPort);
        if (account == NULL) {
            _debug ("UserAgent: Use transport as local UDP server");
            _localUDPTransport = transport;
        } else {
            _debug ("UserAgent: bind transport to account %s", account->getAccountID().c_str());
            account->setAccountTransport (transport);
        }
    }

    return PJ_SUCCESS;
}

std::string SIPVoIPLink::findLocalAddressFromUri (const std::string& uri, pjsip_transport *transport)
{
    pj_str_t localAddress;
    pjsip_transport_type_e transportType;
    pjsip_tpselector *tp_sel;
    pj_pool_t *tmp_pool;

    _debug ("SIP: Find local address from URI");

    // Create a temporary memory pool
    tmp_pool = pj_pool_create (&_cp->factory, "tmpdtmf10", 1000, 1000, NULL);
    if (tmp_pool == NULL) {
    	_error ("UserAgent: Could not initialize memory pool");
    }

    // Find the transport that must be used with the given uri
    pj_str_t tmp;
    pj_strdup2_with_null (tmp_pool, &tmp, uri.c_str());
    pjsip_uri * genericUri = NULL;
    genericUri = pjsip_parse_uri (tmp_pool, tmp.ptr, tmp.slen, 0);

    pj_str_t pjMachineName;
    pj_strdup (tmp_pool, &pjMachineName, pj_gethostname());
    std::string machineName (pjMachineName.ptr, pjMachineName.slen);

    if (genericUri == NULL) {
        _warn ("SIP: generic URI is NULL in findLocalAddressFromUri");
        return machineName;
    }

    pjsip_sip_uri * sip_uri = NULL;

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri (genericUri);

    if (sip_uri == NULL) {
        _warn ("SIP: Invalid uri in findLocalAddressFromURI");
        return machineName;
    }

    if (PJSIP_URI_SCHEME_IS_SIPS (sip_uri)) {
        transportType = PJSIP_TRANSPORT_TLS;

    } else {
        if (transport == NULL) {
            _warn ("SIP: Transport is NULL in findLocalAddressFromUri. Try the local UDP transport");
            transport = _localUDPTransport;
        }

        transportType = PJSIP_TRANSPORT_UDP;
    }

    // Get the transport manager associated with
    // this endpoint
    pjsip_tpmgr * tpmgr = NULL;

    tpmgr = pjsip_endpt_get_tpmgr (_endpt);

    if (tpmgr == NULL) {
        _warn ("SIP: Unexpected: Cannot get tpmgr from endpoint.");
        return machineName;
    }

    // Find the local address (and port) based on the registered
    // transports and the transport type
    int port;

    pj_status_t status;

    /* Init the transport selector */

    //_debug ("Transport ID: %s", transport->obj_name);
    if (transportType == PJSIP_TRANSPORT_UDP) {
        status = initTransportSelector (transport, &tp_sel, tmp_pool);

        if (status == PJ_SUCCESS) {
            status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, tp_sel, &localAddress, &port);
        } else {
            status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, NULL, &localAddress, &port);
        }
    } else {
        status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, NULL, &localAddress, &port);
    }

    if (status != PJ_SUCCESS) {
        _debug ("SIP: Failed to find local address from transport");
        return machineName;
    }

    std::string localaddr (localAddress.ptr, localAddress.slen);

    if (localaddr == "0.0.0.0")
        loadSIPLocalIP (&localaddr);

    _debug ("SIP: Local address discovered from attached transport: %s", localaddr.c_str());

    pj_pool_release(tmp_pool);

    return localaddr;
}



pj_status_t SIPVoIPLink::initTransportSelector (pjsip_transport *transport, pjsip_tpselector **tp_sel, pj_pool_t *tp_pool)
{
    pjsip_tpselector *tp;

    if (transport != NULL) {
        tp = (pjsip_tpselector *) pj_pool_zalloc (tp_pool, sizeof (pjsip_tpselector));
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
    pj_pool_t *tmp_pool;

    _debug ("SIP: Find local port from URI");

    // Create a temporary memory pool
    tmp_pool = pj_pool_create (&_cp->factory, "tmpdtmf10", 1000, 1000, NULL);
    if (tmp_pool == NULL) {
    	_debug ("UserAgent: Could not initialize memory pool");
    	return false;
    }

    // Find the transport that must be used with the given uri
    pj_str_t tmp;
    pj_strdup2_with_null (tmp_pool, &tmp, uri.c_str());
    pjsip_uri * genericUri = NULL;
    genericUri = pjsip_parse_uri (tmp_pool, tmp.ptr, tmp.slen, 0);

    if (genericUri == NULL) {
        _debug ("UserAgent: genericUri is NULL in findLocalPortFromUri");
        return atoi (DEFAULT_SIP_PORT);
    }

    pjsip_sip_uri * sip_uri = NULL;

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri (genericUri);

    if (sip_uri == NULL) {
        _debug ("UserAgent: Invalid uri in findLocalAddressFromTransport");
        return atoi (DEFAULT_SIP_PORT);
    }

    if (PJSIP_URI_SCHEME_IS_SIPS (sip_uri)) {
        transportType = PJSIP_TRANSPORT_TLS;
        port = atoi (DEFAULT_SIP_TLS_PORT);
    } else {
        if (transport == NULL) {
            _debug ("UserAgent: transport is NULL in findLocalPortFromUri - Try the local UDP transport");
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
        _debug ("UserAgent: unexpected, cannot get tpmgr from endpoint.");
        return port;
    }

    // Find the local address (and port) based on the registered
    // transports and the transport type

    /* Init the transport selector */
    pj_status_t status;

    if (transportType == PJSIP_TRANSPORT_UDP) {
        _debug ("UserAgent: transport ID: %s", transport->obj_name);

        status = initTransportSelector (transport, &tp_sel, tmp_pool);

        if (status == PJ_SUCCESS)
            status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, tp_sel, &localAddress, &port);
        else
            status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, NULL, &localAddress, &port);
    } else
        status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, NULL, &localAddress, &port);


    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: failed to find local address from transport");
    }

    _debug ("UserAgent: local port discovered from attached transport: %i", port);

    pj_pool_release(tmp_pool);

    return port;
}


pj_status_t SIPVoIPLink::createTlsTransport (const AccountID& accountID, std::string remoteAddr)
{
    pj_status_t success;

    _debug ("Create TLS transport for account %s\n", accountID.c_str());

    // Retrieve the account information
    SIPAccount * account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accountID));

    if (!account) {
        _debug ("UserAgent: Account is NULL when creating TLS connection, returning");
        return !PJ_SUCCESS;
    }

    pj_sockaddr_in rem_addr;
    pj_str_t remote;

    pj_cstr (&remote, remoteAddr.c_str());

    pj_sockaddr_in_init (&rem_addr, &remote, (pj_uint16_t) 5061);

    // Update TLS settings for account registration using the default listeners
    // Pjsip does not allow to create multiple listener
    // pjsip_tpmgr *mgr = pjsip_endpt_get_tpmgr(_endpt);
    // pjsip_tls_listener_update_settings(_endpt, _pool, mgr, _localTlsListener, account->getTlsSetting());

    // Create a new TLS connection from TLS listener
    pjsip_transport *tls;
    success = pjsip_endpt_acquire_transport (_endpt, PJSIP_TRANSPORT_TLS, &rem_addr, sizeof (rem_addr), NULL, &tls);

    if (success != PJ_SUCCESS)
        _debug ("UserAgent: Error could not create TLS transport");
    else
        account->setAccountTransport (tls);

    return success;
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

    _debug ("UserAgent: Create Alternate UDP transport");

    /*
     * Retrieve the account information
     */
    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    if (account == NULL) {
        _error ("UserAgent: Error: Account is null. Returning");
        return !PJ_SUCCESS;
    }

    stunServer = account->getStunServerName ();

    stunPort = account->getStunPort ();

    status = stunServerResolve (id);

    if (status != PJ_SUCCESS) {
        _error ("UserAgent: Error: Resolving STUN server: %i", status);
        return status;
    }

    // Init socket
    sock = PJ_INVALID_SOCKET;

    _debug ("UserAgent: Initializing IPv4 socket on %s:%i", stunServer.ptr, stunPort);

    status = pj_sockaddr_in_init (&boundAddr, &stunServer, 0);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Error: Initializing IPv4 socket on %s:%i", stunServer.ptr, stunPort);
        return status;
    }

    // Create and bind the socket
    status = pj_sock_socket (pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Error: Unable to create or bind socket (%d)", status);
        return status;
    }

    // Query the mapped IP address and port on the 'outside' of the NAT
    status = pjstun_get_mapped_addr (&_cp->factory, 1, &sock, &stunServer, stunPort, &stunServer, stunPort, &pub_addr);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgwent: Error: Contacting STUN server (%d)", status);
        pj_sock_close (sock);
        return status;
    }

    _debug ("UserAgent: Firewall address : %s:%d", pj_inet_ntoa (pub_addr.sin_addr), pj_ntohs (pub_addr.sin_port));

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
        _debug ("UserAgent: Error: Creating alternate SIP UDP listener (%d)", status);
        return status;
    }

    _debug ("UserAgent: UDP Transport successfully created on %s:%i", listeningAddress.c_str (), listeningPort);

    account->setAccountTransport (transport);

    if (transport) {

        _debug ("UserAgent: Initial ref count: %s %s (refcnt=%i)", transport->obj_name, transport->info,
        (int) pj_atomic_get (transport->ref_cnt));

        pj_sockaddr *addr = (pj_sockaddr*) & (transport->key.rem_addr);

        static char str[PJ_INET6_ADDRSTRLEN];
        pj_inet_ntop ( ( (const pj_sockaddr*) addr)->addr.sa_family, pj_sockaddr_get_addr (addr), str, sizeof (str));


        _debug ("UserAgent: KEY: %s:%d",str, pj_sockaddr_get_port ( (const pj_sockaddr*) & (transport->key.rem_addr)));

    }

    pjsip_tpmgr * tpmgr = pjsip_endpt_get_tpmgr (_endpt);

    pjsip_tpmgr_dump_transports (tpmgr);

    return PJ_SUCCESS;
}


void SIPVoIPLink::shutdownSipTransport (const AccountID& accountID)
{

    _debug ("UserAgent: Shutdown Sip Transport");

    SIPAccount* account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accountID));

    if (!account)
        return;

    if (account->getAccountTransport()) {

        _debug ("Transport bound to account, decrease ref count");

        // decrease reference count added by pjsip_regc_send
        // PJSIP's IDLE timer is set if counter reach 0

        // there is still problems when account registration fails, so comment it for now
        // status = pjsip_transport_dec_ref(account->getAccountTransport());

        // detach transport from this account
        account->setAccountTransport (NULL);

    }

}

std::string SIPVoIPLink::parseDisplayName(char * buffer)
{
    // Parse the display name from "From" header
    char* from_header = strstr (buffer, "From: ");

    std::string displayName;

    if (from_header) {
        std::string temp (from_header);
        int begin_displayName = temp.find ("\"") + 1;
        int end_displayName = temp.rfind ("\"");
        displayName = temp.substr (begin_displayName, end_displayName - begin_displayName);

        if (displayName.size() > 25) {
            displayName = std::string ("");
        }
    } else {
        displayName = std::string ("");
    }

    return displayName;
}

void SIPVoIPLink::stripSipUriPrefix(std::string& sipUri)
{
    //Remove sip: prefix
    size_t found = sipUri.find ("sip:");

    if (found!=std::string::npos)
    	sipUri.erase (found, found+4);

    found = sipUri.find ("@");

    if (found!=std::string::npos)
    	sipUri.erase (found);
}

bool SIPVoIPLink::loadSIPLocalIP (std::string *addr)
{

    bool returnValue = true;
    std::string localAddress = "127.0.0.1";
    pj_sockaddr ip_addr;

    if (pj_gethostip (pj_AF_INET(), &ip_addr) != PJ_SUCCESS) {
        // Update the registration state if no network capabilities found
        _debug ("UserAgent: Get host ip failed!");
        returnValue = false;
    } else {
        localAddress = std::string (pj_inet_ntoa (ip_addr.ipv4.sin_addr));
        _debug ("UserAgent: Checking network, local IP address: %s", localAddress.data());
    }

    *addr = localAddress;

    return returnValue;
}

pjsip_route_hdr *SIPVoIPLink::createRouteSet(Account *account, pj_pool_t *hdr_pool)
{
	size_t found;
	std::string host = "";
	std::string port = "";
	pjsip_route_hdr *route_set;

	_debug ("SIP: Find local port from URI");

	SIPAccount *sipaccount = dynamic_cast<SIPAccount *>(account);
    std::string route = sipaccount->getServiceRoute();
    _error ("UserAgent: Set Service-Route with %s", route.c_str());

    found = route.find(":");
	if(found != std::string::npos) {
		host = route.substr(0, found);
		port = route.substr(found + 1, route.length());
	}
	else {
		host = route;
		port = "0";
	}

	std::cout << "Host: " << host << ", Port: " << port << std::endl;

    route_set = pjsip_route_hdr_create (hdr_pool);
    pjsip_route_hdr *routing = pjsip_route_hdr_create (hdr_pool);
    pjsip_sip_uri *url = pjsip_sip_uri_create (hdr_pool, 0);
    routing->name_addr.uri = (pjsip_uri*) url;
    pj_strdup2 (hdr_pool, &url->host, host.c_str());
    url->port = atoi(port.c_str());

    pj_list_push_back (route_set, pjsip_hdr_clone (hdr_pool, routing));

    return route_set;

}

bool SIPVoIPLink::dnsResolution(pjsip_tx_data *tdata) {
	pj_addrinfo ai;
	unsigned count;
	// pjsip_server_addresses svr_addr;
	pj_status_t status;
	pjsip_host_info dest_info;

	_debug("UserAgent: Dns Resolution");

	int af = pj_AF_INET();

    status = pjsip_process_route_set(tdata, &dest_info);

	pj_sockaddr_init(pj_AF_INET(), &tdata->dest_info.addr.entry[0].addr, NULL, 0);

	/* Resolve */
	count = 1;
	if((status = pj_getaddrinfo(af, &dest_info.addr.host, &count, &ai)) != PJ_SUCCESS) {
		_error("UserAgent: Unable to perform DNS resolution");
	}

	_debug("UserAgent: Found address %s", pj_inet_ntoa (ai.ai_addr.ipv4.sin_addr));

	tdata->dest_info.addr.entry[0].addr.addr.sa_family = (pj_uint16_t)af;
	pj_memcpy(&tdata->dest_info.addr.entry[0].addr, &ai.ai_addr, sizeof(pj_sockaddr));

	tdata->dest_info.addr.count = count;

	return true;
}

void SIPVoIPLink::busySleep (unsigned msec)
{

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

bool SIPVoIPLink::pjsipShutdown (void)
{
    if (_endpt) {
        _debug ("UserAgent: Shutting down...");
        busySleep (1000);
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
        pj_caching_pool_destroy (_cp);
    }

    /* Shutdown PJLIB */
    pj_shutdown();

    _debug ("UserAgent: Shutted down successfully");

    /* Done. */
    return true;
}

int getModId()
{
    return _mod_ua.id;
}

static void dns_cb (pj_status_t status, void *token, const struct pjsip_server_addresses *addr)
{

	_debug("UserAgent: DNS callback");

    struct result * result = (struct result*) token;

    result->status = status;

    if (status == PJ_SUCCESS) {
        pj_memcpy (&result->servers, addr, sizeof (*addr));
    }
}

void setVoicemailInfo (AccountID account, pjsip_msg_body *body)
{

    int voicemail = 0, pos_begin, pos_end;
    std::string voice_str = "Voice-Message: ";
    std::string delimiter = "/";
    std::string msg_body, voicemail_str;

    _debug ("UserAgent: checking the voice message!");
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

void SIPVoIPLink::SIPHandleReinvite (SIPCall *call UNUSED)
{
    _debug ("UserAgent: Handle reinvite");
}

// This callback is called when the invite session state has changed
void invite_session_state_changed_cb (pjsip_inv_session *inv, pjsip_event *e)
{
    _debug ("UserAgent: Call state changed to %s", invitationStateMap[inv->state]);

    /* Retrieve the call information */
    SIPCall *call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);
    if (call == NULL) {
        return;
    }

    //Retrieve the body message
    pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;

    // If the call is a direct IP-to-IP call
    SIPVoIPLink * link = NULL;
    if (call->getCallConfiguration () == Call::IPtoIP) {
        link = SIPVoIPLink::instance ("");
    } else {
        AccountID accId = Manager::instance().getAccountFromCall (call->getCallId());
        link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));
    }

    if (link == NULL) {
        _error ("UserAgent: Error: Link is NULL in call state changed callback");
        return;
    }

    // If this is an outgoing INVITE that was created because of
    // REFER/transfer, send NOTIFY to transferer.
    if (call->getXferSub() && e->type==PJSIP_EVENT_TSX_STATE) {

    	_debug("UserAgent: Call state changed during transfer");

        int st_code = -1;
        pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;

        switch (call->getInvSession()->state) {

            case PJSIP_INV_STATE_NULL:
            	_debug("PJSIP_INV_STATE_NULL");
            	break;
            case PJSIP_INV_STATE_CALLING:
            	_debug("\n");
                /* Do nothing */
            	_debug("PJSIP_INV_STATE_CALLING");
                break;
            case PJSIP_INV_STATE_EARLY:
            case PJSIP_INV_STATE_CONNECTING:
                st_code = e->body.tsx_state.tsx->status_code;
                ev_state = PJSIP_EVSUB_STATE_ACTIVE;
                _debug("PJSIP_INV_STATE_EARLY, PJSIP_INV_STATE_CONNECTING");
                break;
            case PJSIP_INV_STATE_CONFIRMED:
                /* When state is confirmed, send the final 200/OK and terminate
                 * subscription.
                 */
                st_code = e->body.tsx_state.tsx->status_code;
                ev_state = PJSIP_EVSUB_STATE_TERMINATED;
                _debug("PJSIP_INV_STATE_CONFIRMED");
                break;

            case PJSIP_INV_STATE_DISCONNECTED:
                st_code = e->body.tsx_state.tsx->status_code;
                ev_state = PJSIP_EVSUB_STATE_TERMINATED;
                _debug("PJSIP_EVSUB_STATE_TERMINATED");
                break;

            case PJSIP_INV_STATE_INCOMING:
                /* Nothing to do. Just to keep gcc from complaining about
                 * unused enums.
                 */
            	_debug("PJSIP_INV_STATE_INCOMING");
                break;
        }

        if (st_code != -1) {
            pjsip_tx_data *tdata;
            pj_status_t status;

            status = pjsip_xfer_notify (call->getXferSub(), ev_state, st_code, NULL, &tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Unable to create NOTIFY -- %d", status);
            } else {
                status = pjsip_xfer_send_request (call->getXferSub(), tdata);

                if (status != PJ_SUCCESS) {
                    _debug ("UserAgent: Unable to send NOTIFY -- %d", status);
                }
            }
        }

        return;
    }

    if (inv->state != PJSIP_INV_STATE_CONFIRMED) {
        // Update UI with the current status code and description
        pjsip_transaction * tsx = NULL;
        tsx = e->body.tsx_state.tsx;
        int statusCode = 404;

        if (tsx != NULL) {
            statusCode = tsx->status_code;
        }

        const pj_str_t * description = pjsip_get_status_text (statusCode);
        if (statusCode) {
            // test wether or not dbus manager is instantiated, if not no need to notify the client
            if (Manager::instance().getDbusManager())
                DBusManager::instance().getCallManager()->sipCallStateChanged (call->getCallId(), std::string (description->ptr, description->slen), statusCode);
        }
    }

    if (inv->state == PJSIP_INV_STATE_EARLY && e->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
    	// The call is ringing - We need to handle this case only on outgoing call
        call->setConnectionState (Call::Ringing);
        Manager::instance().peerRingingCall (call->getCallId());
    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
    	// After we sent or received a ACK - The connection is established
        link->SIPCallAnswered (call, rdata);
    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

        _debug ("UserAgent: State: %s. Cause: %.*s", invitationStateMap[inv->state], (int) inv->cause_text.slen, inv->cause_text.ptr);

        AccountID accId = Manager::instance().getAccountFromCall (call->getCallId());
        if((link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId))) == NULL)
        	return;

        switch (inv->cause) {
            // The call terminates normally - BYE / CANCEL
            case PJSIP_SC_OK:
            case PJSIP_SC_REQUEST_TERMINATED:
                link->SIPCallClosed (call);
                break;
            case PJSIP_SC_DECLINE:
                _debug ("UserAgent: Call %s is declined", call->getCallId().c_str());
                if (inv->role == PJSIP_ROLE_UAC)
                    link->SIPCallServerFailure (call);
                break;
            case PJSIP_SC_NOT_FOUND:            /* peer not found */
            case PJSIP_SC_REQUEST_TIMEOUT:      /* request timeout */
            case PJSIP_SC_NOT_ACCEPTABLE_HERE:  /* no compatible codecs */
            case PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE:
            case PJSIP_SC_UNSUPPORTED_MEDIA_TYPE:
            case PJSIP_SC_UNAUTHORIZED:
            case PJSIP_SC_FORBIDDEN:
            case PJSIP_SC_REQUEST_PENDING:
            case PJSIP_SC_ADDRESS_INCOMPLETE:
                link->SIPCallServerFailure (call);
                break;
            default:
                link->SIPCallServerFailure (call);
                _error ("UserAgent: Unhandled call state. This is probably a bug.");
                break;
        }
    }

}

void sdp_request_offer_cb (pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{
    _info ("UserAgent: Received SDP offer");


#ifdef CAN_REINVITE
    _debug ("UserAgent: %s (%d): on_rx_offer REINVITE", __FILE__, __LINE__);

    SIPCall *call;
    pj_status_t status;
    AccountID accId;
    SIPVoIPLink *link;

    call = (SIPCall*) inv->mod_data[getModId() ];

    if (!call) {
        return;
    }

    accId = Manager::instance().getAccountFromCall (call->getCallId());

    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));

    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accId));

    status = call->getLocalSDP()->receiveOffer (offer, account->getActiveCodecs ());
    call->getLocalSDP()->startNegotiation();

    status = pjsip_inv_set_sdp_answer (call->getInvSession(), call->getLocalSDP()->getLocalSdpSession());

    if (link) {
        link->SIPHandleReinvite (call);
    }
#endif

}

void sdp_create_offer_cb (pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{
    _info ("UserAgent: Create new SDP offer");

    /* Retrieve the call information */
    SIPCall * call = NULL;
    call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);

    CallID callid = call->getCallId();
    AccountID accountid = Manager::instance().getAccountFromCall (callid);

    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accountid));

    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accountid));

    // Set the local address
    std::string localAddress = link->getInterfaceAddrFromName (account->getLocalInterface ());
    // Set SDP parameters - Set to local
    std::string addrSdp = localAddress;

    _debug ("UserAgent: Local Address for IP2IP call: %s", localAddress.c_str());

    // If local address bound to ANY, reslove it using PJSIP
    if (localAddress == "0.0.0.0") {
        link->loadSIPLocalIP (&localAddress);
    }

    // Local address to appear in SDP
    if (addrSdp == "0.0.0.0") {
        addrSdp = localAddress;
    }

    // Set local address for RTP media
    setCallMediaLocal (call, localAddress);

    // Building the local SDP offer
    call->getLocalSDP()->setLocalIP (addrSdp);
    call->getLocalSDP()->createOffer (account->getActiveCodecs());

    *p_offer = call->getLocalSDP()->getLocalSdpSession();

}

// This callback is called after SDP offer/answer session has completed.
void sdp_media_update_cb (pjsip_inv_session *inv, pj_status_t status)
{
    _debug ("UserAgent: Call media update");

    const pjmedia_sdp_session *remote_sdp;
    const pjmedia_sdp_session *local_sdp;
    SIPVoIPLink * link = NULL;
    SIPCall * call;
    char buffer[1000];

    call = reinterpret_cast<SIPCall *> (inv->mod_data[getModId()]);
    if (call == NULL) {
        _debug ("UserAgent: Call declined by peer, SDP negotiation stopped");
        return;
    }

    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (AccountNULL));
    if (link == NULL) {
        _warn ("UserAgent: Error: Failed to get sip link");
        return;
    }

    if (status != PJ_SUCCESS) {
        _warn ("UserAgent: Error: while negotiating the offer");
        link->hangup (call->getCallId());
        Manager::instance().callFailure (call->getCallId());
        return;
    }

    if (!inv->neg) {
    	_warn ("UserAgent: Error: no negotiator for this session");
        return;
    }

    // Retreive SDP session for this call
    Sdp *sdpSession = call->getLocalSDP();

    // Get active session sessions
    pjmedia_sdp_neg_get_active_remote (inv->neg, &remote_sdp);
    pjmedia_sdp_neg_get_active_local (inv->neg, &local_sdp);

    // Print SDP session
	memset(buffer, 0, 1000);
	pjmedia_sdp_print(remote_sdp, buffer, 1000);
	_debug("SDP: Remote active SDP Session:\n%s", buffer);

	memset(buffer, 0, 1000);
	pjmedia_sdp_print(local_sdp, buffer, 1000);
	_debug("SDP: Local active SDP Session:\n%s", buffer);

	// Set active SDP sessions
    sdpSession->setActiveRemoteSdpSession(remote_sdp);
    sdpSession->setActiveLocalSdpSession(local_sdp);

    // Update internal field for
    sdpSession->updateInternalState();

    try {
        call->getAudioRtp()->updateDestinationIpAddress();
        call->getAudioRtp()->setDtmfPayloadType(sdpSession->getTelephoneEventType());
    } catch (...) {

    }

    // Get the crypto attribute containing srtp's cryptographic context (keys, cipher)
    CryptoOffer crypto_offer;
    call->getLocalSDP()->getRemoteSdpCryptoFromOffer (remote_sdp, crypto_offer);

    bool nego_success = false;

    if (!crypto_offer.empty()) {

        _debug ("UserAgent: Crypto attribute in SDP, init SRTP session");

        // init local cryptografic capabilities for negotiation
        std::vector<sfl::CryptoSuiteDefinition>localCapabilities;

        for (int i = 0; i < 3; i++) {
            localCapabilities.push_back (sfl::CryptoSuites[i]);
        }

        // Mkae sure incoming crypto offer is valid
        sfl::SdesNegotiator sdesnego (localCapabilities, crypto_offer);

        if (sdesnego.negotiate()) {
            _debug ("UserAgent: SDES negotiation successfull");
            nego_success = true;

            _debug ("UserAgent: Set remote cryptographic context");

            try {
                call->getAudioRtp()->setRemoteCryptoInfo (sdesnego);
            } catch (...) {}

            DBusManager::instance().getCallManager()->secureSdesOn (call->getCallId());
        } else {
            DBusManager::instance().getCallManager()->secureSdesOff (call->getCallId());
        }
    }


    // We did not found any crypto context for this media, RTP fallback
    if (!nego_success && call->getAudioRtp()->getAudioRtpType() == sfl::Sdes) {

        // We did not found any crypto context for this media
        // @TODO if SRTPONLY, CallFail

        _debug ("UserAgent: Did not found any crypto or negotiation failed but Sdes enabled");
        call->getAudioRtp()->stop();
        call->getAudioRtp()->setSrtpEnabled (false);

        // if RTPFALLBACK, change RTP session
        AccountID accountID = Manager::instance().getAccountFromCall (call->getCallId());
        SIPAccount *account = (SIPAccount *) Manager::instance().getAccount (accountID);

        if (account->getSrtpFallback())
            call->getAudioRtp()->initAudioRtpSession (call);
    }

    if (!sdpSession)
        return;

    sfl::AudioCodec *sessionMedia = sdpSession->getSessionMedia();

    if (!sessionMedia)
        return;

    AudioCodecType pl = (AudioCodecType) sessionMedia->getPayloadType();

    try {
        Manager::instance().audioLayerMutexLock();
        Manager::instance().getAudioDriver()->startStream();
        Manager::instance().audioLayerMutexUnlock();

        // udate session media only if required
        if (pl != call->getAudioRtp()->getSessionMedia()) {
            sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (pl);

            if (audiocodec == NULL)
                _error ("UserAgent: No audiocodec found");

            call->getAudioRtp()->updateSessionMedia (static_cast<sfl::AudioCodec *>(audiocodec));
        }
    }  // FIXME: should this really be std::exception? If so, it should be caught last
    catch (const SdpException &e) {
        _error("UserAgent: Exception: %s", e.what());
    }
    catch (const std::exception& rtpException) {
        _error ("UserAgent: Exception: %s", rtpException.what());
    } 

}

void outgoing_request_forked_cb (pjsip_inv_session *inv UNUSED, pjsip_event *e UNUSED)
{
}

void transaction_state_changed_cb (pjsip_inv_session *inv UNUSED, pjsip_transaction *tsx, pjsip_event *e)
{
    assert (tsx);

    pjsip_rx_data* r_data;
    pjsip_tx_data* t_data;

    _debug ("UserAgent: Transaction changed to state %s", transactionStateMap[tsx->state]);


    if (tsx->role==PJSIP_ROLE_UAS && tsx->state==PJSIP_TSX_STATE_TRYING &&
        pjsip_method_cmp (&tsx->method, &pjsip_refer_method) ==0) {
        /** Handle the refer method **/
        onCallTransfered (inv, e->body.tsx_state.src.rdata);

    } else if (tsx->role==PJSIP_ROLE_UAS && tsx->state==PJSIP_TSX_STATE_TRYING) {

        if (e && e->body.rx_msg.rdata) {

            r_data = e->body.rx_msg.rdata;

            if (r_data && r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {

                std::string method_info = "INFO";
                std::string method_notify = "NOTIFY";

                std::string request =  pjsip_rx_data_get_info (r_data);

                _debug ("UserAgent: %s", request.c_str());

                if (request.find (method_notify) != std::string::npos) {
  			// Attempt to to get feedback of call transfer status
//                	std::string contentType(e->body.rx_msg.rdata.msg_info.ctype.media.type->ptr,
//                			e->body.rx_msg.rdata.msg_info.ctype.media.type->slen);
//                	_debug("OK: %s", contentType.c_str());
                }
                // Must reply 200 OK on SIP INFO request
                else if (request.find (method_info) != std::string::npos) {
                    pjsip_dlg_create_response (inv->dlg, r_data, PJSIP_SC_OK, NULL, &t_data);
                    pjsip_dlg_send_response (inv->dlg, tsx, t_data);
		    return;
                }
            }
        }

        // Incoming TEXT message
        if (e && e->body.tsx_state.src.rdata) {

            // sender of this message
            std::string from;

            // Get the message inside the transaction
            r_data = e->body.tsx_state.src.rdata;
            std::string formatedMessage = (char*) r_data->msg_info.msg->body->data;

            // Try to determine who is the recipient of the message
            SIPCall *call = reinterpret_cast<SIPCall *> (inv->mod_data[getModId() ]);

            if (!call) {
                _debug ("Incoming TEXT message: Can't find the recipient of the message");
                return;
            }

            // Respond with a 200/OK
            pjsip_dlg_create_response (inv->dlg, r_data, PJSIP_SC_OK, NULL, &t_data);
            pjsip_dlg_send_response (inv->dlg, tsx, t_data);

            std::string message;
            std::string urilist;
            sfl::InstantMessaging::UriList list;

            sfl::InstantMessaging *module = Manager::instance().getInstantMessageModule();

            try {
                // retrive message from formated text
                message = module->findTextMessage (formatedMessage);

                // retreive the recipient-list of this message
                urilist = module->findTextUriList (formatedMessage);

                // parse the recipient list xml
                list = module->parseXmlUriList (urilist);

                // If no item present in the list, peer is considered as the sender
                if (list.empty()) {
                    from = call->getPeerNumber ();
                } else {
                    sfl::InstantMessaging::UriEntry entry = list.front();
                    sfl::InstantMessaging::UriEntry::iterator iterAttr = entry.find (IM_XML_URI);

                    if (iterAttr->second != "Me")
                        from = iterAttr->second;
                    else
                        from = call->getPeerNumber ();
                }

            } catch (sfl::InstantMessageException &e) {
                _error ("SipVoipLink: %s", e.what());
                message = "";
                from = call->getPeerNumber ();
                return;
            }


            // strip < and > characters in case of an IP address
            std::string stripped;

            if (from[0] == '<' && from[from.size()-1] == '>')
                stripped = from.substr (1, from.size()-2);
            else
                stripped = from;

            // Pass through the instant messaging module if needed
            // Right now, it does do anything.
            // And notify the clients

            Manager::instance ().incomingMessage (call->getCallId (), stripped, module->receive (message, stripped, call->getCallId ()));
        }


    }
}

void registration_cb (struct pjsip_regc_cbparam *param)
{
	AccountID *accountid = static_cast<AccountID *>(param->token);
    SIPAccount * account = static_cast<SIPAccount *> (Manager::instance().getAccount(*accountid));

    if (account == NULL) {
        _debug ("Account is NULL in registration_cb.");
        return;
    }

    assert (param);

    const pj_str_t * description = pjsip_get_status_text (param->code);

    if (param->code && description) {

        //std::string descriptionprint(description->ptr, description->slen);
        //_debug("Received client registration callback wiht code: %i, %s\n", param->code, descriptionprint.c_str());
        DBusManager::instance().getCallManager()->registrationStateChanged (account->getAccountID(), std::string (description->ptr, description->slen), param->code);
        std::pair<int, std::string> details (param->code, std::string (description->ptr, description->slen));


        // TODO: there id a race condition for this ressource when closing the application
        if (account)
            account->setRegistrationStateDetailed (details);
    }

    if (param->status == PJ_SUCCESS) {
        if (param->code < 0 || param->code >= 300) {
            /* Sometimes, the status is OK, but we still failed.
             * So checking the code for real result
             */
            _debug ("UserAgent: The error is: %d", param->code);

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

                case 423: { // Expiration Interval Too Brief

                    int expire_value;
                    std::istringstream stream (account->getRegistrationExpire());
                    stream >> expire_value;

                    std::stringstream out;
                    out << (expire_value * 2);
                    std::string s = out.str();

                    account->setRegistrationExpire (s);
                    account->registerVoIPLink();
                }
                break;

                default:
                    account->setRegistrationState (Error);
                    break;
            }

            account->setRegister (false);

            // shutdown this transport since useless
            // if(account->getAccountTransport() != _localUDPTransport) {

            SIPVoIPLink::instance ("")->shutdownSipTransport (account->getAccountID());
            //}

        } else {
            // Registration/Unregistration is success
            if (account->isRegister())
                account->setRegistrationState (Registered);
            else {
                account->setRegistrationState (Unregistered);
                account->setRegister (false);

                SIPVoIPLink::instance ("")->shutdownSipTransport (account->getAccountID());

                // pjsip_regc_destroy(param->regc);
                // account->setRegistrationInfo(NULL);
            }
        }
    } else {
        account->setRegistrationState (ErrorAuth);
        account->setRegister (false);

        SIPVoIPLink::instance ("")->shutdownSipTransport (account->getAccountID());
    }

}

// Optional function to be called to process incoming request message.
pj_bool_t
transaction_request_cb (pjsip_rx_data *rdata)
{
    pj_status_t status;
    unsigned options = 0;
    pjsip_dialog* dialog, *replaced_dlg;
    pjsip_tx_data *tdata;
    pjsip_tx_data *response;
    SIPVoIPLink *link;
    CallID id;
    SIPCall* call;
    pjsip_inv_session *inv;
    pjmedia_sdp_session *r_sdp;

    _info ("UserAgent: Transaction REQUEST received using transport: %s %s (refcnt=%d)",
    						rdata->tp_info.transport->obj_name,
    						rdata->tp_info.transport->info,
    						(int) pj_atomic_get (rdata->tp_info.transport->ref_cnt));

    // No need to go any further on incoming ACK
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg (rdata) != NULL) {
        _info ("UserAgent: received an ACK");
        return true;
    }

    /* First, let's got the username and server name from the invite.
     * We will use them to detect which account is the callee.
     */
    pjsip_uri *uri = rdata->msg_info.to->uri;
    pjsip_sip_uri *sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (uri);

    std::string userName = std::string (sip_uri->user.ptr, sip_uri->user.slen);
    std::string server = std::string (sip_uri->host.ptr, sip_uri->host.slen);
    _debug ("UserAgent: The receiver is: %s@%s", userName.data(), server.data());

    // Get the account id of callee from username and server
    AccountID account_id = Manager::instance().getAccountIdFromNameAndServer (userName, server);
    _debug ("UserAgent: Account ID for this call, %s", account_id.c_str());

    /* If we don't find any account to receive the call */
    if (account_id == AccountNULL) {
        _debug ("UserAgent: Username %s doesn't match any account, using IP2IP!",userName.c_str());
    }

    /* Get the voip link associated to the incoming call */
    /* The account must before have been associated to the call in ManagerImpl */
    if((link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (account_id))) == NULL) {
        _warn ("UserAgent: Error: cannot retrieve the voiplink from the account ID...");
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR,
        							   NULL, NULL, NULL);
        return false;
    }

    // retrive display name from the message buffer
    std::string displayName = SIPVoIPLink::parseDisplayName(rdata->msg_info.msg_buf);
    _debug("UserAgent: Display name for this call %s", displayName.c_str());

    /* Now, it is the time to find the information of the caller */
    uri = rdata->msg_info.from->uri;
    sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (uri);

    // Store the peer number
    char tmp[PJSIP_MAX_URL_SIZE];
    int length = pjsip_uri_print (PJSIP_URI_IN_FROMTO_HDR, sip_uri, tmp, PJSIP_MAX_URL_SIZE);
    std::string peerNumber (tmp, length);

    //Remove sip: prefix
    SIPVoIPLink::stripSipUriPrefix(peerNumber);
    _debug ("UserAgent: Peer number: %s", peerNumber.c_str());

    // Get the server voicemail notification
    // Catch the NOTIFY message
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {

        std::string method_name = "NOTIFY";

        // Retrieve all the message. Should contains only the method name but ...
        std::string request =  rdata->msg_info.msg->line.req.method.name.ptr;

        // Check if the message is a notification
        if (request.find (method_name) != (size_t)-1) {
            /* Notify the right account */
            setVoicemailInfo (account_id, rdata->msg_info.msg->body);
            request.find (method_name);
        }

        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_OK, NULL, NULL, NULL);

        return true;
    }

    // Handle an OPTIONS message
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_OPTIONS_METHOD) {
        handleIncomingOptions (rdata);
        return true;
    }

    // Respond statelessly any non-INVITE requests with 500
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
        if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
            pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED,
            NULL, NULL, NULL);
            return true;
        }
    }

    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));

    getRemoteSdpFromOffer (rdata, &r_sdp);

    if (account->getActiveCodecs().empty()) {
        _warn ("UserAgent: Error: No active codec");
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_NOT_ACCEPTABLE_HERE ,
        											  NULL, NULL, NULL);
        return false;
    }

    // Verify that we can handle the request
    status = pjsip_inv_verify_request (rdata, &options, NULL, NULL, _endpt, NULL);
    if (status != PJ_SUCCESS) {
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED,
        NULL, NULL, NULL);
        return true;
    }

    /******************************************* URL HOOK *********************************************/

    if (Manager::instance().hookPreference.getSipEnabled()) {

        _debug ("UserAgent: Set sip url hooks");

        std::string header_value;

        header_value = fetchHeaderValue (rdata->msg_info.msg,
        Manager::instance().hookPreference.getUrlSipField());

        if (header_value.size () < header_value.max_size()) {
            if (header_value!="") {
                urlhook->addAction (header_value,
                Manager::instance().hookPreference.getUrlCommand());
            }
        } else
            throw std::length_error ("UserAgent: Url exceeds std::string max_size");

    }

    /************************************************************************************************/

    _info ("UserAgent: Create a new call");

    // Generate a new call ID for the incoming call!
    id = Manager::instance().getNewCallID();

    if((call = new SIPCall (id, Call::Incoming, _cp)) == NULL) {
        _warn ("UserAgent: Error: Unable to create an incoming call");
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR,
        NULL, NULL, NULL);
        return false;
    }

    Manager::instance().associateCallToAccount (call->getCallId(), account_id);

    std::string addrToUse, addrSdp ="0.0.0.0";

    pjsip_tpselector *tp;

    if (account != NULL) {

        // May use the published address as well

        addrToUse = SIPVoIPLink::instance ("")->getInterfaceAddrFromName (account->getLocalInterface ());
        account->isStunEnabled () ? addrSdp = account->getPublishedAddress () : addrSdp = addrToUse;
        // Set the appropriate transport to have the right VIA header
        link->initTransportSelector (account->getAccountTransport (), &tp, call->getMemoryPool());

        if (account->getAccountTransport()) {

            _debug ("UserAgent: SIP transport for this account: %s %s (refcnt=%i)",
            account->getAccountTransport()->obj_name,
            account->getAccountTransport()->info,
            (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));
        }

    }

    if (addrToUse == "0.0.0.0") {
        link->loadSIPLocalIP (&addrToUse);
    }

    if (addrSdp == "0.0.0.0") {
        addrSdp = addrToUse;
    }

    call->setConnectionState (Call::Progressing);
    call->setPeerNumber (peerNumber);
    call->setDisplayName (displayName);
    call->initRecFileName (peerNumber);

    _debug ("UserAgent: DisplayName: %s", displayName.c_str());

    // Have to do some stuff with the SDP
    // Set the codec map, IP, peer number and so on... for the SIPCall object
    setCallMediaLocal (call, addrToUse);

    // We retrieve the remote sdp offer in the rdata struct to begin the negotiation
    call->getLocalSDP()->setLocalIP (addrSdp);

    // Init audio rtp session
    try {
        _debug ("UserAgent: Create RTP session for this call");
        call->getAudioRtp()->initAudioRtpConfig (call);
        call->getAudioRtp()->initAudioRtpSession (call);
    } catch (...) {
        _warn ("UserAgent: Error: Failed to create rtp thread from answer");
    }

    // Retreive crypto offer from body, if any
    if (rdata->msg_info.msg->body) {

        char sdpbuffer[1000];
        rdata->msg_info.msg->body->print_body (rdata->msg_info.msg->body, sdpbuffer, 1000);
        std::string sdpoffer = std::string (sdpbuffer);
        size_t start = sdpoffer.find ("a=crypto:");

        // Found crypto header in SDP
        if (start != std::string::npos) {

            std::string cryptoHeader = sdpoffer.substr (start, (sdpoffer.size() - start) -1);
            _debug ("UserAgent: Found incoming crypto offer: %s", cryptoHeader.c_str());

            CryptoOffer crypto_offer;
            crypto_offer.push_back (cryptoHeader);

            bool nego_success = false;

            if (!crypto_offer.empty()) {

                _debug ("UserAgent: Crypto attribute in SDP, init SRTP session");

                // init local cryptografic capabilities for negotiation
                std::vector<sfl::CryptoSuiteDefinition>localCapabilities;

                for (int i = 0; i < 3; i++) {
                    localCapabilities.push_back (sfl::CryptoSuites[i]);
                }

                sfl::SdesNegotiator sdesnego (localCapabilities, crypto_offer);

                if (sdesnego.negotiate()) {
                    _debug ("UserAgent: SDES negotiation successfull \n");
                    nego_success = true;

                    try {
                        _debug ("UserAgent: Create RTP session for this call");
                        call->getAudioRtp()->setRemoteCryptoInfo (sdesnego);
                        call->getAudioRtp()->initLocalCryptoInfo (call);
                    } catch (...) {
                        _warn ("UserAgent: Error: Failed to create rtp thread from answer");
                    }
                }
            }
        }
    }


    status = call->getLocalSDP()->receiveOffer (r_sdp, account->getActiveCodecs ());
    if (status!=PJ_SUCCESS) {
        delete call;
        call = NULL;
        _warn ("UserAgent: fail in receiving initial offer");
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL, NULL);
        return false;
    }

    // Init default codec for early media session
    sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (PAYLOAD_CODEC_ULAW);

    // Init audio rtp session
    try {
        _debug ("UserAgent: Create RTP session for this call");
        call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
    } catch (...) {
        _warn ("UserAgent: Error: Failed to create rtp thread from answer");
    }


    /* Create the local dialog (UAS) */
    status = pjsip_dlg_create_uas (pjsip_ua_instance(), rdata, NULL, &dialog);

    if (status != PJ_SUCCESS) {
        delete call;
        call = NULL;
        _warn ("UserAgent: Error: Failed to create uas dialog");
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR,
        NULL, NULL, NULL);
        return false;
    }

    // Specify media capability during invite session creation
    status = pjsip_inv_create_uas (dialog, rdata, call->getLocalSDP()->getLocalSdpSession(), 0, &inv);

    // Explicitly set the transport, set_transport methods increment transport's reference counter
    status = pjsip_dlg_set_transport (dialog, tp);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    // Associate the call in the invite session
    inv->mod_data[_mod_ua.id] = call;

    // Check whether Replaces header is present in the request and process accordingly.
    status = pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response);
    if (status != PJ_SUCCESS) {
    	_warn("UserAgent: Error: Something wrong with Replaces request.");
        // Respond with 500 (Internal Server Error)
    	pjsip_endpt_respond_stateless(_endpt, rdata, 500, NULL, NULL, NULL);
    }

    // Check if call have been transfered
    if(replaced_dlg) { // If Replace header present

    	_debug("UserAgent: Replace request foud");

    	pjsip_inv_session *replaced_inv;

    	// Always answer the new INVITE with 200, regardless whether
    	// the replaced call is in early or confirmed state.
    	if((status = pjsip_inv_answer(inv, 200, NULL, NULL, &response)) == PJ_SUCCESS)
    		pjsip_inv_send_msg(inv, response);

    	// Get the INVITE session associated with the replaced dialog.
    	replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

    	// Disconnect the "replaced" INVITE session.
         status = pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL, &tdata);
         if (status == PJ_SUCCESS && tdata)
             status = pjsip_inv_send_msg(replaced_inv, tdata);

         call->replaceInvSession(inv);
    }
    else { // Prooceed with normal call flow

        // Send a 180 Ringing response
        _info ("UserAgent: Send a 180 Ringing response");
        status = pjsip_inv_initial_answer (inv, rdata, PJSIP_SC_RINGING, NULL, NULL, &tdata);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

        status = pjsip_inv_send_msg (inv, tdata);
        PJ_ASSERT_RETURN (status == PJ_SUCCESS, 1);

    	// Associate invite session to the current call
    	call->setInvSession (inv);

    	// Update the connection state
    	call->setConnectionState (Call::Ringing);

    	_debug ("UserAgent: Add call to account link");

    	if (Manager::instance().incomingCall (call, account_id)) {
    		// Add this call to the callAccountMap in ManagerImpl
    		Manager::instance().getAccountLink (account_id)->addCall (call);
    	} else {
    		// Fail to notify UI
    		delete call;
    		call = NULL;
    		_warn ("UserAgent: Fail to notify UI!");
    		pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR,
    				NULL, NULL, NULL);
    		return false;
    	}

    }

    /* Done */
    return true;
}

pj_bool_t transaction_response_cb (pjsip_rx_data *rdata)
{
    _info ("UserAgent: Transaction response using transport: %s %s (refcnt=%d)",
    rdata->tp_info.transport->obj_name,
    rdata->tp_info.transport->info,
    (int) pj_atomic_get (rdata->tp_info.transport->ref_cnt));

    pjsip_dialog *dlg;
    dlg = pjsip_rdata_get_dlg (rdata);

    if (dlg != NULL) {
        pjsip_transaction *tsx = pjsip_rdata_get_tsx (rdata);

        if (tsx != NULL && tsx->method.id == PJSIP_INVITE_METHOD) {
            if (tsx->status_code < 200) {
                _info ("UserAgent: Received provisional response");
            } else if (tsx->status_code >= 300) {
                _warn ("UserAgent: Dialog failed");
                // pjsip_dlg_dec_session(dlg);
                // ACK for non-2xx final response is sent by transaction.
            } else {
                _info ("UserAgent: Received 200 OK response");
                sendAck (dlg, rdata);
            }
        }
    }

    return PJ_SUCCESS;
}

static void sendAck (pjsip_dialog *dlg, pjsip_rx_data *rdata)
{

    pjsip_tx_data *tdata;

    // Create ACK request
    pjsip_dlg_create_request (dlg, &pjsip_ack_method, rdata->msg_info.cseq->cseq, &tdata);

    pjsip_dlg_send_request (dlg, tdata,-1, NULL);
}

void onCallTransfered (pjsip_inv_session *inv, pjsip_rx_data *rdata)
{

    pj_status_t status;
    pjsip_tx_data *tdata;
    SIPCall *currentCall;
    const pj_str_t str_refer_to = { (char*) "Refer-To", 8};
    const pj_str_t str_refer_sub = { (char*) "Refer-Sub", 9 };
    const pj_str_t str_ref_by = { (char*) "Referred-By", 11 };
    pjsip_generic_string_hdr *refer_to;
    pjsip_generic_string_hdr *refer_sub;
    pjsip_hdr *ref_by_hdr;
    pj_bool_t no_refer_sub = PJ_FALSE;
    char *uri;
    std::string sipUri;
    pjsip_status_code code;
    pjsip_evsub *sub = NULL;

    currentCall = (SIPCall *) inv->mod_data[_mod_ua.id];
    if (currentCall == NULL) {
        _debug ("UserAgent: Call doesn't exist (%s, %s)", __FILE__, __LINE__);
        return;
    }

    /* Find the Refer-To header */
    refer_to = (pjsip_generic_string_hdr*)
    pjsip_msg_find_hdr_by_name (rdata->msg_info.msg, &str_refer_to, NULL);
    if (refer_to == NULL) {
        /* Invalid Request.
         * No Refer-To header!
         */
        _debug ("UserAgent: Received REFER without Refer-To header!");
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
    pjsip_msg_find_hdr_by_name (rdata->msg_info.msg, &str_ref_by, NULL);

    /* Notify callback */
    code = PJSIP_SC_ACCEPTED;

    _debug ("UserAgent: Call to %.*s is being transfered to %.*s",
    				(int) inv->dlg->remote.info_str.slen,
    				inv->dlg->remote.info_str.ptr,
    				(int) refer_to->hvalue.slen,
    				refer_to->hvalue.ptr);

//    if (no_refer_sub) {
//        /*
//         * Always answer with 2xx.
//         */
//        pjsip_tx_data *tdata;
//        const pj_str_t str_false = { (char*) "false", 5};
//        pjsip_hdr *hdr;
//
//        status = pjsip_dlg_create_response (inv->dlg, rdata, code, NULL,
//        &tdata);
//
//        if (status != PJ_SUCCESS) {
//            _debug ("UserAgent: Unable to create 2xx response to REFER reques -- %d", status);
//            return;
//        }
//
//        /* Add Refer-Sub header */
//        hdr = (pjsip_hdr*)
//        pjsip_generic_string_hdr_create (tdata->pool, &str_refer_sub,
//        &str_false);
//
//        pjsip_msg_add_hdr (tdata->msg, hdr);
//
//
//        /* Send answer */
//        status = pjsip_dlg_send_response (inv->dlg, pjsip_rdata_get_tsx (rdata),
//        tdata);
//
//        if (status != PJ_SUCCESS) {
//            _debug ("UserAgent: Unable to create 2xx response to REFER request -- %d", status);
//            return;
//        }
//
//        /* Don't have subscription */
//        sub = NULL;
//
//    } else {
//
//        struct pjsip_evsub_user xfer_cb;
//        pjsip_hdr hdr_list;
//
//        /* Init callback */
//        pj_bzero (&xfer_cb, sizeof (xfer_cb));
//        xfer_cb.on_evsub_state = &transfer_server_cb;
//
//        /* Init addional header list to be sent with REFER response */
//        pj_list_init (&hdr_list);
//
//        /* Create transferee event subscription */
//        status = pjsip_xfer_create_uas (inv->dlg, &xfer_cb, rdata, &sub);
//
//        if (status != PJ_SUCCESS) {
//            _debug ("UserAgent: Unable to create xfer uas -- %d", status);
//            pjsip_dlg_respond (inv->dlg, rdata, 500, NULL, NULL, NULL);
//            return;
//        }
//
//        /* If there's Refer-Sub header and the value is "true", send back
//         * Refer-Sub in the response with value "true" too.
//         */
//        if (refer_sub) {
//            const pj_str_t str_true = { (char*) "true", 4 };
//            pjsip_hdr *hdr;
//
//            hdr = (pjsip_hdr*)
//            pjsip_generic_string_hdr_create (inv->dlg->pool,
//            &str_refer_sub,
//            &str_true);
//            pj_list_push_back (&hdr_list, hdr);
//
//        }
//
//        /* Accept the REFER request, send 2xx. */
//        pjsip_xfer_accept (sub, rdata, code, &hdr_list);
//
//        /* Create initial NOTIFY request */
//        status = pjsip_xfer_notify (sub, PJSIP_EVSUB_STATE_TERMINATED,
//        100, NULL, &tdata);
//
//        if (status != PJ_SUCCESS) {
//            _debug ("UserAgent: Unable to create NOTIFY to REFER -- %d", status);
//            return;
//        }
//
//        /* Send initial NOTIFY request */
//        status = pjsip_xfer_send_request (sub, tdata);
//
//        if (status != PJ_SUCCESS) {
//            _debug ("UserAgent: Unable to send NOTIFY to REFER -- %d", status);
//            return;
//        }
//    }

    /* We're cheating here.
     * We need to get a null terminated string from a pj_str_t.
     * So grab the pointer from the hvalue and NULL terminate it, knowing
     * that the NULL position will be occupied by a newline.
     */
    uri = refer_to->hvalue.ptr;

    uri[refer_to->hvalue.slen] = '\0';

    /* Now make the outgoing call. */
    sipUri = std::string (uri);

    CallID currentCallId = currentCall->getCallId();
    // AccountID accId = Manager::instance().getAccountFromCall (currentCallId);

    CallID newCallId = Manager::instance().getNewCallID();

    Call *newCall = SIPVoIPLink::instance(IP2IP_PROFILE)->newOutgoingCall(newCallId, sipUri);

   //  if (!Manager::instance().outgoingCall (accId, newCallId, sipUri)) {
    if(newCall == NULL) {

        /* Notify xferer about the error (if we have subscription) */
        if (sub) {
            status = pjsip_xfer_notify (sub, PJSIP_EVSUB_STATE_TERMINATED,
            500, NULL, &tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Unable to create NOTIFY to REFER -- %d", status);
                return;
            }

            status = pjsip_xfer_send_request (sub, tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Unable to send NOTIFY to REFER -- %d", status);
                return;
            }
        }

        return;
    }

    Manager::instance().hangupCall(currentCall->getCallId());

//    SIPCall* sipCall = dynamic_cast<SIPCall *>(newCall);

//    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));
//    if(link == NULL) {
//    	_debug("UserAgent: Error could not retreive voip link from call");
//    	return;
//    }
//
//    if (link) {
//        newCall = dynamic_cast<SIPCall *> (link->getCall (newCallId));
//
//        if (!newCall) {
//            _debug ("UserAgent: can not find the call from sipvoiplink!");
//            return;
//        }
//    }

//    if (sub) {
//        /* Put the server subscription in inv_data.
//         * Subsequent state changed in pjsua_inv_on_state_changed() will be
//         * reported back to the server subscription.
//         */
//        currentCall->setXferSub (sub);
//
//        /* Put the invite_data in the subscription. */
//        pjsip_evsub_set_mod_data (sub, _mod_ua.id, currentCall);
//    }
}



void transfer_client_cb (pjsip_evsub *sub, pjsip_event *event)
{


    PJ_UNUSED_ARG (event);

    /*
     * When subscription is accepted (got 200/OK to REFER), check if
     * subscription suppressed.
     */
    if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_ACCEPTED) {

        _debug ("UserAgent: Transfer received, waiting for notifications. ");

        pjsip_rx_data *rdata;
        pjsip_generic_string_hdr *refer_sub;
        const pj_str_t REFER_SUB = { (char *) "Refer-Sub", 9 };

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
	    _debug("UserAgent: No subscription requested");
        }
	else {
	    _debug("UserAgent: Transfer subscription reqeusted");
	}	
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


        _debug("UserAgent: PJSIP_EVSUB_STATE_ACTIVE PJSIP_EVSUB_STATE_TERMINATED");

        SIPVoIPLink *link = reinterpret_cast<SIPVoIPLink *> (pjsip_evsub_get_mod_data (sub, _mod_ua.id));

        /* When subscription is terminated, clear the xfer_sub member of
         * the inv_data.
         */

        if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
            _debug ("UserAgent: Xfer client subscription terminated");
            // Manager::instance().hangupCall(call->getCallId());

        }

        /* Application is not interested with call progress status */
        if (!link || !event) {
            _warn ("UserAgent: Either link or event is empty in transfer callback");
            return;
        }


        pjsip_rx_data* r_data = event->body.rx_msg.rdata;

        std::string method_notify = "NOTIFY";
        std::string request =  pjsip_rx_data_get_info (r_data);

        /* This better be a NOTIFY request */
        if (r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD &&
        request.find (method_notify) != (size_t)-1) {

            /* Check if there's body */
            msg = r_data->msg_info.msg;
            body = msg->body;

            if (!body) {
                _warn ("UserAgent: Warning! Received NOTIFY without message body");
                return;
            }

            /* Check for appropriate content */
            if (pj_stricmp2 (&body->content_type.type, "message") != 0 ||
            pj_stricmp2 (&body->content_type.subtype, "sipfrag") != 0) {
                _warn ("UserAgent: Warning! Received NOTIFY without message/sipfrag content");
                return;
            }

            /* Try to parse the content */
            status = pjsip_parse_status_line ( (char*) body->data, body->len, &status_line);

            if (status != PJ_SUCCESS) {
                _warn ("UserAgent: Warning! Received NOTIFY with invalid message/sipfrag content");
                return;
            }

        } else {
            _error ("UserAgent: Error: Set code to 500 during transfer");
            status_line.code = 500;
            status_line.reason = *pjsip_get_status_text (500);
        }

        // Get call coresponding to this transaction
        std::string transferID (r_data->msg_info.cid->id.ptr, r_data->msg_info.cid->id.slen);
        std::map<std::string, CallID>::iterator it = transferCallID.find (transferID);
        CallID cid = it->second;
        SIPCall *call = dynamic_cast<SIPCall *> (link->getCall (cid));
        if (!call) {
            _warn ("UserAgent:  Call with id %s doesn't exit!", cid.c_str());
            return;
        }

        /* Notify application */
        is_last = (pjsip_evsub_get_state (sub) ==PJSIP_EVSUB_STATE_TERMINATED);

        cont = !is_last;

        _debug ("UserAgent: Notification status line: %d", status_line.code);

        if (status_line.code/100 == 2) {

            _debug ("UserAgent: Received 200 OK on call transfered, stop call!");
            pjsip_tx_data *tdata;

            status = pjsip_inv_end_session (call->getInvSession(), PJSIP_SC_GONE, NULL, &tdata);

            if (status != PJ_SUCCESS) {
                _debug ("UserAgent: Fail to create end session msg!");
            } else {
                status = pjsip_inv_send_msg (call->getInvSession(), tdata);

                if (status != PJ_SUCCESS) {
                    _debug ("UserAgent: Fail to send end session msg!");
		}
            }

            Manager::instance().hangupCall(call->getCallId());

            cont = PJ_FALSE;
        }

        if (!cont) {
            pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
        }
    }
}


void transfer_server_cb (pjsip_evsub *sub, pjsip_event *event)
{

    PJ_UNUSED_ARG (event);

    /*
     * When subscription is terminated, clear the xfer_sub member of
     * the inv_data.
     */
    switch (pjsip_evsub_get_state (sub)) {
	case PJSIP_EVSUB_STATE_NULL:
		break;
	case PJSIP_EVSUB_STATE_SENT:
		break;
	case PJSIP_EVSUB_STATE_ACCEPTED:
		break;
	case PJSIP_EVSUB_STATE_PENDING:
		break;
	case PJSIP_EVSUB_STATE_ACTIVE:
		break;
	case PJSIP_EVSUB_STATE_TERMINATED:
		break;
	case PJSIP_EVSUB_STATE_UNKNOWN:
		break;
	default:
		break;
	}

    if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        SIPCall *call;

        call = (SIPCall*) pjsip_evsub_get_mod_data (sub, _mod_ua.id);

        if (!call) {
        	_debug("UserAgent: Could not find subscription data");
            return;
        }

        pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);

        call->setXferSub (NULL);

        // Manager::instance().hangupCall(call->getCallId());

        _error ("UserAgent: Xfer server subscription terminated");
    }
}

void handleIncomingOptions (pjsip_rx_data *rdata)
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


bool setCallMediaLocal (SIPCall* call, const std::string &localIP)
{
    SIPAccount *account = NULL;

    _debug ("UserAgent: Set local media information for this call");

    if (call) {

        AccountID account_id = Manager::instance().getAccountFromCall (call->getCallId ());

        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));

        // Setting Audio
        unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
        unsigned int callLocalExternAudioPort = callLocalAudioPort;

        if (account->isStunEnabled ()) {
            // If use Stun server
            callLocalExternAudioPort = account->getStunPort ();
            //localIP = account->getPublishedAddress ();
        }

        _debug ("UserAgent: Setting local ip address: %s", localIP.c_str());
        _debug ("UserAgent: Setting local audio port to: %d", callLocalAudioPort);
        _debug ("UserAgent: Setting local audio port (external) to: %d", callLocalExternAudioPort);

        // Set local audio port for SIPCall(id)
        call->setLocalIp (localIP);
        call->setLocalAudioPort (callLocalAudioPort);

        call->getLocalSDP()->setPortToAllMedia (callLocalExternAudioPort);

        return true;
    } else {

        _error ("UserAgent: Error: No call found while setting media information for this call");

        return false;

    }
}

std::string fetchHeaderValue (pjsip_msg *msg, std::string field)
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

    _debug ("Detecting available interfaces...");

    int i;

    for (i = 0; i < (int) addrCnt; i++) {
        char tmpAddr[PJ_INET_ADDRSTRLEN];
        pj_sockaddr_print (&addrList[i], tmpAddr, sizeof (tmpAddr), 0);
        ifaceList.push_back (std::string (tmpAddr));
        _debug ("Local interface %s", tmpAddr);
    }

    return ifaceList;
}


int get_iface_list (struct ifconf *ifconf)
{
    int sock, rval;

    if ( (sock = socket (AF_INET,SOCK_STREAM,0)) < 0)
        _debug ("get_iface_list error could not open socket\n");


    if ( (rval = ioctl (sock, SIOCGIFCONF , (char*) ifconf)) < 0)
        _debug ("get_iface_list error ioctl(SIOGIFCONF)\n");

    close (sock);

    return rval;
}

std::vector<std::string> SIPVoIPLink::getAllIpInterfaceByName (void)
{
    std::vector<std::string> ifaceList;

    static struct ifreq ifreqs[20];
    struct ifconf ifconf;
    int  nifaces;

    // add the default
    ifaceList.push_back (std::string ("default"));

    memset (&ifconf,0,sizeof (ifconf));
    ifconf.ifc_buf = (char*) (ifreqs);
    ifconf.ifc_len = sizeof (ifreqs);

    if (get_iface_list (&ifconf) < 0)
        _debug ("getAllIpInterfaceByName error could not get interface list\n");

    nifaces =  ifconf.ifc_len/sizeof (struct ifreq);

    _debug ("Interfaces (count = %d):\n", nifaces);

    for (int i = 0; i < nifaces; i++) {
        _debug ("  %s  ", ifreqs[i].ifr_name);
        ifaceList.push_back (std::string (ifreqs[i].ifr_name));
        printf ("    %s\n", getInterfaceAddrFromName (std::string (ifreqs[i].ifr_name)).c_str());
    }

    return ifaceList;
}

std::string SIPVoIPLink::getInterfaceAddrFromName (std::string ifaceName)
{

    struct ifreq ifr;
    int fd;
    int err;

    struct sockaddr_in *saddr_in;
    struct in_addr *addr_in;

    if ( (fd = socket (AF_INET, SOCK_DGRAM,0)) < 0)
        _error ("UserAgent: Error: could not open socket");

    memset (&ifr, 0, sizeof (struct ifreq));

    strcpy (ifr.ifr_name, ifaceName.c_str());
    ifr.ifr_addr.sa_family = AF_INET;

    if ( (err = ioctl (fd, SIOCGIFADDR, &ifr)) < 0)
        _debug ("UserAgent: Use default interface (0.0.0.0)");

    saddr_in = (struct sockaddr_in *) &ifr.ifr_addr;
    addr_in = & (saddr_in->sin_addr);

    std::string addr (inet_ntoa (*addr_in));

    close (fd);

    return addr;
}


pj_bool_t stun_sock_on_status_cb (pj_stun_sock *stun_sock UNUSED, pj_stun_sock_op op UNUSED, pj_status_t status)
{
    if (status == PJ_SUCCESS)
        return PJ_TRUE;
    else
        return PJ_FALSE;
}

pj_bool_t stun_sock_on_rx_data_cb (pj_stun_sock *stun_sock UNUSED, void *pkt UNUSED, unsigned pkt_len UNUSED, const pj_sockaddr_t *src_addr UNUSED, unsigned addr_len UNUSED)
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

    _debug ("UserAgent: Get local address associated to account");

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    // Set the local address

    if (account != NULL && account->getAccountTransport ()) {
        tspt = account->getAccountTransport ();

        if (tspt != NULL) {
            local_addr_ipv4 = tspt->local_addr.ipv4;
        } else {
            _debug ("UserAgent: transport is null");
            local_addr_ipv4 = _localUDPTransport->local_addr.ipv4;
        }
    } else {
        _debug ("UserAgent: account is null");
        local_addr_ipv4 = _localUDPTransport->local_addr.ipv4;
    }

    tmp = pj_str (pj_inet_ntoa (local_addr_ipv4.sin_addr));
    localAddr = std::string (tmp.ptr);

    return localAddr;

}

void getRemoteSdpFromOffer (pjsip_rx_data *rdata, pjmedia_sdp_session** r_sdp)
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

