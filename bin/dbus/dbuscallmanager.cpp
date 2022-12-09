/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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

#include "dbuscallmanager.h"

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/CallManager")
{}

auto
DBusCallManager::placeCall(const std::string& accountId, const std::string& to)
    -> decltype(libjami::placeCall(accountId, to))
{
    return libjami::placeCall(accountId, to);
}
auto
DBusCallManager::placeCallWithMedia(const std::string& accountId,
                                    const std::string& to,
                                    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(libjami::placeCallWithMedia(accountId, to, mediaList))
{
    return libjami::placeCallWithMedia(accountId, to, mediaList);
}

auto
DBusCallManager::requestMediaChange(const std::string& accountId,
                                    const std::string& callId,
                                    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(libjami::requestMediaChange(accountId, callId, mediaList))
{
    return libjami::requestMediaChange(accountId, callId, mediaList);
}

auto
DBusCallManager::refuse(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::refuse(accountId, callId))
{
    return libjami::refuse(accountId, callId);
}

auto
DBusCallManager::accept(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::accept(accountId, callId))
{
    return libjami::accept(accountId, callId);
}

auto
DBusCallManager::acceptWithMedia(const std::string& accountId,
                                 const std::string& callId,
                                 const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(libjami::acceptWithMedia(accountId, callId, mediaList))
{
    return libjami::acceptWithMedia(accountId, callId, mediaList);
}

auto
DBusCallManager::answerMediaChangeRequest(
    const std::string& accountId,
    const std::string& callId,
    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(libjami::answerMediaChangeRequest(accountId, callId, mediaList))
{
    return libjami::answerMediaChangeRequest(accountId, callId, mediaList);
}

auto
DBusCallManager::hangUp(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::hangUp(accountId, callId))
{
    return libjami::hangUp(accountId, callId);
}

auto
DBusCallManager::hold(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::hold(accountId, callId))
{
    return libjami::hold(accountId, callId);
}

auto
DBusCallManager::unhold(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::unhold(accountId, callId))
{
    return libjami::unhold(accountId, callId);
}

auto
DBusCallManager::muteLocalMedia(const std::string& accountId,
                                const std::string& callId,
                                const std::string& mediaType,
                                const bool& mute)
    -> decltype(libjami::muteLocalMedia(accountId, callId, mediaType, mute))
{
    return libjami::muteLocalMedia(accountId, callId, mediaType, mute);
}

auto
DBusCallManager::transfer(const std::string& accountId,
                          const std::string& callId,
                          const std::string& to) -> decltype(libjami::transfer(accountId, callId, to))
{
    return libjami::transfer(accountId, callId, to);
}

auto
DBusCallManager::attendedTransfer(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& targetId)
    -> decltype(libjami::attendedTransfer(accountId, callId, targetId))
{
    return libjami::attendedTransfer(accountId, callId, targetId);
}

auto
DBusCallManager::getCallDetails(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::getCallDetails(accountId, callId))
{
    return libjami::getCallDetails(accountId, callId);
}

auto
DBusCallManager::getCallList(const std::string& accountId)
    -> decltype(libjami::getCallList(accountId))
{
    return libjami::getCallList(accountId);
}

std::vector<std::map<std::string, std::string>>
DBusCallManager::getConferenceInfos(const std::string& accountId, const std::string& confId)
{
    return libjami::getConferenceInfos(accountId, confId);
}

auto
DBusCallManager::joinParticipant(const std::string& accountId,
                                 const std::string& sel_callId,
                                 const std::string& account2Id,
                                 const std::string& drag_callId)
    -> decltype(libjami::joinParticipant(accountId, sel_callId, account2Id, drag_callId))
{
    return libjami::joinParticipant(accountId, sel_callId, account2Id, drag_callId);
}

void
DBusCallManager::createConfFromParticipantList(const std::string& accountId,
                                               const std::vector<std::string>& participants)
{
    libjami::createConfFromParticipantList(accountId, participants);
}

void
DBusCallManager::setConferenceLayout(const std::string& accountId,
                                     const std::string& confId,
                                     const uint32_t& layout)
{
    libjami::setConferenceLayout(accountId, confId, layout);
}

void
DBusCallManager::setActiveParticipant(const std::string& accountId,
                                      const std::string& confId,
                                      const std::string& callId)
{
    libjami::setActiveParticipant(accountId, confId, callId);
}

void
DBusCallManager::muteStream(const std::string& accountId,
                            const std::string& confId,
                            const std::string& accountUri,
                            const std::string& deviceId,
                            const std::string& streamId,
                            const bool& state)
{
    libjami::muteStream(accountId, confId, accountUri, deviceId, streamId, state);
}

void
DBusCallManager::setActiveStream(const std::string& accountId,
                                 const std::string& confId,
                                 const std::string& accountUri,
                                 const std::string& deviceId,
                                 const std::string& streamId,
                                 const bool& state)
{
    libjami::setActiveStream(accountId, confId, accountUri, deviceId, streamId, state);
}

void
DBusCallManager::raiseHand(const std::string& accountId,
                           const std::string& confId,
                           const std::string& accountUri,
                           const std::string& deviceId,
                           const bool& state)
{
    libjami::raiseHand(accountId, confId, accountUri, deviceId, state);
}

auto
DBusCallManager::isConferenceParticipant(const std::string& accountId, const std::string& call_id)
    -> decltype(libjami::isConferenceParticipant(accountId, call_id))
{
    return libjami::isConferenceParticipant(accountId, call_id);
}

auto
DBusCallManager::addParticipant(const std::string& accountId,
                                const std::string& callId,
                                const std::string& account2Id,
                                const std::string& confId)
    -> decltype(libjami::addParticipant(accountId, callId, account2Id, confId))
{
    return libjami::addParticipant(accountId, callId, account2Id, confId);
}

auto
DBusCallManager::addMainParticipant(const std::string& accountId, const std::string& confId)
    -> decltype(libjami::addMainParticipant(accountId, confId))
{
    return libjami::addMainParticipant(accountId, confId);
}

auto
DBusCallManager::detachParticipant(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::detachParticipant(accountId, callId))
{
    return libjami::detachParticipant(accountId, callId);
}

auto
DBusCallManager::joinConference(const std::string& accountId,
                                const std::string& sel_confId,
                                const std::string& account2Id,
                                const std::string& drag_confId)
    -> decltype(libjami::joinConference(accountId, sel_confId, account2Id, drag_confId))
{
    return libjami::joinConference(accountId, sel_confId, account2Id, drag_confId);
}

auto
DBusCallManager::hangUpConference(const std::string& accountId, const std::string& confId)
    -> decltype(libjami::hangUpConference(accountId, confId))
{
    return libjami::hangUpConference(accountId, confId);
}

auto
DBusCallManager::holdConference(const std::string& accountId, const std::string& confId)
    -> decltype(libjami::holdConference(accountId, confId))
{
    return libjami::holdConference(accountId, confId);
}

auto
DBusCallManager::unholdConference(const std::string& accountId, const std::string& confId)
    -> decltype(libjami::unholdConference(accountId, confId))
{
    return libjami::unholdConference(accountId, confId);
}

auto
DBusCallManager::getConferenceList(const std::string& accountId)
    -> decltype(libjami::getConferenceList(accountId))
{
    return libjami::getConferenceList(accountId);
}

auto
DBusCallManager::getParticipantList(const std::string& accountId, const std::string& confId)
    -> decltype(libjami::getParticipantList(accountId, confId))
{
    return libjami::getParticipantList(accountId, confId);
}

auto
DBusCallManager::getConferenceId(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::getConferenceId(accountId, callId))
{
    return libjami::getConferenceId(accountId, callId);
}

auto
DBusCallManager::getConferenceDetails(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::getConferenceDetails(accountId, callId))
{
    return libjami::getConferenceDetails(accountId, callId);
}

auto
DBusCallManager::currentMediaList(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::currentMediaList(accountId, callId))
{
    return libjami::currentMediaList(accountId, callId);
}

auto
DBusCallManager::startRecordedFilePlayback(const std::string& filepath)
    -> decltype(libjami::startRecordedFilePlayback(filepath))
{
    return libjami::startRecordedFilePlayback(filepath);
}

void
DBusCallManager::stopRecordedFilePlayback()
{
    libjami::stopRecordedFilePlayback();
}

auto
DBusCallManager::toggleRecording(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::toggleRecording(accountId, callId))
{
    return libjami::toggleRecording(accountId, callId);
}

void
DBusCallManager::setRecording(const std::string& accountId, const std::string& callId)
{
    libjami::setRecording(accountId, callId);
}

void
DBusCallManager::recordPlaybackSeek(const double& value)
{
    libjami::recordPlaybackSeek(value);
}

auto
DBusCallManager::getIsRecording(const std::string& accountId, const std::string& callId)
    -> decltype(libjami::getIsRecording(accountId, callId))
{
    return libjami::getIsRecording(accountId, callId);
}

bool
DBusCallManager::switchInput(const std::string& accountId,
                             const std::string& callId,
                             const std::string& input)
{
    return libjami::switchInput(accountId, callId, input);
}

bool
DBusCallManager::switchSecondaryInput(const std::string& accountId,
                                      const std::string& conferenceId,
                                      const std::string& input)
{
    return libjami::switchSecondaryInput(accountId, conferenceId, input);
}

void
DBusCallManager::playDTMF(const std::string& key)
{
    libjami::playDTMF(key);
}

void
DBusCallManager::startTone(const int32_t& start, const int32_t& type)
{
    libjami::startTone(start, type);
}

void
DBusCallManager::sendTextMessage(const std::string& accountId,
                                 const std::string& callId,
                                 const std::map<std::string, std::string>& messages,
                                 const bool& isMixed)
{
    libjami::sendTextMessage(accountId, callId, messages, "Me", isMixed);
}

void
DBusCallManager::startSmartInfo(const uint32_t& refreshTimeMs)
{
    libjami::startSmartInfo(refreshTimeMs);
}

void
DBusCallManager::stopSmartInfo()
{
    libjami::stopSmartInfo();
}

void
DBusCallManager::setModerator(const std::string& accountId,
                              const std::string& confId,
                              const std::string& peerId,
                              const bool& state)
{
    libjami::setModerator(accountId, confId, peerId, state);
}

void
DBusCallManager::muteParticipant(const std::string& accountId,
                                 const std::string& confId,
                                 const std::string& peerId,
                                 const bool& state)
{
    libjami::muteParticipant(accountId, confId, peerId, state);
}

void
DBusCallManager::hangupParticipant(const std::string& accountId,
                                   const std::string& confId,
                                   const std::string& peerId,
                                   const std::string& deviceId)
{
    libjami::hangupParticipant(accountId, confId, peerId, deviceId);
}

void
DBusCallManager::raiseParticipantHand(const std::string& accountId,
                                      const std::string& confId,
                                      const std::string& peerId,
                                      const bool& state)
{
    libjami::raiseParticipantHand(accountId, confId, peerId, state);
}
