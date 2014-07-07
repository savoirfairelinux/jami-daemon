/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "sipcall.h"
#include "sip_utils.h"
#include "logger.h" // for _debug
#include "sdp.h"
#include "manager.h"
#ifdef SFL_VIDEO
#include "client/videomanager.h"
#endif
#include "sipvoiplink.h"

static const int INITIAL_SIZE = 16384;
static const int INCREMENT_SIZE = INITIAL_SIZE;

static void
updateSDPFromSTUN(SIPCall &call, SIPAccount &account, const SipTransport &transport)
{
    std::vector<long> socketDescriptors(call.getAudioRtp().getSocketDescriptors());

    try {
        std::vector<pj_sockaddr> stunPorts(transport.getSTUNAddresses(account, socketDescriptors));

        // FIXME: get video sockets
        //stunPorts.resize(4);

        account.setPublishedAddress(stunPorts[0]);
        // published IP MUST be updated first, since RTCP depends on it
        call.getLocalSDP()->setPublishedIP(account.getPublishedAddress());
        call.getLocalSDP()->updatePorts(stunPorts);
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
    }
}

SIPCall::SIPCall(const std::string& id, Call::CallType type,
        pj_caching_pool *caching_pool, const std::string &account_id) :
    Call(id, type, account_id)
    , inv(NULL)
    , audiortp_(this)
#ifdef SFL_VIDEO
    // The ID is used to associate video streams to calls
    , videortp_(id, Manager::instance().getClient()->getVideoManager()->getSettings())
#endif
    , pool_(pj_pool_create(&caching_pool->factory, id.c_str(), INITIAL_SIZE, INCREMENT_SIZE, NULL))
    , local_sdp_(new Sdp(pool_))
    , contactBuffer_()
    , contactHeader_{contactBuffer_, 0}
{}

SIPCall::~SIPCall()
{
    delete local_sdp_;
    pj_pool_release(pool_);
}

void SIPCall::setContactHeader(pj_str_t *contact)
{
    pj_strcpy(&contactHeader_, contact);
}

std::map<std::string, std::string>
SIPCall::createHistoryEntry() const
{
    using sfl::HistoryItem;

    std::map<std::string, std::string> entry(Call::createHistoryEntry());
    return entry;
}

/**
 * Send a reINVITE inside an active dialog to modify its state
 * Local SDP session should be modified before calling this method
 * @param sip call
 */

static int
SIPSessionReinvite(SIPCall *call)
{
    pjmedia_sdp_session *local_sdp = call->getLocalSDP()->getLocalSdpSession();
    pjsip_tx_data *tdata;

    if (local_sdp and call->inv and call->inv->pool_prov and
            pjsip_inv_reinvite(call->inv, NULL, local_sdp, &tdata) == PJ_SUCCESS)
        return pjsip_inv_send_msg(call->inv, tdata);

    return !PJ_SUCCESS;
}

void
SIPCall::offhold(const std::function<void()> &SDPUpdateFunc)
{
    if (not setState(Call::ACTIVE))
        return;

    if (local_sdp_ == NULL)
        throw SdpException("Could not find sdp session");

    std::vector<sfl::AudioCodec*> sessionMedia(local_sdp_->getSessionAudioMedia());

    if (sessionMedia.empty()) {
        WARN("Session media is empty");
        return;
    }

    std::vector<sfl::AudioCodec*> audioCodecs;

    for (auto & i : sessionMedia) {

        if (!i)
            continue;

        // Create a new instance for this codec
        sfl::AudioCodec* ac = Manager::instance().audioCodecFactory.instantiateCodec(i->getPayloadType());

        if (ac == NULL) {
            ERROR("Could not instantiate codec %d", i->getPayloadType());
            throw std::runtime_error("Could not instantiate codec");
        }

        audioCodecs.push_back(ac);
    }

    if (audioCodecs.empty()) {
        throw std::runtime_error("Could not instantiate any codecs");
    }

    audiortp_.initConfig();
    audiortp_.initSession();

    // Invoke closure
    SDPUpdateFunc();

    audiortp_.restoreLocalContext();
    audiortp_.initLocalCryptoInfoOnOffHold();
    audiortp_.start(audioCodecs);

    local_sdp_->removeAttributeFromLocalAudioMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalAudioMedia("sendonly");
    local_sdp_->addAttributeToLocalAudioMedia("sendrecv");

#ifdef SFL_VIDEO
    local_sdp_->removeAttributeFromLocalVideoMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalVideoMedia("sendonly");
    local_sdp_->removeAttributeFromLocalVideoMedia("inactive");
    local_sdp_->addAttributeToLocalVideoMedia("sendrecv");
#endif

    if (SIPSessionReinvite(this) != PJ_SUCCESS) {
        WARN("Reinvite failed, resuming hold");
        onhold();
    }
}

void
SIPCall::onhold()
{
    if (not setState(Call::HOLD))
        return;

    audiortp_.saveLocalContext();
    audiortp_.stop();
#ifdef SFL_VIDEO
    videortp_.stop();
#endif

    if (!local_sdp_)
        throw SdpException("Could not find sdp session");

    local_sdp_->removeAttributeFromLocalAudioMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalAudioMedia("sendonly");
    local_sdp_->addAttributeToLocalAudioMedia("sendonly");

#ifdef SFL_VIDEO
    local_sdp_->removeAttributeFromLocalVideoMedia("sendrecv");
    local_sdp_->removeAttributeFromLocalVideoMedia("inactive");
    local_sdp_->addAttributeToLocalVideoMedia("inactive");
#endif

    if (SIPSessionReinvite(this) != PJ_SUCCESS)
        WARN("Reinvite failed");
}

VoIPLink*
SIPCall::getVoIPLink() const
{ return &SIPVoIPLink::instance(); }

void SIPCall::answer()
{
    SIPAccount *account = Manager::instance().getSipAccount(getAccountId());
    if (!account) {
        ERROR("Could not find account %s", getAccountId().c_str());
        return;
    }

    if (!inv->neg) {
        SIPVoIPLink& siplink = SIPVoIPLink::instance();

        WARN("Negotiator is NULL, we've received an INVITE without an SDP");
        pjmedia_sdp_session *dummy = 0;
        siplink.createSDPOffer(inv, &dummy);

        if (account->isStunEnabled())
            updateSDPFromSTUN(*this, *account, *siplink.sipTransport);
    }

    pj_str_t contact(account->getContactHeader());
    setContactHeader(&contact);

    pjsip_tx_data *tdata;
    if (!inv->last_answer)
        throw std::runtime_error("Should only be called for initial answer");

    // answer with SDP if no SDP was given in initial invite (i.e. inv->neg is NULL)
    if (pjsip_inv_answer(inv, PJSIP_SC_OK, NULL, !inv->neg ? local_sdp_->getLocalSdpSession() : NULL, &tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not init invite request answer (200 OK)");

    // contactStr must stay in scope as long as tdata
    if (contactHeader_.slen) {
        DEBUG("Answering with contact header: %.*s", contactHeader_.slen, contactHeader_.ptr);
        sip_utils::addContactHeader(&contactHeader_, tdata);
    }

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        throw std::runtime_error("Could not send invite request answer (200 OK)");

    setConnectionState(CONNECTED);
    setState(ACTIVE);
}

void
SIPCall::hangup(int reason)
{
    const std::string account_id(getAccountId());

    SIPAccount *account = Manager::instance().getSipAccount(account_id);
    if (not account)
        throw VoipLinkException("Could not find account for this call");

    if (not inv)
        throw VoipLinkException("No invite session for this call");

    pjsip_route_hdr *route = inv->dlg->route_set.next;
    while (route and route != &inv->dlg->route_set) {
        char buf[1024];
        int printed = pjsip_hdr_print_on(route, buf, sizeof(buf));

        if (printed >= 0) {
            buf[printed] = '\0';
            DEBUG("Route header %s", buf);
        }

        route = route->next;
    }

    pjsip_tx_data *tdata = NULL;

    const int status = reason ? reason :
                       inv->state <= PJSIP_INV_STATE_EARLY and inv->role != PJSIP_ROLE_UAC ?
                       PJSIP_SC_CALL_TSX_DOES_NOT_EXIST :
                       inv->state >= PJSIP_INV_STATE_DISCONNECTED ? PJSIP_SC_DECLINE :
                       0;

    // User hangup current call. Notify peer
    if (pjsip_inv_end_session(inv, status, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    // contactStr must stay in scope as long as tdata
    const pj_str_t contactStr(account->getContactHeader());
    sip_utils::addContactHeader(&contactStr, tdata);

    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS)
        return;

    auto& siplink = SIPVoIPLink::instance();

    // Make sure user data is NULL in callbacks
    inv->mod_data[siplink.getMod()->id] = NULL;

    // Stop all RTP streams
    if (Manager::instance().isCurrentCall(getCallId())) {
        getAudioRtp().stop();
#ifdef SFL_VIDEO
        getVideoRtp().stop();
#endif
    }

    siplink.removeSipCall(getCallId());
}
