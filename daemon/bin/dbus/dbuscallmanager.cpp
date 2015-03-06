/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "dbuscallmanager.h"
#include "dring/callmanager_interface.h"

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/CallManager")
{}

auto
DBusCallManager::placeCall(const std::string& accountID, const std::string& to) -> decltype(DRing::placeCall(accountID, to))
{
    return DRing::placeCall(accountID, to);
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
DBusCallManager::transfer(const std::string& callID, const std::string& to) -> decltype(DRing::transfer(callID, to))
{
    return DRing::transfer(callID, to);
}

auto
DBusCallManager::attendedTransfer(const std::string& transferID, const std::string& targetID) -> decltype(DRing::attendedTransfer(transferID, targetID))
{
    return DRing::attendedTransfer(transferID, targetID);
}

auto
DBusCallManager::getCallDetails(const std::string& callID) -> decltype(DRing::getCallDetails(callID))
{
    return DRing::getCallDetails(callID);
}

auto
DBusCallManager::getCallList() -> decltype(DRing::getCallList())
{
    return DRing::getCallList();
}

void
DBusCallManager::removeConference(const std::string& conference_id)
{
    DRing::removeConference(conference_id);
}

auto
DBusCallManager::joinParticipant(const std::string& sel_callID, const std::string& drag_callID) -> decltype(DRing::joinParticipant(sel_callID, drag_callID))
{
    return DRing::joinParticipant(sel_callID, drag_callID);
}

void
DBusCallManager::createConfFromParticipantList(const std::vector< std::string >& participants)
{
    DRing::createConfFromParticipantList(participants);
}

auto
DBusCallManager::isConferenceParticipant(const std::string& call_id) -> decltype(DRing::isConferenceParticipant(call_id))
{
    return DRing::isConferenceParticipant(call_id);
}

auto
DBusCallManager::addParticipant(const std::string& callID, const std::string& confID) -> decltype(DRing::addParticipant(callID, confID))
{
    return DRing::addParticipant(callID, confID);
}

auto
DBusCallManager::addMainParticipant(const std::string& confID) -> decltype(DRing::addMainParticipant(confID))
{
    return DRing::addMainParticipant(confID);
}

auto
DBusCallManager::detachParticipant(const std::string& callID) -> decltype(DRing::detachParticipant(callID))
{
    return DRing::detachParticipant(callID);
}

auto
DBusCallManager::joinConference(const std::string& sel_confID, const std::string& drag_confID) -> decltype(DRing::joinConference(sel_confID, drag_confID))
{
    return DRing::joinConference(sel_confID, drag_confID);
}

auto
DBusCallManager::hangUpConference(const std::string& confID) -> decltype(DRing::hangUpConference(confID))
{
    return DRing::hangUpConference(confID);
}

auto
DBusCallManager::holdConference(const std::string& confID) -> decltype(DRing::holdConference(confID))
{
    return DRing::holdConference(confID);
}

auto
DBusCallManager::unholdConference(const std::string& confID) -> decltype(DRing::unholdConference(confID))
{
    return DRing::unholdConference(confID);
}

auto
DBusCallManager::getConferenceList() -> decltype(DRing::getConferenceList())
{
    return DRing::getConferenceList();
}

auto
DBusCallManager::getParticipantList(const std::string& confID) -> decltype(DRing::getParticipantList(confID))
{
    return DRing::getParticipantList(confID);
}

auto
DBusCallManager::getDisplayNames(const std::string& confID) -> decltype(DRing::getDisplayNames(confID))
{
    return DRing::getDisplayNames(confID);
}

auto
DBusCallManager::getConferenceId(const std::string& callID) -> decltype(DRing::getConferenceId(callID))
{
    return DRing::getConferenceId(callID);
}

auto
DBusCallManager::getConferenceDetails(const std::string& callID) -> decltype(DRing::getConferenceDetails(callID))
{
    return DRing::getConferenceDetails(callID);
}

auto
DBusCallManager::startRecordedFilePlayback(const std::string& filepath) -> decltype(DRing::startRecordedFilePlayback(filepath))
{
    return DRing::startRecordedFilePlayback(filepath);
}

void
DBusCallManager::stopRecordedFilePlayback(const std::string& filepath)
{
    DRing::stopRecordedFilePlayback(filepath);
}

auto
DBusCallManager::toggleRecording(const std::string& callID) -> decltype(DRing::toggleRecording(callID))
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
DBusCallManager::getIsRecording(const std::string& callID) -> decltype(DRing::getIsRecording(callID))
{
    return DRing::getIsRecording(callID);
}

void
DBusCallManager::switchInput(const std::string& callID, const std::string& input)
{
    DRing::switchInput(callID, input);
}

auto
DBusCallManager::getCurrentAudioCodecName(const std::string& callID) -> decltype(DRing::getCurrentAudioCodecName(callID))
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
DBusCallManager::setSASVerified(const std::string& callID)
{
    DRing::setSASVerified(callID);
}

void
DBusCallManager::resetSASVerified(const std::string& callID)
{
    DRing::resetSASVerified(callID);
}

void
DBusCallManager::setConfirmGoClear(const std::string& callID)
{
    DRing::setConfirmGoClear(callID);
}

void
DBusCallManager::requestGoClear(const std::string& callID)
{
    DRing::requestGoClear(callID);
}

void
DBusCallManager::acceptEnrollment(const std::string& callID, const bool& accepted)
{
    DRing::acceptEnrollment(callID, accepted);
}

void
DBusCallManager::sendTextMessage(const std::string& callID, const std::string& message)
{
    DRing::sendTextMessage(callID, message);
}
