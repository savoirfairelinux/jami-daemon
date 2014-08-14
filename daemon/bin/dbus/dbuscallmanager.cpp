/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "sflphone.h"

#include "dbuscallmanager.h"

DBusCallManager::DBusCallManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/org/sflphone/SFLphone/CallManager")
{
}

bool DBusCallManager::placeCall(const std::string& accountID, const std::string& callID, const std::string& to)
{
    return sflph_call_place(accountID, callID, to);
}

bool DBusCallManager::refuse(const std::string& callID)
{
    return sflph_call_refuse(callID);
}

bool DBusCallManager::accept(const std::string& callID)
{
    return sflph_call_accept(callID);
}

bool DBusCallManager::hangUp(const std::string& callID)
{
    return sflph_call_hang_up(callID);
}

bool DBusCallManager::hold(const std::string& callID)
{
    return sflph_call_hold(callID);
}

bool DBusCallManager::unhold(const std::string& callID)
{
    return sflph_call_unhold(callID);
}

bool DBusCallManager::transfer(const std::string& callID, const std::string& to)
{
    return sflph_call_transfer(callID, to);
}

bool DBusCallManager::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    return sflph_call_attended_transfer(transferID, targetID);
}

std::map< std::string, std::string > DBusCallManager::getCallDetails(const std::string& callID)
{
    return sflph_call_get_call_details(callID);
}

std::vector< std::string > DBusCallManager::getCallList()
{
    return sflph_call_get_call_list();
}

void DBusCallManager::removeConference(const std::string& conference_id)
{
    sflph_call_remove_conference(conference_id);
}

bool DBusCallManager::joinParticipant(const std::string& sel_callID, const std::string& drag_callID)
{
    return sflph_call_join_participant(sel_callID, drag_callID);
}

void DBusCallManager::createConfFromParticipantList(const std::vector< std::string >& participants)
{
    sflph_call_create_conf_from_participant_list(participants);
}

bool DBusCallManager::isConferenceParticipant(const std::string& call_id)
{
    return sflph_call_is_conference_participant(call_id);
}

bool DBusCallManager::addParticipant(const std::string& callID, const std::string& confID)
{
    return sflph_call_add_participant(callID, confID);
}

bool DBusCallManager::addMainParticipant(const std::string& confID)
{
    return sflph_call_add_main_participant(confID);
}

bool DBusCallManager::detachParticipant(const std::string& callID)
{
    return sflph_call_detach_participant(callID);
}

bool DBusCallManager::joinConference(const std::string& sel_confID, const std::string& drag_confID)
{
    return sflph_call_join_conference(sel_confID, drag_confID);
}

bool DBusCallManager::hangUpConference(const std::string& confID)
{
    return sflph_call_hang_up_conference(confID);
}

bool DBusCallManager::holdConference(const std::string& confID)
{
    return sflph_call_hold_conference(confID);
}

bool DBusCallManager::unholdConference(const std::string& confID)
{
    return sflph_call_unhold_conference(confID);
}

std::vector<std::string> DBusCallManager::getConferenceList()
{
    return sflph_call_get_conference_list();
}

std::vector<std::string> DBusCallManager::getParticipantList(const std::string& confID)
{
    return sflph_call_get_participant_list(confID);
}

std::vector<std::string> DBusCallManager::getDisplayNames(const std::string& confID)
{
    return sflph_call_get_display_names(confID);
}

std::string DBusCallManager::getConferenceId(const std::string& callID)
{
    return sflph_call_get_conference_id(callID);
}

std::map<std::string, std::string> DBusCallManager::getConferenceDetails(const std::string& callID)
{
    return sflph_call_get_conference_details(callID);
}

bool DBusCallManager::startRecordedFilePlayback(const std::string& filepath)
{
    return sflph_call_play_recorded_file(filepath);
}

void DBusCallManager::stopRecordedFilePlayback(const std::string& filepath)
{
    sflph_call_stop_recorded_file(filepath);
}

bool DBusCallManager::toggleRecording(const std::string& callID)
{
    return sflph_call_toggle_recording(callID);
}

void DBusCallManager::setRecording(const std::string& callID)
{
    sflph_call_set_recording(callID);
}

void DBusCallManager::recordPlaybackSeek(const double& value)
{
    sflph_call_record_playback_seek(value);
}

bool DBusCallManager::getIsRecording(const std::string& callID)
{
    return sflph_call_is_recording(callID);
}

std::string DBusCallManager::getCurrentAudioCodecName(const std::string& callID)
{
    return sflph_call_get_current_audio_codec_name(callID);
}

void DBusCallManager::playDTMF(const std::string& key)
{
    sflph_call_play_dtmf(key);
}

void DBusCallManager::startTone(const int32_t& start, const int32_t& type)
{
    sflph_call_start_tone(start, type);
}

void DBusCallManager::setSASVerified(const std::string& callID)
{
    sflph_call_set_sas_verified(callID);
}

void DBusCallManager::resetSASVerified(const std::string& callID)
{
    sflph_call_reset_sas_verified(callID);
}

void DBusCallManager::setConfirmGoClear(const std::string& callID)
{
    sflph_call_set_confirm_go_clear(callID);
}

void DBusCallManager::requestGoClear(const std::string& callID)
{
    sflph_call_request_go_clear(callID);
}

void DBusCallManager::acceptEnrollment(const std::string& callID, const bool& accepted)
{
    sflph_call_accept_enrollment(callID, accepted);
}

void DBusCallManager::sendTextMessage(const std::string& callID, const std::string& message)
{
    sflph_call_send_text_message(callID, message);
}
