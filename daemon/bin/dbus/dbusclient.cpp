/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
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
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <sflphone.h>

#include "dbusclient.h"
#include "dbus_cpp.h"

#include "dbusinstance.h"
#include "dbuscallmanager.h"
#include "dbusconfigurationmanager.h"

#ifdef SFL_PRESENCE
#include "dbuspresencemanager.h"
#endif

#ifdef SFL_VIDEO
#include "dbusvideomanager.h"
#endif

struct EventCallback : DBus::Callback_Base<void, DBus::DefaultTimeout&>
{
    EventCallback(const std::function<void()> &func) :
        callback_(func)
    {}

    void call(DBus::DefaultTimeout &) const
    {
        callback_();
    }

private:
    std::function<void()> callback_;
};

DBusClient* DBusClient::_lastDbusClient = nullptr;

DBusClient::DBusClient(int sflphFlags, bool persistent) :
    callManager_(nullptr)
    , configurationManager_(nullptr)
#ifdef SFL_PRESENCE
    , presenceManager_(nullptr)
#endif
    , instanceManager_(nullptr)
    , dispatcher_(new DBus::BusDispatcher)
#ifdef SFL_VIDEO
    , videoManager_(nullptr)
#endif
{
    try {
        DBus::_init_threading();
        DBus::default_dispatcher = dispatcher_;

        // timeout and expired are deleted internally by dispatcher_'s
        // destructor, so we must NOT delete them ourselves.
        DBus::DefaultTimeout *timeout = new DBus::DefaultTimeout(10 /* ms */,
                                                                 true,
                                                                 dispatcher_);
        // Poll for SIP/IAX events
        timeout->expired = new EventCallback(sflph_poll_events);

        DBus::Connection sessionConnection(DBus::Connection::SessionBus());
        sessionConnection.request_name("org.sflphone.SFLphone");

        callManager_ = new DBusCallManager(sessionConnection);
        configurationManager_ = new DBusConfigurationManager(sessionConnection);

#ifdef SFL_PRESENCE
        presenceManager_ = new DBusPresenceManager(sessionConnection);
#endif

        DBusInstance::OnNoMoreClientFunc onNoMoreClientFunc;

        if (!persistent) {
            onNoMoreClientFunc = [this] () {
                this->exit();
            };
        }

        instanceManager_ = new DBusInstance(sessionConnection, onNoMoreClientFunc);

#ifdef SFL_VIDEO
        videoManager_ = new DBusVideoManager(sessionConnection);
#endif
    } catch (const DBus::Error &err) {
        throw std::runtime_error("cannot initialize DBus stuff");
    }

    auto ret = initLibrary(sflphFlags);

    if (ret < 0) {
        throw std::runtime_error("cannot initialize libsflphone");
    }

    instanceManager_->started();
}

DBusClient::~DBusClient()
{
#ifdef SFL_VIDEO
    delete videoManager_;
#endif

    delete instanceManager_;

#ifdef SFL_PRESENCE
    delete presenceManager_;
#endif

    delete configurationManager_;
    delete callManager_;
    delete dispatcher_;

    finiLibrary();
}

int DBusClient::initLibrary(int sflphFlags)
{
    if (!_lastDbusClient) {
        _lastDbusClient = this;
    }

    // Call event handlers
    sflph_call_ev_handlers callEvHandlers = {
        callOnStateChange,
        callOnTransferFail,
        callOnTransferSuccess,
        callOnRecordPlaybackStopped,
        callOnVoiceMailNotify,
        callOnIncomingMessage,
        callOnIncomingCall,
        callOnRecordPlaybackFilepath,
        callOnConferenceCreated,
        callOnConferenceChanged,
        callOnUpdatePlaybackScale,
        callOnConferenceRemove,
        callOnNewCall,
        callOnSipCallStateChange,
        callOnRecordStateChange,
        callOnSecureSdesOn,
        callOnSecureSdesOff,
        callOnSecureZrtpOn,
        callOnSecureZrtpOff,
        callOnShowSas,
        callOnZrtpNotSuppOther,
        callOnZrtpNegotiationFail,
        callOnRtcpReceiveReport,
    };

    // Configuration event handlers
    sflph_config_ev_handlers configEvHandlers = {
        configOnVolumeChange,
        configOnAccountsChange,
        configOnHistoryChange,
        configOnStunStatusFail,
        configOnRegistrationStateChange,
        configOnSipRegistrationStateChange,
        configOnError,
    };

#ifdef SFL_PRESENCE
    // Presence event handlers
    sflph_pres_ev_handlers presEvHandlers = {
        presOnNewServerSubscriptionRequest,
        presOnServerError,
        presOnNewBuddyNotification,
        presOnSubscriptionStateChange,
    };
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    // Video event handlers
    sflph_video_ev_handlers videoEvHandlers = {
        videoOnDeviceEvent,
        videoOnStartDecoding,
        videoOnStopDecoding,
    };
#endif // SFL_VIDEO

    // All event handlers
    sflph_ev_handlers evHandlers;
    std::memset(std::addressof(evHandlers), 0, sizeof(evHandlers));

    evHandlers.call_ev_handlers = callEvHandlers;
    evHandlers.config_ev_handlers = configEvHandlers;

#ifdef SFL_PRESENCE
    evHandlers.pres_ev_handlers = presEvHandlers;
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
    evHandlers.video_ev_handlers = videoEvHandlers;
#endif // SFL_VIDEO

    // Initialize now
    return sflph_init(&evHandlers, static_cast<sflph_init_flag>(sflphFlags));
}

void DBusClient::finiLibrary()
{
    sflph_fini();
}

int DBusClient::event_loop()
{
    try {
        dispatcher_->enter();
    } catch (const DBus::Error &err) {
        std::cerr << "quitting: " << err.name() << ": " << err.what() << std::endl;
        return 1;
    } catch (const std::exception &err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}

int DBusClient::exit()
{
    // Avoid libsflphone events from now on
    _lastDbusClient = nullptr;

    try {
        dispatcher_->leave();
    } catch (const DBus::Error &err) {
        std::cerr << "quitting: " << err.name() << ": " << err.what() << std::endl;
        return 1;
    } catch (const std::exception &err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}

DBusCallManager* DBusClient::getCallManager()
{
    return callManager_;
}

DBusConfigurationManager* DBusClient::getConfigurationManager()
{
    return configurationManager_;
}

#ifdef SFL_PRESENCE
DBusPresenceManager* DBusClient::getPresenceManager()
{
    return presenceManager_;
}
#endif

#ifdef SFL_VIDEO
DBusVideoManager* DBusClient::getVideoManager()
{
    return videoManager_;
}
#endif

void DBusClient::callOnStateChange(const std::string& call_id, const std::string& state)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->callStateChanged(call_id, state);
    }
}

void DBusClient::callOnTransferFail(void)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->transferFailed();
    }
}

void DBusClient::callOnTransferSuccess(void)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->transferSucceeded();
    }
}

void DBusClient::callOnRecordPlaybackStopped(const std::string& path)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->recordPlaybackStopped(path);
    }
}

void DBusClient::callOnVoiceMailNotify(const std::string& call_id, int nd_msg)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->voiceMailNotify(call_id, nd_msg);
    }
}

void DBusClient::callOnIncomingMessage(const std::string& id, const std::string& from, const std::string& msg)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->incomingMessage(id, from, msg);
    }
}

void DBusClient::callOnIncomingCall(const std::string& account_id, const std::string& call_id, const std::string& from)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->incomingCall(account_id, call_id, from);
    }
}

void DBusClient::callOnRecordPlaybackFilepath(const std::string& id, const std::string& filename)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->recordPlaybackFilepath(id, filename);
    }
}

void DBusClient::callOnConferenceCreated(const std::string& conf_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->conferenceCreated(conf_id);
    }
}

void DBusClient::callOnConferenceChanged(const std::string& conf_id, const std::string& state)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->conferenceChanged(conf_id, state);
    }
}

void DBusClient::callOnUpdatePlaybackScale(const std::string& filepath, int position, int scale)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->updatePlaybackScale(filepath, position, scale);
    }
}

void DBusClient::callOnConferenceRemove(const std::string& conf_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->conferenceRemoved(conf_id);
    }
}

void DBusClient::callOnNewCall(const std::string& account_id, const std::string& call_id, const std::string& to)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->newCallCreated(account_id, call_id, to);
    }
}

void DBusClient::callOnSipCallStateChange(const std::string& call_id, const std::string& state, int code)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->sipCallStateChanged(call_id, state, code);
    }
}

void DBusClient::callOnRecordStateChange(const std::string& call_id, int state)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->recordingStateChanged(call_id, state);
    }
}

void DBusClient::callOnSecureSdesOn(const std::string& call_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->secureSdesOn(call_id);
    }
}

void DBusClient::callOnSecureSdesOff(const std::string& call_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->secureSdesOff(call_id);
    }
}

void DBusClient::callOnSecureZrtpOn(const std::string& call_id, const std::string& cipher)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->secureZrtpOn(call_id, cipher);
    }
}

void DBusClient::callOnSecureZrtpOff(const std::string& call_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->secureZrtpOff(call_id);
    }
}

void DBusClient::callOnShowSas(const std::string& call_id, const std::string& sas, int verified)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->showSAS(call_id, sas, verified);
    }
}

void DBusClient::callOnZrtpNotSuppOther(const std::string& call_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->zrtpNotSuppOther(call_id);
    }
}

void DBusClient::callOnZrtpNegotiationFail(const std::string& call_id, const std::string& reason, const std::string& severity)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->zrtpNegotiationFailed(call_id, reason, severity);
    }
}

void DBusClient::callOnRtcpReceiveReport(const std::string& call_id, const std::map<std::string, int>& stats)
{
    if (_lastDbusClient) {
        _lastDbusClient->getCallManager()->onRtcpReportReceived(call_id, stats);
    }
}

void DBusClient::configOnVolumeChange(const std::string& device, int value)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->volumeChanged(device, value);
    }
}

void DBusClient::configOnAccountsChange(void)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->accountsChanged();
    }
}

void DBusClient::configOnHistoryChange(void)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->historyChanged();
    }
}

void DBusClient::configOnStunStatusFail(const std::string& account_id)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->stunStatusFailure(account_id);
    }
}

void DBusClient::configOnRegistrationStateChange(const std::string& account_id, int state)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->registrationStateChanged(account_id, state);
    }
}

void DBusClient::configOnSipRegistrationStateChange(const std::string& account_id, const std::string& state, int code)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->sipRegistrationStateChanged(account_id, state, code);
    }
}

void DBusClient::configOnError(int alert)
{
    if (_lastDbusClient) {
        _lastDbusClient->getConfigurationManager()->errorAlert(alert);
    }
}

#ifdef SFL_PRESENCE
void DBusClient::presOnNewServerSubscriptionRequest(const std::string& remote)
{
    if (_lastDbusClient) {
        _lastDbusClient->getPresenceManager()->newServerSubscriptionRequest(remote);
    }
}

void DBusClient::presOnServerError(const std::string& account_id, const std::string& error, const std::string& msg)
{
    if (_lastDbusClient) {
        _lastDbusClient->getPresenceManager()->serverError(account_id, error, msg);
    }
}

void DBusClient::presOnNewBuddyNotification(const std::string& account_id, const std::string& buddy_uri, int status, const std::string& line_status)
{
    if (_lastDbusClient) {
        _lastDbusClient->getPresenceManager()->newBuddyNotification(account_id, buddy_uri, status, line_status);
    }
}

void DBusClient::presOnSubscriptionStateChange(const std::string& account_id, const std::string& buddy_uri, int state)
{
    if (_lastDbusClient) {
        _lastDbusClient->getPresenceManager()->subscriptionStateChanged(account_id, buddy_uri, state);
    }
}
#endif // SFL_PRESENCE

#ifdef SFL_VIDEO
void DBusClient::videoOnDeviceEvent(void)
{
    if (_lastDbusClient) {
        _lastDbusClient->getVideoManager()->deviceEvent();
    }
}

void DBusClient::videoOnStartDecoding(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer)
{
    if (_lastDbusClient) {
        _lastDbusClient->getVideoManager()->startedDecoding(id, shm_path, w, h, is_mixer);
    }
}

void DBusClient::videoOnStopDecoding(const std::string& id, const std::string& shm_path, bool is_mixer)
{
    if (_lastDbusClient) {
        _lastDbusClient->getVideoManager()->stoppedDecoding(id, shm_path, is_mixer);
    }
}
#endif // SFL_VIDEO
