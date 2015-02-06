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
#include <iostream>

#include "dbuscallmanager.h"
#include "managerimpl.h"
#include "manager.h"
#include "client/callmanager.h"

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/CallManager")
{
    callManager_ = ring::Manager::instance().getCallManager();
}

bool DBusCallManager::placeCall(const std::string& accountID, const std::string& callID, const std::string& to)
{
    return callManager_->placeCall(accountID, callID, to);
}

bool DBusCallManager::refuse(const std::string& callID)
{
    return callManager_->refuse(callID);
}

bool DBusCallManager::accept(const std::string& callID)
{
    return callManager_->accept(callID);
}

bool DBusCallManager::hangUp(const std::string& callID)
{
    return callManager_->hangUp(callID);
}

bool DBusCallManager::hold(const std::string& callID)
{
    return callManager_->hold(callID);
}

bool DBusCallManager::unhold(const std::string& callID)
{
    return callManager_->unhold(callID);
}

bool DBusCallManager::transfer(const std::string& callID, const std::string& to)
{
    return callManager_->transfer(callID, to);
}

bool DBusCallManager::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return callManager_->attendedTransfer(transferID, targetID);
}

std::map< std::string, std::string > DBusCallManager::getCallDetails(const std::string& callID)
{
    return callManager_->getCallDetails(callID);
}

std::vector< std::string > DBusCallManager::getCallList()
{
    return callManager_->getCallList();
}

void DBusCallManager::removeConference(const std::string& conference_id)
{
    callManager_->removeConference(conference_id);
}

bool DBusCallManager::joinParticipant(const std::string& sel_callID, const std::string& drag_callID)
{
    return callManager_->joinParticipant(sel_callID, drag_callID);
}

void DBusCallManager::createConfFromParticipantList(const std::vector< std::string >& participants)
{
    callManager_->createConfFromParticipantList(participants);
}

bool DBusCallManager::isConferenceParticipant(const std::string& call_id)
{
    return callManager_->isConferenceParticipant(call_id);
}

bool DBusCallManager::addParticipant(const std::string& callID, const std::string& confID)
{
    return callManager_->addParticipant(callID, confID);
}

bool DBusCallManager::addMainParticipant(const std::string& confID)
{
    return callManager_->addMainParticipant(confID);
}

bool DBusCallManager::detachParticipant(const std::string& callID)
{
    return callManager_->detachParticipant(callID);
}

bool DBusCallManager::joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return callManager_->joinConference(sel_confID, drag_confID);
}

bool DBusCallManager::hangUpConference(const std::string& confID)
{
    return callManager_->hangUpConference(confID);
}

bool DBusCallManager::holdConference(const std::string& confID)
{
    return callManager_->holdConference(confID);
}

bool DBusCallManager::unholdConference(const std::string& confID)
{
    return callManager_->unholdConference(confID);
}

std::vector<std::string> DBusCallManager::getConferenceList()
{
    return callManager_->getConferenceList();
}

std::vector<std::string> DBusCallManager::getParticipantList(const std::string& confID)
{
    return callManager_->getParticipantList(confID);
}

std::vector<std::string> DBusCallManager::getDisplayNames(const std::string& confID)
{
    return callManager_->getDisplayNames(confID);
}

std::string DBusCallManager::getConferenceId(const std::string& callID)
{
    return callManager_->getConferenceId(callID);
}

std::map<std::string, std::string> DBusCallManager::getConferenceDetails(const std::string& callID)
{
    return callManager_->getConferenceDetails(callID);
}

bool DBusCallManager::startRecordedFilePlayback(const std::string& filepath)
{
    return callManager_->startRecordedFilePlayback(filepath);
}

void DBusCallManager::stopRecordedFilePlayback(const std::string& filepath)
{
    callManager_->stopRecordedFilePlayback(filepath);
}

bool DBusCallManager::toggleRecording(const std::string& callID)
{
    return callManager_->toggleRecording(callID);
}

void DBusCallManager::setRecording(const std::string& callID)
{
    callManager_->setRecording(callID);
}

void DBusCallManager::recordPlaybackSeek(const double& value)
{
    callManager_->recordPlaybackSeek(value);
}

bool DBusCallManager::getIsRecording(const std::string& callID)
{
    return callManager_->getIsRecording(callID);
}

std::string DBusCallManager::getCurrentAudioCodecName(const std::string& callID)
{
    return callManager_->getCurrentAudioCodecName(callID);
}

void DBusCallManager::playDTMF(const std::string& key)
{
    callManager_->playDTMF(key);
}

void DBusCallManager::startTone(const int32_t& start, const int32_t& type)
{
    callManager_->startTone(start, type);
}

void DBusCallManager::setSASVerified(const std::string& callID)
{
    callManager_->setSASVerified(callID);
}

void DBusCallManager::resetSASVerified(const std::string& callID)
{
    callManager_->resetSASVerified(callID);
}

void DBusCallManager::setConfirmGoClear(const std::string& callID)
{
    callManager_->setConfirmGoClear(callID);
}

void DBusCallManager::requestGoClear(const std::string& callID)
{
    callManager_->requestGoClear(callID);
}

void DBusCallManager::acceptEnrollment(const std::string& callID, const bool& accepted)
{
    callManager_->acceptEnrollment(callID, accepted);
}

void DBusCallManager::sendTextMessage(const std::string& callID, const std::string& message)
{
    callManager_->sendTextMessage(callID, message);
}
