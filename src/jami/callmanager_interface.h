/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 */

#ifndef DRING_CALLMANAGERI_H
#define DRING_CALLMANAGERI_H

#include "def.h"

#include <stdexcept>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "jami.h"

namespace DRing {

[[deprecated("Replaced by registerSignalHandlers")]] DRING_PUBLIC void registerCallHandlers(
    const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

/* Call related methods */
DRING_PUBLIC std::string placeCall(const std::string& accountId, const std::string& to);

DRING_PUBLIC std::string placeCallWithMedia(
    const std::string& accountId,
    const std::string& to,
    const std::vector<std::map<std::string, std::string>>& mediaList);
DRING_PUBLIC bool refuse(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool accept(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool hangUp(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool hold(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool unhold(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool muteLocalMedia(const std::string& accountId,
                                 const std::string& callId,
                                 const std::string& mediaType,
                                 bool mute);
DRING_PUBLIC bool transfer(const std::string& accountId,
                           const std::string& callId,
                           const std::string& to);
DRING_PUBLIC bool attendedTransfer(const std::string& accountId,
                                   const std::string& callId,
                                   const std::string& targetID);
DRING_PUBLIC std::map<std::string, std::string> getCallDetails(const std::string& accountId,
                                                               const std::string& callId);
DRING_PUBLIC std::vector<std::string> getCallList(const std::string& accountId);

/* APIs that supports an arbitrary number of media */
DRING_PUBLIC bool acceptWithMedia(const std::string& accountId,
                                  const std::string& callId,
                                  const std::vector<DRing::MediaMap>& mediaList);
DRING_PUBLIC bool requestMediaChange(const std::string& accountId,
                                     const std::string& callId,
                                     const std::vector<DRing::MediaMap>& mediaList);

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
DRING_PUBLIC bool answerMediaChangeRequest(const std::string& accountId,
                                           const std::string& callId,
                                           const std::vector<DRing::MediaMap>& mediaList);

/* Conference related methods */
DRING_PUBLIC bool joinParticipant(const std::string& accountId,
                                  const std::string& sel_callId,
                                  const std::string& account2Id,
                                  const std::string& drag_callId);
DRING_PUBLIC void createConfFromParticipantList(const std::string& accountId,
                                                const std::vector<std::string>& participants);
DRING_PUBLIC void setConferenceLayout(const std::string& accountId,
                                      const std::string& confId,
                                      uint32_t layout);
DRING_PUBLIC void setActiveParticipant(const std::string& accountId,
                                       const std::string& confId,
                                       const std::string& callId);
DRING_PUBLIC bool isConferenceParticipant(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool addParticipant(const std::string& accountId,
                                 const std::string& callId,
                                 const std::string& account2Id,
                                 const std::string& confId);
DRING_PUBLIC bool addMainParticipant(const std::string& accountId, const std::string& confId);
DRING_PUBLIC bool detachLocalParticipant();
DRING_PUBLIC bool detachParticipant(const std::string& accountId, const std::string& callId);
DRING_PUBLIC bool joinConference(const std::string& accountId,
                                 const std::string& sel_confId,
                                 const std::string& account2Id,
                                 const std::string& drag_confId);
DRING_PUBLIC bool hangUpConference(const std::string& accountId, const std::string& confId);
DRING_PUBLIC bool holdConference(const std::string& accountId, const std::string& confId);
DRING_PUBLIC bool unholdConference(const std::string& accountId, const std::string& confId);
DRING_PUBLIC std::vector<std::string> getConferenceList(const std::string& accountId);
DRING_PUBLIC std::vector<std::string> getParticipantList(const std::string& accountId,
                                                         const std::string& confId);
DRING_PUBLIC std::string getConferenceId(const std::string& accountId, const std::string& callId);
DRING_PUBLIC std::map<std::string, std::string> getConferenceDetails(const std::string& accountId,
                                                                     const std::string& callId);
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getConferenceInfos(
    const std::string& accountId, const std::string& confId);
DRING_PUBLIC void setModerator(const std::string& accountId,
                               const std::string& confId,
                               const std::string& peerId,
                               const bool& state);
DRING_PUBLIC void muteParticipant(const std::string& accountId,
                                  const std::string& confId,
                                  const std::string& peerId,
                                  const bool& state);
DRING_PUBLIC void hangupParticipant(const std::string& accountId,
                                    const std::string& confId,
                                    const std::string& participant);
DRING_PUBLIC void raiseParticipantHand(const std::string& accountId,
                                       const std::string& confId,
                                       const std::string& peerId,
                                       const bool& state);

/* Statistic related methods */
DRING_PUBLIC void startSmartInfo(uint32_t refreshTimeMs);
DRING_PUBLIC void stopSmartInfo();

/* File Playback methods */
DRING_PUBLIC bool startRecordedFilePlayback(const std::string& filepath);
DRING_PUBLIC void stopRecordedFilePlayback();

/* General audio methods */
DRING_PUBLIC bool toggleRecording(const std::string& accountId, const std::string& callId);
/* DEPRECATED */
DRING_PUBLIC void setRecording(const std::string& accountId, const std::string& callId);

DRING_PUBLIC void recordPlaybackSeek(double value);
DRING_PUBLIC bool getIsRecording(const std::string& accountId, const std::string& callId);
DRING_PUBLIC void playDTMF(const std::string& key);
DRING_PUBLIC void startTone(int32_t start, int32_t type);

DRING_PUBLIC bool switchInput(const std::string& accountId,
                              const std::string& callId,
                              const std::string& resource);
DRING_PUBLIC bool switchSecondaryInput(const std::string& accountId,
                                       const std::string& confId,
                                       const std::string& resource);

/* Instant messaging */
DRING_PUBLIC void sendTextMessage(const std::string& accountId,
                                  const std::string& callId,
                                  const std::map<std::string, std::string>& messages,
                                  const std::string& from,
                                  bool isMixed);

// Call signal type definitions
struct DRING_PUBLIC CallSignal
{
    struct DRING_PUBLIC StateChange
    {
        constexpr static const char* name = "StateChange";
        using cb_type = void(const std::string&, const std::string&, const std::string&, int);
    };
    struct DRING_PUBLIC TransferFailed
    {
        constexpr static const char* name = "TransferFailed";
        using cb_type = void(void);
    };
    struct DRING_PUBLIC TransferSucceeded
    {
        constexpr static const char* name = "TransferSucceeded";
        using cb_type = void(void);
    };
    struct DRING_PUBLIC RecordPlaybackStopped
    {
        constexpr static const char* name = "RecordPlaybackStopped";
        using cb_type = void(const std::string&);
    };
    struct DRING_PUBLIC VoiceMailNotify
    {
        constexpr static const char* name = "VoiceMailNotify";
        using cb_type = void(const std::string&, int32_t, int32_t, int32_t);
    };
    struct DRING_PUBLIC IncomingMessage
    {
        constexpr static const char* name = "IncomingMessage";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::map<std::string, std::string>&);
    };
    struct DRING_PUBLIC IncomingCall
    {
        constexpr static const char* name = "IncomingCall";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct DRING_PUBLIC IncomingCallWithMedia
    {
        constexpr static const char* name = "IncomingCallWithMedia";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
    struct DRING_PUBLIC MediaChangeRequested
    {
        constexpr static const char* name = "MediaChangeRequested";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
    struct DRING_PUBLIC RecordPlaybackFilepath
    {
        constexpr static const char* name = "RecordPlaybackFilepath";
        using cb_type = void(const std::string&, const std::string&);
    };
    struct DRING_PUBLIC ConferenceCreated
    {
        constexpr static const char* name = "ConferenceCreated";
        using cb_type = void(const std::string&, const std::string&);
    };
    struct DRING_PUBLIC ConferenceChanged
    {
        constexpr static const char* name = "ConferenceChanged";
        using cb_type = void(const std::string&, const std::string&, const std::string&);
    };
    struct DRING_PUBLIC UpdatePlaybackScale
    {
        constexpr static const char* name = "UpdatePlaybackScale";
        using cb_type = void(const std::string&, unsigned, unsigned);
    };
    struct DRING_PUBLIC ConferenceRemoved
    {
        constexpr static const char* name = "ConferenceRemoved";
        using cb_type = void(const std::string&, const std::string&);
    };
    struct DRING_PUBLIC RecordingStateChanged
    {
        constexpr static const char* name = "RecordingStateChanged";
        using cb_type = void(const std::string&, int);
    };
    struct DRING_PUBLIC RtcpReportReceived
    {
        constexpr static const char* name = "RtcpReportReceived";
        using cb_type = void(const std::string&, const std::map<std::string, int>&);
    };
    struct DRING_PUBLIC PeerHold
    {
        constexpr static const char* name = "PeerHold";
        using cb_type = void(const std::string&, bool);
    };
    struct DRING_PUBLIC VideoMuted
    {
        constexpr static const char* name = "VideoMuted";
        using cb_type = void(const std::string&, bool);
    };
    struct DRING_PUBLIC AudioMuted
    {
        constexpr static const char* name = "AudioMuted";
        using cb_type = void(const std::string&, bool);
    };
    struct DRING_PUBLIC SmartInfo
    {
        constexpr static const char* name = "SmartInfo";
        using cb_type = void(const std::map<std::string, std::string>&);
    };
    struct DRING_PUBLIC ConnectionUpdate
    {
        constexpr static const char* name = "ConnectionUpdate";
        using cb_type = void(const std::string&, int);
    };
    struct DRING_PUBLIC OnConferenceInfosUpdated
    {
        constexpr static const char* name = "OnConferenceInfosUpdated";
        using cb_type = void(const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
    struct DRING_PUBLIC RemoteRecordingChanged
    {
        constexpr static const char* name = "RemoteRecordingChanged";
        using cb_type = void(const std::string&, const std::string&, bool);
    };
    // Report media negotiation status
    struct DRING_PUBLIC MediaNegotiationStatus
    {
        constexpr static const char* name = "MediaNegotiationStatus";
        using cb_type = void(const std::string&,
                             const std::string&,
                             const std::vector<std::map<std::string, std::string>>&);
    };
};

} // namespace DRing

#endif // DRING_CALLMANAGERI_H
