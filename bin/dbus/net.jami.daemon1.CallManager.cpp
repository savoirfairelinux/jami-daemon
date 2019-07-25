/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

auto
placeCall(const std::string& accountID, const std::string& to) -> decltype(DRing::placeCall(accountID, to))
{
    return DRing::placeCall(accountID, to);
}

auto
placeCallWithDetails(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& VolatileCallDetails)
    -> decltype (DRing::placeCall(accountID, to, VolatileCallDetails))
{
    return DRing::placeCall(accountID, to, VolatileCallDetails);
}

auto
refuse(const std::string& callID) -> decltype(DRing::refuse(callID))
{
    return DRing::refuse(callID);
}

auto
accept(const std::string& callID) -> decltype(DRing::accept(callID))
{
    return DRing::accept(callID);
}

auto
hangUp(const std::string& callID) -> decltype(DRing::hangUp(callID))
{
    return DRing::hangUp(callID);
}

auto
hold(const std::string& callID) -> decltype(DRing::hold(callID))
{
    return DRing::hold(callID);
}

auto
unhold(const std::string& callID) -> decltype(DRing::unhold(callID))
{
    return DRing::unhold(callID);
}

auto
muteLocalMedia(const std::string& callid, const std::string& mediaType, const bool& mute)
    -> decltype(DRing::muteLocalMedia(callid, mediaType, mute))
{
    return DRing::muteLocalMedia(callid, mediaType, mute);
}

auto
transfer(const std::string& callID, const std::string& to) -> decltype(DRing::transfer(callID, to))
{
    return DRing::transfer(callID, to);
}

auto
attendedTransfer(const std::string& transferID, const std::string& targetID) -> decltype(DRing::attendedTransfer(transferID, targetID))
{
    return DRing::attendedTransfer(transferID, targetID);
}

auto
getCallDetails(const std::string& callID) -> decltype(DRing::getCallDetails(callID))
{
    return DRing::getCallDetails(callID);
}

auto
getCallList() -> decltype(DRing::getCallList())
{
    return DRing::getCallList();
}

void
removeConference(const std::string& conference_id)
{
    DRing::removeConference(conference_id);
}

auto
joinParticipant(const std::string& sel_callID, const std::string& drag_callID) -> decltype(DRing::joinParticipant(sel_callID, drag_callID))
{
    return DRing::joinParticipant(sel_callID, drag_callID);
}

void
createConfFromParticipantList(const std::vector< std::string >& participants)
{
    DRing::createConfFromParticipantList(participants);
}

auto
isConferenceParticipant(const std::string& call_id) -> decltype(DRing::isConferenceParticipant(call_id))
{
    return DRing::isConferenceParticipant(call_id);
}

auto
addParticipant(const std::string& callID, const std::string& confID) -> decltype(DRing::addParticipant(callID, confID))
{
    return DRing::addParticipant(callID, confID);
}

auto
addMainParticipant(const std::string& confID) -> decltype(DRing::addMainParticipant(confID))
{
    return DRing::addMainParticipant(confID);
}

auto
detachLocalParticipant() -> decltype(DRing::detachLocalParticipant())
{
    return DRing::detachLocalParticipant();
}

auto
detachParticipant(const std::string& callID) -> decltype(DRing::detachParticipant(callID))
{
    return DRing::detachParticipant(callID);
}

auto
joinConference(const std::string& sel_confID, const std::string& drag_confID) -> decltype(DRing::joinConference(sel_confID, drag_confID))
{
    return DRing::joinConference(sel_confID, drag_confID);
}

auto
hangUpConference(const std::string& confID) -> decltype(DRing::hangUpConference(confID))
{
    return DRing::hangUpConference(confID);
}

auto
holdConference(const std::string& confID) -> decltype(DRing::holdConference(confID))
{
    return DRing::holdConference(confID);
}

auto
unholdConference(const std::string& confID) -> decltype(DRing::unholdConference(confID))
{
    return DRing::unholdConference(confID);
}

auto
getConferenceList() -> decltype(DRing::getConferenceList())
{
    return DRing::getConferenceList();
}

auto
getParticipantList(const std::string& confID) -> decltype(DRing::getParticipantList(confID))
{
    return DRing::getParticipantList(confID);
}

auto
getDisplayNames(const std::string& confID) -> decltype(DRing::getDisplayNames(confID))
{
    return DRing::getDisplayNames(confID);
}

auto
getConferenceId(const std::string& callID) -> decltype(DRing::getConferenceId(callID))
{
    return DRing::getConferenceId(callID);
}

auto
getConferenceDetails(const std::string& callID) -> decltype(DRing::getConferenceDetails(callID))
{
    return DRing::getConferenceDetails(callID);
}

auto
startRecordedFilePlayback(const std::string& filepath) -> decltype(DRing::startRecordedFilePlayback(filepath))
{
    return DRing::startRecordedFilePlayback(filepath);
}

void
stopRecordedFilePlayback()
{
    DRing::stopRecordedFilePlayback();
}

auto
toggleRecording(const std::string& callID) -> decltype(DRing::toggleRecording(callID))
{
    return DRing::toggleRecording(callID);
}

void
setRecording(const std::string& callID)
{
    DRing::setRecording(callID);
}

void
recordPlaybackSeek(const double& value)
{
    DRing::recordPlaybackSeek(value);
}

auto
getIsRecording(const std::string& callID) -> decltype(DRing::getIsRecording(callID))
{
    return DRing::getIsRecording(callID);
}

void
switchInput(const std::string& callID, const std::string& input)
{
    DRing::switchInput(callID, input);
}

auto
getCurrentAudioCodecName(const std::string& callID) -> decltype(DRing::getCurrentAudioCodecName(callID))
{
    return DRing::getCurrentAudioCodecName(callID);
}

void
playDTMF(const std::string& key)
{
    DRing::playDTMF(key);
}

void
startTone(const int32_t& start, const int32_t& type)
{
    DRing::startTone(start, type);
}

void
sendTextMessage(const std::string& callID, const std::map<std::string, std::string>& messages, const bool& isMixed)
{
    DRing::sendTextMessage(callID, messages, "Me", isMixed);
}

void
startSmartInfo(const uint32_t& refreshTimeMs)
{
    DRing::startSmartInfo(refreshTimeMs);
}

void
stopSmartInfo()
{
    DRing::stopSmartInfo();
}
