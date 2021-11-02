/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "jami/callmanager_interface.h"

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/CallManager")
{}

auto
DBusCallManager::placeCall(const std::string& accountId, const std::string& to)
    -> decltype(DRing::placeCall(accountId, to))
{
    return DRing::placeCall(accountId, to);
}
auto
DBusCallManager::placeCallWithMedia(const std::string& accountId,
                                    const std::string& to,
                                    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::placeCallWithMedia(accountId, to, mediaList))
{
    return DRing::placeCallWithMedia(accountId, to, mediaList);
}

auto
DBusCallManager::requestMediaChange(const std::string& accountId,
                                    const std::string& callId,
                                    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::requestMediaChange(accountId, callId, mediaList))
{
    return DRing::requestMediaChange(accountId, callId, mediaList);
}

auto
DBusCallManager::refuse(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::refuse(accountId, callId))
{
    return DRing::refuse(accountId, callId);
}

auto
DBusCallManager::accept(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::accept(accountId, callId))
{
    return DRing::accept(accountId, callId);
}

auto
DBusCallManager::acceptWithMedia(const std::string& accountId,
                                 const std::string& callId,
                                 const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::acceptWithMedia(accountId, callId, mediaList))
{
    return DRing::acceptWithMedia(accountId, callId, mediaList);
}

auto
DBusCallManager::answerMediaChangeRequest(
    const std::string& accountId,
    const std::string& callId,
    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::answerMediaChangeRequest(accountId, callId, mediaList))
{
    return DRing::answerMediaChangeRequest(accountId, callId, mediaList);
}

auto
DBusCallManager::hangUp(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::hangUp(accountId, callId))
{
    return DRing::hangUp(accountId, callId);
}

auto
DBusCallManager::hold(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::hold(accountId, callId))
{
    return DRing::hold(accountId, callId);
}

auto
DBusCallManager::unhold(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::unhold(accountId, callId))
{
    return DRing::unhold(accountId, callId);
}

auto
DBusCallManager::muteLocalMedia(const std::string& accountId,
                                const std::string& callId,
                                const std::string& mediaType,
                                const bool& mute)
    -> decltype(DRing::muteLocalMedia(accountId, callId, mediaType, mute))
{
    return DRing::muteLocalMedia(accountId, callId, mediaType, mute);
}

auto
DBusCallManager::transfer(const std::string& accountId,
                          const std::string& callId,
                          const std::string& to) -> decltype(DRing::transfer(accountId, callId, to))
{
    return DRing::transfer(accountId, callId, to);
}

auto
DBusCallManager::attendedTransfer(const std::string& accountId,
                                  const std::string& callId,
                                  const std::string& targetId)
    -> decltype(DRing::attendedTransfer(accountId, callId, targetId))
{
    return DRing::attendedTransfer(accountId, callId, targetId);
}

auto
DBusCallManager::getCallDetails(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::getCallDetails(accountId, callId))
{
    return DRing::getCallDetails(accountId, callId);
}

auto
DBusCallManager::getCallList(const std::string& accountId)
    -> decltype(DRing::getCallList(accountId))
{
    return DRing::getCallList(accountId);
}

std::vector<std::map<std::string, std::string>>
DBusCallManager::getConferenceInfos(const std::string& accountId, const std::string& confId)
{
    return DRing::getConferenceInfos(accountId, confId);
}

auto
DBusCallManager::joinParticipant(const std::string& accountId,
                                 const std::string& sel_callId,
                                 const std::string& account2Id,
                                 const std::string& drag_callId)
    -> decltype(DRing::joinParticipant(accountId, sel_callId, account2Id, drag_callId))
{
    return DRing::joinParticipant(accountId, sel_callId, account2Id, drag_callId);
}

void
DBusCallManager::createConfFromParticipantList(const std::string& accountId,
                                               const std::vector<std::string>& participants)
{
    DRing::createConfFromParticipantList(accountId, participants);
}

void
DBusCallManager::setConferenceLayout(const std::string& accountId,
                                     const std::string& confId,
                                     const uint32_t& layout)
{
    DRing::setConferenceLayout(accountId, confId, layout);
}

void
DBusCallManager::setActiveParticipant(const std::string& accountId,
                                      const std::string& confId,
                                      const std::string& callId)
{
    DRing::setActiveParticipant(accountId, confId, callId);
}

auto
DBusCallManager::isConferenceParticipant(const std::string& accountId, const std::string& call_id)
    -> decltype(DRing::isConferenceParticipant(accountId, call_id))
{
    return DRing::isConferenceParticipant(accountId, call_id);
}

auto
DBusCallManager::addParticipant(const std::string& accountId,
                                const std::string& callId,
                                const std::string& account2Id,
                                const std::string& confId)
    -> decltype(DRing::addParticipant(accountId, callId, account2Id, confId))
{
    return DRing::addParticipant(accountId, callId, account2Id, confId);
}

auto
DBusCallManager::addMainParticipant(const std::string& accountId, const std::string& confId)
    -> decltype(DRing::addMainParticipant(accountId, confId))
{
    return DRing::addMainParticipant(accountId, confId);
}

auto
DBusCallManager::detachLocalParticipant() -> decltype(DRing::detachLocalParticipant())
{
    return DRing::detachLocalParticipant();
}

auto
DBusCallManager::detachParticipant(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::detachParticipant(accountId, callId))
{
    return DRing::detachParticipant(accountId, callId);
}

auto
DBusCallManager::joinConference(const std::string& accountId,
                                const std::string& sel_confId,
                                const std::string& account2Id,
                                const std::string& drag_confId)
    -> decltype(DRing::joinConference(accountId, sel_confId, account2Id, drag_confId))
{
    return DRing::joinConference(accountId, sel_confId, account2Id, drag_confId);
}

auto
DBusCallManager::hangUpConference(const std::string& accountId, const std::string& confId)
    -> decltype(DRing::hangUpConference(accountId, confId))
{
    return DRing::hangUpConference(accountId, confId);
}

auto
DBusCallManager::holdConference(const std::string& accountId, const std::string& confId)
    -> decltype(DRing::holdConference(accountId, confId))
{
    return DRing::holdConference(accountId, confId);
}

auto
DBusCallManager::unholdConference(const std::string& accountId, const std::string& confId)
    -> decltype(DRing::unholdConference(accountId, confId))
{
    return DRing::unholdConference(accountId, confId);
}

auto
DBusCallManager::getConferenceList(const std::string& accountId)
    -> decltype(DRing::getConferenceList(accountId))
{
    return DRing::getConferenceList(accountId);
}

auto
DBusCallManager::getParticipantList(const std::string& accountId, const std::string& confId)
    -> decltype(DRing::getParticipantList(accountId, confId))
{
    return DRing::getParticipantList(accountId, confId);
}

auto
DBusCallManager::getConferenceId(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::getConferenceId(accountId, callId))
{
    return DRing::getConferenceId(accountId, callId);
}

auto
DBusCallManager::getConferenceDetails(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::getConferenceDetails(accountId, callId))
{
    return DRing::getConferenceDetails(accountId, callId);
}

auto
DBusCallManager::startRecordedFilePlayback(const std::string& filepath)
    -> decltype(DRing::startRecordedFilePlayback(filepath))
{
    return DRing::startRecordedFilePlayback(filepath);
}

void
DBusCallManager::stopRecordedFilePlayback()
{
    DRing::stopRecordedFilePlayback();
}

auto
DBusCallManager::toggleRecording(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::toggleRecording(accountId, callId))
{
    return DRing::toggleRecording(accountId, callId);
}

void
DBusCallManager::setRecording(const std::string& accountId, const std::string& callId)
{
    DRing::setRecording(accountId, callId);
}

void
DBusCallManager::recordPlaybackSeek(const double& value)
{
    DRing::recordPlaybackSeek(value);
}

auto
DBusCallManager::getIsRecording(const std::string& accountId, const std::string& callId)
    -> decltype(DRing::getIsRecording(accountId, callId))
{
    return DRing::getIsRecording(accountId, callId);
}

bool
DBusCallManager::switchInput(const std::string& accountId,
                             const std::string& callId,
                             const std::string& input)
{
    return DRing::switchInput(accountId, callId, input);
}

bool
DBusCallManager::switchSecondaryInput(const std::string& accountId,
                                      const std::string& conferenceId,
                                      const std::string& input)
{
    return DRing::switchSecondaryInput(accountId, conferenceId, input);
}

void
DBusCallManager::playDTMF(const std::string& key)
{
    DRing::playDTMF(key);
}

void
DBusCallManager::startTone(const int32_t& start, const int32_t& type)
{
    DRing::startTone(start, type);
}

void
DBusCallManager::sendTextMessage(const std::string& accountId,
                                 const std::string& callId,
                                 const std::map<std::string, std::string>& messages,
                                 const bool& isMixed)
{
    DRing::sendTextMessage(accountId, callId, messages, "Me", isMixed);
}

void
DBusCallManager::startSmartInfo(const uint32_t& refreshTimeMs)
{
    DRing::startSmartInfo(refreshTimeMs);
}

void
DBusCallManager::stopSmartInfo()
{
    DRing::stopSmartInfo();
}

void
DBusCallManager::setModerator(const std::string& accountId,
                              const std::string& confId,
                              const std::string& peerId,
                              const bool& state)
{
    DRing::setModerator(accountId, confId, peerId, state);
}

void
DBusCallManager::muteParticipant(const std::string& accountId,
                                 const std::string& confId,
                                 const std::string& peerId,
                                 const bool& state)
{
    DRing::muteParticipant(accountId, confId, peerId, state);
}

void
DBusCallManager::hangupParticipant(const std::string& accountId,
                                   const std::string& confId,
                                   const std::string& peerId)
{
    DRing::hangupParticipant(accountId, confId, peerId);
}

void
DBusCallManager::raiseParticipantHand(const std::string& accountId,
                                      const std::string& confId,
                                      const std::string& peerId,
                                      const bool& state)
{
    DRing::raiseParticipantHand(accountId, confId, peerId, state);
}