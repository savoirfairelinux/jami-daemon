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

#ifndef LIBJAMI_CALLMANAGERI_H
#define LIBJAMI_CALLMANAGERI_H

#include "def.h"

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "jami.h"

namespace libjami {

[[deprecated("Replaced by registerSignalHandlers")]] LIBJAMI_PUBLIC void registerCallHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

/* Call related methods */
LIBJAMI_PUBLIC std::string placeCall(const std::string& accountId, const std::string& to);

LIBJAMI_PUBLIC std::string placeCallWithMedia(const std::string& accountId,
                                              const std::string& to,
                                              const std::vector<std::map<std::string, std::string>>& mediaList);
LIBJAMI_PUBLIC bool refuse(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool accept(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool hangUp(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool hold(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool resume(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool muteLocalMedia(const std::string& accountId,
                                   const std::string& callId,
                                   const std::string& mediaType,
                                   bool mute);
LIBJAMI_PUBLIC bool transfer(const std::string& accountId, const std::string& callId, const std::string& to);
LIBJAMI_PUBLIC bool attendedTransfer(const std::string& accountId,
                                     const std::string& callId,
                                     const std::string& targetID);
LIBJAMI_PUBLIC std::map<std::string, std::string> getCallDetails(const std::string& accountId,
                                                                 const std::string& callId);
LIBJAMI_PUBLIC std::vector<std::string> getCallList(const std::string& accountId);

/* APIs that supports an arbitrary number of media */
LIBJAMI_PUBLIC bool acceptWithMedia(const std::string& accountId,
                                    const std::string& callId,
                                    const std::vector<libjami::MediaMap>& mediaList);

/* External media endpoint APIs. The SDP session is provided/consumed by the
 * API caller (e.g. a WebRTC browser endpoint): the daemon performs the call
 * signaling but does not run local ICE/RTP for the call media. */
LIBJAMI_PUBLIC std::string placeCallWithExternalMedia(const std::string& accountId,
                                                      const std::string& to,
                                                      const std::string& sdpOffer);
LIBJAMI_PUBLIC bool acceptWithExternalMedia(const std::string& accountId,
                                            const std::string& callId,
                                            const std::string& sdpAnswer);
LIBJAMI_PUBLIC bool answerMediaChangeRequestWithExternalMedia(const std::string& accountId,
                                                              const std::string& callId,
                                                              const std::string& sdpAnswer);
LIBJAMI_PUBLIC bool setVideoOrientation(const std::string& accountId,
                                        const std::string& callId,
                                        int streamIdx,
                                        int rotation);
LIBJAMI_PUBLIC bool requestMediaChange(const std::string& accountId,
                                       const std::string& callId,
                                       const std::vector<libjami::MediaMap>& mediaList);

/**
 * Answer a media change request
 * @param accountId
 * @param callId
 * @param mediaList the list of media attributes. The client can
 * control the media through the attributes. The list should have
 * the same size as the list reported in the media change request.
 * The client can ignore the media update request by not calling this
 * method, or calling it with an empty media list.
 */
LIBJAMI_PUBLIC bool answerMediaChangeRequest(const std::string& accountId,
                                             const std::string& callId,
                                             const std::vector<libjami::MediaMap>& mediaList);

/* Conference related methods */
LIBJAMI_PUBLIC bool joinParticipant(const std::string& accountId,
                                    const std::string& sel_callId,
                                    const std::string& account2Id,
                                    const std::string& drag_callId);
LIBJAMI_PUBLIC void createConfFromParticipantList(const std::string& accountId,
                                                  const std::vector<std::string>& participants);
LIBJAMI_PUBLIC void setConferenceLayout(const std::string& accountId, const std::string& confId, uint32_t layout);
LIBJAMI_PUBLIC bool isConferenceParticipant(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool addParticipant(const std::string& accountId,
                                   const std::string& callId,
                                   const std::string& account2Id,
                                   const std::string& confId);
LIBJAMI_PUBLIC bool addMainParticipant(const std::string& accountId, const std::string& confId);
LIBJAMI_PUBLIC bool detachLocalParticipant();
LIBJAMI_PUBLIC bool detachParticipant(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC bool joinConference(const std::string& accountId,
                                   const std::string& sel_confId,
                                   const std::string& account2Id,
                                   const std::string& drag_confId);
LIBJAMI_PUBLIC bool hangUpConference(const std::string& accountId, const std::string& confId);
/* Returns false for an invalid/duplicate request or a conversation not held by this account.
 * An accepted request reports its SDP answer and media readiness asynchronously. */
LIBJAMI_PUBLIC bool startBrowserConference(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& requestId,
                                           const std::string& sdpOffer);
LIBJAMI_PUBLIC bool cancelBrowserConference(const std::string& accountId, const std::string& requestId);
/* WAITING until the call is answered and its ICE/DTLS-SRTP send and receive
 * paths are running; READY only for an established native-media call;
 * UNAVAILABLE for absent/ended/external-media calls. A WAITING call can fail:
 * observe StateChange and impose a dialing deadline before hanging it up. */
LIBJAMI_PUBLIC std::string getNativeCallMediaState(const std::string& accountId, const std::string& callId);
/* Dial a native participant for an already Ready browser host. Its audio and
 * optional video senders use the conference mixer, never local input devices.
 * An empty call ID means dialing could not start. Wait for CURRENT and READY
 * before adding. */
LIBJAMI_PUBLIC std::string placeBrowserParticipantCall(const std::string& accountId,
                                                       const std::string& to,
                                                       const std::string& requestId,
                                                       bool isVideo = false);
/* Adds a READY native-media call to a ready browser host. True means it was
 * bound; false leaves the caller responsible for retrying or hanging up. */
LIBJAMI_PUBLIC bool addNativeToBrowserConference(const std::string& accountId,
                                                 const std::string& callId,
                                                 const std::string& requestId);
/* Classify the active external-media SIP dialog without changing it.
 * AUDIO_ONLY and AUDIO_VIDEO are eligible; UNSUPPORTED_MEDIA means its SDP
 * cannot be migrated, UNAVAILABLE means the call/dialog is not promotable.
 * Recheck immediately before promotion: dialog state can change meanwhile. */
LIBJAMI_PUBLIC std::string getExternalMediaPromotionState(const std::string& accountId, const std::string& callId);
/* True means the in-dialog re-INVITE was sent. Keep browser media until
 * ExternalMediaPromotionReady; a late failure is reported, but restoration of
 * the original browser RTP cannot be guaranteed after peer acceptance. */
LIBJAMI_PUBLIC bool requestExternalMediaPromotion(const std::string& accountId,
                                                  const std::string& callId,
                                                  const std::string& requestId);
/* Cancel a pending promotion; the dialog is ended rather than reverting to browser RTP. */
LIBJAMI_PUBLIC bool cancelExternalMediaPromotion(const std::string& accountId,
                                                 const std::string& callId,
                                                 const std::string& requestId);
/* Reverse a browser-hosted native call on its existing SIP dialog. True means
 * the browser SDP re-INVITE was sent, not that direct browser ICE is ready.
 * The conference remains active until completeExternalMediaDemotion is called
 * after the browser establishes direct ICE. Late failure is not guaranteed
 * recoverable after the peer accepts the re-INVITE. */
LIBJAMI_PUBLIC bool requestExternalMediaDemotion(const std::string& accountId,
                                                 const std::string& callId,
                                                 const std::string& conferenceId,
                                                 const std::string& requestId,
                                                 const std::string& sdpOffer);
LIBJAMI_PUBLIC bool completeExternalMediaDemotion(const std::string& accountId,
                                                  const std::string& callId,
                                                  const std::string& requestId);
/* Called when the account drops a browser-hosted conference, even without Node listeners. */
LIBJAMI_PUBLIC void browserConferenceRemoved(const std::string& accountId, const std::string& confId);
/* Detach the browser media owner without hanging up native SIP participants.
 * Used only after direct browser ICE is ready and the last call is being
 * detached from its hosted conference. */
LIBJAMI_PUBLIC void retireBrowserConferenceHost(const std::string& accountId, const std::string& confId);
LIBJAMI_PUBLIC void cancelBrowserConferencesForAccount(const std::string& accountId);
LIBJAMI_PUBLIC void cancelAllBrowserConferences();
LIBJAMI_PUBLIC void allowBrowserConferences();
LIBJAMI_PUBLIC bool holdConference(const std::string& accountId, const std::string& confId);
LIBJAMI_PUBLIC bool resumeConference(const std::string& accountId, const std::string& confId);
LIBJAMI_PUBLIC std::vector<std::string> getConferenceList(const std::string& accountId);
/// NOTE: Despite its name, this returns the list of call IDs (subcalls) in the conference,
/// not the actual participant URIs. This naming is historical.
/// However the URIs are accessible from the confInfos structure passed with the OnConferenceInfosUpdated signal.
/// It should be noted that as of February 2026, confInfos is primarily generated by the VideoMixer
/// based on active RTP streams, which means it can be unreliable during state transitions
/// (e.g., camera activation/deactivation) where streams are temporarily missing.
LIBJAMI_PUBLIC std::vector<std::string> getParticipantList(const std::string& accountId, const std::string& confId);
LIBJAMI_PUBLIC std::string getConferenceId(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC std::map<std::string, std::string> getConferenceDetails(const std::string& accountId,
                                                                       const std::string& callId);
LIBJAMI_PUBLIC std::vector<libjami::MediaMap> currentMediaList(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getConferenceInfos(const std::string& accountId,
                                                                                  const std::string& confId);
LIBJAMI_PUBLIC void setModerator(const std::string& accountId,
                                 const std::string& confId,
                                 const std::string& accountUri,
                                 const bool& state);
/// DEPRECATED USE muteStream
LIBJAMI_PUBLIC void muteParticipant(const std::string& accountId,
                                    const std::string& confId,
                                    const std::string& accountUri,
                                    const bool& state);
// Note: muting Audio not supported yet
LIBJAMI_PUBLIC void muteStream(const std::string& accountId,
                               const std::string& confId,
                               const std::string& accountUri,
                               const std::string& deviceId,
                               const std::string& streamId,
                               const bool& state);
/// DEPRECATED, USE setActiveStream
LIBJAMI_PUBLIC void setActiveParticipant(const std::string& accountId,
                                         const std::string& confId,
                                         const std::string& callId);
LIBJAMI_PUBLIC void setActiveStream(const std::string& accountId,
                                    const std::string& confId,
                                    const std::string& accountUri,
                                    const std::string& deviceId,
                                    const std::string& streamId,
                                    const bool& state);
LIBJAMI_PUBLIC void hangupParticipant(const std::string& accountId,
                                      const std::string& confId,
                                      const std::string& accountUri,
                                      const std::string& deviceId);
/// DEPRECATED, use raiseHand
LIBJAMI_PUBLIC void raiseParticipantHand(const std::string& accountId,
                                         const std::string& confId,
                                         const std::string& peerId,
                                         const bool& state);
LIBJAMI_PUBLIC void raiseHand(const std::string& accountId,
                              const std::string& confId,
                              const std::string& accountUri,
                              const std::string& deviceId,
                              const bool& state);

/* File Playback methods */
LIBJAMI_PUBLIC bool startRecordedFilePlayback(const std::string& filepath);
LIBJAMI_PUBLIC void stopRecordedFilePlayback();

/* General audio methods */
LIBJAMI_PUBLIC bool toggleRecording(const std::string& accountId, const std::string& callId);
/* DEPRECATED */
LIBJAMI_PUBLIC void setRecording(const std::string& accountId, const std::string& callId);

LIBJAMI_PUBLIC void recordPlaybackSeek(double value);
LIBJAMI_PUBLIC bool getIsRecording(const std::string& accountId, const std::string& callId);
LIBJAMI_PUBLIC void playDTMF(const std::string& key);
LIBJAMI_PUBLIC void startTone(int32_t start, int32_t type);

LIBJAMI_PUBLIC bool switchInput(const std::string& accountId, const std::string& callId, const std::string& resource);

/* Instant messaging */
LIBJAMI_PUBLIC void sendTextMessage(const std::string& accountId,
                                    const std::string& callId,
                                    const std::map<std::string, std::string>& messages,
                                    const std::string& from,
                                    bool isMixed);

// Call signal type definitions
struct LIBJAMI_PUBLIC CallSignal
{
    struct LIBJAMI_PUBLIC StateChange
    {
        constexpr static const char* name = "StateChange";
        using cb_type = void(const std::string&, const std::string&, const std::string&, int);
    };
    struct LIBJAMI_PUBLIC TransferFailed
    {
        constexpr static const char* name = "TransferFailed";
        using cb_type = void(void);
    };
    struct LIBJAMI_PUBLIC RemoteSdpReceived
    {
        constexpr static const char* name = "RemoteSdpReceived";
        // accountId, callId, sdp
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC VideoOrientationChanged
    {
        constexpr static const char* name = "VideoOrientationChanged";
        // accountId, callId, streamIdx, rotation
        using cb_type = void(const std::string&, const std::string&, int, int);
    };
    struct LIBJAMI_PUBLIC TransferSucceeded
    {
        constexpr static const char* name = "TransferSucceeded";
        using cb_type = void(void);
    };
    struct LIBJAMI_PUBLIC RecordPlaybackStopped
    {
        constexpr static const char* name = "RecordPlaybackStopped";
        using cb_type = void(const std::string&);
    };
    struct LIBJAMI_PUBLIC VoiceMailNotify
    {
        constexpr static const char* name = "VoiceMailNotify";
        using cb_type = void(const std::string&, int32_t, int32_t, int32_t);
    };
    struct LIBJAMI_PUBLIC IncomingMessage
    {
        constexpr static const char* name = "IncomingMessage";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::map<std::string, std::string>&);
    };
    struct LIBJAMI_PUBLIC IncomingCall
    {
        constexpr static const char* name = "IncomingCall";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
    struct LIBJAMI_PUBLIC MediaChangeRequested
    {
        constexpr static const char* name = "MediaChangeRequested";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
    struct LIBJAMI_PUBLIC RecordPlaybackFilepath
    {
        constexpr static const char* name = "RecordPlaybackFilepath";
        using cb_type = void(const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ConferenceCreated
    {
        constexpr static const char* name = "ConferenceCreated";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ConferenceChanged
    {
        constexpr static const char* name = "ConferenceChanged";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC UpdatePlaybackScale
    {
        constexpr static const char* name = "UpdatePlaybackScale";
        using cb_type = void(const std::string&, unsigned, unsigned);
    };
    struct LIBJAMI_PUBLIC ConferenceRemoved
    {
        constexpr static const char* name = "ConferenceRemoved";
        using cb_type = void(const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC BrowserConferenceHostAnswer
    {
        constexpr static const char* name = "BrowserConferenceHostAnswer";
        using cb_type = void(const std::string&, const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC BrowserConferenceHostReady
    {
        constexpr static const char* name = "BrowserConferenceHostReady";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC BrowserConferenceHostFailure
    {
        constexpr static const char* name = "BrowserConferenceHostFailure";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ExternalMediaPromotionAccepted
    {
        constexpr static const char* name = "ExternalMediaPromotionAccepted";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ExternalMediaPromotionReady
    {
        constexpr static const char* name = "ExternalMediaPromotionReady";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ExternalMediaPromotionFailure
    {
        constexpr static const char* name = "ExternalMediaPromotionFailure";
        using cb_type = void(const std::string&, const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ExternalMediaDemotionAccepted
    {
        constexpr static const char* name = "ExternalMediaDemotionAccepted";
        using cb_type = void(const std::string&, const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ExternalMediaDemotionReady
    {
        constexpr static const char* name = "ExternalMediaDemotionReady";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC ExternalMediaDemotionFailure
    {
        constexpr static const char* name = "ExternalMediaDemotionFailure";
        using cb_type = void(const std::string&, const std::string&, const std::string&, const std::string&);
    };
    struct LIBJAMI_PUBLIC RecordingStateChanged
    {
        constexpr static const char* name = "RecordingStateChanged";
        using cb_type = void(const std::string&, int);
    };
    struct LIBJAMI_PUBLIC RtcpReportReceived
    {
        constexpr static const char* name = "RtcpReportReceived";
        using cb_type = void(const std::string&, const std::map<std::string, int>&);
    };
    struct LIBJAMI_PUBLIC PeerHold
    {
        constexpr static const char* name = "PeerHold";
        using cb_type = void(const std::string&, bool);
    };
    struct LIBJAMI_PUBLIC VideoMuted
    {
        constexpr static const char* name = "VideoMuted";
        using cb_type = void(const std::string&, bool);
    };
    struct LIBJAMI_PUBLIC AudioMuted
    {
        constexpr static const char* name = "AudioMuted";
        using cb_type = void(const std::string&, bool);
    };
    struct LIBJAMI_PUBLIC ConnectionUpdate
    {
        constexpr static const char* name = "ConnectionUpdate";
        using cb_type = void(const std::string&, int);
    };
    struct LIBJAMI_PUBLIC OnConferenceInfosUpdated
    {
        constexpr static const char* name = "OnConferenceInfosUpdated";
        using cb_type = void(const std::string&, const std::vector<std::map<std::string, std::string>>&);
    };
    struct LIBJAMI_PUBLIC RemoteRecordingChanged
    {
        constexpr static const char* name = "RemoteRecordingChanged";
        using cb_type = void(const std::string&, const std::string&, bool);
    };
    // Report media negotiation status
    struct LIBJAMI_PUBLIC MediaNegotiationStatus
    {
        constexpr static const char* name = "MediaNegotiationStatus";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
};

} // namespace libjami

#endif // LIBJAMI_CALLMANAGERI_H
