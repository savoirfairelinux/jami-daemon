/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#ifndef __RING_CALLMANAGERI_H__
#define __RING_CALLMANAGERI_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdexcept>
#include <map>
#include <vector>
#include <string>
#include <functional>

#include "dring.h"

namespace ring {

/* call events */
struct ring_call_ev_handlers
{
    std::function<void (const std::string& /*call_id*/, const std::string& /*state*/)> on_state_change;
    std::function<void ()> on_transfer_fail;
    std::function<void ()> on_transfer_success;
    std::function<void (const std::string& /*path*/)> on_record_playback_stopped;
    std::function<void (const std::string& /*call_id*/, int /*nd_msg*/)> on_voice_mail_notify;
    std::function<void (const std::string& /*id*/, const std::string& /*from*/, const std::string& /*msg*/)> on_incoming_message;
    std::function<void (const std::string& /*account_id*/, const std::string& /*call_id*/, const std::string& /*from*/)> on_incoming_call;
    std::function<void (const std::string& /*id*/, const std::string& /*filename*/)> on_record_playback_filepath;
    std::function<void (const std::string& /*conf_id*/)> on_conference_created;
    std::function<void (const std::string& /*conf_id*/, const std::string& /*state*/)> on_conference_changed;
    std::function<void (const std::string& /*filepath*/, int /*position*/, int /*scale*/)> on_update_playback_scale;
    std::function<void (const std::string& /*conf_id*/)> on_conference_remove;
    std::function<void (const std::string& /*account_id*/, const std::string& /*call_id*/, const std::string& /*to*/)> on_new_call;
    std::function<void (const std::string& /*call_id*/, const std::string& /*state*/, int /*code*/)> on_sip_call_state_change;
    std::function<void (const std::string& /*call_id*/, int /*state*/)> on_record_state_change;
    std::function<void (const std::string& /*call_id*/)> on_secure_sdes_on;
    std::function<void (const std::string& /*call_id*/)> on_secure_sdes_off;
    std::function<void (const std::string& /*call_id*/, const std::string& /*cipher*/)> on_secure_zrtp_on;
    std::function<void (const std::string& /*call_id*/)> on_secure_zrtp_off;
    std::function<void (const std::string& /*call_id*/, const std::string& /*sas*/, int /*verified*/)> on_show_sas;
    std::function<void (const std::string& /*call_id*/)> on_zrtp_not_supp_other;
    std::function<void (const std::string& /*call_id*/, const std::string& /*reason*/, const std::string& /*severity*/)> on_zrtp_negotiation_fail;
    std::function<void (const std::string& /*call_id*/, const std::map<std::string, int>& /*stats*/)> on_rtcp_receive_report;
};

class CallManagerException;

class CallManagerI
{
    public:
        void registerEvHandlers(ring_call_ev_handlers* evHandlers);

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

        void recordPlaybackSeek(double value);
        bool getIsRecording(const std::string& callID);
        std::string getCurrentAudioCodecName(const std::string& callID);
        void playDTMF(const std::string& key);
        void startTone(int32_t start, int32_t type);

        /* Security related methods */
        void setSASVerified(const std::string& callID);
        void resetSASVerified(const std::string& callID);
        void setConfirmGoClear(const std::string& callID);
        void requestGoClear(const std::string& callID);
        void acceptEnrollment(const std::string& callID, bool accepted);

        /* Instant messaging */
        void sendTextMessage(const std::string& callID, const std::string& message);
        void sendTextMessage(const std::string& callID, const std::string& message, const std::string& from);
};

} // namespace ring

#endif//CALLMANAGERI_H
