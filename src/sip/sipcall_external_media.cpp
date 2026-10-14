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

bool
SIPCall::canPromoteExternalMedia() const
{
    std::lock_guard lk {callMutex_};
    return promotionStateLocked() == "AUDIO_ONLY";
}

std::string
SIPCall::promotionStateLocked() const
{
    if (!promotionRequestId_.empty() || !hasExternalMedia() || !rtpStreams_.empty()
        || isSubcall() || isConferenceParticipant() || getState() != CallState::ACTIVE
        || getConnectionState() != ConnectionState::CONNECTED || !inviteSession_
        || inviteSession_->state != PJSIP_INV_STATE_CONFIRMED || inviteSession_->invite_tsx
        || !sdp_ || !sdp_->getActiveLocalSdpSession() || !sdp_->getActiveRemoteSdpSession()
        || isWaitingForIceAndMedia_)
        return "UNAVAILABLE";

    auto account = getSIPAccount();
    if (!account || !account->isIceForMediaEnabled() || !account->isSrtpEnabled()
        || !dtlsCertificate_ || !dtlsPrivateKey_ || account->createDefaultMediaList(false).size() != 1)
        return "UNAVAILABLE";

    const auto* local = sdp_->getActiveLocalSdpSession();
    const auto* remote = sdp_->getActiveRemoteSdpSession();
    const auto slots = sdp_->getMediaSlots();
    if (local->media_count != 1 || remote->media_count != 1 || slots.size() != 1)
        return "UNSUPPORTED_MEDIA";
    const auto& [localAudio, remoteAudio] = slots.front();
    if (localAudio.type == MediaType::MEDIA_AUDIO
        && remoteAudio.type == MediaType::MEDIA_AUDIO
        && localAudio.enabled && remoteAudio.enabled && !localAudio.hold && !remoteAudio.hold
        && localAudio.direction_ == MediaDirection::SENDRECV
        && remoteAudio.direction_ == MediaDirection::SENDRECV
        && localAudio.transport == MediaTransport::UDP_TLS_RTP_SAVPF
        && remoteAudio.transport == MediaTransport::UDP_TLS_RTP_SAVPF)
        return "AUDIO_ONLY";
    return "UNSUPPORTED_MEDIA";
}

bool
SIPCall::promoteExternalMedia(const std::string& requestId)
{
    std::unique_lock lk {callMutex_};
    if (requestId.empty() || promotionStateLocked() != "AUDIO_ONLY") {
        JAMI_WARNING("[call:{}] Promotion unavailable for the active external-media dialog", getCallId());
        return false;
    }

    auto account = getSIPAccount();
    auto mediaList = account->createDefaultMediaList(false);
    promotionRequestId_ = requestId;
    promotionNegotiated_ = false;
    enableIce_ = true;
    sdp_->setSecureMediaKeyExchange(KeyExchangeProtocol::DTLS);
    if (initMediaStreams(mediaList) != mediaList.size()) {
        failPromotion("Could not create native RTP sessions");
        lk.unlock();
        hangup(PJSIP_SC_INTERNAL_SERVER_ERROR);
        return false;
    }
    try {
        if (trySIPSessionReinvite(mediaList, true) != ReinviteResult::Sent) {
            failPromotion("Could not send media re-INVITE");
            lk.unlock();
            hangup(PJSIP_SC_INTERNAL_SERVER_ERROR);
            return false;
        }
    } catch (const std::exception& e) {
        failPromotion(e.what());
        lk.unlock();
        hangup(PJSIP_SC_INTERNAL_SERVER_ERROR);
        return false;
    }
    externalSdp_.clear();
    externalRemoteSdp_.clear();
    emitSignal<libjami::CallSignal::ExternalMediaPromotionAccepted>(getAccountId(), getCallId(), requestId);
    return true;
}

bool
SIPCall::cancelExternalMediaPromotion(const std::string& requestId)
{
    std::unique_lock lk {callMutex_};
    if (requestId.empty() || promotionRequestId_ != requestId)
        return false;
    failPromotion("Promotion cancelled");
    if (inviteSession_ && inviteSession_->invite_tsx)
        promotionEnding_ = true;
    else {
        lk.unlock();
        hangup(PJSIP_SC_REQUEST_TERMINATED);
    }
    return true;
}

void
SIPCall::failPromotion(const std::string& reason)
{
    if (promotionRequestId_.empty())
        return;
    auto requestId = std::move(promotionRequestId_);
    promotionRequestId_.clear();
    JAMI_ERROR("[call:{}] External-media promotion failed: {}", getCallId(), reason);
    emitSignal<libjami::CallSignal::ExternalMediaPromotionFailure>(getAccountId(), getCallId(), requestId, reason);
}

void
SIPCall::checkPromotionReady()
{
    std::lock_guard lk {callMutex_};
    if (promotionRequestId_.empty())
        return;
    if (promotionNegotiated_ && isNativeMediaReadyLocked()) {
        auto requestId = std::move(promotionRequestId_);
        promotionRequestId_.clear();
        emitSignal<libjami::CallSignal::ExternalMediaPromotionReady>(getAccountId(), getCallId(), requestId);
    }
}

} // namespace jami
