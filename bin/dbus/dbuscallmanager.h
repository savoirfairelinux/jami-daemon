/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#ifndef __RING_DBUSCALLMANAGER_H__
#define __RING_DBUSCALLMANAGER_H__

#include <vector>
#include <map>
#include <string>

#include "dbus_cpp.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbuscallmanager.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#include <stdexcept>

class DBusCallManager :
    public cx::ring::Ring::CallManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
{
    public:
        DBusCallManager(DBus::Connection& connection);

        // Methods
        std::string placeCall(const std::string& accountID, const std::string& to);
        bool refuse(const std::string& callID);
        bool accept(const std::string& callID);
        bool hangUp(const std::string& callID);
        bool hold(const std::string& callID);
        bool unhold(const std::string& callID);
        bool muteLocalMedia(const std::string& callid, const std::string& mediaType, const bool& mute);
        bool transfer(const std::string& callID, const std::string& to);
        bool attendedTransfer(const std::string& transferID, const std::string& targetID);
        std::map<std::string, std::string> getCallDetails(const std::string& callID);
        std::vector<std::string> getCallList();
        void removeConference(const std::string& conference_id);
        bool joinParticipant(const std::string& sel_callID, const std::string& drag_callID);
        void createConfFromParticipantList(const std::vector< std::string >& participants);
        bool isConferenceParticipant(const std::string& call_id);
        bool addParticipant(const std::string& callID, const std::string& confID);
        bool addMainParticipant(const std::string& confID);
        bool detachParticipant(const std::string& callID);
        bool joinConference(const std::string& sel_confID, const std::string& drag_confID);
        bool hangUpConference(const std::string& confID);
        bool holdConference(const std::string& confID);
        bool unholdConference(const std::string& confID);
        std::vector<std::string> getConferenceList();
        std::vector<std::string> getParticipantList(const std::string& confID);
        std::vector<std::string> getDisplayNames(const std::string& confID);
        std::string getConferenceId(const std::string& callID);
        std::map<std::string, std::string> getConferenceDetails(const std::string& callID);
        bool startRecordedFilePlayback(const std::string& filepath);
        void stopRecordedFilePlayback(const std::string& filepath);
        bool toggleRecording(const std::string& callID);
        void setRecording(const std::string& callID);
        void recordPlaybackSeek(const double& value);
        bool getIsRecording(const std::string& callID);
        void switchInput(const std::string& callID, const std::string& input);
        std::string getCurrentAudioCodecName(const std::string& callID);
        void playDTMF(const std::string& key);
        void startTone(const int32_t& start, const int32_t& type);
        void sendTextMessage(const std::string& callID, const std::map<std::string, std::string>& messages, const bool& isMixed);
        void startSmartInfo(const uint32_t& refreshTimeMs);
        void stopSmartInfo();
};

#endif // __RING_CALLMANAGER_H__
