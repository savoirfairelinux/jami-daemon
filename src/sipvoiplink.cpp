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

/**************** EXTERN VARIABLES AND FUNCTIONS (callbacks) **************************/

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


/**
 * Get the number of voicemail waiting in a SIP message
 */
void set_voicemail_info( AccountID account, pjsip_msg_body *body );

/**
 * Set audio (SDP) configuration for a call
 * localport, localip, localexternalport
 * @param call a SIPCall valid pointer
 * @return bool True
 */
bool setCallAudioLocal(SIPCall* call, std::string localIP, bool stun, std::string server);

// Documentated from the PJSIP Developer's Guide, available on the pjsip website/

/*
 * Session callback
 * Called when the invite session state has changed.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	e	A pointer on a pjsip_event structure
 */
void call_on_state_changed( pjsip_inv_session *inv, pjsip_event *e);

/*
 * Session callback
 * Called after SDP offer/answer session has completed.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	status	A pj_status_t structure
 */
void call_on_media_update( pjsip_inv_session *inv UNUSED, pj_status_t status UNUSED);

/*
 * Called when the invote usage module has created a new dialog and invite
 * because of forked outgoing request.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	e	A pointer on a pjsip_event structure
 */
void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);

/*
 * Session callback
 * Called whenever any transactions within the session has changed their state.
 * Useful to monitor the progress of an outgoing request.
 *
 * @param	inv	A pointer on a pjsip_inv_session structure
 * @param	tsx	A pointer on a pjsip_transaction structure
 * @param	e	A pointer on a pjsip_event structure
 */
void call_on_tsx_changed(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e);

/*
 * Registration callback
 */
void regc_cb(struct pjsip_regc_cbparam *param);

/*
 * Called to handle incoming requests outside dialogs
 * @param   rdata
 * @return  pj_bool_t
 */
pj_bool_t mod_on_rx_request(pjsip_rx_data *rdata);

/*
 * Called to handle incoming response
 * @param	rdata
 * @return	pj_bool_t
 */
pj_bool_t mod_on_rx_response(pjsip_rx_data *rdata UNUSED) ;

/*
 * Transfer callbacks
 */
void xfer_func_cb( pjsip_evsub *sub, pjsip_event *event);
void xfer_svr_cb(pjsip_evsub *sub, pjsip_event *event);
void onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata);

/*************************************************************************************************/

SIPVoIPLink* SIPVoIPLink::_instance = NULL;


    SIPVoIPLink::SIPVoIPLink(const AccountID& accountID)
    : VoIPLink(accountID)
      , _nbTryListenAddr(2) // number of times to try to start SIP listener
      , _stunServer("")
    , _localExternAddress("") 
    , _localExternPort(0)
    , _audiortp(new AudioRtp())
    ,_regPort(DEFAULT_SIP_PORT)
    , _useStun(false)
      , _clients(0)
{
    _debug("SIPVoIPLink::~SIPVoIPLink(): sipvoiplink constructor called \n");    

    // to get random number for RANDOM_PORT
    srand (time(NULL));

    /* Start pjsip initialization step */
    init();
}

SIPVoIPLink::~SIPVoIPLink()
{
    _debug("SIPVoIPLink::~SIPVoIPLink(): sipvoiplink destructor called \n");
    terminate();
}

SIPVoIPLink* SIPVoIPLink::instance( const AccountID& id)
{

    if(!_instance ){
        _instance = new SIPVoIPLink( id );
    }

    return _instance;
}

void SIPVoIPLink::decrementClients (void)
{
    _clients--;
    if(_clients == 0){
        terminate();
        SIPVoIPLink::_instance=NULL;
    }
}

bool SIPVoIPLink::init()
{
    if(initDone())
        return false;

    /* Instanciate the C++ thread */
    _evThread = new EventThread(this);

    /* Initialize the pjsip library */
    pjsip_init();
    initDone(true);

    return true;
}

    void 
SIPVoIPLink::terminate()
{
    if (_evThread){
        delete _evThread; _evThread = NULL;
    }

    /* Clean shutdown of pjsip library */
    if( initDone() )
    {
        pjsip_shutdown();
    }
    initDone(false);
}

    void
SIPVoIPLink::terminateSIPCall()
{
    ost::MutexLock m(_callMapMutex);
    CallMap::iterator iter = _callMap.begin();
    SIPCall *call;
    while( iter != _callMap.end() ) {
        call = dynamic_cast<SIPCall*>(iter->second);
        if (call) {
            // terminate the sip call
            _debug("SIPVoIPLink::terminateSIPCall()::the call is deleted, should close recording file \n");
            delete call; call = 0;
        }
        iter++;
    }
    _callMap.clear();
}

    void
SIPVoIPLink::terminateOneCall(const CallID& id)
{
    _debug("SIPVoIPLink::terminateOneCall(): function called \n");

    SIPCall *call = getSIPCall(id);
    if (call) {
        // terminate the sip call
        _debug("SIPVoIPLink::terminateOneCall()::the call is deleted, should close recording file \n");
        delete call; call = 0;
    }
}



    void
SIPVoIPLink::getEvent()
{
    // We have to register the external thread so it could access the pjsip framework
    if(!pj_thread_is_registered())
        pj_thread_register( NULL, desc, &thread );

    // PJSIP polling
    pj_time_val timeout = {0, 10};
    pjsip_endpt_handle_events( _endpt, &timeout);

}

int SIPVoIPLink::sendRegister( AccountID id )
{
    pj_status_t status;
    int expire_value;
    char contactTmp[256];
    pj_str_t svr, aor, contact;
    pjsip_tx_data *tdata;
    std::string tmp, hostname, username, password;
    SIPAccount *account;
    pjsip_regc *regc;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount(id));
    hostname = account->getHostname();
    username = account->getUsername();
    password = account->getPassword();

    _mutexSIP.enterMutex(); 

    /* Get the client registration information for this particular account */
    regc = account->getRegistrationInfo();
    /* If the registration already exists, delete it */
    if(regc) {
        status = pjsip_regc_destroy(regc);
        regc = NULL;
        PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );
    }

    account->setRegister(true);

    /* Set the expire value of the message from the config file */
    expire_value = Manager::instance().getRegistrationExpireValue();

    /* Update the state of the voip link */
    account->setRegistrationState(Trying);

    if (!validStunServer) {
        account->setRegistrationState(ErrorExistStun);
        account->setRegister(false);
        _mutexSIP.leaveMutex(); 
        return false;
    }

    /* Create the registration according to the account ID */
    status = pjsip_regc_create(_endpt, (void*)account, &regc_cb, &regc);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to create regc.\n");
        _mutexSIP.leaveMutex(); 
        return false;
    }

    tmp = "sip:" + hostname;
    pj_strdup2(_pool, &svr, tmp.data());

    tmp = "<sip:" + username + "@" + hostname + ">";
    pj_strdup2(_pool, &aor, tmp.data());

    sprintf(contactTmp, "<sip:%s@%s:%d>", username.data(), _localExternAddress.data(), _localExternPort);
    pj_strdup2(_pool, &contact, contactTmp);
    account->setContact(contactTmp);

    status = pjsip_regc_init(regc, &svr, &aor, &aor, 1, &contact, 600); //timeout);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to initialize regc. %d\n", status); //, regc->str_srv_url.ptr);
        _mutexSIP.leaveMutex(); 
        return false;
    }

    pjsip_cred_info *cred = account->getCredInfo();

    if(!cred)
        cred = new pjsip_cred_info();

    pj_bzero(cred, sizeof (pjsip_cred_info));
    pj_strdup2(_pool, &cred->username, username.data());
    cred->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    pj_strdup2(_pool, &cred->data, password.data());
    pj_strdup2(_pool, &cred->realm, "*");
    pj_strdup2(_pool, &cred->scheme, "digest");
    pjsip_regc_set_credentials(regc, 1, cred);

    account->setCredInfo(cred);

    status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to register regc.\n");
        _mutexSIP.leaveMutex(); 
        return false;
    }

    status = pjsip_regc_send(regc, tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to send regc request.\n");
        _mutexSIP.leaveMutex(); 
        return false;
    }

    _mutexSIP.leaveMutex(); 

    account->setRegistrationInfo(regc);

    return true;
}

    int 
SIPVoIPLink::sendUnregister( AccountID id )
{
    pj_status_t status = 0;
    pjsip_tx_data *tdata = NULL;
    SIPAccount *account;
    pjsip_regc *regc;

    account = dynamic_cast<SIPAccount *> (Manager::instance().getAccount(id));
    regc = account->getRegistrationInfo();

    if(!account->isRegister()){
        account->setRegistrationState(Unregistered); 
        return true;
    }

    if(regc) {
        status = pjsip_regc_unregister(regc, &tdata);
        if(status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to unregister regc.\n");
            return false;
        }

        status = pjsip_regc_send( regc, tdata );
        if(status != PJ_SUCCESS) {
            _debug("UserAgent: Unable to send regc request.\n");
            return false;
        }
    } else {
        _debug("UserAgent: regc is null!\n");
        return false;
    }

    account->setRegistrationInfo(regc);
    account->setRegister(false);

    return true;
}

    Call* 
SIPVoIPLink::newOutgoingCall(const CallID& id, const std::string& toUrl)
{
    Account* account;

    SIPCall* call = new SIPCall(id, Call::Outgoing);

    if (call) {
        account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(Manager::instance().getAccountFromCall(id)));
        if(!account)
        {
            _debug("Error retrieving the account to the make the call with\n");
            call->setConnectionState(Call::Disconnected);
            call->setState(Call::Error);
            delete call; call=0;
            return call;
        }
        //call->setPeerNumber(toUrl);
        call->setPeerNumber(getSipTo(toUrl, account->getHostname()));
        _debug("Try to make a call to: %s with call ID: %s\n", toUrl.data(), id.data());
        // we have to add the codec before using it in SIPOutgoingInvite...
        call->setCodecMap(Manager::instance().getCodecDescriptorMap());
        if ( SIPOutgoingInvite(call) ) {
            call->setConnectionState(Call::Progressing);
            call->setState(Call::Active);
            addCall(call);
        } else {
            delete call; call = 0;
        }
    }
    return call;
}

    bool
SIPVoIPLink::answer(const CallID& id)
{

    int i;
    SIPCall *call;
    pj_status_t status;
    pjsip_tx_data *tdata;

    _debug("- SIP Action: start answering\n");

    call = getSIPCall(id);

    if (call==0) {
        _debug("! SIP Failure: SIPCall doesn't exists\n");
        return false;
    }

    // User answered the incoming call, tell peer this news
    if (call->startNegociation(_pool)) {
        // Create and send a 200(OK) response
        _debug("UserAgent: Negociation success!\n");
        status = pjsip_inv_answer(call->getInvSession(), PJSIP_SC_OK, NULL, NULL, &tdata);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
        status = pjsip_inv_send_msg(call->getInvSession(), tdata);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

        _debug("* SIP Info: Starting AudioRTP when answering\n");
        if (_audiortp->createNewSession(call) >= 0) {
            call->setAudioStart(true);
            call->setConnectionState(Call::Connected);
            call->setState(Call::Active);
            return true;
        } else {
            _debug("! SIP Failure: Unable to start sound when answering %s/%d\n", __FILE__, __LINE__);
        }
    }
    terminateOneCall(call->getCallId());
    removeCall(call->getCallId());
    return false;
}

    bool
SIPVoIPLink::hangup(const CallID& id)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    SIPCall* call;

    call = getSIPCall(id);

    if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

    // User hangup current call. Notify peer
    status = pjsip_inv_end_session(call->getInvSession(), 404, NULL, &tdata);
    if(status != PJ_SUCCESS)
        return false;

    if(tdata == NULL)
        return true;

    status = pjsip_inv_send_msg(call->getInvSession(), tdata);
    if(status != PJ_SUCCESS)
        return false;

    call->getInvSession()->mod_data[getModId()] = NULL;


    // Release RTP thread
    if (Manager::instance().isCurrentCall(id)) {
        _debug("* SIP Info: Stopping AudioRTP for hangup\n");
        _audiortp->closeRtpSession();
    }

    terminateOneCall(id);
    removeCall(id);

    return true;
}

    bool
SIPVoIPLink::peerHungup(const CallID& id)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    SIPCall* call;

    call = getSIPCall(id);

    if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

    // User hangup current call. Notify peer
    status = pjsip_inv_end_session(call->getInvSession(), 404, NULL, &tdata);
    if(status != PJ_SUCCESS)
        return false;

    if(tdata == NULL)
        return true;

    status = pjsip_inv_send_msg(call->getInvSession(), tdata);
    if(status != PJ_SUCCESS)
        return false;

    call->getInvSession()->mod_data[getModId()] = NULL;

    // Release RTP thread
    if (Manager::instance().isCurrentCall(id)) {
        _debug("* SIP Info: Stopping AudioRTP for hangup\n");
        _audiortp->closeRtpSession();
    }

    terminateOneCall(id);
    removeCall(id);

    return true;
}

    bool
SIPVoIPLink::cancel(const CallID& id)
{
    SIPCall* call = getSIPCall(id);
    if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

    _debug("- SIP Action: Cancel call %s [cid: %3d]\n", id.data(), call->getCid()); 

    terminateOneCall(id);
    removeCall(id);

    return true;
}

    bool
SIPVoIPLink::onhold(const CallID& id)
{

    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_session* local_sdp;
    SIPCall* call;

    call = getSIPCall(id);

    if (call==0) { _debug("! SIP Error: call doesn't exist\n"); return false; }  


    // Stop sound
    call->setAudioStart(false);
    call->setState(Call::Hold);
    _debug("* SIP Info: Stopping AudioRTP for onhold action\n");
    //_mutexSIP.enterMutex();
    _audiortp->closeRtpSession();
    //_mutexSIP.leaveMutex();

    local_sdp = call->getLocalSDPSession();

    if( local_sdp == NULL ){
        _debug("! SIP Failure: unable to find local_sdp\n");
        return false;
    }

    /* Create re-INVITE with new offer */
    // Remove all the attributes with the specified name
    pjmedia_sdp_media_remove_all_attr(local_sdp->media[0], "sendrecv");
    attr = pjmedia_sdp_attr_create(_pool, "sendonly", NULL);
    pjmedia_sdp_media_add_attr(local_sdp->media[0], attr);

    status = pjsip_inv_reinvite( call->getInvSession(), NULL, local_sdp, &tdata);
    if( status != PJ_SUCCESS )
    {
        _debug("On hold: creation of the Re-invite request failed\n");
        return false;
    }
    /* Send the request */
    status = pjsip_inv_send_msg( call->getInvSession(), tdata);

    return (status == PJ_SUCCESS);
}

    bool 
SIPVoIPLink::offhold(const CallID& id)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_session* local_sdp;

    call = getSIPCall(id);

    if (call==0) { 
        _debug("! SIP Error: Call doesn't exist\n"); 
        return false; 
    }

    local_sdp = call->getLocalSDPSession();
    if( local_sdp == NULL ){
        _debug("! SIP Failure: unable to find local_sdp\n");
        return false;
    }

    /* Create re-INVITE with new offer */
    // Remove all the attributes with the specified name
    pjmedia_sdp_media_remove_all_attr(local_sdp->media[0], "sendonly");
    attr = pjmedia_sdp_attr_create(_pool, "sendrecv", NULL);
    pjmedia_sdp_media_add_attr(local_sdp->media[0], attr);

    status = pjsip_inv_reinvite( call->getInvSession(), NULL, local_sdp , &tdata);
    if( status != PJ_SUCCESS )
    {
        _debug("Off hold: creation of the Re-invite request failed\n");
        return false;
    }

    /* Send the request */
    status = pjsip_inv_send_msg( call->getInvSession(), tdata);
    if( status != PJ_SUCCESS )
        return false;

    // Enable audio
    _debug("* SIP Info: Starting AudioRTP when offhold\n");
    call->setState(Call::Active);
    // it's sure that this is the current call id...
    if (_audiortp->createNewSession(call) < 0) {
        _debug("! SIP Failure: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
        return false;
    }

    return true;
}

    bool 
SIPVoIPLink::transfer(const CallID& id, const std::string& to)
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


    call = getSIPCall(id);
    call->stopRecording();
    account_id = Manager::instance().getAccountFromCall(id);
    account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(account_id));

    if (call==0) { 
        _debug("! SIP Failure: Call doesn't exist\n"); 
        return false; 
    }  

    tmp_to = SIPToHeader(to);
    if (tmp_to.find("@") == std::string::npos) {
        tmp_to = tmp_to + "@" + account->getHostname();
    }

    _debug("In transfer, tmp_to is %s\n", tmp_to.data());

    pj_strdup2(_pool, &dest, tmp_to.data());

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
    pjsip_evsub_set_mod_data(sub, getModId(), this);

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

bool SIPVoIPLink::transferStep2()
{
    _debug("SIPVoIPLink::transferStep2():When is this function called?");
    _audiortp->closeRtpSession();
    return true;
}

    bool
SIPVoIPLink::refuse (const CallID& id)
{
    SIPCall *call;
    pj_status_t status;
    pjsip_tx_data *tdata;

    _debug("SIPVoIPLink::refuse() : teh call is refused \n");
    call = getSIPCall(id);

    if (call==0) { 
        _debug("Call doesn't exist\n"); 
        return false; 
    }  

    // can't refuse outgoing call or connected
    if (!call->isIncoming() || call->getConnectionState() == Call::Connected) { 
        _debug("It's not an incoming call, or it's already answered\n");
        return false; 
    }

    // User refuse current call. Notify peer
    status = pjsip_inv_end_session(call->getInvSession(), PJSIP_SC_DECLINE, NULL, &tdata); //603
    if(status != PJ_SUCCESS)
        return false;

    status = pjsip_inv_send_msg(call->getInvSession(), tdata);
    if(status != PJ_SUCCESS)
        return false;

    call->getInvSession()->mod_data[getModId()] = NULL;

    terminateOneCall(id);
    return true;
}

    void 
SIPVoIPLink::setRecording(const CallID& id)
{
    SIPCall* call = getSIPCall(id);

    call->setRecording();

    // _audiortp->setRecording();
}

    bool
SIPVoIPLink::isRecording(const CallID& id)
{
    SIPCall* call = getSIPCall(id);

    return call->isRecording();
}

    bool 
SIPVoIPLink::carryingDTMFdigits(const CallID& id, char code)
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

    call = getSIPCall(id);

    if (call==0) { 
        _debug("Call doesn't exist\n"); 
        return false; 
    }

    duration = Manager::instance().getConfigInt(SIGNALISATION, PULSE_LENGTH);
    dtmf_body = new char[body_len];

    snprintf(dtmf_body, body_len - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);

    pj_strdup2(_pool, &methodName, "INFO");
    pjsip_method_init_np(&method, &methodName);

    /* Create request message. */
    status = pjsip_dlg_create_request( call->getInvSession()->dlg, &method, -1, &tdata);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to create INFO request -- %d\n", status);
        return false;
    }

    /* Get MIME type */
    pj_strdup2(_pool, &ctype.type, "application");
    pj_strdup2(_pool, &ctype.subtype, "dtmf-relay");

    /* Create "application/dtmf-relay" message body. */
    pj_strdup2(_pool, &content, dtmf_body);
    tdata->msg->body = pjsip_msg_body_create( tdata->pool, &ctype.type, &ctype.subtype, &content);
    if (tdata->msg->body == NULL) {
        _debug("UserAgent: Unable to create msg body!\n");
        pjsip_tx_data_dec_ref(tdata);
        return false;
    }

    /* Send the request. */
    status = pjsip_dlg_send_request( call->getInvSession()->dlg, tdata, getModId(), NULL);
    if (status != PJ_SUCCESS) {
        _debug("UserAgent: Unable to send MESSAGE request -- %d\n", status);
        return false;
    }

    return true;
}

    bool
SIPVoIPLink::SIPOutgoingInvite(SIPCall* call) 
{
    // If no SIP proxy setting for direct call with only IP address
    if (!SIPStartCall(call, "")) {
        _debug("! SIP Failure: call not started\n");
        return false;
    }
    return true;
}

    bool
SIPVoIPLink::SIPStartCall(SIPCall* call, const std::string& subject UNUSED) 
{
    std::string strTo, strFrom;
    pj_status_t status;
    pjsip_dialog *dialog;
    pjsip_tx_data *tdata;
    pj_str_t from, to, contact;
    AccountID id;
    SIPAccount *account;

    if (!call) 
        return false;

    id = Manager::instance().getAccountFromCall(call->getCallId());
    // Get the basic information about the callee account
    account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(id));

    strTo = getSipTo(call->getPeerNumber(), account->getHostname());
    _debug("            To: %s\n", strTo.data());

    // Generate the from URI
    strFrom = "sip:" + account->getUsername() + "@" + account->getHostname();

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
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    setCallAudioLocal(call, getLocalIPAddress(), useStun(), getStunServer());
    call->setIp(getLocalIP());

    // Building the local SDP offer
    call->createInitialOffer(_pool);

    // Create the invite session for this call
    pjsip_inv_session *inv;
    status = pjsip_inv_create_uac(dialog, call->getLocalSDPSession(), 0, &inv);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    // Set auth information
    pjsip_auth_clt_set_credentials(&dialog->auth_sess, 1, account->getCredInfo());

    // Associate current call in the invite session
    inv->mod_data[getModId()] = call;

    status = pjsip_inv_invite(inv, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, false);

    // Associate current invite session in the call
    call->setInvSession(inv);

    status = pjsip_inv_send_msg(inv, tdata);
    if(status != PJ_SUCCESS) {
        return false;
    }

    return true;
}

std::string SIPVoIPLink::getSipTo(const std::string& to_url, std::string hostname) {
    // Form the From header field basis on configuration panel
    //bool isRegistered = (_eXosipRegID == EXOSIP_ERROR_STD) ? false : true;

    // add a @host if we are registered and there is no one inside the url
    if (to_url.find("@") == std::string::npos) {// && isRegistered) {
        if(!hostname.empty()) {
            return SIPToHeader(to_url + "@" + hostname);
        }
    }
    return SIPToHeader(to_url);
    }

    std::string SIPVoIPLink::SIPToHeader(const std::string& to) 
    {
        if (to.find("sip:") == std::string::npos) {
            return ("sip:" + to );
        } else {
            return to;
        }
    }

    bool
        SIPVoIPLink::SIPCheckUrl(const std::string& url UNUSED)
        {
            return true;
        }

    bool setCallAudioLocal(SIPCall* call, std::string localIP, bool stun, std::string server) 
    {
        // Setting Audio
        unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
        unsigned int callLocalExternAudioPort = callLocalAudioPort;
        if (stun) {
            // If use Stun server
            if (Manager::instance().behindNat(server, callLocalAudioPort)) {
                callLocalExternAudioPort = Manager::instance().getFirewallPort();
            }
        }
        _debug("            Setting local audio port to: %d\n", callLocalAudioPort);
        _debug("            Setting local audio port (external) to: %d\n", callLocalExternAudioPort);

        // Set local audio port for SIPCall(id)
        call->setLocalIp(localIP);
        call->setLocalAudioPort(callLocalAudioPort);
        call->setLocalExternAudioPort(callLocalExternAudioPort);

        return true;
    }

    void
        SIPVoIPLink::SIPCallServerFailure(SIPCall *call) 
        {
            //if (!event->response) { return; }
            //switch(event->response->status_code) {
            //case SIP_SERVICE_UNAVAILABLE: // 500
            //case SIP_BUSY_EVRYWHERE:     // 600
            //case SIP_DECLINE:             // 603
            //SIPCall* call = findSIPCallWithCid(event->cid);
            if (call != 0) {
                _debug("Server error!\n");
                CallID id = call->getCallId();
                Manager::instance().callFailure(id);
                terminateOneCall(id);
                removeCall(id);
            }
            //break;
            //}
        }

    void
        SIPVoIPLink::SIPCallClosed(SIPCall *call) 
        {

            _debug("SIPVoIPLink::SIPCallClosed():: function called when peer hangup");
            // it was without did before
            //SIPCall* call = findSIPCallWithCid(event->cid);
            if (!call) { return; }

            CallID id = call->getCallId();
            //call->setDid(event->did);
            if (Manager::instance().isCurrentCall(id)) {
                call->setAudioStart(false);
                _debug("* SIP Info: Stopping AudioRTP when closing\n");
                _audiortp->closeRtpSession();
            }
            _debug("After close RTP\n");
            Manager::instance().peerHungupCall(id);
            terminateOneCall(id);
            removeCall(id);
            _debug("After remove call ID\n");
        }

    void
        SIPVoIPLink::SIPCallReleased(SIPCall *call)
        {
            // do cleanup if exists
            // only cid because did is always 0 in these case..
            //SIPCall* call = findSIPCallWithCid(event->cid);
            if (!call) { return; }

            // if we are here.. something when wrong before...
            _debug("SIP call release\n");
            CallID id = call->getCallId();
            Manager::instance().callFailure(id);
            terminateOneCall(id);
            removeCall(id);
        }


    void
        SIPVoIPLink::SIPCallAnswered(SIPCall *call, pjsip_rx_data *rdata)
        {
            //SIPCall* call = dynamic_cast<SIPCall *>(theCall);//findSIPCallWithCid(event->cid);
            if (!call) {
                _debug("! SIP Failure: unknown call\n");
                return;
            }
            //call->setDid(event->did);

            if (call->getConnectionState() != Call::Connected) {
                //call->SIPCallAnswered(event);
                call->SIPCallAnsweredWithoutHold(rdata);

                call->setConnectionState(Call::Connected);
                call->setState(Call::Active);

                Manager::instance().peerAnsweredCall(call->getCallId());
                if (Manager::instance().isCurrentCall(call->getCallId())) {
                    _debug("* SIP Info: Starting AudioRTP when answering\n");
                    if ( _audiortp->createNewSession(call) < 0) {
                        _debug("RTP Failure: unable to create new session\n");
                    } else {
                        call->setAudioStart(true);
                    }
                }
            } else {
                _debug("* SIP Info: Answering call (on/off hold to send ACK)\n");
                //call->SIPCallAnswered(event);
            }
        }


    SIPCall*
        SIPVoIPLink::getSIPCall(const CallID& id) 
        {
            Call* call = getCall(id);
            if (call) {
                return dynamic_cast<SIPCall*>(call);
            }
            return NULL;
        }

    void SIPVoIPLink::setStunServer( const std::string &server )
    {
        if(server != "") {
            useStun(true);
            _stunServer = server;
        } else {
            useStun(false);
            _stunServer = std::string("");
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Private functions
    ///////////////////////////////////////////////////////////////////////////////

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

        name_mod = "sflphone";

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

        // Create the SIP endpoint 
        status = pjsip_endpt_create(&_cp.factory, pj_gethostname()->ptr, &_endpt);
        PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

        /* Start resolving STUN server */
        // if we useStun and we failed to receive something on port 5060, we try a random port
        // If use STUN server, firewall address setup
        if (!loadSIPLocalIP()) {
            _debug("UserAgent: Unable to determine network capabilities\n");
            return false;
        }

        port = _regPort;

        /* Retrieve the STUN configuration */
        useStun = Manager::instance().getConfigInt( SIGNALISATION, SIP_USE_STUN );
        this->setStunServer(Manager::instance().getConfigString( SIGNALISATION, SIP_STUN_SERVER ));
        this->useStun( useStun!=0 ? true : false);

        if (useStun && !Manager::instance().behindNat(getStunServer(), port)) {
            port = RANDOM_SIP_PORT;
            if (!Manager::instance().behindNat(getStunServer(), port)) {
                _debug("UserAgent: Unable to check NAT setting\n");
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
                _debug("UserAgent: Could not initialize SIP listener on port %d\n", port);
                return errPjsip;
            }
        } else {
            _localExternAddress = _localIPAddress;
            _localExternPort = _localPort;
            errPjsip = createUDPServer();
            if (errPjsip != 0) {
                _debug("UserAgent: Could not initialize SIP listener on port %d\n", _localExternPort);
                _localExternPort = _localPort = RANDOM_SIP_PORT;
                _debug("UserAgent: Try to initialize SIP listener on port %d\n", _localExternPort);
                errPjsip = createUDPServer();
                if (errPjsip != 0) {
                    _debug("UserAgent: Fail to initialize SIP listener on port %d\n", _localExternPort);
                    return errPjsip;
                }
            }
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
        _mod_ua.name = pj_str((char*)name_mod.c_str());
        _mod_ua.id = -1;
        _mod_ua.priority = PJSIP_MOD_PRIORITY_APPLICATION;
        _mod_ua.on_rx_request = &mod_on_rx_request;
        _mod_ua.on_rx_response = &mod_on_rx_response;

        status = pjsip_endpt_register_module(_endpt, &_mod_ua);
        PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

        // Init the event subscription module.
        // It extends PJSIP by supporting SUBSCRIBE and NOTIFY methods
        status = pjsip_evsub_init_module(_endpt);
        PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

        // Init xfer/REFER module
        status = pjsip_xfer_init_module(_endpt);
        PJ_ASSERT_RETURN( status == PJ_SUCCESS, 1 );

        // Init the callback for INVITE session: 
        pj_bzero(&inv_cb, sizeof (inv_cb));

        inv_cb.on_state_changed = &call_on_state_changed;
        inv_cb.on_new_session = &call_on_forked;
        inv_cb.on_media_update = &call_on_media_update;
        inv_cb.on_tsx_state_changed = &call_on_tsx_changed;

        // Initialize session invite module 
        status = pjsip_inv_usage_init(_endpt, &inv_cb);
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

        _debug("UserAgent: VOIP callbacks initialized\n");

        // Add endpoint capabilities (INFO, OPTIONS, etc) for this UA
        pj_str_t allowed[] = { {(char*)"INFO", 4}, {(char*)"REGISTER", 8} }; //  //{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6},  {"OPTIONS", 7}, 
        accepted = pj_str((char*)"application/sdp");

        // Register supported methods
        pjsip_endpt_add_capability(_endpt, &_mod_ua, PJSIP_H_ALLOW, NULL, PJ_ARRAY_SIZE(allowed), allowed);

        // Register "application/sdp" in ACCEPT header
        pjsip_endpt_add_capability(_endpt, &_mod_ua, PJSIP_H_ACCEPT, NULL, 1, &accepted);

        _debug("UserAgent: pjsip version %s for %s initialized\n", pj_get_version(), PJ_OS_NAME);

        // Create the secondary thread to poll sip events
        _evThread->start();

        /* Done! */
        return PJ_SUCCESS;
    }

    pj_status_t SIPVoIPLink::stunServerResolve( void )
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
        pj_stun_config_init(&stunCfg, &_cp.factory, 0, pjsip_endpt_get_ioqueue(_endpt), pjsip_endpt_get_timer_heap(_endpt));

        stun_status = PJ_EPENDING;

        // Init STUN socket
        pos = stun_server.find(':');
        if(pos == std::string::npos) {
            pj_strdup2(_pool, &stun_adr, stun_server.data());
            stun_status = pj_sockaddr_in_init(&stun_srv.ipv4, &stun_adr, (pj_uint16_t) 3478);
        } else {
            serverName = stun_server.substr(0, pos);
            serverPort = stun_server.substr(pos + 1);
            nPort = atoi(serverPort.data());
            pj_strdup2(_pool, &stun_adr, serverName.data());
            stun_status = pj_sockaddr_in_init(&stun_srv.ipv4, &stun_adr, (pj_uint16_t) nPort);
        }

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

    int SIPVoIPLink::createUDPServer( void ) 
    {

        pj_status_t status;
        pj_sockaddr_in bound_addr;
        pjsip_host_port a_name;
        char tmpIP[32];
        pj_sock_t sock;

        // Init bound address to ANY
        pj_memset(&bound_addr, 0, sizeof (bound_addr));
        bound_addr.sin_addr.s_addr = PJ_INADDR_ANY;

        // Create UDP server socket
        status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: (%d) UDP socket() error\n", status);
            return status;
        }

        status = pj_sock_bind_in(sock, pj_ntohl(bound_addr.sin_addr.s_addr), (pj_uint16_t) _localPort);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: (%d) UDP bind() error\n", status);
            pj_sock_close(sock);
            return status;
        }

        _debug("UserAgent: Use IP: %s\n", _localExternAddress.data());

        // Create UDP-Server (default port: 5060)
        strcpy(tmpIP, _localExternAddress.data());
        pj_strdup2(_pool, &a_name.host, tmpIP);
        a_name.port = (pj_uint16_t) _localExternPort;

        _debug("a_name: host: %s  - port : %i\n", a_name.host.ptr, a_name.port);

        status = pjsip_udp_transport_attach(_endpt, sock, &a_name, 1, NULL);
        if (status != PJ_SUCCESS) {
            _debug("UserAgent: (%d) Unable to start UDP transport!\n", status);
            return -1;
        } else {
            _debug("UserAgent: UDP server listening on port %d\n", _localExternPort);
        }

        return PJ_SUCCESS;
    }

    bool SIPVoIPLink::loadSIPLocalIP() {

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

    void SIPVoIPLink::busy_sleep(unsigned msec)
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

    bool SIPVoIPLink::pjsip_shutdown( void )
    {
        if (_endpt) {
            _debug("UserAgent: Shutting down...\n");
            busy_sleep(1000);
        }

        pj_thread_join( thread );
        pj_thread_destroy( thread );
        thread = NULL;

        /* Destroy endpoint. */
        if (_endpt) {
            pjsip_endpt_destroy(_endpt);
            _endpt = NULL;
        }

        /* Destroy pool and pool factory. */
        if (_pool) {
            pj_pool_release(_pool);
            _pool = NULL;
            pj_caching_pool_destroy(&_cp);
        }

        /* Shutdown PJLIB */
        pj_shutdown();

        /* Done. */    
    }

    int SIPVoIPLink::getModId(){
        return _mod_ua.id;
    }

    void set_voicemail_info( AccountID account, pjsip_msg_body *body ){

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

    /*******************************/
    /*   CALLBACKS IMPLEMENTATION  */
    /*******************************/

    void call_on_state_changed( pjsip_inv_session *inv, pjsip_event *e){

        PJ_UNUSED_ARG(inv);

        SIPCall *call = reinterpret_cast<SIPCall*> (inv->mod_data[_mod_ua.id]);
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

    void call_on_media_update( pjsip_inv_session *inv UNUSED, pj_status_t status UNUSED) {
        _debug("call_on_media_updated\n");
    }

    void call_on_forked(pjsip_inv_session *inv, pjsip_event *e){
        _debug("call_on_forked\n");
    }

    void call_on_tsx_changed(pjsip_inv_session *inv, pjsip_transaction *tsx, pjsip_event *e){

        pjsip_rx_data *rdata;
        AccountID accId;
        SIPCall *call;
        SIPVoIPLink *link;
        pjsip_msg *msg;

        if(pj_strcmp2(&tsx->method.name, "INFO") == 0) {
            // Receive a INFO message, ingore it!
            return;
        }

        //Retrieve the body message
        rdata = e->body.tsx_state.src.rdata;

        if (tsx->role == PJSIP_ROLE_UAC) {
            switch (tsx->state) {
                case PJSIP_TSX_STATE_TERMINATED:
                    if (tsx->status_code == 200 &&
                            pjsip_method_cmp(&tsx->method, pjsip_get_refer_method()) != 0) {
                        // Peer answered the outgoing call
                        _debug("UserAgent: Peer answered the outgoing call!\n");
                        call = reinterpret_cast<SIPCall *> (inv->mod_data[_mod_ua.id]);
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

                    call = reinterpret_cast<SIPCall *> (inv->mod_data[_mod_ua.id]);
                    if (call == NULL)
                        return;

                    if (msg->line.status.code == 180) {
                        _debug("UserAgent: Peer is ringing!\n");

                        call->setConnectionState(Call::Ringing);
                        Manager::instance().peerRingingCall(call->getCallId());
                    }
                    break;
                case PJSIP_TSX_STATE_COMPLETED:
                    if (tsx->status_code == 407 || tsx->status_code == 401) //FIXME
                        break;
                    if (tsx->status_code / 100 == 6 || tsx->status_code / 100 == 4) {
                        // We get error message of outgoing call from server
                        _debug("UserAgent: Server error message is received!\n");
                        call = reinterpret_cast<SIPCall *> (inv->mod_data[_mod_ua.id]);
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
                        //onCallTransfered(inv, e->body.tsx_state.src.rdata);
                    }
                    break;
                case PJSIP_TSX_STATE_COMPLETED:
                    if (tsx->status_code == 200 && tsx->method.id == PJSIP_BYE_METHOD) {
                        // Peer hangup the call
                        _debug("UserAgent: Peer hangup(bye) message is received!\n");
                        call = reinterpret_cast<SIPCall *> (inv->mod_data[_mod_ua.id]);
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
                        call = reinterpret_cast<SIPCall *> (inv->mod_data[_mod_ua.id]);
                        if (call == NULL) {
                            _debug("UserAgent: Call has been removed!\n");
                            return;
                        }

                        accId = Manager::instance().getAccountFromCall(call->getCallId());
                        link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(accId));
                        if (link) {
                            link->SIPCallClosed(call);
                        }
                    }
                    break;
                default:
                    break;
            } // end of switch
        }
    }

    void regc_cb(struct pjsip_regc_cbparam *param){

        //AccountID *id = static_cast<AccountID *> (param->token);
        SIPAccount *account;

        //_debug("UserAgent: Account ID is %s, Register result: %d, Status: %d\n", id->data(), param->status, param->code);
        account = static_cast<SIPAccount *>(param->token);
        if(!account)
            return;

        if (param->status == PJ_SUCCESS) {
            if (param->code < 0 || param->code >= 300) {
                /* Sometimes, the status is OK, but we still failed.
                 * So checking the code for real result
                 */
                _debug("UserAgent: The error is: %d\n", param->code);
                switch(param->code) {
                    case 408:
                    case 606:
                        account->setRegistrationState(ErrorConfStun);
                        break;
                    case 503:
                        account->setRegistrationState(ErrorHost);
                        break;
                    case 401:
                    case 403:
                    case 404:
                        account->setRegistrationState(ErrorAuth);
                        break;
                    default:
                        account->setRegistrationState(Error);
                        break;
                }
                account->setRegister(false);
            } else {
                // Registration/Unregistration is success

                if(account->isRegister())
                    account->setRegistrationState(Registered);
                else {
                    account->setRegistrationState(Unregistered);
                    account->setRegister(false);
                }
            }
        } else {
            account->setRegistrationState(ErrorAuth);
            account->setRegister(false);
        }

    }

    pj_bool_t 
        mod_on_rx_request(pjsip_rx_data *rdata)
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

            // voicemail part
            std::string method_name;
            std::string request;

            // Handle the incoming call invite in this function 
            _debug("UserAgent: Callback on_rx_request is involved!\n");

            /* First, let's got the username and server name from the invite.
             * We will use them to detect which account is the callee.
             */ 
            uri = rdata->msg_info.to->uri;
            sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(uri);

            userName = std::string(sip_uri->user.ptr, sip_uri->user.slen);
            server = std::string(sip_uri->host.ptr, sip_uri->host.slen) ;

            // Get the account id of callee from username and server
            account_id = Manager::instance().getAccountIdFromNameAndServer(userName, server);

            /* If we don't find any account to receive the call */
            if(account_id == AccountNULL) {
                _debug("UserAgent: Username %s doesn't match any account!\n",userName.c_str());
                return false;
            }

            /* Get the voip link associated to the incoming call */
            /* The account must before have been associated to the call in ManagerImpl */
            link = dynamic_cast<SIPVoIPLink *> (Manager::instance().getAccountLink(account_id));

            /* If we can't find any voIP link to handle the incoming call */
            if( link == 0 )
            {
                _debug("ERROR: can not retrieve the voiplink from the account ID...\n");
                return false;
            }

            _debug("UserAgent: The receiver is : %s@%s\n", userName.data(), server.data());
            _debug("UserAgent: The callee account id is %s\n", account_id.c_str());

            /* Now, it is the time to find the information of the caller */
            uri = rdata->msg_info.from->uri;
            sip_uri = (pjsip_sip_uri *) pjsip_uri_get_uri(uri);

            /* Retrieve only the fisrt characters */
            caller = std::string(sip_uri->user.ptr, sip_uri->user.slen);
            callerServer = std::string(sip_uri->host.ptr, sip_uri->host.slen);
            peerNumber = caller + "@" + callerServer;

            // Get the server voicemail notification
            // Catch the NOTIFY message
            if( rdata->msg_info.msg->line.req.method.id == PJSIP_OTHER_METHOD )
            {
                method_name = "NOTIFY";
                // Retrieve all the message. Should contains only the method name but ...
                request =  rdata->msg_info.msg->line.req.method.name.ptr;
                // Check if the message is a notification
                if( request.find( method_name ) != (size_t)-1 ) {
                    /* Notify the right account */
                    set_voicemail_info( account_id, rdata->msg_info.msg->body );
                }
                pjsip_endpt_respond_stateless(_endpt, rdata, PJSIP_SC_OK, NULL, NULL, NULL);
                return true;
            }

            // Respond statelessly any non-INVITE requests with 500
            if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
                if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
                    pj_strdup2(_pool, &reason, "user agent unable to handle this request ");
                    pjsip_endpt_respond_stateless( _endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, &reason, NULL,
                            NULL);
                    return true;
                }
            }

            // Verify that we can handle the request
            status = pjsip_inv_verify_request(rdata, &options, NULL, NULL, _endpt, NULL);
            if (status != PJ_SUCCESS) {
                pj_strdup2(_pool, &reason, "user agent unable to handle this INVITE ");
                pjsip_endpt_respond_stateless( _endpt, rdata, PJSIP_SC_METHOD_NOT_ALLOWED, &reason, NULL,
                        NULL);
                return true;
            }

            // Generate a new call ID for the incoming call!
            id = Manager::instance().getNewCallID();
            call = new SIPCall(id, Call::Incoming);

            /* If an error occured at the call creation */
            if (!call) {
                _debug("UserAgent: unable to create an incoming call");
                return false;
            }

            // Set the codec map, IP, peer number and so on... for the SIPCall object
            setCallAudioLocal(call, link->getLocalIPAddress(), link->useStun(), link->getStunServer());
            call->setCodecMap(Manager::instance().getCodecDescriptorMap());
            call->setConnectionState(Call::Progressing);
            call->setIp(link->getLocalIPAddress());
            call->setPeerNumber(peerNumber);

            /* Call the SIPCallInvite function to generate the local sdp,
             * remote sdp and negociator.
             * This function is also used to set the parameters of audio RTP, including:
             *     local IP and port number 
             *     remote IP and port number
             *     possilbe audio codec will be used in this call
             */
            if (call->SIPCallInvite(rdata, _pool)) {

                // Notify UI there is an incoming call
                if (Manager::instance().incomingCall(call, account_id)) {
                    // Add this call to the callAccountMap in ManagerImpl
                    Manager::instance().getAccountLink(account_id)->addCall(call);
                } else {
                    // Fail to notify UI
                    delete call;
                    call = NULL;
                    _debug("UserAgent: Fail to notify UI!\n");
                    return false;
                }
            } else {
                // Fail to collect call information
                delete call;
                call = NULL;
                _debug("UserAgent: Call SIPCallInvite failed!\n");
                return false;
            }

            /* Create the local dialog (UAS) */
            status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, NULL, &dialog);
            if (status != PJ_SUCCESS) {
                pjsip_endpt_respond_stateless( _endpt, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, &reason, NULL,
                        NULL);
                return true;
            }

            // Specify media capability during invite session creation
            pjsip_inv_session *inv;
            status = pjsip_inv_create_uas(dialog, rdata, call->getLocalSDPSession(), 0, &inv);
            PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

            // Associate the call in the invite session
            inv->mod_data[_mod_ua.id] = call;

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
            return true;

        }

    pj_bool_t mod_on_rx_response(pjsip_rx_data *rdata UNUSED) {
        _debug("mod_on_rx_response\n");
        return PJ_SUCCESS;
    }

    void onCallTransfered(pjsip_inv_session *inv, pjsip_rx_data *rdata)
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

        existing_call = (SIPCall *) inv->mod_data[_mod_ua.id];

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
            pjsip_evsub_set_mod_data(sub, _mod_ua.id,
                    newCall);
        }    
    }



    void xfer_func_cb( pjsip_evsub *sub, pjsip_event *event){

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
                        _mod_ua.id));

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
                        _mod_ua.id));

            /* When subscription is terminated, clear the xfer_sub member of 
             * the inv_data.
             */
            if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
                pjsip_evsub_set_mod_data(sub, _mod_ua.id, NULL);
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
                pjsip_evsub_set_mod_data(sub, _mod_ua.id, NULL);
            }
        }

    }


    void xfer_svr_cb(pjsip_evsub *sub, pjsip_event *event)
    {
        PJ_UNUSED_ARG(event);

        /*
         * When subscription is terminated, clear the xfer_sub member of 
         * the inv_data.
         */
        if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
            SIPCall *call;

            call = (SIPCall*) pjsip_evsub_get_mod_data(sub, _mod_ua.id);
            if (!call)
                return;

            pjsip_evsub_set_mod_data(sub, _mod_ua.id, NULL);
            call->setXferSub(NULL);

            _debug("UserAgent: Xfer server subscription terminated\n");
        }    
    }
