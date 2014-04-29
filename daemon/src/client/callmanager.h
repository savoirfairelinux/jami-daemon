/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __SFL_CALLMANAGER_H__
#define __SFL_CALLMANAGER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_DBUS

#include "dbus/dbus_cpp.h"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbus/callmanager-glue.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#else
// these includes normally come with DBus C++
#include <vector>
#include <map>
#include <string>
#endif  // HAVE_DBUS

#include <stdexcept>

class CallManagerException: public std::runtime_error {
    public:
        CallManagerException(const std::string& str = "") :
            std::runtime_error("A CallManagerException occured: " + str) {}
};

namespace sfl {
class AudioZrtpSession;
}

class CallManager
#if HAVE_DBUS
    : public org::sflphone::SFLphone::CallManager_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
#endif
{
    public:

#if HAVE_DBUS
        CallManager(DBus::Connection& connection);
#else
        CallManager();
#endif

        /* methods exported by this interface,
         * you will have to implement them in your ObjectAdaptor
         */

        /* Call related methods */
        bool placeCall(const std::string& accountID, const std::string& callID, const std::string& to);

        bool refuse(const std::string& callID);
        bool accept(const std::string& callID);
        bool hangUp(const std::string& callID);
        bool hold(const std::string& callID);
        bool unhold(const std::string& callID);
        bool transfer(const std::string& callID, const std::string& to);
        bool attendedTransfer(const std::string& transferID, const std::string& targetID);
        std::map< std::string, std::string > getCallDetails(const std::string& callID);
        std::vector< std::string > getCallList();

        /* Conference related methods */
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

        /* File Playback methods */
        bool startRecordedFilePlayback(const std::string& filepath);
        void stopRecordedFilePlayback(const std::string& filepath);

        /* General audio methods */
        bool toggleRecording(const std::string& callID);
        /* DEPRECATED */
        void setRecording(const std::string& callID);

        void recordPlaybackSeek(const double& value);
        bool getIsRecording(const std::string& callID);
        std::string getCurrentAudioCodecName(const std::string& callID);
        void playDTMF(const std::string& key);
        void startTone(const int32_t& start, const int32_t& type);

        /* Security related methods */
        void setSASVerified(const std::string& callID);
        void resetSASVerified(const std::string& callID);
        void setConfirmGoClear(const std::string& callID);
        void requestGoClear(const std::string& callID);
        void acceptEnrollment(const std::string& callID, const bool& accepted);

        /* Instant messaging */
        void sendTextMessage(const std::string& callID, const std::string& message);
        void sendTextMessage(const std::string& callID, const std::string& message, const std::string& from);

        /* the following signals must be implemented manually for any
         * platform or configuration that does not supply dbus */
#if !HAVE_DBUS
        void callStateChanged(const std::string& callID, const std::string& state);

        void transferFailed();

        void transferSucceeded();

        void recordPlaybackStopped(const std::string& path);

        void voiceMailNotify(const std::string& callID, const int32_t& nd_msg);

        void incomingMessage(const std::string& ID, const std::string& from, const std::string& msg);

        void incomingCall(const std::string& accountID, const std::string& callID, const std::string& from);

        void recordPlaybackFilepath(const std::string& id, const std::string& filename);

        void conferenceCreated(const std::string& confID);

        void conferenceChanged(const std::string& confID,const std::string& state);

        void updatePlaybackScale(const std::string&, const int32_t&, const int32_t&);
        void conferenceRemoved(const std::string&);
        void newCallCreated(const std::string&, const std::string&, const std::string&);
        void sipCallStateChanged(const std::string&, const std::string&, const int32_t&);
        void recordingStateChanged(const std::string& callID, const bool& state);
        void secureSdesOn(const std::string& arg);
        void secureSdesOff(const std::string& arg);

        void secureZrtpOn(const std::string& callID, const std::string& cipher);
        void secureZrtpOff(const std::string& callID);
        void showSAS(const std::string& callID, const std::string& sas, const bool& verified);
        void zrtpNotSuppOther(const std::string& callID);
        void zrtpNegotiationFailed(const std::string& callID, const std::string& arg2, const std::string& arg3);

        void onRtcpReportReceived(const std::string& callID, const std::map<std::string, int>& stats);
#endif // !HAVE_DBUS

private:

#if HAVE_ZRTP
        sfl::AudioZrtpSession * getAudioZrtpSession(const std::string& callID);
#endif
};

#endif//CALLMANAGER_H
