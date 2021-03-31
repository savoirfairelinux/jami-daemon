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
#include "dring/callmanager_interface.h"

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/CallManager")
{}

auto
DBusCallManager::placeCall(const std::string& accountID, const std::string& to)
    -> decltype(DRing::placeCall(accountID, to))
{
    return DRing::placeCall(accountID, to);
}

auto
DBusCallManager::placeCallWithDetails(const std::string& accountID,
                                      const std::string& to,
                                      const std::map<std::string, std::string>& VolatileCallDetails)
    -> decltype(DRing::placeCall(accountID, to, VolatileCallDetails))
{
    return DRing::placeCall(accountID, to, VolatileCallDetails);
}

auto
DBusCallManager::placeCallWithMedia(const std::string& accountID,
                                    const std::string& to,
                                    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::placeCallWithMedia(accountID, to, mediaList))
{
    return DRing::placeCallWithMedia(accountID, to, mediaList);
}

auto
DBusCallManager::requestMediaChange(const std::string& callID,
                                    const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::requestMediaChange(callID, mediaList))
{
    return DRing::requestMediaChange(callID, mediaList);
}

auto
DBusCallManager::refuse(const std::string& callID) -> decltype(DRing::refuse(callID))
{
    return DRing::refuse(callID);
}

auto
DBusCallManager::accept(const std::string& callID) -> decltype(DRing::accept(callID))
{
    return DRing::accept(callID);
}

auto
DBusCallManager::acceptWithMedia(const std::string& callID,
                                 const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::acceptWithMedia(callID, mediaList))
{
    return DRing::acceptWithMedia(callID, mediaList);
}

auto
DBusCallManager::answerMediaChangeRequest(
    const std::string& callID, const std::vector<std::map<std::string, std::string>>& mediaList)
    -> decltype(DRing::answerMediaChangeRequest(callID, mediaList))
{
    return DRing::answerMediaChangeRequest(callID, mediaList);
}

auto
DBusCallManager::hangUp(const std::string& callID) -> decltype(DRing::hangUp(callID))
{
    return DRing::hangUp(callID);
}

auto
DBusCallManager::hold(const std::string& callID) -> decltype(DRing::hold(callID))
{
    return DRing::hold(callID);
}

auto
DBusCallManager::unhold(const std::string& callID) -> decltype(DRing::unhold(callID))
{
    return DRing::unhold(callID);
}

auto
DBusCallManager::muteLocalMedia(const std::string& callid,
                                const std::string& mediaType,
                                const bool& mute)
    -> decltype(DRing::muteLocalMedia(callid, mediaType, mute))
{
    return DRing::muteLocalMedia(callid, mediaType, mute);
}

auto
DBusCallManager::transfer(const std::string& callID, const std::string& to)
    -> decltype(DRing::transfer(callID, to))
{
    return DRing::transfer(callID, to);
}

auto
DBusCallManager::attendedTransfer(const std::string& transferID, const std::string& targetID)
    -> decltype(DRing::attendedTransfer(transferID, targetID))
{
    return DRing::attendedTransfer(transferID, targetID);
}

auto
DBusCallManager::getCallDetails(const std::string& callID)
    -> decltype(DRing::getCallDetails(callID))
{
    return DRing::getCallDetails(callID);
}

auto
DBusCallManager::getCallList() -> decltype(DRing::getCallList())
{
    return DRing::getCallList();
}

std::vector<std::map<std::string, std::string>>
DBusCallManager::getConferenceInfos(const std::string& confId)
{
    return DRing::getConferenceInfos(confId);
}

void
DBusCallManager::removeConference(const std::string& conference_id)
{
    DRing::removeConference(conference_id);
}

auto
DBusCallManager::joinParticipant(const std::string& sel_callID, const std::string& drag_callID)
    -> decltype(DRing::joinParticipant(sel_callID, drag_callID))
{
    return DRing::joinParticipant(sel_callID, drag_callID);
}

void
DBusCallManager::createConfFromParticipantList(const std::vector<std::string>& participants)
{
    DRing::createConfFromParticipantList(participants);
}

void
DBusCallManager::setConferenceLayout(const std::string& confId, const uint32_t& layout)
{
    DRing::setConferenceLayout(confId, layout);
}

void
DBusCallManager::setActiveParticipant(const std::string& confId, const std::string& callId)
{
    DRing::setActiveParticipant(confId, callId);
}

auto
DBusCallManager::isConferenceParticipant(const std::string& call_id)
    -> decltype(DRing::isConferenceParticipant(call_id))
{
    return DRing::isConferenceParticipant(call_id);
}

auto
DBusCallManager::addParticipant(const std::string& callID, const std::string& confID)
    -> decltype(DRing::addParticipant(callID, confID))
{
    return DRing::addParticipant(callID, confID);
}

auto
DBusCallManager::addMainParticipant(const std::string& confID)
    -> decltype(DRing::addMainParticipant(confID))
{
    return DRing::addMainParticipant(confID);
}

auto
DBusCallManager::detachLocalParticipant() -> decltype(DRing::detachLocalParticipant())
{
    return DRing::detachLocalParticipant();
}

auto
DBusCallManager::detachParticipant(const std::string& callID)
    -> decltype(DRing::detachParticipant(callID))
{
    return DRing::detachParticipant(callID);
}

auto
DBusCallManager::joinConference(const std::string& sel_confID, const std::string& drag_confID)
    -> decltype(DRing::joinConference(sel_confID, drag_confID))
{
    return DRing::joinConference(sel_confID, drag_confID);
}

auto
DBusCallManager::hangUpConference(const std::string& confID)
    -> decltype(DRing::hangUpConference(confID))
{
    return DRing::hangUpConference(confID);
}

auto
DBusCallManager::holdConference(const std::string& confID)
    -> decltype(DRing::holdConference(confID))
{
    return DRing::holdConference(confID);
}

auto
DBusCallManager::unholdConference(const std::string& confID)
    -> decltype(DRing::unholdConference(confID))
{
    return DRing::unholdConference(confID);
}

auto
DBusCallManager::getConferenceList() -> decltype(DRing::getConferenceList())
{
    return DRing::getConferenceList();
}

auto
DBusCallManager::getParticipantList(const std::string& confID)
    -> decltype(DRing::getParticipantList(confID))
{
    return DRing::getParticipantList(confID);
}

auto
DBusCallManager::getDisplayNames(const std::string& confID)
    -> decltype(DRing::getDisplayNames(confID))
{
    return DRing::getDisplayNames(confID);
}

auto
DBusCallManager::getConferenceId(const std::string& callID)
    -> decltype(DRing::getConferenceId(callID))
{
    return DRing::getConferenceId(callID);
}

auto
DBusCallManager::getConferenceDetails(const std::string& callID)
    -> decltype(DRing::getConferenceDetails(callID))
{
    return DRing::getConferenceDetails(callID);
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
DBusCallManager::toggleRecording(const std::string& callID)
    -> decltype(DRing::toggleRecording(callID))
{
    return DRing::toggleRecording(callID);
}

void
DBusCallManager::setRecording(const std::string& callID)
{
    DRing::setRecording(callID);
}

void
DBusCallManager::recordPlaybackSeek(const double& value)
{
    DRing::recordPlaybackSeek(value);
}

auto
DBusCallManager::getIsRecording(const std::string& callID)
    -> decltype(DRing::getIsRecording(callID))
{
    return DRing::getIsRecording(callID);
}

bool
DBusCallManager::switchInput(const std::string& callID, const std::string& input)
{
    return DRing::switchInput(callID, input);
}

bool
DBusCallManager::switchSecondaryInput(const std::string& conferenceId, const std::string& input)
{
    return DRing::switchSecondaryInput(conferenceId, input);
}

auto
DBusCallManager::getCurrentAudioCodecName(const std::string& callID)
    -> decltype(DRing::getCurrentAudioCodecName(callID))
{
    return DRing::getCurrentAudioCodecName(callID);
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
DBusCallManager::sendTextMessage(const std::string& callID,
                                 const std::map<std::string, std::string>& messages,
                                 const bool& isMixed)
{
    DRing::sendTextMessage(callID, messages, "Me", isMixed);
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
DBusCallManager::setModerator(const std::string& confId,
                              const std::string& peerId,
                              const bool& state)
{
    DRing::setModerator(confId, peerId, state);
}

void
DBusCallManager::muteParticipant(const std::string& confId,
                                 const std::string& peerId,
                                 const bool& state)
{
    DRing::muteParticipant(confId, peerId, state);
}

void
DBusCallManager::hangupParticipant(const std::string& confId, const std::string& peerId)
{
    DRing::hangupParticipant(confId, peerId);
}