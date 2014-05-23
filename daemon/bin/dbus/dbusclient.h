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

#ifndef __DBUSCLIENT_H__
#define __DBUSCLIENT_H__

#include <sflphone.h>

class DBusConfigurationManager;
class DBusCallManager;
class DBusNetworkManager;
class DBusInstance;

#ifdef SFL_PRESENCE
class DBusPresenceManager;
#endif

#ifdef SFL_VIDEO
class DBusVideoManager;
#endif

namespace DBus {
    class BusDispatcher;
}

class DBusClient {
    public:
        DBusClient(int sflphFlags, bool persistent);
        ~DBusClient();

        DBusCallManager* getCallManager();
        DBusConfigurationManager* getConfigurationManager();

#ifdef SFL_PRESENCE
        DBusPresenceManager* getPresenceManager();
#endif

#ifdef SFL_VIDEO
        DBusVideoManager* getVideoManager();
#endif

        int event_loop();
        int exit();

    private:
        int initLibrary(int sflphFlags);
        void finiLibrary();

    private:
        static DBusClient* _lastDbusClient;

        static void callOnStateChange(const std::string& call_id, const std::string& state);
        static void callOnTransferFail(void);
        static void callOnTransferSuccess(void);
        static void callOnRecordPlaybackStopped(const std::string& path);
        static void callOnVoiceMailNotify(const std::string& call_id, int nd_msg);
        static void callOnIncomingMessage(const std::string& id, const std::string& from, const std::string& msg);
        static void callOnIncomingCall(const std::string& account_id, const std::string& call_id, const std::string& from);
        static void callOnRecordPlaybackFilepath(const std::string& id, const std::string& filename);
        static void callOnConferenceCreated(const std::string& conf_id);
        static void callOnConferenceChanged(const std::string& conf_id, const std::string& state);
        static void callOnUpdatePlaybackScale(const std::string& filepath, int position, int scale);
        static void callOnConferenceRemove(const std::string& conf_id);
        static void callOnNewCall(const std::string& account_id, const std::string& call_id, const std::string& to);
        static void callOnSipCallStateChange(const std::string& call_id, const std::string& state, int code);
        static void callOnRecordStateChange(const std::string& call_id, int state);
        static void callOnSecureSdesOn(const std::string& call_id);
        static void callOnSecureSdesOff(const std::string& call_id);
        static void callOnSecureZrtpOn(const std::string& call_id, const std::string& cipher);
        static void callOnSecureZrtpOff(const std::string& call_id);
        static void callOnShowSas(const std::string& call_id, const std::string& sas, int verified);
        static void callOnZrtpNotSuppOther(const std::string& call_id);
        static void callOnZrtpNegotiationFail(const std::string& call_id, const std::string& reason, const std::string& severity);
        static void callOnRtcpReceiveReport(const std::string& call_id, const std::map<std::string, int>& stats);
        static void configOnVolumeChange(const std::string& device, int value);
        static void configOnAccountsChange(void);
        static void configOnHistoryChange(void);
        static void configOnStunStatusFail(const std::string& account_id);
        static void configOnRegistrationStateChange(const std::string& account_id, int state);
        static void configOnSipRegistrationStateChange(const std::string& account_id, const std::string& state, int code);
        static void configOnError(int alert);

#ifdef SFL_PRESENCE
        static void presOnNewServerSubscriptionRequest(const std::string& remote);
        static void presOnServerError(const std::string& account_id, const std::string& error, const std::string& msg);
        static void presOnNewBuddyNotification(const std::string& account_id, const std::string& buddy_uri, int status, const std::string& line_status);
        static void presOnSubscriptionStateChange(const std::string& account_id, const std::string& buddy_uri, int state);
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
        static void videoOnDeviceEvent(void);
        static void videoOnStartDecoding(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer);
        static void videoOnStopDecoding(const std::string& id, const std::string& shm_path, bool is_mixer);
#endif // SFL_VIDEO

    private:
        DBusCallManager*          callManager_;
        DBusConfigurationManager* configurationManager_;

#ifdef SFL_PRESENCE
        DBusPresenceManager*      presenceManager_;
#endif

        DBusInstance*             instanceManager_;
        DBus::BusDispatcher*  dispatcher_;

#ifdef SFL_VIDEO
        DBusVideoManager *videoManager_;
#endif
};

#endif
