/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#pragma once

#include "dbuscallmanager.adaptor.h"
#include <callmanager_interface.h>

class DBusCallManager : public sdbus::AdaptorInterfaces<cx::ring::Ring::CallManager_adaptor>
{
public:
    DBusCallManager(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, "/cx/ring/Ring/CallManager")
    {
        registerAdaptor();
        registerSignalHandlers();
    }

    ~DBusCallManager()
    {
        unregisterAdaptor();
    }

    auto
    placeCall(const std::string& accountId, const std::string& to)
        -> decltype(libjami::placeCall(accountId, to))
    {
        return libjami::placeCall(accountId, to);
    }
    auto
    placeCallWithMedia(const std::string& accountId,
                       const std::string& to,
                       const std::vector<std::map<std::string, std::string>>& mediaList)
        -> decltype(libjami::placeCallWithMedia(accountId, to, mediaList))
    {
        return libjami::placeCallWithMedia(accountId, to, mediaList);
    }

    auto
    requestMediaChange(const std::string& accountId,
                       const std::string& callId,
                       const std::vector<std::map<std::string, std::string>>& mediaList)
        -> decltype(libjami::requestMediaChange(accountId, callId, mediaList))
    {
        return libjami::requestMediaChange(accountId, callId, mediaList);
    }

    auto
    refuse(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::refuse(accountId, callId))
    {
        return libjami::refuse(accountId, callId);
    }

    auto
    accept(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::accept(accountId, callId))
    {
        return libjami::accept(accountId, callId);
    }

    auto
    acceptWithMedia(const std::string& accountId,
                    const std::string& callId,
                    const std::vector<std::map<std::string, std::string>>& mediaList)
        -> decltype(libjami::acceptWithMedia(accountId, callId, mediaList))
    {
        return libjami::acceptWithMedia(accountId, callId, mediaList);
    }

    auto
    answerMediaChangeRequest(
        const std::string& accountId,
        const std::string& callId,
        const std::vector<std::map<std::string, std::string>>& mediaList)
        -> decltype(libjami::answerMediaChangeRequest(accountId, callId, mediaList))
    {
        return libjami::answerMediaChangeRequest(accountId, callId, mediaList);
    }

    auto
    hangUp(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::hangUp(accountId, callId))
    {
        return libjami::hangUp(accountId, callId);
    }

    auto
    hold(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::hold(accountId, callId))
    {
        return libjami::hold(accountId, callId);
    }

    auto
    unhold(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::unhold(accountId, callId))
    {
        return libjami::unhold(accountId, callId);
    }

    auto
    muteLocalMedia(const std::string& accountId,
                   const std::string& callId,
                   const std::string& mediaType,
                   const bool& mute)
        -> decltype(libjami::muteLocalMedia(accountId, callId, mediaType, mute))
    {
        return libjami::muteLocalMedia(accountId, callId, mediaType, mute);
    }

    auto
    transfer(const std::string& accountId,
             const std::string& callId,
             const std::string& to) -> decltype(libjami::transfer(accountId, callId, to))
    {
        return libjami::transfer(accountId, callId, to);
    }

    auto
    attendedTransfer(const std::string& accountId,
                     const std::string& callId,
                     const std::string& targetId)
        -> decltype(libjami::attendedTransfer(accountId, callId, targetId))
    {
        return libjami::attendedTransfer(accountId, callId, targetId);
    }

    auto
    getCallDetails(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::getCallDetails(accountId, callId))
    {
        return libjami::getCallDetails(accountId, callId);
    }

    auto
    getCallList(const std::string& accountId)
        -> decltype(libjami::getCallList(accountId))
    {
        return libjami::getCallList(accountId);
    }

    std::vector<std::map<std::string, std::string>>
    getConferenceInfos(const std::string& accountId, const std::string& confId)
    {
        return libjami::getConferenceInfos(accountId, confId);
    }

    auto
    joinParticipant(const std::string& accountId,
                    const std::string& sel_callId,
                    const std::string& account2Id,
                    const std::string& drag_callId)
        -> decltype(libjami::joinParticipant(accountId, sel_callId, account2Id, drag_callId))
    {
        return libjami::joinParticipant(accountId, sel_callId, account2Id, drag_callId);
    }

    void
    createConfFromParticipantList(const std::string& accountId,
                                  const std::vector<std::string>& participants)
    {
        libjami::createConfFromParticipantList(accountId, participants);
    }

    void
    setConferenceLayout(const std::string& accountId,
                        const std::string& confId,
                        const uint32_t& layout)
    {
        libjami::setConferenceLayout(accountId, confId, layout);
    }

    void
    setActiveParticipant(const std::string& accountId,
                         const std::string& confId,
                         const std::string& callId)
    {
        libjami::setActiveParticipant(accountId, confId, callId);
    }

    void
    muteStream(const std::string& accountId,
               const std::string& confId,
               const std::string& accountUri,
               const std::string& deviceId,
               const std::string& streamId,
               const bool& state)
    {
        libjami::muteStream(accountId, confId, accountUri, deviceId, streamId, state);
    }

    void
    setActiveStream(const std::string& accountId,
                    const std::string& confId,
                    const std::string& accountUri,
                    const std::string& deviceId,
                    const std::string& streamId,
                    const bool& state)
    {
        libjami::setActiveStream(accountId, confId, accountUri, deviceId, streamId, state);
    }

    void
    raiseHand(const std::string& accountId,
              const std::string& confId,
              const std::string& accountUri,
              const std::string& deviceId,
              const bool& state)
    {
        libjami::raiseHand(accountId, confId, accountUri, deviceId, state);
    }

    auto
    isConferenceParticipant(const std::string& accountId, const std::string& call_id)
        -> decltype(libjami::isConferenceParticipant(accountId, call_id))
    {
        return libjami::isConferenceParticipant(accountId, call_id);
    }

    auto
    addParticipant(const std::string& accountId,
                   const std::string& callId,
                   const std::string& account2Id,
                   const std::string& confId)
        -> decltype(libjami::addParticipant(accountId, callId, account2Id, confId))
    {
        return libjami::addParticipant(accountId, callId, account2Id, confId);
    }

    auto
    addMainParticipant(const std::string& accountId, const std::string& confId)
        -> decltype(libjami::addMainParticipant(accountId, confId))
    {
        return libjami::addMainParticipant(accountId, confId);
    }

    auto
    detachParticipant(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::detachParticipant(accountId, callId))
    {
        return libjami::detachParticipant(accountId, callId);
    }

    auto
    joinConference(const std::string& accountId,
                   const std::string& sel_confId,
                   const std::string& account2Id,
                   const std::string& drag_confId)
        -> decltype(libjami::joinConference(accountId, sel_confId, account2Id, drag_confId))
    {
        return libjami::joinConference(accountId, sel_confId, account2Id, drag_confId);
    }

    auto
    hangUpConference(const std::string& accountId, const std::string& confId)
        -> decltype(libjami::hangUpConference(accountId, confId))
    {
        return libjami::hangUpConference(accountId, confId);
    }

    auto
    holdConference(const std::string& accountId, const std::string& confId)
        -> decltype(libjami::holdConference(accountId, confId))
    {
        return libjami::holdConference(accountId, confId);
    }

    auto
    unholdConference(const std::string& accountId, const std::string& confId)
        -> decltype(libjami::unholdConference(accountId, confId))
    {
        return libjami::unholdConference(accountId, confId);
    }

    auto
    getConferenceList(const std::string& accountId)
        -> decltype(libjami::getConferenceList(accountId))
    {
        return libjami::getConferenceList(accountId);
    }

    auto
    getParticipantList(const std::string& accountId, const std::string& confId)
        -> decltype(libjami::getParticipantList(accountId, confId))
    {
        return libjami::getParticipantList(accountId, confId);
    }

    auto
    getConferenceId(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::getConferenceId(accountId, callId))
    {
        return libjami::getConferenceId(accountId, callId);
    }

    auto
    getConferenceDetails(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::getConferenceDetails(accountId, callId))
    {
        return libjami::getConferenceDetails(accountId, callId);
    }

    auto
    currentMediaList(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::currentMediaList(accountId, callId))
    {
        return libjami::currentMediaList(accountId, callId);
    }

    auto
    startRecordedFilePlayback(const std::string& filepath)
        -> decltype(libjami::startRecordedFilePlayback(filepath))
    {
        return libjami::startRecordedFilePlayback(filepath);
    }

    void
    stopRecordedFilePlayback()
    {
        libjami::stopRecordedFilePlayback();
    }

    auto
    toggleRecording(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::toggleRecording(accountId, callId))
    {
        return libjami::toggleRecording(accountId, callId);
    }

    void
    setRecording(const std::string& accountId, const std::string& callId)
    {
        libjami::setRecording(accountId, callId);
    }

    void
    recordPlaybackSeek(const double& value)
    {
        libjami::recordPlaybackSeek(value);
    }

    auto
    getIsRecording(const std::string& accountId, const std::string& callId)
        -> decltype(libjami::getIsRecording(accountId, callId))
    {
        return libjami::getIsRecording(accountId, callId);
    }

    bool
    switchInput(const std::string& accountId,
                const std::string& callId,
                const std::string& input)
    {
        return libjami::switchInput(accountId, callId, input);
    }

    bool
    switchSecondaryInput(const std::string& accountId,
                         const std::string& conferenceId,
                         const std::string& input)
    {
        return libjami::switchSecondaryInput(accountId, conferenceId, input);
    }

    void
    playDTMF(const std::string& key)
    {
        libjami::playDTMF(key);
    }

    void
    startTone(const int32_t& start, const int32_t& type)
    {
        libjami::startTone(start, type);
    }

    void
    sendTextMessage(const std::string& accountId,
                    const std::string& callId,
                    const std::map<std::string, std::string>& messages,
                    const bool& isMixed)
    {
        libjami::sendTextMessage(accountId, callId, messages, "Me", isMixed);
    }

    void
    startSmartInfo(const uint32_t& refreshTimeMs)
    {
        libjami::startSmartInfo(refreshTimeMs);
    }

    void
    stopSmartInfo()
    {
        libjami::stopSmartInfo();
    }

    void
    setModerator(const std::string& accountId,
                 const std::string& confId,
                 const std::string& peerId,
                 const bool& state)
    {
        libjami::setModerator(accountId, confId, peerId, state);
    }

    void
    muteParticipant(const std::string& accountId,
                    const std::string& confId,
                    const std::string& peerId,
                    const bool& state)
    {
        libjami::muteParticipant(accountId, confId, peerId, state);
    }

    void
    hangupParticipant(const std::string& accountId,
                      const std::string& confId,
                      const std::string& peerId,
                      const std::string& deviceId)
    {
        libjami::hangupParticipant(accountId, confId, peerId, deviceId);
    }

    void
    raiseParticipantHand(const std::string& accountId,
                         const std::string& confId,
                         const std::string& peerId,
                         const bool& state)
    {
        libjami::raiseParticipantHand(accountId, confId, peerId, state);
    }

private:

    void
    registerSignalHandlers()
    {
        using namespace std::placeholders;

        using libjami::exportable_serialized_callback;
        using libjami::CallSignal;
        using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

        const std::map<std::string, SharedCallback> callEvHandlers
            = {exportable_serialized_callback<CallSignal::StateChange>(
                    std::bind(&DBusCallManager::emitCallStateChanged, this, _1, _2, _3, _4)),
                exportable_serialized_callback<CallSignal::TransferFailed>(
                    std::bind(&DBusCallManager::emitTransferFailed, this)),
                exportable_serialized_callback<CallSignal::TransferSucceeded>(
                    std::bind(&DBusCallManager::emitTransferSucceeded, this)),
                exportable_serialized_callback<CallSignal::RecordPlaybackStopped>(
                    std::bind(&DBusCallManager::emitRecordPlaybackStopped, this, _1)),
                exportable_serialized_callback<CallSignal::VoiceMailNotify>(
                    std::bind(&DBusCallManager::emitVoiceMailNotify, this, _1, _2, _3, _4)),
                exportable_serialized_callback<CallSignal::IncomingMessage>(
                    std::bind(&DBusCallManager::emitIncomingMessage, this, _1, _2, _3, _4)),
                exportable_serialized_callback<CallSignal::IncomingCall>(
                    std::bind(&DBusCallManager::emitIncomingCall, this, _1, _2, _3)),
                exportable_serialized_callback<CallSignal::IncomingCallWithMedia>(
                    std::bind(&DBusCallManager::emitIncomingCallWithMedia, this, _1, _2, _3, _4)),
                exportable_serialized_callback<CallSignal::MediaChangeRequested>(
                    std::bind(&DBusCallManager::emitMediaChangeRequested, this, _1, _2, _3)),
                exportable_serialized_callback<CallSignal::RecordPlaybackFilepath>(
                    std::bind(&DBusCallManager::emitRecordPlaybackFilepath, this, _1, _2)),
                exportable_serialized_callback<CallSignal::ConferenceCreated>(
                    std::bind(&DBusCallManager::emitConferenceCreated, this, _1, _2)),
                exportable_serialized_callback<CallSignal::ConferenceChanged>(
                    std::bind(&DBusCallManager::emitConferenceChanged, this, _1, _2, _3)),
                exportable_serialized_callback<CallSignal::UpdatePlaybackScale>(
                    std::bind(&DBusCallManager::emitUpdatePlaybackScale, this, _1, _2, _3)),
                exportable_serialized_callback<CallSignal::ConferenceRemoved>(
                    std::bind(&DBusCallManager::emitConferenceRemoved, this, _1, _2)),
                exportable_serialized_callback<CallSignal::RecordingStateChanged>(
                    std::bind(&DBusCallManager::emitRecordingStateChanged, this, _1, _2)),
                exportable_serialized_callback<CallSignal::RtcpReportReceived>(
                    std::bind(&DBusCallManager::emitOnRtcpReportReceived, this, _1, _2)),
                exportable_serialized_callback<CallSignal::OnConferenceInfosUpdated>(
                    std::bind(&DBusCallManager::emitOnConferenceInfosUpdated, this, _1, _2)),
                exportable_serialized_callback<CallSignal::PeerHold>(
                    std::bind(&DBusCallManager::emitPeerHold, this, _1, _2)),
                exportable_serialized_callback<CallSignal::AudioMuted>(
                    std::bind(&DBusCallManager::emitAudioMuted, this, _1, _2)),
                exportable_serialized_callback<CallSignal::VideoMuted>(
                    std::bind(&DBusCallManager::emitVideoMuted, this, _1, _2)),
                exportable_serialized_callback<CallSignal::SmartInfo>(
                    std::bind(&DBusCallManager::emitSmartInfo, this, _1)),
                exportable_serialized_callback<CallSignal::RemoteRecordingChanged>(
                    std::bind(&DBusCallManager::emitRemoteRecordingChanged, this, _1, _2, _3)),
                exportable_serialized_callback<CallSignal::MediaNegotiationStatus>(
                    std::bind(&DBusCallManager::emitMediaNegotiationStatus, this, _1, _2, _3))
            };

        libjami::registerSignalHandlers(callEvHandlers);
    }
};
