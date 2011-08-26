/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

using namespace sfl;

static const char * invitationStateMap[] = {
    "PJSIP_INV_STATE_NULL",
    "PJSIP_INV_STATE_CALLING",
    "PJSIP_INV_STATE_INCOMING",
    "PJSIP_INV_STATE_EARLY",
    "PJSIP_INV_STATE_CONNECTING",
    "PJSIP_INV_STATE_CONFIRMED",
    "PJSIP_INV_STATE_DISCONNECTED"
};

static const char * transactionStateMap[] = {
    "PJSIP_TSX_STATE_NULL" ,
    "PJSIP_TSX_STATE_CALLING",
    "PJSIP_TSX_STATE_TRYING",
    "PJSIP_TSX_STATE_PROCEEDING",
    "PJSIP_TSX_STATE_COMPLETED",
    "PJSIP_TSX_STATE_CONFIRMED",
    "PJSIP_TSX_STATE_TERMINATED",
    "PJSIP_TSX_STATE_DESTROYED",
    "PJSIP_TSX_STATE_MAX"
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
std::map<std::string, std::string> transferCallID;


const pj_str_t STR_USER_AGENT = { (char*) "User-Agent", 10 };

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

/*
 * Retrieve the SDP of the peer contained in the offer
 *
 * @param rdata The request data
 * @param r_sdp The pjmedia_sdp_media to stock the remote SDP
 */
int getModId();

/**
 * Set audio (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 */
void setCallMediaLocal (SIPCall* call, const std::string &localIP);

/**
 * Helper function to parser header from incoming sip messages
 */
std::string fetchHeaderValue (pjsip_msg *msg, std::string field);

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

/**
 * Get the number of voicemail waiting in a SIP message
 */
void setVoicemailInfo (std::string account, pjsip_msg_body *body);

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


SIPVoIPLink::SIPVoIPLink ()
    : _nbTryListenAddr (2)   // number of times to try to start SIP listener
    , _regPort (DEFAULT_SIP_PORT)
{
    srand (time (NULL));    // to get random number for RANDOM_PORT

    /* Start pjsip initialization step */
    // TODO This port should be the one configured for the IP profile
    // and not the global one
    _regPort = Manager::instance().getLocalIp2IpPort();

    /* Instanciate the C++ thread */
    _evThread = new EventThread (this);

    /* Initialize the pjsip library */
    pjsipInit();
}

SIPVoIPLink::~SIPVoIPLink()
{
	delete _evThread;
	pjsipShutdown();
}

SIPVoIPLink* SIPVoIPLink::instance ()
{
    if (!_instance) {
        _debug ("UserAgent: Create new SIPVoIPLink instance");
        _instance = new SIPVoIPLink;
    }

    return _instance;
}

void SIPVoIPLink::init() {}

void SIPVoIPLink::terminate() {}

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

void SIPVoIPLink::sendRegister (Account *a)
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsip_hdr hdr_list;

    SIPAccount *account = static_cast<SIPAccount*>(a);

    // Resolve hostname here and keep its
    // IP address for the whole time the
    // account is connected. This was a
    // workaround meant to help issue
    // #1852 that we hope should be fixed
    // soon.
    if (account->isResolveOnce()) {
        pjsip_host_info destination;

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
    acquireTransport (account);

    if (account->getAccountTransport()) {
        _debug ("Acquire transport in account registration: %s %s (refcnt=%d)",
                account->getAccountTransport()->obj_name,
                account->getAccountTransport()->info,
                (int) pj_atomic_get (account->getAccountTransport()->ref_cnt));
    }

    _mutexSIP.enterMutex();

    // Get the client registration information for this particular account
    pjsip_regc *regc = account->getRegistrationInfo();
    account->setRegister (true);

    // Set the expire value of the message from the config file
    int expire_value;
    std::istringstream stream (account->getRegistrationExpire());
    stream >> expire_value;
    if (!expire_value)
        expire_value = PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;

    // Update the state of the voip link
    account->setRegistrationState (Trying);

    // Create the registration according to the account ID
    if (pjsip_regc_create (_endpt, (void *) account, &registration_cb, &regc) != PJ_SUCCESS) {
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

    std::string contactUri(account->getContactHeader (address, portStr));

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
    if (pjsip_regc_init (regc, &pjSrv, &pjFrom, &pjFrom, 1, &pjContact, expire_value) != PJ_SUCCESS) {
        _mutexSIP.leaveMutex();
        throw VoipLinkException("Unable to initialize account registration structure");
    }

    // Fill route set
    if (!account->getServiceRoute().empty()) {
        pjsip_route_hdr *route_set = createRouteSet(account, _pool);
        pjsip_regc_set_route_set (regc, route_set);
    }

    unsigned count = account->getCredentialCount();
    pjsip_cred_info *info = account->getCredInfo();
    pjsip_regc_set_credentials (regc, count, info);

    // Add User-Agent Header
    pj_list_init (&hdr_list);

	const std::string agent(account->getUserAgentName());
    pj_str_t useragent = pj_str ((char*)agent.c_str());
    pjsip_generic_string_hdr *h = pjsip_generic_string_hdr_create (_pool, &STR_USER_AGENT, &useragent);

    pj_list_push_back (&hdr_list, (pjsip_hdr*) h);
    pjsip_regc_add_headers (regc, &hdr_list);


    if (pjsip_regc_register (regc, PJ_TRUE, &tdata) != PJ_SUCCESS) {
        _mutexSIP.leaveMutex();
        throw VoipLinkException("Unable to initialize transaction data for account registration");
    }

    pjsip_tpselector *tp = initTransportSelector (account->getAccountTransport(), _pool);

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

    pjsip_transport *transport = account->getAccountTransport();
    if (transport)
        _debug ("Sent account registration using transport: %s %s (refcnt=%d)",
                transport->obj_name, transport->info, (int) pj_atomic_get(transport->ref_cnt));
}

void SIPVoIPLink::sendUnregister (Account *a) throw(VoipLinkException)
{
    pjsip_tx_data *tdata = NULL;
    SIPAccount *account = (SIPAccount *)a;

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
    if(!regc)
    	throw VoipLinkException("Registration structure is NULL");

    if (pjsip_regc_unregister (regc, &tdata) != PJ_SUCCESS)
    	throw VoipLinkException("Unable to unregister sip account");

    if (pjsip_regc_send (regc, tdata) != PJ_SUCCESS)
    	throw VoipLinkException("Unable to send request to unregister sip account");

    account->setRegister (false);
}

Call *SIPVoIPLink::newOutgoingCall (const std::string& id, const std::string& toUrl) throw (VoipLinkException)
{
    SIPAccount * account = NULL;
    std::string localAddr, addrSdp;

    // Create a new SIP call
    SIPCall* call = new SIPCall (id, Call::Outgoing, _cp);

    // Find the account associated to this call
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (Manager::instance().getAccountFromCall (id)));
    if (account == NULL) {
    	_error ("UserAgent: Error: Could not retrieving account to make call with");
    	call->setConnectionState (Call::Disconnected);
    	call->setState (Call::Error);
    	delete call;
    	// TODO: We should investigate how we could get rid of this error and create a IP2IP call instead
    	throw VoipLinkException("Could not get account for this call");
    }

    // If toUri is not a well formated sip URI, use account information to process it
    std::string toUri;
    if((toUrl.find("sip:") != std::string::npos) or
    		toUrl.find("sips:") != std::string::npos) {
    	toUri = toUrl;
    }
    else
        toUri = account->getToUri (toUrl);

    call->setPeerNumber (toUri);
    _debug ("UserAgent: New outgoing call %s to %s", id.c_str(), toUri.c_str());

    localAddr = getInterfaceAddrFromName (account->getLocalInterface ());
    _debug ("UserAgent: Local address for thi call: %s", localAddr.c_str());

    if (localAddr == "0.0.0.0")
    	localAddr = loadSIPLocalIP ();

    setCallMediaLocal (call, localAddr);

    // May use the published address as well
    account->isStunEnabled () ? addrSdp = account->getPublishedAddress () : addrSdp = getInterfaceAddrFromName (account->getLocalInterface ());

    if (addrSdp == "0.0.0.0")
		addrSdp = loadSIPLocalIP ();

    // Initialize the session using ULAW as default codec in case of early media
    // The session should be ready to receive media once the first INVITE is sent, before
    // the session initialization is completed
    sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (PAYLOAD_CODEC_ULAW);
    if (audiocodec == NULL) {
    	_error ("UserAgent: Could not instantiate codec");
    	delete call;
    	throw VoipLinkException ("Could not instantiate codec for early media");
    }

	try {
		_info ("UserAgent: Creating new rtp session");
		call->getAudioRtp()->initAudioRtpConfig ();
		call->getAudioRtp()->initAudioSymmetricRtpSession ();
		call->getAudioRtp()->initLocalCryptoInfo ();
		_info ("UserAgent: Start audio rtp session");
		call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
	} catch (...) {
        delete call;
		throw VoipLinkException ("Could not start rtp session for early media");
	}

	// init file name according to peer phone number
	call->initRecFileName (toUrl);

	// Building the local SDP offer
	call->getLocalSDP()->setLocalIP (addrSdp);
	if (call->getLocalSDP()->createOffer (account->getActiveCodecs ()) != PJ_SUCCESS) {
		delete call;
		throw VoipLinkException ("Could not create local sdp offer for new call");
	}

	if (SIPStartCall (call)) {
		call->setConnectionState (Call::Progressing);
		call->setState (Call::Active);
		addCall (call);
	} else {
		delete call;
		throw VoipLinkException("Could not send outgoing INVITE request for new call");
	}

	return call;
}

void
SIPVoIPLink::answer (Call *c) throw (VoipLinkException)
{
    pjsip_tx_data *tdata;

    _debug ("UserAgent: Answering call");

    SIPCall *call = (SIPCall*)c;

    pjsip_inv_session *inv_session = call->getInvSession();

	_debug ("UserAgent: SDP negotiation success! : call %s ", call->getCallId().c_str());
	// Create and send a 200(OK) response
	if (pjsip_inv_answer (inv_session, PJSIP_SC_OK, NULL, NULL, &tdata) != PJ_SUCCESS)
		throw VoipLinkException("Could not init invite request answer (200 OK)");

	if (pjsip_inv_send_msg (inv_session, tdata) != PJ_SUCCESS)
		throw VoipLinkException("Could not send invite request answer (200 OK)");

	call->setConnectionState (Call::Connected);
	call->setState (Call::Active);
}

void
SIPVoIPLink::hangup (const std::string& id) throw (VoipLinkException)
{
    pjsip_tx_data *tdata = NULL;

    SIPCall* call = getSIPCall (id);
    if (call == NULL) {
        throw VoipLinkException("Call is NULL while hanging up");
    }

    std::string account_id = Manager::instance().getAccountFromCall (id);
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
    if (pjsip_inv_end_session (inv, 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (pjsip_inv_send_msg (inv, tdata) != PJ_SUCCESS)
        return;

    // Make sure user data is NULL in callbacks
    inv->mod_data[getModId()] = NULL;

    // Release RTP thread
    try {
        if (Manager::instance().isCurrentCall (id))
            call->getAudioRtp()->stop();
    }
    catch(...) {
    	throw VoipLinkException("Could not stop audio rtp session");
    }

    removeCall (id);
}

void
SIPVoIPLink::peerHungup (const std::string& id) throw (VoipLinkException)
{
    _info ("UserAgent: Peer hungup");

    SIPCall* call = getSIPCall (id);
    if (!call)
        throw VoipLinkException("Call does not exist");

    // User hangup current call. Notify peer
    pjsip_tx_data *tdata = NULL;
    if (pjsip_inv_end_session (call->getInvSession(), 404, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    if (pjsip_inv_send_msg (call->getInvSession(), tdata) != PJ_SUCCESS)
        return;

    // Make sure user data is NULL in callbacks
    call->getInvSession()->mod_data[getModId() ] = NULL;

    // Release RTP thread
    try {
        if (Manager::instance().isCurrentCall (id)) {
            _debug ("UserAgent: Stopping AudioRTP for hangup");
            call->getAudioRtp()->stop();
        }
    }
    catch(...) {
    	throw VoipLinkException("Could not stop audio rtp session");
    }

    removeCall (id);
}

void
SIPVoIPLink::cancel (const std::string& id) throw (VoipLinkException)
{
    _info ("UserAgent: Cancel call %s", id.c_str());

    SIPCall* call = getSIPCall (id);
    if (!call) {
    	throw VoipLinkException("Call does not exist");
    }

    removeCall (id);
}


bool
SIPVoIPLink::onhold (const std::string& id) throw (VoipLinkException)
{
    SIPCall *call = getSIPCall (id);
    if (!call)
    	throw VoipLinkException("Could not find call");

    // Stop sound
    call->setState (Call::Hold);

    try {
        call->getAudioRtp()->stop();
    }
    catch (...) {
    	throw VoipLinkException("Could not stop audio rtp session");
    }

    _debug ("UserAgent: Stopping RTP session for on hold action");

	Sdp *sdpSession = call->getLocalSDP();
    if (!sdpSession)
    	throw VoipLinkException("Could not find sdp session");

    sdpSession->removeAttributeFromLocalAudioMedia("sendrecv");
    sdpSession->removeAttributeFromLocalAudioMedia("sendonly");

    sdpSession->addAttributeToLocalAudioMedia("sendonly");

    // Create re-INVITE with new offer
    return SIPSessionReinvite (call) == PJ_SUCCESS;
}

bool
SIPVoIPLink::offhold (const std::string& id) throw (VoipLinkException)
{
    _debug ("UserAgent: retrive call from hold status");

    SIPCall *call = getSIPCall (id);
    if (call == NULL) {
    	throw VoipLinkException("Could not find call");
    }

	Sdp *sdpSession = call->getLocalSDP();
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

        call->getAudioRtp()->initAudioRtpConfig ();
        call->getAudioRtp()->initAudioSymmetricRtpSession ();
        call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));

    }
    catch (const SdpException &e) {
    	_error("UserAgent: Exception: %s", e.what());
    } 
    catch (...) {
    	throw VoipLinkException("Could not create audio rtp session");
    }

    sdpSession->removeAttributeFromLocalAudioMedia("sendrecv");
    sdpSession->removeAttributeFromLocalAudioMedia("sendonly");

    sdpSession->addAttributeToLocalAudioMedia("sendrecv");

    /* Create re-INVITE with new offer */
    if (SIPSessionReinvite (call) != PJ_SUCCESS)
        return false;

    call->setState (Call::Active);
    return true;
}

bool
SIPVoIPLink::sendTextMessage (sfl::InstantMessaging *module, const std::string& callID, const std::string& message, const std::string& from)
{
    _debug ("SipVoipLink: Send text message to %s, from %s", callID.c_str(), from.c_str());

    SIPCall *call = getSIPCall (callID);
    if (!call) {
        /* Notify the client of an error */
        /*Manager::instance ().incomingMessage (	"",
        										"sflphoned",
        										"Unable to send a message outside a call.");*/
    	return !PJ_SUCCESS;
    }

	/* Send IM message */
	sfl::InstantMessaging::UriList list;
	sfl::InstantMessaging::UriEntry entry;
	entry[sfl::IM_XML_URI] = std::string ("\"" + from + "\""); // add double quotes for xml formating

	list.push_front (entry);

	std::string formatedMessage = module->appendUriList (message, list);

	return module->send_sip_message (call->getInvSession (), callID, formatedMessage);
}

int SIPSessionReinvite (SIPCall *call)
{
    pjsip_tx_data *tdata;

    _debug("UserAgent: Sending re-INVITE request");

    pjmedia_sdp_session *local_sdp = call->getLocalSDP()->getLocalSdpSession();
    if (!local_sdp) {
        _debug ("UserAgent: Error: Unable to find local sdp");
        return !PJ_SUCCESS;
    }

    // Build the reinvite request
    pj_status_t status = pjsip_inv_reinvite (call->getInvSession(), NULL, local_sdp, &tdata);
    if (status != PJ_SUCCESS)
        return status;

    // Send it
    return pjsip_inv_send_msg (call->getInvSession(), tdata);
}

bool
SIPVoIPLink::transfer (const std::string& id, const std::string& to) throw (VoipLinkException)
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

    std::string account_id = Manager::instance().getAccountFromCall (id);
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
    transferCallID.insert (std::pair<std::string, std::string> (callidtransfer, call->getCallId()));

    /* Send. */
    status = pjsip_xfer_send_request (sub, tdata);
    if (status != PJ_SUCCESS) {
    	throw VoipLinkException("Could not send xfer request");
    }

    return true;
}

bool SIPVoIPLink::attendedTransfer(const std::string& transferId, const std::string& targetId)
{
	char str_dest_buf[PJSIP_MAX_URL_SIZE*2];

	_debug("UserAgent: Attended transfer");

	pjsip_dialog *target_dlg = getSIPCall (targetId)->getInvSession()->dlg;

    /* Print URI */
	pj_str_t str_dest = { NULL, 0 };
    str_dest_buf[0] = '<';
    str_dest.slen = 1;

	pjsip_uri *uri = (pjsip_uri*) pjsip_uri_get_uri(target_dlg->remote.info->uri);
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
	struct pjsip_evsub_user xfer_cb;
    pj_bzero (&xfer_cb, sizeof (xfer_cb));
    xfer_cb.on_evsub_state = &transfer_client_cb;
	pjsip_evsub *sub;

    if (pjsip_xfer_create_uac (transferCall->getInvSession()->dlg, &xfer_cb, &sub) != PJ_SUCCESS) {
    	_warn ("UserAgent: Unable to create xfer");
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
	pjsip_tx_data *tdata;
    if (pjsip_xfer_initiate (sub, &str_dest, &tdata) != PJ_SUCCESS) {
    	_error ("UserAgent: Unable to create REFER request");
    	return false;
    }

    // Put SIP call id in map in order to retrieve call during transfer callback
    std::string callidtransfer (transferCall->getInvSession()->dlg->call_id->id.ptr,
    							transferCall->getInvSession()->dlg->call_id->id.slen);
    _debug ("%s", callidtransfer.c_str());
    transferCallID.insert (std::pair<std::string, std::string> (callidtransfer, transferCall->getCallId()));


    /* Send. */
    return pjsip_xfer_send_request (sub, tdata) == PJ_SUCCESS;
}

bool
SIPVoIPLink::refuse (const std::string& id)
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

std::string
SIPVoIPLink::getCurrentCodecName(Call *call)
{
    Sdp* sdp = (dynamic_cast<SIPCall*>(call))->getLocalSDP();

    try {
        if (sdp->hasSessionMedia())
            return sdp->getSessionMedia()->getMimeSubtype();
    } catch (const SdpException &e) {
    	_error("UserAgent: Exception: %s", e.what());
    }

    return "";
}

bool
SIPVoIPLink::carryingDTMFdigits (const std::string& id, char code)
{
    SIPCall *call = getSIPCall (id);

    if (!call) {
        //_error ("UserAgent: Error: Call doesn't exist while sending DTMF");
        return false;
    }

    std::string accountID = Manager::instance().getAccountFromCall (id);
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
    char dtmf_body[1000];
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

    snprintf (dtmf_body, sizeof dtmf_body - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);

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

void
SIPVoIPLink::dtmfOverRtp (SIPCall* call, char code)
{
    call->getAudioRtp()->sendDtmfDigit (atoi (&code));
}

bool
SIPVoIPLink::SIPStartCall (SIPCall* call)
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

    std::string id = Manager::instance().getAccountFromCall (call->getCallId());

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

    std::string contactUri(account->getContactHeader (address, portStr));

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
    pjsip_auth_clt_set_credentials (&dialog->auth_sess, account->getCredentialCount(), account->getCredInfo());

    // Associate current call in the invite session
    inv->mod_data[getModId() ] = call;

    status = pjsip_inv_invite (inv, &tdata);
    PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

    // Associate current invite session in the call
    call->setInvSession (inv);

    // Set the appropriate transport
    pjsip_tpselector *tp = initTransportSelector (account->getAccountTransport (), inv->pool);

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
        std::string id = call->getCallId();
        Manager::instance().callFailure (id);
        removeCall (id);
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

    std::string id = call->getCallId();

    if (Manager::instance().isCurrentCall (id)) {
        _debug ("UserAgent: Stopping AudioRTP when closing");
        call->getAudioRtp()->stop();
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

    std::string id = call->getCallId();

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
SIPVoIPLink::getSIPCall (const std::string& id)
{
    return dynamic_cast<SIPCall*> (getCall (id));
}

bool SIPVoIPLink::SIPNewIpToIpCall (const std::string& id, const std::string& to)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;

    _debug ("UserAgent: New IP2IP call %s to %s", id.c_str(), to.c_str());

    /* Create the call */
    call = new SIPCall (id, Call::Outgoing, _cp);

    call->setCallConfiguration (Call::IPtoIP);

    // Init recfile name using to uri
    call->initRecFileName (to);

    SIPAccount * account = NULL;
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));

    if (account == NULL) {
        _error ("UserAgent: Error: Account %s is null. Returning", IP2IP_PROFILE);
        delete call;
        return false;
    }

    // Set the local address
    std::string localAddress = getInterfaceAddrFromName (account->getLocalInterface ());
    // If local address bound to ANY, resolve it using PJSIP
    if (localAddress == "0.0.0.0")
        localAddress = loadSIPLocalIP ();


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
        call->getAudioRtp()->initAudioRtpConfig ();
        call->getAudioRtp()->initAudioSymmetricRtpSession ();
        call->getAudioRtp()->initLocalCryptoInfo ();
        call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
    } catch (...) {
        _debug ("UserAgent: Unable to create RTP Session in new IP2IP call (%s:%d)", __FILE__, __LINE__);
    }

    // Building the local SDP offer
    call->getLocalSDP()->setLocalIP (localAddress);
    status = call->getLocalSDP()->createOffer (account->getActiveCodecs ());
    if (status != PJ_SUCCESS)
        _error("UserAgent: Failed to create local offer\n");

    // Init TLS transport if enabled
    if (account->isTlsEnabled()) {
        _debug ("UserAgent: TLS enabled for IP2IP calls");
        int at = toUri.find ("@");
        int trns = toUri.find (";transport");
        std::string remoteAddr = toUri.substr (at+1, trns-at-1);

        if (toUri.find ("sips:") != 1) {
            _debug ("UserAgent: Error \"sips\" scheme required for TLS call");
            delete call;
            return false;
        }

        if (createTlsTransport (account, remoteAddr) != PJ_SUCCESS) {
            delete call;
            return false;
        }
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

    std::string contactUri(account->getContactHeader (address, portStr));

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
    pjsip_tpselector *tp = initTransportSelector (account->getAccountTransport(), inv->pool);

    if (!account->getAccountTransport()) {
        _error ("UserAgent: Error: Transport is NULL in IP2IP call");
    }

    // set_transport methods increment transport's ref_count
    status = pjsip_dlg_set_transport (dialog, tp);

    if (status != PJ_SUCCESS) {
        _error ("UserAgent: Error: Failed to set the transport for an IP2IP call");
        delete call;
        return false;
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
        return false;
    }

    call->setConnectionState (Call::Progressing);

    call->setState (Call::Active);
    addCall (call);
    return true;
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

    addr = loadSIPLocalIP();
    if (addr.empty()) {
        _debug ("UserAgent: Unable to determine network capabilities");
        return false;
    }

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


static pj_bool_t stun_sock_on_status_cb (pj_stun_sock *stun_sock UNUSED, pj_stun_sock_op op UNUSED, pj_status_t status)
{
    return status == PJ_SUCCESS;
}

static pj_bool_t stun_sock_on_rx_data_cb (pj_stun_sock *stun_sock UNUSED, void *pkt UNUSED, unsigned pkt_len UNUSED, const pj_sockaddr_t *src_addr UNUSED, unsigned addr_len UNUSED)
{
    return PJ_TRUE;
}


pj_status_t SIPVoIPLink::stunServerResolve (SIPAccount *account)
{
    pj_stun_sock *stun_sock;
    pj_stun_config stunCfg;
    pj_status_t status;

    pj_str_t stunServer = account->getStunServerName ();

    // Initialize STUN configuration
    pj_stun_config_init (&stunCfg, &_cp->factory, 0, pjsip_endpt_get_ioqueue (_endpt), pjsip_endpt_get_timer_heap (_endpt));

    static const pj_stun_sock_cb stun_sock_cb = {
    		stun_sock_on_rx_data_cb,
    		NULL,
    		stun_sock_on_status_cb
    };

    status = pj_stun_sock_create (&stunCfg, "stunresolve", pj_AF_INET(), &stun_sock_cb, NULL, NULL, &stun_sock);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror (status, errmsg, sizeof (errmsg));
        _debug ("Error creating STUN socket for %.*s: %s", (int) stunServer.slen, stunServer.ptr, errmsg);
        return status;
    }

    status = pj_stun_sock_start (stun_sock, &stunServer, account->getStunPort (), NULL);

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror (status, errmsg, sizeof (errmsg));
        _debug ("Error starting STUN socket for %.*s: %s", (int) stunServer.slen, stunServer.ptr, errmsg);
        pj_stun_sock_destroy (stun_sock);
    }

    return status;
}

bool SIPVoIPLink::acquireTransport (SIPAccount *account)
{
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
    if (createSipTransport (account))
        return true;

    // A transport is already created on this port, use it
	_debug ("Could not create a new transport (%d)", account->getLocalPort());

	// Could not create new transport, this transport may already exists
	pjsip_transport* tr = _transportMap[account->getLocalPort()];
	if (tr) {
		account->setAccountTransport (tr);

		// Increment newly associated transport reference counter
		// If the account is shutdowning, time is automatically canceled
		pjsip_transport_add_ref (tr);

		return true;
	}

	// Transport could not either be created, socket not available
	_debug ("Did not find transport (%d) in transport map", account->getLocalPort());

	account->setAccountTransport (_localUDPTransport);
	std::string localHostName (_localUDPTransport->local_name.host.ptr, _localUDPTransport->local_name.host.slen);

	_debug ("Use default one instead (%s:%i)", localHostName.c_str(), _localUDPTransport->local_name.port);

	// account->setLocalAddress(localHostName);
	account->setLocalPort (_localUDPTransport->local_name.port);

	// Transport could not either be created or found in the map, socket not available
	return false;
}


bool SIPVoIPLink::createDefaultSipUdpTransport()
{
    SIPAccount * account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));
    assert(account);

    // Create a UDP listener meant for all accounts for which TLS was not enabled
    // Cannot acquireTransport since default UDP transport must be created regardless of TLS

    pjsip_transport *transport = createUdpTransport (account, true);
    if (!transport)
    	return false;

    if (transport) {
    	_transportMap[account->getLocalPort()] = transport;
        _localUDPTransport = transport;
		account->setAccountTransport (transport);
    }
    return true;
}


void SIPVoIPLink::createDefaultSipTlsListener()
{
    SIPAccount * account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (IP2IP_PROFILE));
    if (account->isTlsEnabled())
        createTlsListener (account);
}


void SIPVoIPLink::createTlsListener (SIPAccount *account)
{
    pjsip_tpfactory *tls;
    pj_sockaddr_in local_addr;
    pjsip_host_port a_name;

    _debug ("Create TLS listener");

    /* Grab the tls settings, populated
     * from configuration file.
     */

    // Init local address for this listener to be bound (ADDR_ANY on port 5061).
    pj_sockaddr_in_init (&local_addr, 0, 0);
    pj_uint16_t localTlsPort = account->getTlsListenerPort();
    local_addr.sin_port = pj_htons (localTlsPort);

    pj_str_t pjAddress;
    pj_cstr (&pjAddress, PJ_INADDR_ANY);
    pj_sockaddr_in_set_str_addr (&local_addr, &pjAddress);


    // Init published address for this listener (Local IP address on port 5061)
    std::string publishedAddress = loadSIPLocalIP ();

    pj_bzero (&a_name, sizeof (pjsip_host_port));
    pj_cstr (&a_name.host, publishedAddress.c_str());
    a_name.port = account->getTlsListenerPort();

    _debug ("UserAgent: TLS transport to be initialized with published address %.*s,"
    " published port %d,\n                  local address %.*s, local port %d",
    (int) a_name.host.slen, a_name.host.ptr,
    a_name.port, pjAddress.slen, pjAddress.ptr, (int) localTlsPort);


    pj_status_t status = pjsip_tls_transport_start (_endpt, account->getTlsSetting(), &local_addr, &a_name, 1, &tls);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Error creating SIP TLS listener (%d)", status);
    } else {
        _localTlsListener = tls;
    }
}


bool SIPVoIPLink::createSipTransport (SIPAccount *account)
{
    if (account->isTlsEnabled()) {
        if (_localTlsListener == NULL)
            createTlsListener (account);

        // Parse remote address to establish connection
        std::string remoteSipUri = account->getServerUri();
        int sips = remoteSipUri.find ("<sips:") + 6;
        int trns = remoteSipUri.find (";transport");
        std::string remoteAddr = remoteSipUri.substr (sips, trns-sips);

        // Nothing to do, TLS listener already created at pjsip's startup and TLS connection
        // is automatically handled in pjsip when sending registration messages.
        return createTlsTransport (account, remoteAddr) == PJ_SUCCESS;
    } else {

        // Launch a new UDP listener/transport, using the published address
        if (account->isStunEnabled ()) {

            if (createAlternateUdpTransport (account) != PJ_SUCCESS) {
                _debug ("Failed to init UDP transport with STUN published address");
                return false;
            }

        } else {
            pjsip_transport *transport = createUdpTransport (account, false);
            if (!transport) {
                _debug ("Failed to initialize UDP transport");
                return false;
            }

			// If transport successfully created, store it in the internal map.
			// STUN aware transport are account specific and should not be stored in map.
			// TLS transport is ephemeral and is managed by PJSIP, should not be stored either.

			_transportMap[account->getLocalPort()] = transport;
			account->setAccountTransport (transport);
        }
    }

    return true;
}

pjsip_transport *SIPVoIPLink::createUdpTransport (SIPAccount *account, bool local)
{
    pj_sockaddr_in bound_addr;

    /* Use my local address as default value */
    std::string listeningAddress = loadSIPLocalIP ();
	if (listeningAddress.empty())
        return NULL;

	// We are trying to initialize a UDP transport available for all local accounts and direct IP calls
	if (account->getLocalInterface () != "default")
		listeningAddress = getInterfaceAddrFromName (account->getLocalInterface());

    int listeningPort = account->getLocalPort ();

    pj_memset (&bound_addr, 0, sizeof (bound_addr));

    if (account->getLocalInterface () == "default") {

        // Init bound address to ANY
        bound_addr.sin_addr.s_addr = pj_htonl (PJ_INADDR_ANY);
        listeningAddress = loadSIPLocalIP ();
    } else {
        // bind this account to a specific interface
        pj_str_t temporary_address;
        pj_strdup2 (_pool, &temporary_address, listeningAddress.c_str());
        bound_addr.sin_addr = pj_inet_addr (&temporary_address);
    }

    bound_addr.sin_port = pj_htons ( (pj_uint16_t) listeningPort);
    bound_addr.sin_family = PJ_AF_INET;
    pj_bzero (bound_addr.sin_zero, sizeof (bound_addr.sin_zero));

    // Create UDP-Server (default port: 5060)
    // Use here either the local information or the published address
    if (!account->getPublishedSameasLocal ()) {
        listeningAddress = account->getPublishedAddress ();
        listeningPort = account->getPublishedPort ();
        _debug ("UserAgent: Creating UDP transport published %s:%i", listeningAddress.c_str (), listeningPort);
    }

    // We must specify this here to avoid the IP2IP_PROFILE
    // to create a transport with name 0.0.0.0 to appear in the via header
    if (local)
    	listeningAddress = loadSIPLocalIP ();

    if (listeningAddress == "" || listeningPort == 0) {
        _error ("UserAgent: Error invalid address for new udp transport");
        return NULL;
    }

    /* Init published name */
    pjsip_host_port a_name;
    pj_bzero (&a_name, sizeof (pjsip_host_port));
    pj_cstr (&a_name.host, listeningAddress.c_str());
    a_name.port = listeningPort;

    pjsip_transport *transport;
    if (pjsip_udp_transport_start (_endpt, &bound_addr, &a_name, 1, &transport) != PJ_SUCCESS)
        transport = NULL;

    // Print info from transport manager associated to endpoint
    pjsip_tpmgr * tpmgr = pjsip_endpt_get_tpmgr (_endpt);
    pjsip_tpmgr_dump_transports (tpmgr);

    return transport;
}

std::string SIPVoIPLink::findLocalAddressFromUri (const std::string& uri, pjsip_transport *transport)
{
    pj_str_t localAddress;
    pjsip_transport_type_e transportType;
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
        pjsip_tpselector *tp_sel = NULL;
    	if (transport)
			tp_sel = initTransportSelector (transport, tmp_pool);

		status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, tp_sel, &localAddress, &port);
    } else {
        status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, NULL, &localAddress, &port);
    }

    if (status != PJ_SUCCESS) {
        _debug ("SIP: Failed to find local address from transport");
        return machineName;
    }

    std::string localaddr (localAddress.ptr, localAddress.slen);

    if (localaddr == "0.0.0.0")
    	localaddr = loadSIPLocalIP ();

    _debug ("SIP: Local address discovered from attached transport: %s", localaddr.c_str());

    pj_pool_release(tmp_pool);

    return localaddr;
}



pjsip_tpselector *SIPVoIPLink::initTransportSelector (pjsip_transport *transport, pj_pool_t *tp_pool)
{
	assert(transport);
	pjsip_tpselector *tp = (pjsip_tpselector *) pj_pool_zalloc (tp_pool, sizeof (pjsip_tpselector));
	tp->type = PJSIP_TPSELECTOR_TRANSPORT;
	tp->u.transport = transport;
	return tp;
}

int SIPVoIPLink::findLocalPortFromUri (const std::string& uri, pjsip_transport *transport)
{
    pj_str_t localAddress;
    pjsip_transport_type_e transportType;
    int port;
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
    pjsip_uri * genericUri = pjsip_parse_uri (tmp_pool, tmp.ptr, tmp.slen, 0);
    if (!genericUri) {
        _debug ("UserAgent: genericUri is NULL in findLocalPortFromUri");
        return DEFAULT_SIP_PORT;
    }

    pjsip_sip_uri * sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri (genericUri);
    if (!sip_uri) {
        _debug ("UserAgent: Invalid uri in findLocalAddressFromTransport");
        return DEFAULT_SIP_PORT;
    }

    if (PJSIP_URI_SCHEME_IS_SIPS (sip_uri)) {
        transportType = PJSIP_TRANSPORT_TLS;
        port = DEFAULT_SIP_TLS_PORT;
    } else {
        if (transport == NULL) {
            _debug ("UserAgent: transport is NULL in findLocalPortFromUri - Try the local UDP transport");
            transport = _localUDPTransport;
        }

        transportType = PJSIP_TRANSPORT_UDP;

        port = DEFAULT_SIP_PORT;
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
        pjsip_tpselector *tp_sel = NULL;

        if (transport)
        	tp_sel = initTransportSelector (transport, tmp_pool);

        status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, tp_sel, &localAddress, &port);
    } else
        status = pjsip_tpmgr_find_local_addr (tpmgr, tmp_pool, transportType, NULL, &localAddress, &port);


    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: failed to find local address from transport");
    }

    _debug ("UserAgent: local port discovered from attached transport: %i", port);

    pj_pool_release(tmp_pool);

    return port;
}


pj_status_t SIPVoIPLink::createTlsTransport (SIPAccount *account, std::string remoteAddr)
{
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
    pj_status_t success = pjsip_endpt_acquire_transport (_endpt, PJSIP_TRANSPORT_TLS, &rem_addr, sizeof (rem_addr), NULL, &tls);

    if (success != PJ_SUCCESS)
        _debug ("UserAgent: Error could not create TLS transport");
    else
        account->setAccountTransport (tls);

    return success;
}

pj_status_t SIPVoIPLink::createAlternateUdpTransport (SIPAccount *account)
{
    pj_status_t status;
    pj_sockaddr_in pub_addr;

    _debug ("UserAgent: Create Alternate UDP transport");

    pj_str_t stunServer = account->getStunServerName ();
    pj_uint16_t stunPort = account->getStunPort ();

    status = stunServerResolve (account);
    if (status != PJ_SUCCESS) {
        _error ("UserAgent: Error: Resolving STUN server: %i", status);
        return status;
    }

    pj_sock_t sock = PJ_INVALID_SOCKET;

    _debug ("UserAgent: Initializing IPv4 socket on %s:%i", stunServer.ptr, stunPort);
    pj_sockaddr_in boundAddr;
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

    pjsip_host_port a_name;
    a_name.host = pj_str (pj_inet_ntoa (pub_addr.sin_addr));
    a_name.port = pj_ntohs (pub_addr.sin_port);

    std::string listeningAddress = std::string (a_name.host.ptr, a_name.host.slen);
    int listeningPort = a_name.port;

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

    pjsip_tpmgr_dump_transports (pjsip_endpt_get_tpmgr (_endpt));

    return PJ_SUCCESS;
}


void SIPVoIPLink::shutdownSipTransport (SIPAccount *account)
{
    _debug ("UserAgent: Shutdown Sip Transport");

    pjsip_transport *tr = account->getAccountTransport();
    if (tr) {
        pjsip_transport_dec_ref(tr);
        account->setAccountTransport (NULL);
    }
}

std::string SIPVoIPLink::parseDisplayName(char * buffer)
{
    // Parse the display name from "From" header
    char* from_header = strstr (buffer, "From: ");
    if (!from_header)
    	return "";

	std::string temp (from_header);
	int begin_displayName = temp.find ("\"") + 1;
	int end_displayName = temp.rfind ("\"");
	std::string displayName = temp.substr (begin_displayName, end_displayName - begin_displayName);

	if (displayName.size() > 25)
		return "";
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

std::string SIPVoIPLink::loadSIPLocalIP ()
{
    pj_sockaddr ip_addr;
    if (pj_gethostip (pj_AF_INET(), &ip_addr) == PJ_SUCCESS)
        return std::string (pj_inet_ntoa (ip_addr.ipv4.sin_addr));
    return "";
}

pjsip_route_hdr *SIPVoIPLink::createRouteSet(Account *account, pj_pool_t *hdr_pool)
{
	std::string host = "";
	std::string port = "";

	SIPAccount *sipaccount = dynamic_cast<SIPAccount *>(account);
    std::string route = sipaccount->getServiceRoute();
    _debug ("UserAgent: Set Service-Route with %s", route.c_str());

    size_t found = route.find(":");
	if(found != std::string::npos) {
		host = route.substr(0, found);
		port = route.substr(found + 1, route.length());
	}
	else {
		host = route;
		port = "0";
	}

	pjsip_route_hdr *route_set = pjsip_route_hdr_create (hdr_pool);
    pjsip_route_hdr *routing = pjsip_route_hdr_create (hdr_pool);
    pjsip_sip_uri *url = pjsip_sip_uri_create (hdr_pool, 0);
    routing->name_addr.uri = (pjsip_uri*) url;
    pj_strdup2 (hdr_pool, &url->host, host.c_str());
    url->port = atoi(port.c_str());

    pj_list_push_back (route_set, pjsip_hdr_clone (hdr_pool, routing));

    return route_set;

}

void SIPVoIPLink::pjsipShutdown (void)
{
    if (_endpt) {
        _debug ("UserAgent: Shutting down...");
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
    /* Ideally we shouldn't call pj_thread_sleep() and rather
     * CActiveScheduler::WaitForAnyRequest() here, but that will
     * drag in Symbian header and it doesn't look pretty.
     */
    pj_thread_sleep (1000);
#else
    pj_time_val timeout, now, tv;

    pj_gettimeofday (&timeout);
    timeout.msec += 1000;
    pj_time_val_normalize (&timeout);

    tv.sec = 0;
    tv.msec = 10;

    do {
        pjsip_endpt_handle_events (_endpt, &tv);
        pj_gettimeofday (&now);
    } while (PJ_TIME_VAL_LT (now, timeout));

#endif
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
    if (status == PJ_SUCCESS)
        result->servers = *addr;
}

void setVoicemailInfo (std::string account, pjsip_msg_body *body)
{
    _debug ("UserAgent: checking the voice message!");
    // The voicemail message is formated like that:
    // Voice-Message: 1/0  . 1 is the number we want to retrieve in this case

	int voicemail;
	if (sscanf((const char*)body->data, "Voice-Message: %d/", &voicemail) == 1 && voicemail != 0)
		Manager::instance().startVoiceMessageNotification (account, voicemail);
}

// This callback is called when the invite session state has changed
void invite_session_state_changed_cb (pjsip_inv_session *inv, pjsip_event *e)
{
    _debug ("UserAgent: Call state changed to %s", invitationStateMap[inv->state]);

    /* Retrieve the call information */
    SIPCall *call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);
    if (call == NULL)
        return;

    // If the call is a direct IP-to-IP call
    SIPVoIPLink * link = NULL;
    if (call->getCallConfiguration () == Call::IPtoIP) {
        link = SIPVoIPLink::instance ();
    } else {
        std::string accId = Manager::instance().getAccountFromCall (call->getCallId());
        link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));
    }

    if (inv->state != PJSIP_INV_STATE_CONFIRMED) {
        // Update UI with the current status code and description
        pjsip_transaction * tsx = e->body.tsx_state.tsx;
        int statusCode = tsx ? tsx->status_code : 404;
        if (statusCode) {
            const pj_str_t * description = pjsip_get_status_text (statusCode);
            // test wether or not dbus manager is instantiated, if not no need to notify the client
            Manager::instance().getDbusManager()->getCallManager()->sipCallStateChanged (call->getCallId(), std::string (description->ptr, description->slen), statusCode);
        }
    }

    if (inv->state == PJSIP_INV_STATE_EARLY && e->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
    	// The call is ringing - We need to handle this case only on outgoing call
        call->setConnectionState (Call::Ringing);
        Manager::instance().peerRingingCall (call->getCallId());
    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
    	// After we sent or received a ACK - The connection is established
        link->SIPCallAnswered (call, e->body.tsx_state.src.rdata);
    } else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

        _debug ("UserAgent: State: %s. Cause: %.*s", invitationStateMap[inv->state], (int) inv->cause_text.slen, inv->cause_text.ptr);

        std::string accId = Manager::instance().getAccountFromCall (call->getCallId());
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
    _debug ("UserAgent: %s (%d): on_rx_offer REINVITE", __FILE__, __LINE__);

    SIPCall *call = (SIPCall*) inv->mod_data[getModId() ];
    if (!call)
        return;

    std::string accId = Manager::instance().getAccountFromCall (call->getCallId());
    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accId));

    call->getLocalSDP()->receiveOffer (offer, account->getActiveCodecs ());
    call->getLocalSDP()->startNegotiation();

    pjsip_inv_set_sdp_answer (call->getInvSession(), call->getLocalSDP()->getLocalSdpSession());
}

void sdp_create_offer_cb (pjsip_inv_session *inv, pjmedia_sdp_session **p_offer)
{
    _info ("UserAgent: Create new SDP offer");

    /* Retrieve the call information */
    SIPCall * call = NULL;
    call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);

    std::string callid = call->getCallId();
    std::string accountid = Manager::instance().getAccountFromCall (callid);

    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (accountid));

    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accountid));

    // Set the local address
    std::string localAddress = link->getInterfaceAddrFromName (account->getLocalInterface ());
    // Set SDP parameters - Set to local
    std::string addrSdp = localAddress;

    _debug ("UserAgent: Local Address for IP2IP call: %s", localAddress.c_str());

    // If local address bound to ANY, reslove it using PJSIP
    if (localAddress == "0.0.0.0") {
    	localAddress = link->loadSIPLocalIP ();
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

    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (""));
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

            Manager::instance().getDbusManager()->getCallManager()->secureSdesOn (call->getCallId());
        } else {
            Manager::instance().getDbusManager()->getCallManager()->secureSdesOff (call->getCallId());
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
        std::string accountID = Manager::instance().getAccountFromCall (call->getCallId());
        SIPAccount *account = (SIPAccount *) Manager::instance().getAccount (accountID);

        if (account->getSrtpFallback())
            call->getAudioRtp()->initAudioSymmetricRtpSession ();
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
    _debug ("UserAgent: Transaction changed to state %s", transactionStateMap[tsx->state]);

    if (tsx->role != PJSIP_ROLE_UAS || tsx->state != PJSIP_TSX_STATE_TRYING)
    	return;

    if (pjsip_method_cmp (&tsx->method, &pjsip_refer_method) ==0) {
        /** Handle the refer method **/
        onCallTransfered (inv, e->body.tsx_state.src.rdata);
        return;
    }

    if (!e)
    	return;

    pjsip_tx_data* t_data;

    if (e->body.rx_msg.rdata) {
        pjsip_rx_data *r_data = e->body.rx_msg.rdata;
		if (r_data && r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {
			std::string request =  pjsip_rx_data_get_info (r_data);
			_debug ("UserAgent: %s", request.c_str());

			if (request.find ("NOTIFY") == std::string::npos && request.find ("INFO") != std::string::npos) {
				pjsip_dlg_create_response (inv->dlg, r_data, PJSIP_SC_OK, NULL, &t_data);
				pjsip_dlg_send_response (inv->dlg, tsx, t_data);
				return;
			}
		}
	}

	// Incoming TEXT message
	if (e->body.tsx_state.src.rdata) {
		// Get the message inside the transaction
	    pjsip_rx_data *r_data = e->body.tsx_state.src.rdata;
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

		std::string message, from;

		sfl::InstantMessaging *module = Manager::instance().getInstantMessageModule();

		try {
			// retrive message from formated text
			message = module->findTextMessage (formatedMessage);

			// retreive the recipient-list of this message
			std::string urilist = module->findTextUriList (formatedMessage);
			sfl::InstantMessaging::UriList list = module->parseXmlUriList (urilist);

			// If no item present in the list, peer is considered as the sender
			if (list.empty()) {
				from = call->getPeerNumber ();
			} else {
				sfl::InstantMessaging::UriEntry entry = list.front();
				from = entry[IM_XML_URI];
				if (from == "Me")
					from = call->getPeerNumber ();
			}

		} catch (sfl::InstantMessageException &e) {
			_error ("SipVoipLink: %s", e.what());
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

void registration_cb (struct pjsip_regc_cbparam *param)
{
	SIPAccount *account = static_cast<SIPAccount *>(param->token);

    if (account == NULL) {
        _error("Account is NULL in registration_cb.");
        return;
    }

    const pj_str_t * description = pjsip_get_status_text (param->code);

    if (param->code && description) {
        std::string state(description->ptr, description->slen);
        Manager::instance().getDbusManager()->getCallManager()->registrationStateChanged (account->getAccountID(), state, param->code);
        std::pair<int, std::string> details (param->code, state);
        // TODO: there id a race condition for this ressource when closing the application
        account->setRegistrationStateDetailed (details);
    }

    if (param->status != PJ_SUCCESS) {
        account->setRegistrationState (ErrorAuth);
        account->setRegister (false);

        SIPVoIPLink::instance ()->shutdownSipTransport (account);
        return;
    }

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

		SIPVoIPLink::instance ()->shutdownSipTransport (account);

	} else {
		if (account->isRegister())
			account->setRegistrationState (Registered);
		else {
			account->setRegistrationState (Unregistered);
			SIPVoIPLink::instance ()->shutdownSipTransport (account);
		}
	}
}

static void getRemoteSdpFromOffer (pjsip_rx_data *rdata, pjmedia_sdp_session** r_sdp)
{
    pjsip_msg_body *body = rdata->msg_info.msg->body;
    if (!body || pjmedia_sdp_parse (rdata->tp_info.pool, (char*) body->data, body->len, r_sdp) != PJ_SUCCESS)
		*r_sdp = NULL;
}


/**
 * Helper function to parse incoming OPTION message
 */
static void handleIncomingOptions (pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    pjsip_response_addr res_addr;
    const pjsip_hdr *cap_hdr;

    /* Create basic response. */
    if (pjsip_endpt_create_response (_endpt, rdata, PJSIP_SC_OK, NULL, &tdata) != PJ_SUCCESS)
        return;

    /* Add Allow header */
    cap_hdr = pjsip_endpt_get_capability (_endpt, PJSIP_H_ALLOW, NULL);
    if (cap_hdr)
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));

    /* Add Accept header */
    cap_hdr = pjsip_endpt_get_capability (_endpt, PJSIP_H_ACCEPT, NULL);
    if (cap_hdr)
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));

    /* Add Supported header */
    cap_hdr = pjsip_endpt_get_capability (_endpt, PJSIP_H_SUPPORTED, NULL);
    if (cap_hdr)
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));

    /* Add Allow-Events header from the evsub module */
    cap_hdr = pjsip_evsub_get_allow_events_hdr (NULL);
    if (cap_hdr)
        pjsip_msg_add_hdr (tdata->msg, (pjsip_hdr*) pjsip_hdr_clone (tdata->pool, cap_hdr));

    /* Send response statelessly */
    pjsip_get_response_addr (tdata->pool, rdata, &res_addr);

    if (pjsip_endpt_send_response (_endpt, &res_addr, tdata, NULL, NULL) != PJ_SUCCESS)
        pjsip_tx_data_dec_ref (tdata);
}


// Optional function to be called to process incoming request message.
pj_bool_t
transaction_request_cb (pjsip_rx_data *rdata)
{
    unsigned options = 0;
    pjsip_dialog* dialog, *replaced_dlg;
    pjsip_tx_data *tdata;
    pjsip_tx_data *response;
    pjsip_inv_session *inv;
    pjmedia_sdp_session *r_sdp;

    _info ("UserAgent: Transaction REQUEST received using transport: %s %s (refcnt=%d)",
    						rdata->tp_info.transport->obj_name,
    						rdata->tp_info.transport->info,
    						(int) pj_atomic_get (rdata->tp_info.transport->ref_cnt));

    // No need to go any further on incoming ACK
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD && pjsip_rdata_get_dlg (rdata)) {
        _info ("UserAgent: received an ACK");
        return true;
    }

    /* First, let's got the username and server name from the invite.
     * We will use them to detect which account is the callee.
     */
    pjsip_sip_uri *sip_to_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (rdata->msg_info.to->uri);
    pjsip_sip_uri *sip_from_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (rdata->msg_info.from->uri);

    std::string userName = std::string (sip_to_uri->user.ptr, sip_to_uri->user.slen);
    std::string server = std::string (sip_from_uri->host.ptr, sip_from_uri->host.slen);
    _debug ("UserAgent: Call to %s, from %s", userName.data(), server.data());

    // Get the account id of callee from username and server
    std::string account_id = Manager::instance().getAccountIdFromNameAndServer (userName, server);
    _debug ("UserAgent: Account ID for this call, %s", account_id.c_str());

    /* If we don't find any account to receive the call */
    if (account_id.empty())
        _debug ("UserAgent: Username %s doesn't match any account, using IP2IP!",userName.c_str());

    /* Get the voip link associated to the incoming call */
    /* The account must before have been associated to the call in ManagerImpl */
    SIPVoIPLink *link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (account_id));
    assert(link);

    // retrive display name from the message buffer
    std::string displayName = SIPVoIPLink::parseDisplayName(rdata->msg_info.msg_buf);
    _debug("UserAgent: Display name for this call %s", displayName.c_str());

    /* Now, it is the time to find the information of the caller */
    // Store the peer number
    char tmp[PJSIP_MAX_URL_SIZE];
    int length = pjsip_uri_print (PJSIP_URI_IN_FROMTO_HDR, sip_from_uri, tmp, PJSIP_MAX_URL_SIZE);
    std::string peerNumber (tmp, length);

    //Remove sip: prefix
    SIPVoIPLink::stripSipUriPrefix(peerNumber);
    _debug ("UserAgent: Peer number: %s", peerNumber.c_str());

    // Get the server voicemail notification
    // Catch the NOTIFY message
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD) {

        std::string method_name = "NOTIFY";

        // Retrieve all the message. Should contains only the method name but ...
        std::string request(rdata->msg_info.msg->line.req.method.name.ptr);

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
    if (pjsip_inv_verify_request (rdata, &options, NULL, NULL, _endpt, NULL) != PJ_SUCCESS) {
        pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED,
        NULL, NULL, NULL);
        return true;
    }

    /******************************************* URL HOOK *********************************************/

    if (Manager::instance().hookPreference.getSipEnabled()) {
        _debug ("UserAgent: Set sip url hooks");

        std::string header_value(fetchHeaderValue (rdata->msg_info.msg, Manager::instance().hookPreference.getUrlSipField()));

        if (header_value.size () < header_value.max_size()) {
            if (not header_value.empty()) {
                UrlHook::runAction (Manager::instance().hookPreference.getUrlCommand(), header_value);
            }
        } else
            throw std::length_error ("UserAgent: Url exceeds std::string max_size");
    }

    /************************************************************************************************/
    _info ("UserAgent: Create a new call");

    // Generate a new call ID for the incoming call!
    SIPCall* call = new SIPCall (Manager::instance().getNewCallID(), Call::Incoming, _cp);
    Manager::instance().associateCallToAccount (call->getCallId(), account_id);

	// May use the published address as well
    std::string addrToUse = SIPVoIPLink::instance ()->getInterfaceAddrFromName (account->getLocalInterface ());
	std::string addrSdp = account->isStunEnabled ()
			? account->getPublishedAddress ()
			: addrToUse;

	pjsip_transport* transport = account->getAccountTransport();
	assert(transport);

	// Set the appropriate transport to have the right VIA header
    pjsip_tpselector *tp = link->initTransportSelector (transport, call->getMemoryPool());

	if (transport)
		_debug ("UserAgent: SIP transport for this account: %s %s (refcnt=%i)",
			transport->obj_name, transport->info, (int) pj_atomic_get (transport->ref_cnt));

    if (addrToUse == "0.0.0.0")
    	addrToUse = link->loadSIPLocalIP ();

    if (addrSdp == "0.0.0.0")
        addrSdp = addrToUse;

    call->setConnectionState (Call::Progressing);
    call->setPeerNumber (peerNumber);
    call->setDisplayName (displayName);
    call->initRecFileName (peerNumber);

    // Have to do some stuff with the SDP
    // Set the codec map, IP, peer number and so on... for the SIPCall object
    setCallMediaLocal (call, addrToUse);

    // We retrieve the remote sdp offer in the rdata struct to begin the negotiation
    call->getLocalSDP()->setLocalIP (addrSdp);

    // Init audio rtp session
    try {
        _debug ("UserAgent: Create RTP session for this call");
        call->getAudioRtp()->initAudioRtpConfig ();
        call->getAudioRtp()->initAudioSymmetricRtpSession ();
    } catch (...) {
        _warn ("UserAgent: Error: Failed to create rtp thread from answer");
    }

    // Retreive crypto offer from body, if any
    if (rdata->msg_info.msg->body) {
        char sdpbuffer[1000];
        int len = rdata->msg_info.msg->body->print_body (rdata->msg_info.msg->body, sdpbuffer, sizeof sdpbuffer);
        if (len == -1) // error
        	len = 0;
        std::string sdpoffer = std::string (sdpbuffer, len);
        size_t start = sdpoffer.find ("a=crypto:");

        // Found crypto header in SDP
        if (start != std::string::npos) {
            std::string cryptoHeader = sdpoffer.substr (start, (sdpoffer.size() - start) -1);
            _debug ("UserAgent: Found incoming crypto offer: %s, init SRTP session", cryptoHeader.c_str());

            CryptoOffer crypto_offer;
            crypto_offer.push_back (cryptoHeader);

			// init local cryptografic capabilities for negotiation
			std::vector<sfl::CryptoSuiteDefinition>localCapabilities;
			for (int i = 0; i < 3; i++)
				localCapabilities.push_back (sfl::CryptoSuites[i]);
			sfl::SdesNegotiator sdesnego (localCapabilities, crypto_offer);
			if (sdesnego.negotiate()) {
				_debug ("UserAgent: SDES negotiation successfull, create RTP session for this call");
				try {
					call->getAudioRtp()->setRemoteCryptoInfo (sdesnego);
					call->getAudioRtp()->initLocalCryptoInfo ();
				} catch (...) {
					_warn ("UserAgent: Error: Failed to create rtp thread from answer");
				}
			}
        }
    }

    if (call->getLocalSDP()->receiveOffer (r_sdp, account->getActiveCodecs ()) != PJ_SUCCESS) {
        _warn ("UserAgent: fail in receiving initial offer");
        goto fail;
    }

    // Init audio rtp session
    try {
        _debug ("UserAgent: Create RTP session for this call");
        // Init default codec for early media session
        sfl::Codec* audiocodec = Manager::instance().getAudioCodecFactory().instantiateCodec (PAYLOAD_CODEC_ULAW);
        call->getAudioRtp()->start (static_cast<sfl::AudioCodec *>(audiocodec));
    } catch (...) {
        _warn ("UserAgent: Error: Failed to create rtp thread from answer");
    }


    /* Create the local dialog (UAS) */
    if (pjsip_dlg_create_uas (pjsip_ua_instance(), rdata, NULL, &dialog) != PJ_SUCCESS) {
        _warn ("UserAgent: Error: Failed to create uas dialog");
        goto fail;
    }

    // Specify media capability during invite session creation
    pjsip_inv_create_uas (dialog, rdata, call->getLocalSDP()->getLocalSdpSession(), 0, &inv);

    // Explicitly set the transport, set_transport methods increment transport's reference counter
    PJ_ASSERT_RETURN (pjsip_dlg_set_transport (dialog, tp) == PJ_SUCCESS, 1);

    // Associate the call in the invite session
    inv->mod_data[_mod_ua.id] = call;

    // Check whether Replaces header is present in the request and process accordingly.
    if (pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response) != PJ_SUCCESS) {
    	_warn("UserAgent: Error: Something wrong with Replaces request.");
        // Respond with 500 (Internal Server Error)
    	pjsip_endpt_respond_stateless(_endpt, rdata, 500, NULL, NULL, NULL);
    }

    // Check if call have been transfered
    if (replaced_dlg) { // If Replace header present
    	_debug("UserAgent: Replace request foud");

    	// Always answer the new INVITE with 200, regardless whether
    	// the replaced call is in early or confirmed state.
    	if (pjsip_inv_answer(inv, 200, NULL, NULL, &response) == PJ_SUCCESS)
    		pjsip_inv_send_msg(inv, response);

    	// Get the INVITE session associated with the replaced dialog.
    	pjsip_inv_session *replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

    	// Disconnect the "replaced" INVITE session.
         if (pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL, &tdata) == PJ_SUCCESS && tdata)
             pjsip_inv_send_msg(replaced_inv, tdata);

         call->replaceInvSession(inv);
    } else { // Prooceed with normal call flow

        // Send a 180 Ringing response
        _info ("UserAgent: Send a 180 Ringing response");
        PJ_ASSERT_RETURN (pjsip_inv_initial_answer (inv, rdata, PJSIP_SC_RINGING, NULL, NULL, &tdata) == PJ_SUCCESS, 1);
        PJ_ASSERT_RETURN (pjsip_inv_send_msg (inv, tdata) == PJ_SUCCESS, 1);

    	call->setInvSession (inv);
    	call->setConnectionState (Call::Ringing);

    	_debug ("UserAgent: Add call to account link");

    	if (Manager::instance().incomingCall (call, account_id)) {
    		// Add this call to the callAccountMap in ManagerImpl
    		Manager::instance().getAccountLink (account_id)->addCall (call);
    	} else {
    		_warn ("UserAgent: Fail to notify UI!");
    		goto fail;
    	}
    }

    return true;

fail:
	delete call;
	pjsip_endpt_respond_stateless (_endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR,
			NULL, NULL, NULL);
	return false;
}

pj_bool_t transaction_response_cb (pjsip_rx_data *rdata)
{
    _info ("UserAgent: Transaction response using transport: %s %s (refcnt=%d)",
		rdata->tp_info.transport->obj_name,
		rdata->tp_info.transport->info,
		(int) pj_atomic_get (rdata->tp_info.transport->ref_cnt));

    pjsip_dialog *dlg = pjsip_rdata_get_dlg (rdata);
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
            	/**
            	 * Send an ACK message inside a transaction. PJSIP send automatically, non-2xx ACK response.
            	 * ACK for a 2xx response must be send using this method.
            	 */
                _info ("UserAgent: Received 200 OK response");
                pjsip_tx_data *tdata;
                pjsip_dlg_create_request (dlg, &pjsip_ack_method, rdata->msg_info.cseq->cseq, &tdata);
                pjsip_dlg_send_request (dlg, tdata,-1, NULL);
            }
        }
    }

    return PJ_SUCCESS;
}

void onCallTransfered (pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    SIPCall *currentCall = (SIPCall *) inv->mod_data[_mod_ua.id];
    if (currentCall == NULL) {
        _error ("UserAgent: Call doesn't exist (%s, %s)", __FILE__, __LINE__);
        return;
    }

    static const pj_str_t str_refer_to = { (char*) "Refer-To", 8};
    pjsip_generic_string_hdr *refer_to =
		(pjsip_generic_string_hdr*) pjsip_msg_find_hdr_by_name (rdata->msg_info.msg, &str_refer_to, NULL);
    if (!refer_to) {
        /* Invalid Request : No Refer-To header! */
        _debug ("UserAgent: Received REFER without Refer-To header!");
        pjsip_dlg_respond (inv->dlg, rdata, 400, NULL, NULL, NULL);
        return;
    }

    std::string sipUri = std::string (refer_to->hvalue.ptr, refer_to->hvalue.slen);

    _debug ("UserAgent: Call to %.*s is being transfered to %s",
    				(int) inv->dlg->remote.info_str.slen,
    				inv->dlg->remote.info_str.ptr,
    				sipUri.c_str());

    /* Now make the outgoing call. */
    SIPVoIPLink::instance()->newOutgoingCall(Manager::instance().getNewCallID(), sipUri);
    Manager::instance().hangupCall(currentCall->getCallId());
}

void transfer_client_cb (pjsip_evsub *sub, pjsip_event *event)
{
    /*
     * When subscription is accepted (got 200/OK to REFER), check if
     * subscription suppressed.
     */
    switch(pjsip_evsub_get_state (sub)) {
    case PJSIP_EVSUB_STATE_ACCEPTED:
        _debug ("UserAgent: Transfer received, waiting for notifications. ");
        /* Must be receipt of response message */
        pj_assert(event->type == PJSIP_EVENT_TSX_STATE &&
                  event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
        break;

	case PJSIP_EVSUB_STATE_ACTIVE:
	case PJSIP_EVSUB_STATE_TERMINATED:
	{
        /*
         * On incoming NOTIFY, notify application about call transfer progress.
         */

        _debug("UserAgent: PJSIP_EVSUB_STATE_ACTIVE PJSIP_EVSUB_STATE_TERMINATED");

        SIPVoIPLink *link = reinterpret_cast<SIPVoIPLink *> (pjsip_evsub_get_mod_data (sub, _mod_ua.id));

        /* When subscription is terminated, clear the xfer_sub member of
         * the inv_data.
         */

        if (pjsip_evsub_get_state (sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
            _debug ("UserAgent: Xfer client subscription terminated");
        }

        /* Application is not interested with call progress status */
        if (!link or !event)
            return;

        pjsip_rx_data* r_data = event->body.rx_msg.rdata;
        if (!r_data)
        	return;
        std::string request =  pjsip_rx_data_get_info (r_data);

        pjsip_status_line status_line;

        /* This better be a NOTIFY request */
        if (r_data->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD and
                request.find ("NOTIFY") != std::string::npos) {

            pjsip_msg_body *body = r_data->msg_info.msg->body;
            if (!body) {
                _warn ("UserAgent: Warning! Received NOTIFY without message body");
                return;
            }

            /* Check for appropriate content */
            if (pj_stricmp2 (&body->content_type.type, "message") != 0 or
                    pj_stricmp2 (&body->content_type.subtype, "sipfrag") != 0) {
                _warn ("UserAgent: Warning! Received NOTIFY without message/sipfrag content");
                return;
            }

            /* Try to parse the content */
            if (pjsip_parse_status_line ( (char*) body->data, body->len, &status_line) != PJ_SUCCESS) {
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
        std::string cid = transferCallID[transferID];
        SIPCall *call = dynamic_cast<SIPCall *> (link->getCall (cid));
        if (!call) {
            _warn ("UserAgent:  Call with id %s doesn't exit!", cid.c_str());
            return;
        }

        /* Notify application */
        pj_bool_t cont = pjsip_evsub_get_state (sub) != PJSIP_EVSUB_STATE_TERMINATED;

        _debug ("UserAgent: Notification status line: %d", status_line.code);

        if (status_line.code/100 == 2) {
            _debug ("UserAgent: Received 200 OK on call transfered, stop call!");
            pjsip_tx_data *tdata;

            if (pjsip_inv_end_session (call->getInvSession(), PJSIP_SC_GONE, NULL, &tdata) != PJ_SUCCESS)
                _debug ("UserAgent: Fail to create end session msg!");
            else if (pjsip_inv_send_msg (call->getInvSession(), tdata) != PJ_SUCCESS)
				_debug ("UserAgent: Fail to send end session msg!");

            Manager::instance().hangupCall(call->getCallId());
            cont = PJ_FALSE;
        }

        if (!cont)
            pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
        break;
	}
	default:
		break;
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
    case PJSIP_EVSUB_STATE_TERMINATED:
		pjsip_evsub_set_mod_data (sub, _mod_ua.id, NULL);
        _debug ("UserAgent: Xfer server subscription terminated");
        break;
	default:
		break;
	}
}

/*****************************************************************************************************************/


void setCallMediaLocal (SIPCall* call, const std::string &localIP)
{
	assert(call);
	std::string account_id = Manager::instance().getAccountFromCall (call->getCallId ());
    SIPAccount *account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));

	unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
	unsigned int callLocalExternAudioPort = account->isStunEnabled()
					? account->getStunPort()
					: callLocalAudioPort;

	call->setLocalIp (localIP);
	call->setLocalAudioPort (callLocalAudioPort);
	call->getLocalSDP()->setPortToAllMedia (callLocalExternAudioPort);
}

std::string fetchHeaderValue (pjsip_msg *msg, std::string field)
{
    /* Convert the field name into pjsip type */
    pj_str_t name = pj_str ( (char*) field.c_str());

    /* Get the header value and convert into string*/
    pjsip_generic_string_hdr *hdr = (pjsip_generic_string_hdr*) pjsip_msg_find_hdr_by_name (msg, &name, NULL);
    if (!hdr)
        return "";

    std::string value = std::string(hdr->hvalue.ptr, hdr->hvalue.slen);

    size_t pos = value.find ("\n");
    if (pos == std::string::npos)
        return "";

    return value.substr (0, pos);
}

std::vector<std::string> SIPVoIPLink::getAllIpInterface (void)
{
    pj_sockaddr addrList[16];
    unsigned int addrCnt = PJ_ARRAY_SIZE (addrList);

    std::vector<std::string> ifaceList;

    if (pj_enum_ip_interface (pj_AF_INET(), &addrCnt, addrList) != PJ_SUCCESS)
        return ifaceList;

    for (int i = 0; i < (int) addrCnt; i++) {
        char tmpAddr[PJ_INET_ADDRSTRLEN];
        pj_sockaddr_print (&addrList[i], tmpAddr, sizeof (tmpAddr), 0);
        ifaceList.push_back (std::string (tmpAddr));
        _debug ("Local interface %s", tmpAddr);
    }

    return ifaceList;
}


static int get_iface_list (struct ifconf *ifconf)
{
    int sock, rval;

    if ( (sock = socket (AF_INET,SOCK_STREAM,0)) < 0)
    	return -1;

    if ( (rval = ioctl (sock, SIOCGIFCONF , (char*) ifconf)) < 0)
    	return -2;

    close (sock);

    return rval;
}

std::vector<std::string> SIPVoIPLink::getAllIpInterfaceByName (void)
{
    std::vector<std::string> ifaceList;

    static struct ifreq ifreqs[20];
    struct ifconf ifconf;

    // add the default
    ifaceList.push_back ("default");

    ifconf.ifc_buf = (char*) (ifreqs);
    ifconf.ifc_len = sizeof (ifreqs);

    if (get_iface_list (&ifconf) < 0)
        _debug ("getAllIpInterfaceByName error could not get interface list\n");

    int nifaces =  ifconf.ifc_len/sizeof (struct ifreq);

    _debug ("Interfaces (count = %d):\n", nifaces);

    for (int i = 0; i < nifaces; i++) {
        _debug ("  %s  ", ifreqs[i].ifr_name);
        ifaceList.push_back (std::string (ifreqs[i].ifr_name));
        _debug ("    %s\n", getInterfaceAddrFromName (ifreqs[i].ifr_name).c_str());
    }

    return ifaceList;
}

std::string SIPVoIPLink::getInterfaceAddrFromName (std::string ifaceName)
{
    int fd = socket (AF_INET, SOCK_DGRAM,0);
    if (fd < 0) {
        _error ("UserAgent: Error: could not open socket: %m");
        return "";
    }

    struct ifreq ifr;
    memset (&ifr, 0, sizeof (struct ifreq));
    strcpy (ifr.ifr_name, ifaceName.c_str());
    ifr.ifr_addr.sa_family = AF_INET;

    if (ioctl (fd, SIOCGIFADDR, &ifr) < 0)
        _debug ("UserAgent: Use default interface (0.0.0.0)");

    struct sockaddr_in *saddr_in = (struct sockaddr_in *) &ifr.ifr_addr;
    std::string addr (inet_ntoa (saddr_in->sin_addr));

    close (fd);

    return addr;
}
