/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
/* placeholder for the jni-glue */
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#include <vector>
#include <map>

 #include "client/android/jni_callbacks.h"

namespace sfl {
    class AudioZrtpSession;
}

class CallManager {
    public:

        CallManager();

        /* methods exported by this interface,
         * you will have to implement them
         */

        /* Call related methods */
        void placeCall(const std::string& accountID, const std::string& callID, const std::string& to);
        void placeCallFirstAccount(const std::string& callID, const std::string& to);

        void refuse(const std::string& callID);
        void accept(const std::string& callID);
        void hangUp(const std::string& callID);
        void hold(const std::string& callID);
        void unhold(const std::string& callID);
        bool transfer(const std::string& callID, const std::string& to);
        bool attendedTransfer(const std::string& transferID, const std::string& targetID);
        std::map< std::string, std::string > getCallDetails(const std::string& callID);
        std::vector< std::string > getCallList();


        /* Conference related methods */
        void removeConference(const std::string& conference_id);
        void joinParticipant(const std::string& sel_callID, const std::string& drag_callID);
        void createConfFromParticipantList(const std::vector< std::string >& participants);
        void createConference(const std::string& id1, const std::string& id2);
        void addParticipant(const std::string& callID, const std::string& confID);
        void addMainParticipant(const std::string& confID);
        void detachParticipant(const std::string& callID);
        void joinConference(const std::string& sel_confID, const std::string& drag_confID);
        void hangUpConference(const std::string& confID);
        void holdConference(const std::string& confID);
        void unholdConference(const std::string& confID);
        bool isConferenceParticipant(const std::string& call_id);
        std::vector<std::string> getConferenceList();
        std::vector<std::string> getParticipantList(const std::string& confID);
        std::string getConferenceId(const std::string& callID);
        std::map<std::string, std::string> getConferenceDetails(const std::string& callID);

        bool sendTextMessage(const std::string& callID, const std::string& message, const std::string& from);

        /* File Playback methods */
        bool toggleRecordingCall(const std::string& id);
        bool startRecordedFilePlayback(const std::string& filepath);
        void stopRecordedFilePlayback(const std::string& filepath);

        /* General audio methods */
        void setVolume(const std::string& device, const double& value);
        double getVolume(const std::string& device);
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

        void callStateChanged(const std::string& callID, const std::string& state);
        void transferFailed();
        void transferSucceeded();
        void recordPlaybackStopped(const std::string& path);
        void recordPlaybackFilepath(const std::string& id, const std::string& filename);
        void voiceMailNotify(const std::string& callID, const std::string& nd_msg);
        void incomingMessage(const std::string& ID, const std::string& from, const std::string& msg);
        void incomingCall(const std::string& accountID, const std::string& callID, const std::string& from);
        void conferenceCreated(const std::string& confID);
        void conferenceChanged(const std::string& confID,const std::string& state);
        void conferenceRemoved(const std::string& confID);
        void newCallCreated(const std::string& accountID, const std::string& callID, const std::string& to);
        void registrationStateChanged(const std::string& accoundID, const std::string& state, const int32_t& code);
        void sipCallStateChanged(const std::string& accoundID, const std::string& state, const int32_t& code);
        
        
    private:

#if HAVE_ZRTP
        sfl::AudioZrtpSession * getAudioZrtpSession(const std::string& callID);
#endif
};

#endif//CALLMANAGER_H__
