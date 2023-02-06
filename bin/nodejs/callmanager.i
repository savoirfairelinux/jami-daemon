/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
 *          Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *          Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

%header %{

#include "jami/jami.h"
#include "jami/callmanager_interface.h"

class Callback {
public:
    virtual ~Callback() {}
    virtual void callStateChanged(const std::string& accountId, const std::string& callId, const std::string& state, int detail_code){}
    virtual void transferFailed(void){}
    virtual void transferSucceeded(void){}
    virtual void recordPlaybackStopped(const std::string& path){}
    virtual void voiceMailNotify(const std::string& accountId, int newCount, int oldCount, int urgentCount){}
    virtual void incomingMessage(const std::string& accountId, const std::string& callId, const std::string& from, const std::map<std::string, std::string>& messages){}
    virtual void incomingCall(const std::string& accountId, const std::string& callId, const std::string& from){}
    virtual void incomingCallWithMedia(const std::string& accountId, const std::string& callId, const std::string& from,
        const std::vector<std::map<std::string, std::string>>& mediaList){}
    virtual void mediaChangeRequested(const std::string& accountId, const std::string& callId,
        const std::vector<std::map<std::string, std::string>>& mediaList){}
    virtual void recordPlaybackFilepath(const std::string& id, const std::string& filename){}
    virtual void conferenceCreated(const std::string& accountId, const std::string& confId){}
    virtual void conferenceChanged(const std::string& accountId, const std::string& confId, const std::string& state){}
    virtual void conferenceRemoved(const std::string& accountId, const std::string& confId){}
    virtual void updatePlaybackScale(const std::string& filepath, int position, int scale){}
    virtual void newCall(const std::string& accountId, const std::string& callId, const std::string& to){}
    virtual void recordingStateChanged(const std::string& callId, int code){}
    virtual void recordStateChange(const std::string& callId, int state){}
    virtual void onRtcpReportReceived(const std::string& callId, const std::map<std::string, int>& stats){}
    virtual void onConferenceInfosUpdated(const std::string& confId, const std::vector<std::map<std::string, std::string>>& infos) {}
    virtual void peerHold(const std::string& callId, bool holding){}
    virtual void audioMuted(const std::string& callId, bool muted){}
    virtual void videoMuted(const std::string& callId, bool muted){}
    virtual void connectionUpdate(const std::string& id, int state){}
    virtual void remoteRecordingChanged(const std::string& callId, const std::string& peer_number, bool state){}
    virtual void mediaNegotiationStatus(const std::string& callId, const std::string& event,
        const std::vector<std::map<std::string, std::string>>& mediaList){}
};


%}

%feature("director") Callback;

namespace libjami {

/* Call related methods */
std::string placeCallWithMedia(const std::string& accountId,
                               const std::string& to,
                               const std::vector<std::map<std::string, std::string>>& mediaList);
bool requestMediaChange(const std::string& accountId, const std::string& callId, const std::vector<std::map<std::string, std::string>>& mediaList);
bool refuse(const std::string& accountId, const std::string& callId);
bool accept(const std::string& accountId, const std::string& callId);
bool acceptWithMedia(const std::string& accountId, const std::string& callId, const std::vector<std::map<std::string, std::string>>& mediaList);
bool answerMediaChangeRequest(const std::string& accountId, const std::string& callId, const std::vector<std::map<std::string, std::string>>& mediaList);
bool hangUp(const std::string& accountId, const std::string& callId);
bool hold(const std::string& accountId, const std::string& callId);
bool unhold(const std::string& accountId, const std::string& callId);
bool muteLocalMedia(const std::string& accountId, const std::string& callId, const std::string& mediaType, bool mute);
bool transfer(const std::string& accountId, const std::string& callId, const std::string& to);
bool attendedTransfer(const std::string& accountId, const std::string& transferID, const std::string& targetID);
std::map<std::string, std::string> getCallDetails(const std::string& accountId, const std::string& callId);
std::vector<std::string> getCallList(const std::string& accountId);

/* Conference related methods */
bool joinParticipant(const std::string& accountId, const std::string& sel_callId, const std::string& account2Id, const std::string& drag_callId);
void createConfFromParticipantList(const std::string& accountId, const std::vector<std::string>& participants);
void setConferenceLayout(const std::string& accountId, const std::string& confId, int layout);
void setActiveParticipant(const std::string& accountId, const std::string& confId, const std::string& callId);
bool isConferenceParticipant(const std::string& accountId, const std::string& callId);
bool addParticipant(const std::string& accountId, const std::string& callId, const std::string& account2Id, const std::string& confId);
bool addMainParticipant(const std::string& accountId, const std::string& confId);
bool detachParticipant(const std::string& accountId, const std::string& callId);
bool joinConference(const std::string& accountId, const std::string& sel_confId, const std::string& account2Id, const std::string& drag_confId);
bool hangUpConference(const std::string& accountId, const std::string& confId);
bool holdConference(const std::string& accountId, const std::string& confId);
bool unholdConference(const std::string& accountId, const std::string& confId);
std::vector<std::string> getConferenceList(const std::string& accountId);
std::vector<std::string> getParticipantList(const std::string& accountId, const std::string& confId);
std::string getConferenceId(const std::string& accountId, const std::string& callId);
std::map<std::string, std::string> getConferenceDetails(const std::string& accountId, const std::string& callId);
std::vector<libjami::MediaMap> currentMediaList(const std::string& accountId, const std::string& callId);
std::vector<std::map<std::string, std::string>> getConferenceInfos(const std::string& accountId, const std::string& confId);
void setModerator(const std::string& accountId, const std::string& confId, const std::string& peerId, const bool& state);
void muteStream(const std::string& accountId,
                    const std::string& confId,
                    const std::string& accountUri,
                    const std::string& deviceId,
                    const std::string& streamId,
                    const bool& state);
void setActiveStream(const std::string& accountId,
                    const std::string& confId,
                    const std::string& accountUri,
                    const std::string& deviceId,
                    const std::string& streamId,
                    const bool& state);
void hangupParticipant(const std::string& accountId,
                const std::string& confId,
                const std::string& accountUri,
                const std::string& deviceId);
void raiseHand(const std::string& accountId,
                const std::string& confId,
                const std::string& accountUri,
                const std::string& deviceId,
                const bool& state);
// DEPRECATED
void muteParticipant(const std::string& accountId, const std::string& confId, const std::string& peerId, const bool& state);
void raiseParticipantHand(const std::string& accountId, const std::string& confId, const std::string& peerId, const bool& state);

/* File Playback methods */
bool startRecordedFilePlayback(const std::string& filepath);
void stopRecordedFilePlayback();

/* General audio methods */
bool toggleRecording(const std::string& accountId, const std::string& callId);
/* DEPRECATED */
void setRecording(const std::string& accountId, const std::string& callId);

void recordPlaybackSeek(double value);
bool getIsRecording(const std::string& accountId, const std::string& callId);
void playDTMF(const std::string& key);
void startTone(int32_t start, int32_t type);

bool switchInput(const std::string& accountId, const std::string& callId, const std::string& resource);

/* Instant messaging */
void sendTextMessage(const std::string& accountId, const std::string& callId, const std::map<std::string, std::string>& messages, const std::string& from, const bool& isMixed);

}

class Callback {
public:
    virtual ~Callback() {}
    virtual void callStateChanged(const std::string& accountId, const std::string& callId, const std::string& state, int detail_code){}
    virtual void transferFailed(void){}
    virtual void transferSucceeded(void){}
    virtual void recordPlaybackStopped(const std::string& path){}
    virtual void voiceMailNotify(const std::string& accountId, int newCount, int oldCount, int urgentCount){}
    virtual void incomingMessage(const std::string& accountId, const std::string& callId, const std::string& from, const std::map<std::string, std::string>& messages){}
    virtual void incomingCall(const std::string& accountId, const std::string& callId, const std::string& from){}
    virtual void incomingCallWithMedia(const std::string& accountId, const std::string& callId, const std::string& from,
        const std::vector<std::map<std::string, std::string>>& mediaList){}
    virtual void mediaChangeRequested(const std::string& accountId, const std::string& callId,
        const std::vector<std::map<std::string, std::string>>& mediaList){}
    virtual void recordPlaybackFilepath(const std::string& id, const std::string& filename){}
    virtual void conferenceCreated(const std::string& accountId, const std::string& confId){}
    virtual void conferenceChanged(const std::string& accountId, const std::string& confId, const std::string& state){}
    virtual void conferenceRemoved(const std::string& accountId, const std::string& confId){}
    virtual void updatePlaybackScale(const std::string& filepath, int position, int scale){}
    virtual void newCall(const std::string& accountId, const std::string& callId, const std::string& to){}
    virtual void recordingStateChanged(const std::string& callId, int code){}
    virtual void recordStateChange(const std::string& callId, int state){}
    virtual void onRtcpReportReceived(const std::string& callId, const std::map<std::string, int>& stats){}
    virtual void onConferenceInfosUpdated(const std::string& confId, const std::vector<std::map<std::string, std::string>>& infos) {}
    virtual void peerHold(const std::string& callId, bool holding){}
    virtual void audioMuted(const std::string& callId, bool muted){}
    virtual void videoMuted(const std::string& callId, bool muted){}
    virtual void connectionUpdate(const std::string& id, int state){}
    virtual void remoteRecordingChanged(const std::string& callId, const std::string& peer_number, bool state){}
    virtual void mediaNegotiationStatus(const std::string& callId, const std::string& event,
        const std::vector<std::map<std::string, std::string>>& mediaList){}
};
