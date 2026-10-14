/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sip/sipcall.h"

#include "client/jami_signal.h"
#include "connectivity/sip_utils.h"
#include "conference.h"
#include "jami/callmanager_interface.h"
#include "logger.h"
#include "manager.h"
#include "media/audio/audio_rtp_session.h"
#include "sip/sdp.h"
#include "sip/sipaccountbase.h"

#ifdef ENABLE_VIDEO
#include "media/video/video_rtp_session.h"
#endif

#include <mutex>
#include <pjsip-ua/sip_inv.h>
#include <utility>

namespace jami {

using sip_utils::CONST_PJ_STR;

static bool
isBrowserMediaLayout(const std::vector<MediaDescription>& media)
{
    return (media.size() == 1 || media.size() == 2) && media.front().type == MEDIA_AUDIO
           && (media.size() == 1 || media.back().type == MEDIA_VIDEO);
}

static bool
isCompatibleBrowserReturnOffer(const Sdp& browserSdp,
                               const pjmedia_sdp_session* offer,
                               const std::vector<Sdp::MediaSlot>& slots)
{
    if (!offer || offer->media_count != slots.size())
        return false;
    const auto mediaDescriptions = browserSdp.getMediaDescriptions(offer, false);
    if (!isBrowserMediaLayout(mediaDescriptions) || mediaDescriptions.size() != slots.size())
        return false;

    for (size_t i = 0; i < slots.size(); ++i) {
        const auto& [local, remote] = slots[i];
        auto* media = offer->media[i];
        const auto& browser = mediaDescriptions[i];
        if (!media || local.type != browser.type || remote.type != browser.type
            || !local.enabled || !remote.enabled || !media->desc.port
            || browser.transport != MediaTransport::UDP_TLS_RTP_SAVPF
            || browser.dtls_fingerprint.empty() || browser.dtls_setup == DtlsSetup::NONE)
            return false;
        auto hasAttr = [offer, media](const char* name) {
            auto* attr = pjmedia_sdp_attr_find2(media->attr_count, media->attr, name, nullptr);
            if (!attr)
                attr = pjmedia_sdp_attr_find2(offer->attr_count, offer->attr, name, nullptr);
            return attr && (pj_stricmp2(&attr->name, "rtcp-mux") == 0 || attr->value.slen > 0);
        };
        if (!hasAttr("ice-ufrag") || !hasAttr("ice-pwd") || !hasAttr("fingerprint")
            || !hasAttr("rtcp-mux"))
            return false;
    }
    return true;
}

bool
SIPCall::requestExternalMediaDemotion(const std::string& requestId,
                                      const std::string& sdpOffer,
                                      const std::shared_ptr<Conference>& conference)
{
    std::lock_guard lk {callMutex_};
    if (requestId.empty() || sdpOffer.empty() || !conference || getConference() != conference
        || !promotionRequestId_.empty() || !demotionRequestId_.empty() || hasExternalMedia()
        || !isNativeMediaReadyLocked() || !inviteSession_
        || inviteSession_->state != PJSIP_INV_STATE_CONFIRMED || inviteSession_->invite_tsx)
        return false;
    const auto participants = conference->getSubCalls();
    if (participants.size() != 1 || !participants.contains(getCallId()))
        return false;

    Sdp browserOffer(getCallId() + "-external-return");
    if (!browserOffer.createOfferFromExternalSdp(sdpOffer))
        return false;
    const auto slots = sdp_->getMediaSlots();
    if (!isCompatibleBrowserReturnOffer(browserOffer, browserOffer.getLocalSdpSession(), slots))
        return false;
    const auto priorOffer = sdp_->saveLocalOfferState();
    if (!sdp_->createOfferFromExternalSdp(sdpOffer)) {
        sdp_->restoreLocalOfferState(priorOffer);
        return false;
    }

    pjsip_tx_data* tdata = nullptr;
    auto result = pjsip_inv_reinvite(inviteSession_.get(), nullptr, sdp_->getLocalSdpSession(), &tdata);
    if (result != PJ_SUCCESS || !tdata) {
        JAMI_ERROR("[call:{}] Could not create reverse external-media re-INVITE", getCallId());
        sdp_->restoreLocalOfferState(priorOffer);
        return false;
    }
    pj_str_t type = CONST_PJ_STR("application");
    pj_str_t subtype = CONST_PJ_STR("sdp");
    pj_str_t content {const_cast<char*>(sdpOffer.data()), (pj_ssize_t) sdpOffer.size()};
    // Preserve WebRTC attributes that pjmedia can rewrite when serializing SDP.
    tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &content);
    sip_utils::addUserAgentHeader(getSIPAccount()->getUserAgentName(), tdata);

    demotionRequestId_ = requestId;
    demotionConference_ = conference;
    demotionAnswerReceived_ = false;
    demotionPriorOffer_ = priorOffer;
    externalSdp_ = sdpOffer;
    externalRemoteSdp_.clear();
    if (pjsip_inv_send_msg(inviteSession_.get(), tdata) != PJ_SUCCESS) {
        pjsip_tx_data* cancel = nullptr;
        pjsip_inv_cancel_reinvite(inviteSession_.get(), &cancel);
        failDemotion("Could not send reverse media re-INVITE");
        return false;
    }
    return true;
}

bool
SIPCall::completeExternalMediaDemotion(const std::string& requestId)
{
    std::lock_guard lk {callMutex_};
    if (requestId.empty() || demotionRequestId_ != requestId || !demotionAnswerReceived_
        || !demotionConference_ || getConference() != demotionConference_
        || !inviteSession_ || inviteSession_->state != PJSIP_INV_STATE_CONFIRMED)
        return false;
    const auto participants = demotionConference_->getSubCalls();
    if (participants.size() != 1 || !participants.contains(getCallId()))
        return false;
    auto conference = std::move(demotionConference_);
    stopAllMedia();
    rtpStreams_.clear();
    conferenceMediaSource_.reset();
    enableIce_ = false;
    demotionRequestId_.clear();
    demotionAnswerReceived_ = false;
    demotionPriorOffer_.reset();
    libjami::retireBrowserConferenceHost(getAccountId(), conference->getConfId());
    Manager::instance().removeParticipant(*this);
    emitSignal<libjami::CallSignal::ExternalMediaDemotionReady>(getAccountId(), getCallId(), requestId);
    return true;
}

void
SIPCall::failDemotion(const std::string& reason)
{
    if (demotionRequestId_.empty())
        return;
    auto requestId = std::move(demotionRequestId_);
    demotionRequestId_.clear();
    if (demotionPriorOffer_) {
        sdp_->restoreLocalOfferState(*demotionPriorOffer_);
        demotionPriorOffer_.reset();
    }
    externalSdp_.clear();
    externalRemoteSdp_.clear();
    demotionConference_.reset();
    demotionAnswerReceived_ = false;
    emitSignal<libjami::CallSignal::ExternalMediaDemotionFailure>(
        getAccountId(), getCallId(), requestId, reason);
}

std::string
SIPCall::getExternalMediaPromotionState() const
{
    std::lock_guard lk {callMutex_};
    return promotionStateLocked();
}

std::string
SIPCall::promotionStateLocked() const
{
    if (!hasExternalMedia() || !rtpStreams_.empty() || !promotionRequestId_.empty() || isSubcall()
        || isConferenceParticipant())
        return "UNAVAILABLE";
    if (getState() != CallState::ACTIVE || getConnectionState() != ConnectionState::CONNECTED || !inviteSession_
        || inviteSession_->state != PJSIP_INV_STATE_CONFIRMED || inviteSession_->invite_tsx || isWaitingForIceAndMedia_
        || !sdp_ || !sdp_->getActiveLocalSdpSession() || !sdp_->getActiveRemoteSdpSession())
        return "UNAVAILABLE";

    auto account = getSIPAccount();
    if (!account || !account->isIceForMediaEnabled() || !account->isSrtpEnabled() || !dtlsCertificate_
        || !dtlsPrivateKey_)
        return "UNAVAILABLE";

    const auto* local = sdp_->getActiveLocalSdpSession();
    const auto* remote = sdp_->getActiveRemoteSdpSession();
    const auto localMedia = sdp_->getMediaDescriptions(local, false);
    const auto remoteMedia = sdp_->getMediaDescriptions(remote, true);
    if (local->media_count != remote->media_count || !isBrowserMediaLayout(localMedia))
        return "UNSUPPORTED_MEDIA";
#ifndef ENABLE_VIDEO
    if (localMedia.size() == 2)
        return "UNSUPPORTED_MEDIA";
#endif
    for (size_t i = 0; i < localMedia.size(); ++i) {
        const auto& native = localMedia[i];
        const auto& peer = remoteMedia[i];
        if (native.type != peer.type || !native.enabled || !peer.enabled
            || native.direction_ != MediaDirection::SENDRECV || peer.direction_ != MediaDirection::SENDRECV
            || native.transport != MediaTransport::UDP_TLS_RTP_SAVPF
            || peer.transport != MediaTransport::UDP_TLS_RTP_SAVPF)
            return "UNSUPPORTED_MEDIA";
    }
    return localMedia.size() == 1 ? "AUDIO_ONLY" : "AUDIO_VIDEO";
}

bool
SIPCall::promoteExternalMedia(const std::string& requestId, const std::shared_ptr<Conference>& conference)
{
    std::unique_lock lk {callMutex_};
    const auto state = promotionStateLocked();
    if (requestId.empty() || (state != "AUDIO_ONLY" && state != "AUDIO_VIDEO")) {
        JAMI_WARNING("[call:{}] Promotion unavailable for the active external-media dialog", getCallId());
        return false;
    }
#ifdef ENABLE_VIDEO
    if (state == "AUDIO_VIDEO" && (!conference || !conference->getVideoMixer())) {
        JAMI_WARNING("[call:{}] Promotion unavailable without conference video mixer", getCallId());
        return false;
    }
#endif

    auto mediaList = Sdp::getMediaAttributeListFromSdp(sdp_->getActiveLocalSdpSession());
    promotionRequestId_ = requestId;
    promotionNegotiated_ = false;
    promotionAccepted_ = false;
    enableIce_ = true;
    sdp_->setSecureMediaKeyExchange(KeyExchangeProtocol::DTLS);
    if (initMediaStreams(mediaList) != mediaList.size()) {
        auto failedRequest = takePromotionFailureLocked();
        lk.unlock();
        reportPromotionFailure(failedRequest, "Could not create native RTP sessions");
        hangup(PJSIP_SC_INTERNAL_SERVER_ERROR);
        return false;
    }
#ifdef ENABLE_VIDEO
    if (state == "AUDIO_VIDEO") {
        if (!prepareBrowserConferenceVideo(conference)) {
            auto failedRequest = takePromotionFailureLocked();
            lk.unlock();
            reportPromotionFailure(failedRequest, "Conference video mixer unavailable before RTP startup");
            hangup(PJSIP_SC_INTERNAL_SERVER_ERROR);
            return false;
        }
    }
#endif
    std::string failure;
    try {
        if (trySIPSessionReinvite(mediaList, true) != ReinviteResult::Sent)
            failure = "Could not send media re-INVITE";
    } catch (const std::exception& e) {
        failure = fmt::format("Could not send media re-INVITE: {}", e.what());
    }
    if (!failure.empty()) {
        auto failedRequest = takePromotionFailureLocked();
        lk.unlock();
        reportPromotionFailure(failedRequest, failure);
        hangup(PJSIP_SC_INTERNAL_SERVER_ERROR);
        return false;
    }
    externalSdp_.clear();
    externalRemoteSdp_.clear();
    lk.unlock();
    emitSignal<libjami::CallSignal::ExternalMediaPromotionAccepted>(getAccountId(), getCallId(), requestId);
    {
        std::lock_guard acceptedLock {callMutex_};
        if (promotionRequestId_ == requestId)
            promotionAccepted_ = true;
    }
    checkPromotionReady();
    return true;
}

bool
SIPCall::prepareBrowserConferenceVideo(const std::shared_ptr<Conference>& conference)
{
#ifdef ENABLE_VIDEO
    if (!conference || !conference->getVideoMixer())
        return false;
    const auto videoSessions = getRtpSessionList(MediaType::MEDIA_VIDEO);
    if (videoSessions.size() != 1
        || !std::static_pointer_cast<video::VideoRtpSession>(videoSessions.front())->prepareConference(conference))
        return false;
    conferenceMediaSource_ = conference;
    return true;
#else
    (void) conference;
    return false;
#endif
}

bool
SIPCall::cancelExternalMediaPromotion(const std::string& requestId)
{
    std::unique_lock lk {callMutex_};
    if (requestId.empty() || promotionRequestId_ != requestId)
        return false;
    auto failedRequest = takePromotionFailureLocked();
    const bool pendingInvite = inviteSession_ && inviteSession_->invite_tsx;
    if (pendingInvite)
        promotionEnding_ = true;
    lk.unlock();
    reportPromotionFailure(failedRequest, "Promotion cancelled");
    if (!pendingInvite)
        hangup(PJSIP_SC_REQUEST_TERMINATED);
    return true;
}

void
SIPCall::queuePromotionFailureLocked(const std::string& reason)
{
    auto requestId = takePromotionFailureLocked();
    if (requestId.empty())
        return;
    runOnMainThread([call = shared(), requestId = std::move(requestId), reason] {
        // A teardown caller may have acquired callMutex_ before calling us.
        {
            std::lock_guard lk {call->callMutex_};
        }
        call->reportPromotionFailure(requestId, reason);
    });
}

std::string
SIPCall::takePromotionFailureLocked()
{
    promotionAccepted_ = false;
    return std::exchange(promotionRequestId_, {});
}

void
SIPCall::reportPromotionFailure(const std::string& requestId, const std::string& reason) const
{
    JAMI_ERROR("[call:{}] External-media promotion failed: {}", getCallId(), reason);
    emitSignal<libjami::CallSignal::ExternalMediaPromotionFailure>(getAccountId(), getCallId(), requestId, reason);
}

void
SIPCall::checkPromotionReady()
{
    std::string requestId;
    {
        std::lock_guard lk {callMutex_};
        requestId = takePromotionReadyLocked();
    }
    if (!requestId.empty())
        reportPromotionReady(requestId);
}

std::string
SIPCall::takePromotionReadyLocked()
{
    if (promotionRequestId_.empty() || !promotionAccepted_ || !promotionNegotiated_ || !isNativeMediaReadyLocked())
        return {};
    promotionAccepted_ = false;
    return std::exchange(promotionRequestId_, {});
}

void
SIPCall::reportPromotionReady(const std::string& requestId) const
{
    emitSignal<libjami::CallSignal::ExternalMediaPromotionReady>(getAccountId(), getCallId(), requestId);
}

} // namespace jami
