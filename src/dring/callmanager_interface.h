/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include <stdexcept>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "dring.h"

namespace DRing {

void registerCallHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>&);

/* Call related methods */
std::string placeCall(const std::string& accountID, const std::string& to);

bool refuse(const std::string& callID);
bool accept(const std::string& callID);
bool hangUp(const std::string& callID);
bool hold(const std::string& callID);
bool unhold(const std::string& callID);
bool muteLocalMedia(const std::string& callid, const std::string& mediaType, bool mute);
bool transfer(const std::string& callID, const std::string& to);
bool attendedTransfer(const std::string& transferID, const std::string& targetID);
std::map<std::string, std::string> getCallDetails(const std::string& callID);
std::vector<std::string> getCallList();

/* Conference related methods */
void removeConference(const std::string& conference_id);
bool joinParticipant(const std::string& sel_callID, const std::string& drag_callID);
void createConfFromParticipantList(const std::vector<std::string>& participants);
bool isConferenceParticipant(const std::string& call_id);
bool addParticipant(const std::string& callID, const std::string& confID);
bool addMainParticipant(const std::string& confID);
bool detachParticipant(const std::string& callID);
bool joinConference(const std::string& sel_confID, const std::string& drag_confID);
bool hangUpConference(const std::string& confID);
bool holdConference(const std::string& confID);
bool unholdConference(const std::string& confID);
std::vector<std::string> getConferenceList();
std::vector<std::string> getParticipantList(const std::string& confID);
std::vector<std::string> getDisplayNames(const std::string& confID);
std::string getConferenceId(const std::string& callID);
std::map<std::string, std::string> getConferenceDetails(const std::string& callID);

/* Statistic related methods */
void startSmartInfo(uint32_t refreshTimeMs);
void stopSmartInfo();

/* File Playback methods */
bool startRecordedFilePlayback(const std::string& filepath);
void stopRecordedFilePlayback(const std::string& filepath);

/* General audio methods */
bool toggleRecording(const std::string& callID);
/* DEPRECATED */
void setRecording(const std::string& callID);

void recordPlaybackSeek(double value);
bool getIsRecording(const std::string& callID);
std::string getCurrentAudioCodecName(const std::string& callID);
void playDTMF(const std::string& key);
void startTone(int32_t start, int32_t type);

bool switchInput(const std::string& callID, const std::string& resource);

/* Security related methods */
void setSASVerified(const std::string& callID);
void resetSASVerified(const std::string& callID);
void setConfirmGoClear(const std::string& callID);
void requestGoClear(const std::string& callID);
void acceptEnrollment(const std::string& callID, bool accepted);

/* Instant messaging */
void sendTextMessage(const std::string& callID, const std::map<std::string, std::string>& messages, const std::string& from, bool isMixed);

// Call signal type definitions
struct CallSignal {
        struct StateChange {
                constexpr static const char* name = "StateChange";
                using cb_type = void(const std::string&, const std::string&, int);
        };
        struct TransferFailed {
                constexpr static const char* name = "TransferFailed";
                using cb_type = void(void);
        };
        struct TransferSucceeded {
                constexpr static const char* name = "TransferSucceeded";
                using cb_type = void(void);
        };
        struct RecordPlaybackStopped {
                constexpr static const char* name = "RecordPlaybackStopped";
                using cb_type = void(const std::string&);
        };
        struct VoiceMailNotify {
                constexpr static const char* name = "VoiceMailNotify";
                using cb_type = void(const std::string&, int32_t);
        };
        struct IncomingMessage {
                constexpr static const char* name = "IncomingMessage";
                using cb_type = void(const std::string&, const std::string&, const std::map<std::string, std::string>&);
        };
        struct IncomingCall {
                constexpr static const char* name = "IncomingCall";
                using cb_type = void(const std::string&, const std::string&, const std::string&);
        };
        struct RecordPlaybackFilepath {
                constexpr static const char* name = "RecordPlaybackFilepath";
                using cb_type = void(const std::string&, const std::string&);
        };
        struct ConferenceCreated {
                constexpr static const char* name = "ConferenceCreated";
                using cb_type = void(const std::string&);
        };
        struct ConferenceChanged {
                constexpr static const char* name = "ConferenceChanged";
                using cb_type = void(const std::string&, const std::string&);
        };
        struct UpdatePlaybackScale {
                constexpr static const char* name = "UpdatePlaybackScale";
                using cb_type = void(const std::string&, unsigned, unsigned);
        };
        struct ConferenceRemoved {
                constexpr static const char* name = "ConferenceRemoved";
                using cb_type = void(const std::string&);
        };
        struct NewCallCreated {
                constexpr static const char* name = "NewCallCreated";
                using cb_type = void(const std::string&, const std::string&, const std::string&);
        };
        struct SipCallStateChanged {
                constexpr static const char* name = "SipCallStateChanged";
                using cb_type = void(const std::string&, const std::string&, int);
        };
        struct RecordingStateChanged {
                constexpr static const char* name = "RecordingStateChanged";
                using cb_type = void(const std::string&, int);
        };
        struct SecureSdesOn {
                constexpr static const char* name = "SecureSdesOn";
                using cb_type = void(const std::string&);
        };
        struct SecureSdesOff {
                constexpr static const char* name = "SecureSdesOff";
                using cb_type = void(const std::string&);
        };
        struct RtcpReportReceived {
                constexpr static const char* name = "RtcpReportReceived";
                using cb_type = void(const std::string&, const std::map<std::string, int>&);
        };
        struct PeerHold {
                constexpr static const char* name = "PeerHold";
                using cb_type = void(const std::string&, bool);
        };
        struct VideoMuted {
                constexpr static const char* name = "VideoMuted";
                using cb_type = void(const std::string&, bool);
        };
        struct AudioMuted {
                constexpr static const char* name = "AudioMuted";
                using cb_type = void(const std::string&, bool);
        };
        struct SmartInfo {
                constexpr static const char* name = "SmartInfo";
                using cb_type = void(const std::map<std::string, std::string>&);
        };
};

}; // namespace DRing

#endif // DRING_CALLMANAGERI_H
