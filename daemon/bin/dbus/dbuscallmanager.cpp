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

static ring::CallManager* getCallManager()
{
    return ring::Manager::instance().getCallManager();
}

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/CallManager")
{
}

bool DBusCallManager::placeCall(const std::string& accountID, const std::string& callID, const std::string& to)
{
    return getCallManager()->placeCall(accountID, callID, to);
}

bool DBusCallManager::refuse(const std::string& callID)
{
    return getCallManager()->refuse(callID);
}

bool DBusCallManager::accept(const std::string& callID)
{
    return getCallManager()->accept(callID);
}

bool DBusCallManager::hangUp(const std::string& callID)
{
    return getCallManager()->hangUp(callID);
}

bool DBusCallManager::hold(const std::string& callID)
{
    return getCallManager()->hold(callID);
}

bool DBusCallManager::unhold(const std::string& callID)
{
    return getCallManager()->unhold(callID);
}

bool DBusCallManager::transfer(const std::string& callID, const std::string& to)
{
    return getCallManager()->transfer(callID, to);
}

bool DBusCallManager::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return getCallManager()->attendedTransfer(transferID, targetID);
}

std::map< std::string, std::string > DBusCallManager::getCallDetails(const std::string& callID)
{
    return getCallManager()->getCallDetails(callID);
}

std::vector< std::string > DBusCallManager::getCallList()
{
    return getCallManager()->getCallList();
}

void DBusCallManager::removeConference(const std::string& conference_id)
{
    getCallManager()->removeConference(conference_id);
}

bool DBusCallManager::joinParticipant(const std::string& sel_callID, const std::string& drag_callID)
{
    return getCallManager()->joinParticipant(sel_callID, drag_callID);
}

void DBusCallManager::createConfFromParticipantList(const std::vector< std::string >& participants)
{
    getCallManager()->createConfFromParticipantList(participants);
}

bool DBusCallManager::isConferenceParticipant(const std::string& call_id)
{
    return getCallManager()->isConferenceParticipant(call_id);
}

bool DBusCallManager::addParticipant(const std::string& callID, const std::string& confID)
{
    return getCallManager()->addParticipant(callID, confID);
}

bool DBusCallManager::addMainParticipant(const std::string& confID)
{
    return getCallManager()->addMainParticipant(confID);
}

bool DBusCallManager::detachParticipant(const std::string& callID)
{
    return getCallManager()->detachParticipant(callID);
}

bool DBusCallManager::joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return getCallManager()->joinConference(sel_confID, drag_confID);
}

bool DBusCallManager::hangUpConference(const std::string& confID)
{
    return getCallManager()->hangUpConference(confID);
}

bool DBusCallManager::holdConference(const std::string& confID)
{
    return getCallManager()->holdConference(confID);
}

bool DBusCallManager::unholdConference(const std::string& confID)
{
    return getCallManager()->unholdConference(confID);
}

std::vector<std::string> DBusCallManager::getConferenceList()
{
    return getCallManager()->getConferenceList();
}

std::vector<std::string> DBusCallManager::getParticipantList(const std::string& confID)
{
    return getCallManager()->getParticipantList(confID);
}

std::vector<std::string> DBusCallManager::getDisplayNames(const std::string& confID)
{
    return getCallManager()->getDisplayNames(confID);
}

std::string DBusCallManager::getConferenceId(const std::string& callID)
{
    return getCallManager()->getConferenceId(callID);
}

std::map<std::string, std::string> DBusCallManager::getConferenceDetails(const std::string& callID)
{
    return getCallManager()->getConferenceDetails(callID);
}

bool DBusCallManager::startRecordedFilePlayback(const std::string& filepath)
{
    return getCallManager()->startRecordedFilePlayback(filepath);
}

void DBusCallManager::stopRecordedFilePlayback(const std::string& filepath)
{
    getCallManager()->stopRecordedFilePlayback(filepath);
}

bool DBusCallManager::toggleRecording(const std::string& callID)
{
    return getCallManager()->toggleRecording(callID);
}

void DBusCallManager::setRecording(const std::string& callID)
{
    getCallManager()->setRecording(callID);
}

void DBusCallManager::recordPlaybackSeek(const double& value)
{
    getCallManager()->recordPlaybackSeek(value);
}

bool DBusCallManager::getIsRecording(const std::string& callID)
{
    return getCallManager()->getIsRecording(callID);
}

std::string DBusCallManager::getCurrentAudioCodecName(const std::string& callID)
{
    return getCallManager()->getCurrentAudioCodecName(callID);
}

void DBusCallManager::playDTMF(const std::string& key)
{
    getCallManager()->playDTMF(key);
}

void DBusCallManager::startTone(const int32_t& start, const int32_t& type)
{
    getCallManager()->startTone(start, type);
}

void DBusCallManager::setSASVerified(const std::string& callID)
{
    getCallManager()->setSASVerified(callID);
}

void DBusCallManager::resetSASVerified(const std::string& callID)
{
    getCallManager()->resetSASVerified(callID);
}

void DBusCallManager::setConfirmGoClear(const std::string& callID)
{
    getCallManager()->setConfirmGoClear(callID);
}

void DBusCallManager::requestGoClear(const std::string& callID)
{
    getCallManager()->requestGoClear(callID);
}

void DBusCallManager::acceptEnrollment(const std::string& callID, const bool& accepted)
{
    getCallManager()->acceptEnrollment(callID, accepted);
}

void DBusCallManager::sendTextMessage(const std::string& callID, const std::string& message)
{
    getCallManager()->sendTextMessage(callID, message);
}
