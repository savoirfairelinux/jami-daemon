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

static bool
isBrowserMediaLayout(const std::vector<MediaDescription>& media)
{
    return (media.size() == 1 || media.size() == 2) && media.front().type == MEDIA_AUDIO
           && (media.size() == 1 || media.back().type == MEDIA_VIDEO);
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
