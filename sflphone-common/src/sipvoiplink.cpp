/*
 *  Copyright (C) 2004-2009 Savoir-Faire Linux inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include "sipvoiplink.h"
#include "eventthread.h"
#include "sipcall.h"
#include "sipaccount.h"
#include "audio/audiortp.h"

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#define CAN_REINVITE        1


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
bool setCallAudioLocal (SIPCall* call, std::string localIP, bool stun, std::string server);

void handle_incoming_options (pjsip_rx_data *rxdata);

std::string fetch_header_value (pjsip_msg *msg, std::string field);

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
void call_on_media_update (pjsip_inv_session *inv UNUSED, pj_status_t status UNUSED);

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
        , _stunServer ("")
        , _localExternAddress ("")
        , _localExternPort (0)
        , _audiortp (new AudioRtp())
        ,_regPort (DEFAULT_SIP_PORT)
        , _useStun (false)
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
    if (_evThread) {
        delete _evThread;
        _evThread = NULL;
    }


    /* Clean shutdown of pjsip library */
    if (initDone()) {
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
    // We have to register the external thread so it could access the pjsip framework
    if (!pj_thread_is_registered())
        pj_thread_register (NULL, desc, &thread);

    // PJSIP polling
    pj_time_val timeout = {0, 10};

    pjsip_endpt_handle_events (_endpt, &timeout);

}

int SIPVoIPLink::sendRegister (AccountID id)
{
    pj_status_t status;
    int expire_value;
    char contactTmp[256];
    pj_str_t svr, aor, contact, useragent;
    pjsip_tx_data *tdata;
    std::string tmp, hostname, username, password;
    SIPAccount *account;
    pjsip_regc *regc;
    pjsip_generic_string_hdr *h;
    pjsip_hdr hdr_list;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));
    hostname = account->getHostname();
    username = account->getUsername();
    password = account->getPassword(); 
    
    _mutexSIP.enterMutex();

    /* Get the client registration information for this particular account */
    regc = account->getRegistrationInfo();
    /* TODO If the registration already exists, delete it */
    /*if(regc) {
        status = pjsip_regc_destroy(regc);
        regc = NULL;
        PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
    }*/

    account->setRegister (true);

    /* Set the expire value of the message from the config file */
    expire_value = Manager::instance().getRegistrationExpireValue();

    /* Update the state of the voip link */
    account->setRegistrationState (Trying);

    if (!validStunServer) {
        account->setRegistrationState (ErrorExistStun);
        account->setRegister (false);
        _mutexSIP.leaveMutex();
        return false;
    }

    /* Create the registration according to the account ID */
    status = pjsip_regc_create (_endpt, (void*) account, &regc_cb, &regc);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to create regc.\n");
        _mutexSIP.leaveMutex();
        return false;
    }

    tmp = "sip:" + hostname;

    pj_strdup2 (_pool, &svr, tmp.data());

    // tmp = "<sip:" + username + "@" + hostname + ";transport=tls>";
    tmp = "<sip:" + username + "@" + hostname + ">";
    pj_strdup2 (_pool, &aor, tmp.data());

    _debug ("<sip:%s@%s:%d>\n", username.data(), _localExternAddress.data(), _localExternPort);
    sprintf (contactTmp, "<sip:%s@%s:%d>", username.data(), _localExternAddress.data(), _localExternPort);
    pj_strdup2 (_pool, &contact, contactTmp);
    account->setContact (contactTmp);

    status = pjsip_regc_init (regc, &svr, &aor, &aor, 1, &contact, 600);   //timeout);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to initialize regc. %d\n", status);   //, regc->str_srv_url.ptr);
        _mutexSIP.leaveMutex();
        return false;
    }

    pjsip_cred_info *cred = account->getCredInfo();

    if (!cred)
        cred = new pjsip_cred_info();

    pj_bzero (cred, sizeof (pjsip_cred_info));

    pj_strdup2 (_pool, &cred->username, username.data());

    cred->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;

    pj_strdup2 (_pool, &cred->data, password.data());

    pj_strdup2 (_pool, &cred->realm, "*");

    pj_strdup2 (_pool, &cred->scheme, "digest");

    pjsip_regc_set_credentials (regc, 1, cred);

    account->setCredInfo (cred);

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

    status = pjsip_regc_send (regc, tdata);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: Unable to send regc request.\n");
        _mutexSIP.leaveMutex();
        return false;
    }

    _mutexSIP.leaveMutex();

    account->setRegistrationInfo (regc);

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
    Account* account;
    pj_status_t status;

    SIPCall* call = new SIPCall (id, Call::Outgoing, _pool);


    if (call) {
        account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (Manager::instance().getAccountFromCall (id)));

        if (!account) {
            _debug ("Error retrieving the account to the make the call with\n");
            call->setConnectionState (Call::Disconnected);
            call->setState (Call::Error);
            delete call;
            call=0;
            return call;
        }

        call->setPeerNumber (getSipTo (toUrl, account->getHostname()));

        setCallAudioLocal (call, getLocalIPAddress(), useStun(), getStunServer());


        try {
            _debug ("CREATE NEW RTP SESSION FROM NEWOUTGOINGCALL\n");
            _audiortp->createNewSession (call);
        } catch (...) {
            _debug ("Failed to create rtp thread from newOutGoingCall\n");
        }




        call->initRecFileName();

        _debug ("Try to make a call to: %s with call ID: %s\n", toUrl.data(), id.data());
        // Building the local SDP offer
        call->getLocalSDP()->set_ip_address (getLocalIP());
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

    int i;
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
        _audiortp->createNewSession (call);
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

        ;

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
        _audiortp->closeRtpSession();
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
        _audiortp->closeRtpSession();
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

    _audiortp->closeRtpSession();

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
        _audiortp->createNewSession (call);
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
    pj_str_t dest;
    AccountID account_id;
    Account* account;

    call = getSIPCall (id);
    call->stopRecording();
    account_id = Manager::instance().getAccountFromCall (id);
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (account_id));

    if (call==0) {
        _debug ("! SIP Failure: Call doesn't exist\n");
        return false;
    }

    tmp_to = SIPToHeader (to);

    if (account) {
        if (tmp_to.find ("@") == std::string::npos) {
            tmp_to = tmp_to + "@" + account->getHostname();
        }
    }

    else {

    }

    _debug ("In transfer, tmp_to is %s\n", tmp_to.data());

    pj_strdup2 (_pool, &dest, tmp_to.data());

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

    _debug ("SIP port listener = %i ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++", _localExternPort);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate (sub, &dest, &tdata);

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

bool SIPVoIPLink::transferStep2()
{
    _audiortp->closeRtpSession();
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

void
SIPVoIPLink::setRecording (const CallID& id)
{
    SIPCall* call = getSIPCall (id);

    if (call)
        call->setRecording();

    // _audiortp->setRecording();
}

bool
SIPVoIPLink::isRecording (const CallID& id)
{
    SIPCall* call = getSIPCall (id);
    _debug ("call->isRecording() %i \n",call->isRecording());

    if (call)
        return call->isRecording();
    else
        return false;
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
    std::string strTo, strFrom;
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_tx_data *tdata;
    pj_str_t from, to, contact;
    AccountID id;
    SIPAccount *account;
    pjsip_inv_session *inv;

    if (!call)
        return false;

    id = Manager::instance().getAccountFromCall (call->getCallId());

    // Get the basic information about the callee account
    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount (id));

    strTo = getSipTo (call->getPeerNumber(), account->getHostname());

    _debug ("            To: %s\n", strTo.data());

    // Generate the from URI
    strFrom = "sip:" + account->getUsername() + "@" + account->getHostname();

    _debug ("              From: %s\n", strFrom.c_str());

    // pjsip need the from and to information in pj_str_t format
    pj_strdup2 (_pool, &from, strFrom.data());

    pj_strdup2 (_pool, &to, strTo.data());

    pj_strdup2 (_pool, &contact, account->getContact().data());

    //_debug("%s %s %s\n", from.ptr, contact.ptr, to.ptr);
    // create the dialog (UAC)
    status = pjsip_dlg_create_uac (pjsip_ua_instance(), &from,
                                   &contact,
                                   &to,
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

    status = pjsip_inv_send_msg (inv, tdata);

    if (status != PJ_SUCCESS) {
        return false;
    }

    return true;
}

std::string SIPVoIPLink::getSipTo (const std::string& to_url, std::string hostname)
{
    // Form the From header field basis on configuration panel
    //bool isRegistered = (_eXosipRegID == EXOSIP_ERROR_STD) ? false : true;

    // add a @host if we are registered and there is no one inside the url
    if (to_url.find ("@") == std::string::npos) {    // && isRegistered) {
        if (!hostname.empty()) {
            return SIPToHeader (to_url + "@" + hostname);
        }
    }

    return SIPToHeader (to_url);
}

std::string SIPVoIPLink::SIPToHeader (const std::string& to)
{
    if (to.find ("sip:") == std::string::npos) {
        return ("sip:" + to);
    } else {
        return to;
    }
}

bool
SIPVoIPLink::SIPCheckUrl (const std::string& url UNUSED)
{
    return true;
}

void
SIPVoIPLink::SIPCallServerFailure (SIPCall *call)
{
    //if (!event->response) { return; }
    //switch(event->response->status_code) {
    //case SIP_SERVICE_UNAVAILABLE: // 500
    //case SIP_BUSY_EVRYWHERE:     // 600
    //case SIP_DECLINE:             // 603
    //SIPCall* call = findSIPCallWithCid(event->cid);
    if (call != 0) {
        _debug ("Server error!\n");
        CallID id = call->getCallId();
        Manager::instance().callFailure (id);
        terminateOneCall (id);
        removeCall (id);
    }

    //break;
    //}
}

void
SIPVoIPLink::SIPCallClosed (SIPCall *call)
{


    // it was without did before
    //SIPCall* call = findSIPCallWithCid(event->cid);
    if (!call) {
        return;
    }

    CallID id = call->getCallId();

    //call->setDid(event->did);

    if (Manager::instance().isCurrentCall (id)) {
        call->setAudioStart (false);
        _debug ("* SIP Info: Stopping AudioRTP when closing\n");
        _audiortp->closeRtpSession();
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
    // do cleanup if exists
    // only cid because did is always 0 in these case..
    //SIPCall* call = findSIPCallWithCid(event->cid);
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


void SIPVoIPLink::setStunServer (const std::string &server)
{
    if (server != "") {

        useStun (true);
        _stunServer = server;
    } else {
        useStun (false);
        _stunServer = std::string ("");
    }
}

bool SIPVoIPLink::new_ip_to_ip_call (const CallID& id, const std::string& to)
{
    SIPCall *call;
    pj_status_t status;
    std::string uri_from, uri_to, hostname;
    std::ostringstream uri_contact;
    pj_str_t from, str_to, contact;
    pjsip_dialog *dialog;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;

    /* Create the call */
    call = new SIPCall (id, Call::Outgoing, _pool);

    if (call) {

        call->setCallConfiguration (Call::IPtoIP);
        call->setPeerNumber (getSipTo (to, getLocalIPAddress()));

        // Generate the from URI
        hostname = pj_gethostname()->ptr;
        uri_from = "sip:" + hostname + "@" + getLocalIPAddress() ;

        // Generate the from URI
        uri_to = "sip:" + to.substr (4, to.length());

        _debug ("get local ip address: %s \n", getLocalIPAddress().c_str());
        // Generate the to URI
        setCallAudioLocal (call, getLocalIPAddress(), useStun(), getStunServer());

        call->initRecFileName();

        // Building the local SDP offer
        call->getLocalSDP()->set_ip_address (getLocalIP());
        call->getLocalSDP()->create_initial_offer();

        try {
            _audiortp->createNewSession (call);
        } catch (...) {
            _debug ("! SIP Failure: Unable to create RTP Session  in SIPVoIPLink::new_ip_to_ip_call (%s:%d)\n", __FILE__, __LINE__);
        }

        // Generate the contact URI
        // uri_contact << "<" << uri_from << ":" << call->getLocalSDP()->get_local_extern_audio_port() << ">";
        uri_contact << "<" << uri_from << ":" << _localExternPort << ">";

        // pjsip need the from and to information in pj_str_t format
        pj_strdup2 (_pool, &from, uri_from.data());

        pj_strdup2 (_pool, &str_to, uri_to.data());

        pj_strdup2 (_pool, &contact, uri_contact.str().data());

        // create the dialog (UAC)
        status = pjsip_dlg_create_uac (pjsip_ua_instance(), &from, &contact, &str_to, NULL, &dialog);

        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

        // Create the invite session for this call
        status = pjsip_inv_create_uac (dialog, call->getLocalSDP()->get_local_sdp_session(), 0, &inv);

        PJ_ASSERT_RETURN (status == PJ_SUCCESS, false);

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

    //nameservers.push_back ("192.168.50.3");
    *servers = nameservers;

    return true;
}

pj_status_t SIPVoIPLink::enable_dns_srv_resolver (pjsip_endpoint *endpt, pj_dns_resolver **p_resv)
{

    pj_status_t status;
    pj_dns_resolver *resv;
    std::vector <std::string> dns_servers;
    pj_uint16_t port = 5353;
    pjsip_resolver_t *res;
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
    bool useStun;
    validStunServer = true;
    pj_dns_resolver *p_resv;

    name_mod = "sflphone";

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

    /* Start resolving STUN server */
    // if we useStun and we failed to receive something on port 5060, we try a random port
    // If use STUN server, firewall address setup
    if (!loadSIPLocalIP()) {
        _debug ("UserAgent: Unable to determine network capabilities\n");
        return false;
    }

    port = _regPort;

    /* Retrieve the STUN configuration */
    useStun = Manager::instance().getConfigInt (SIGNALISATION, SIP_USE_STUN);
    this->setStunServer (Manager::instance().getConfigString (SIGNALISATION, SIP_STUN_SERVER));
    this->useStun (useStun!=0 ? true : false);

    if (useStun && !Manager::instance().behindNat (getStunServer(), port)) {
        port = RANDOM_SIP_PORT;

        if (!Manager::instance().behindNat (getStunServer(), port)) {
            _debug ("UserAgent: Unable to check NAT setting\n");
            validStunServer = false;
            return false; // hoho we can't use the random sip port too...
        }
    }

    _localPort = port;

    if (useStun) {
        // set by last behindNat() call (ish)...
        stunServerResolve();
        _localExternAddress = Manager::instance().getFirewallAddress();
        _localExternPort = Manager::instance().getFirewallPort();
        errPjsip = createUDPServer();

        if (errPjsip != 0) {
            _debug ("UserAgent: Could not initialize SIP listener on port %d\n", port);
            return errPjsip;
        }
    } else {
        _localExternAddress = _localIPAddress;
        _localExternPort = _localPort;
        errPjsip = createUDPServer();

        if (errPjsip != 0) {
            _debug ("UserAgent: Could not initialize SIP listener on port %d\n", _localExternPort);
            _localExternPort = _localPort = RANDOM_SIP_PORT;
            _debug ("UserAgent: Try to initialize SIP listener on port %d\n", _localExternPort);
            errPjsip = createUDPServer();

            if (errPjsip != 0) {
                _debug ("UserAgent: Fail to initialize SIP listener on port %d\n", _localExternPort);
                return errPjsip;
            }
        }
    }

    _debug ("UserAgent: SIP Init -- listening on port %d\n", _localExternPort);

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

pj_status_t SIPVoIPLink::stunServerResolve (void)
{
    pj_str_t stun_adr;
    pj_hostent he;
    pj_stun_config stunCfg;
    pj_status_t stun_status;
    pj_sockaddr stun_srv;
    size_t pos;
    std::string serverName, serverPort;
    int nPort;
    std::string stun_server;

    stun_server = getStunServer();

    // Initialize STUN configuration
    pj_stun_config_init (&stunCfg, &_cp.factory, 0, pjsip_endpt_get_ioqueue (_endpt), pjsip_endpt_get_timer_heap (_endpt));

    stun_status = PJ_EPENDING;

    // Init STUN socket
    pos = stun_server.find (':');

    if (pos == std::string::npos) {
        pj_strdup2 (_pool, &stun_adr, stun_server.data());
        stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);
    } else {
        serverName = stun_server.substr (0, pos);
        serverPort = stun_server.substr (pos + 1);
        nPort = atoi (serverPort.data());
        pj_strdup2 (_pool, &stun_adr, serverName.data());
        stun_status = pj_sockaddr_in_init (&stun_srv.ipv4, &stun_adr, (pj_uint16_t) nPort);
    }

    if (stun_status != PJ_SUCCESS) {
        _debug ("UserAgent: Unresolved stun server!\n");
        stun_status = pj_gethostbyname (&stun_adr, &he);

        if (stun_status == PJ_SUCCESS) {
            pj_sockaddr_in_init (&stun_srv.ipv4, NULL, 0);
            stun_srv.ipv4.sin_addr = * (pj_in_addr*) he.h_addr;
            stun_srv.ipv4.sin_port = pj_htons ( (pj_uint16_t) 3478);
        }
    }

    return stun_status;
}

int SIPVoIPLink::createUDPServer (void)
{

    pj_status_t status;
    pj_sockaddr_in bound_addr;
    pjsip_host_port a_name;
    char tmpIP[32];
    pj_sock_t sock;


    // Init bound address to ANY
    pj_memset (&bound_addr, 0, sizeof (bound_addr));


    bound_addr.sin_addr.s_addr = pj_htonl (PJ_INADDR_ANY);
    bound_addr.sin_port = pj_htons ( (pj_uint16_t) _localPort);
    bound_addr.sin_family = PJ_AF_INET;
    pj_bzero (bound_addr.sin_zero, sizeof (bound_addr.sin_zero));

    // Create UDP-Server (default port: 5060)
    strcpy (tmpIP, _localExternAddress.data());
    pj_strdup2 (_pool, &a_name.host, tmpIP);
    a_name.port = (pj_uint16_t) _localExternPort;


    status = pjsip_udp_transport_start (_endpt, &bound_addr, &a_name, 1, NULL);

    if (status != PJ_SUCCESS) {
        _debug ("UserAgent: (%d) Unable to start UDP transport!\n", status);
        return -1;
    } else {
        _debug ("UserAgent: UDP server listening on port %d\n", _localExternPort);
    }


    _debug ("Transport initialized successfully! \n");

    return PJ_SUCCESS;
}

bool SIPVoIPLink::loadSIPLocalIP()
{

    bool returnValue = true;

    if (_localIPAddress == "127.0.0.1") {
        pj_sockaddr ip_addr;

        if (pj_gethostip (pj_AF_INET(), &ip_addr) != PJ_SUCCESS) {
            // Update the registration state if no network capabilities found
            _debug ("UserAgent: Get host ip failed!\n");
            returnValue = false;
        } else {
            _localIPAddress = std::string (pj_inet_ntoa (ip_addr.ipv4.sin_addr));
            _debug ("UserAgent: Checking network, setting local IP address to: %s\n", _localIPAddress.data());
        }
    }

    return returnValue;
}

void SIPVoIPLink::busy_sleep (unsigned msec)
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

    /* Done. */
}

int getModId()
{
    return _mod_ua.id;
}

void set_voicemail_info (AccountID account, pjsip_msg_body *body)
{

    int voicemail, pos_begin, pos_end;
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
    _audiortp->closeRtpSession ();
    call->setAudioStart (false);

    _debug ("Create new rtp session from handle_reinvite \n");

    try {
        _audiortp->createNewSession (call);
    } catch (...) {
        _debug ("! SIP Failure: Unable to create RTP Session (%s:%d)\n", __FILE__, __LINE__);
    }
}


/*******************************/
/*   CALLBACKS IMPLEMENTATION  */
/*******************************/

void call_on_state_changed (pjsip_inv_session *inv, pjsip_event *e)
{

    _debug ("--------------------- call_on_state_changed --------------------- %i\n", inv->state);

    SIPCall *call;
    AccountID accId;
    SIPVoIPLink *link;
    pjsip_rx_data *rdata;


    /* Retrieve the call information */
    call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);

    if (!call)
        return;

    //Retrieve the body message
    rdata = e->body.tsx_state.src.rdata;


    /* If this is an outgoing INVITE that was created because of
     * REFER/transfer, send NOTIFY to transferer.
     */
    if (call->getXferSub() && e->type==PJSIP_EVENT_TSX_STATE) {
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
    } else {

        // The call is ringing - We need to handle this case only on outgoing call
        if (inv->state == PJSIP_INV_STATE_EARLY && e->body.tsx_state.tsx->role == PJSIP_ROLE_UAC) {
            call->setConnectionState (Call::Ringing);
            Manager::instance().peerRingingCall (call->getCallId());
        }

        // We receive a ACK - The connection is established
        else if (inv->state == PJSIP_INV_STATE_CONFIRMED) {

            /* If the call is a direct IP-to-IP call */
            if (call->getCallConfiguration () == Call::IPtoIP) {
                link = SIPVoIPLink::instance ("");
            } else {
                accId = Manager::instance().getAccountFromCall (call->getCallId());
                link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));
            }

            if (link)
                link->SIPCallAnswered (call, rdata);
        }

        else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
            int count = 0;
            _debug ("------------------- Call disconnected ---------------------\n");
            _debug ("State: %i, Disconnection cause: %i\n", inv->state, inv->cause);

            switch (inv->cause) {
                    /* The call terminates normally - BYE / CANCEL */

                case PJSIP_SC_OK:

                case PJSIP_SC_DECLINE:

                case PJSIP_SC_REQUEST_TERMINATED:

                    accId = Manager::instance().getAccountFromCall (call->getCallId());
                    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink (accId));

                    if (link) {
                        link->SIPCallClosed (call);
                    }

                    break;

                    /* The call connection failed */

                case PJSIP_SC_NOT_FOUND:            /* peer not found */

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
                    _debug ("sipvoiplink.cpp - line 1635 : Unhandled call state. This is probably a bug.\n");
                    break;
            }
        }
    }
}

void call_on_media_update (pjsip_inv_session *inv, pj_status_t status)
{
    _debug ("--------------------- call_on_media_update --------------------- \n");
    
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    
    SIPVoIPLink * link = NULL;
    SIPCall * call;

    if (status != PJ_SUCCESS) {
        _debug ("Error while negociating the offer\n");
        return;
    }

    // Get the new sdp, result of the negociation
    pjmedia_sdp_neg_get_active_local (inv->neg, &local_sdp);
    pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
     
    call = reinterpret_cast<SIPCall *> (inv->mod_data[getModId() ]);

    if (!call) {
        _debug ("Call declined by peer, SDP negociation stopped\n");
        return;
    }
        
    // Clean the resulting sdp offer to create a new one (in case of a reinvite)
    call->getLocalSDP()->clean_session_media();
    
    // Set the fresh negociated one
    call->getLocalSDP()->set_negociated_offer (local_sdp);

    // Set remote ip / port  
    call->getLocalSDP()->set_media_transport_info_from_remote_sdp (remote_sdp); 
                
    link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getSIPAccountLink());
    if(link == NULL) {
        _debug ("Failed to get sip link\n");
        return;
    }
    
    try {    
        link->getAudioRtp()->start();
        call->setAudioStart (true);
    } catch(exception& rtpException) {
        _debug("%s\n", rtpException.what());
    }

}

void call_on_forked (pjsip_inv_session *inv, pjsip_event *e)
{
}

void call_on_tsx_changed (pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e)
{

    _debug ("--------------------- call_on_tsx_changed --------------------- %i\n", tsx->state);

    if (tsx->role==PJSIP_ROLE_UAS && tsx->state==PJSIP_TSX_STATE_TRYING &&
            pjsip_method_cmp (&tsx->method, &pjsip_refer_method) ==0) {
        /** Handle the refer method **/
        onCallTransfered (inv, e->body.tsx_state.src.rdata);
    }
}

void regc_cb (struct pjsip_regc_cbparam *param)
{

    //AccountID *id = static_cast<AccountID *> (param->token);
    SIPAccount *account;

    //_debug("UserAgent: Account ID is %s, Register result: %d, Status: %d\n", id->data(), param->status, param->code);
    account = static_cast<SIPAccount *> (param->token);

    if (!account)
        return;

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
    std::string userName, server, caller, callerServer, peerNumber;
    SIPVoIPLink *link;
    CallID id;
    SIPCall* call;
    pjsip_inv_session *inv;
    pjmedia_sdp_session *r_sdp;

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

    std::cout << userName << " ------------------ " << server << std::endl;

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

    _debug ("UserAgent: The receiver is : %s@%s\n", userName.data(), server.data());

    _debug ("UserAgent: The callee account id is %s\n", account_id.c_str());

    /* Now, it is the time to find the information of the caller */
    uri = rdata->msg_info.from->uri;
    sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri (uri);


    /* Retrieve only the fisrt characters */
    caller = std::string (sip_uri->user.ptr, sip_uri->user.slen);
    callerServer = std::string (sip_uri->host.ptr, sip_uri->host.slen);
    peerNumber = caller + "@" + callerServer;

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

    // Have to do some stuff with the SDP
    // Set the codec map, IP, peer number and so on... for the SIPCall object
    setCallAudioLocal (call, link->getLocalIPAddress(), link->useStun(), link->getStunServer());

    // We retrieve the remote sdp offer in the rdata struct to begin the negociation
    call->getLocalSDP()->set_ip_address (link->getLocalIPAddress());

    get_remote_sdp_from_offer (rdata, &r_sdp);

// 	_debug("r_sdp = %s\n", r_sdp);
    status = call->getLocalSDP()->receiving_initial_offer (r_sdp);

    if (status!=PJ_SUCCESS) {
        delete call;
        call=0;
        return false;
    }


    call->setConnectionState (Call::Progressing);

    call->setPeerNumber (peerNumber);

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

    SIPCall* newCall;

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


        if (event->body.rx_msg.rdata->msg_info.msg_buf != NULL) {
            request = event->body.rx_msg.rdata->msg_info.msg_buf;

            if (request.find (noresource) != -1) {
                _debug ("UserAgent: NORESOURCE for transfer!\n");
                link->transferStep2();
                pjsip_evsub_terminate (sub, PJ_TRUE);

                Manager::instance().transferFailed();
                return;
            }

            if (request.find (ringing) != -1) {
                _debug ("UserAgent: transfered call RINGING!\n");
                link->transferStep2();
                pjsip_evsub_terminate (sub, PJ_TRUE);

                Manager::instance().transferSucceded();
                return;
            }
        }


        // Get current call
        SIPCall *call = dynamic_cast<SIPCall *> (link->getCall (Manager::instance().getCurrentCallId()));

        if (!call) {
            _debug ("UserAgent: Call doesn't exit!\n");
            return;
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

            link->transferStep2();

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


bool setCallAudioLocal (SIPCall* call, std::string localIP, bool stun, std::string server)
{

    // Setting Audio
    unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
    unsigned int callLocalExternAudioPort = callLocalAudioPort;

    if (stun) {
        // If use Stun server
        if (Manager::instance().behindNat (server, callLocalAudioPort)) {
            callLocalExternAudioPort = Manager::instance().getFirewallPort();
        }
    }

    _debug ("            Setting local audio port to: %d\n", callLocalAudioPort);

    _debug ("            Setting local audio port (external) to: %d\n", callLocalExternAudioPort);

    // Set local audio port for SIPCall(id)
    call->setLocalIp (localIP);
    call->setLocalAudioPort (callLocalAudioPort);
    call->setLocalExternAudioPort (callLocalExternAudioPort);

    call->getLocalSDP()->attribute_port_to_all_media (callLocalExternAudioPort);

    return true;
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


