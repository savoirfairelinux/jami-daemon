/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <atomic>
#include <functional>

#include "conference.h"

#include "account_factory.h"
#include "call_factory.h"

#include "audio/audiolayer.h"
#include "audio/tonecontrol.h"

#include "preferences.h"
#include "noncopyable.h"



namespace ring {
class ConfCallStage;
class IncomingCall;
class HoldCall;
class CurrentCall;
class InactiveCall;
class ActiveCall;
class ConnectingCall;
class RingingCall;
class HungupCall;
class BusyCall;
class OverCall;
class FailureCall;


namespace Conf {
class YamlParser;
class YamlEmitter;
}

namespace video {
class SinkClient;
}
class PluginManager;
class AudioFile;
class DTMF;
class RingBufferPool;
class VideoManager;

/** To send multiple string */
typedef std::list<std::string> TokenList;

/** To store conference objects by conference ids */
typedef std::map<std::string, std::shared_ptr<Conference> > ConferenceMap;

typedef std::set<std::string> CallIDSet;

static const char * const default_conf = "conf";

typedef std::set<std::string> CallIDSet;

/** Manager (controller) of Ring daemon */
class Manager {
    private:
        std::unique_ptr<PluginManager> pluginManager_;

    public:

        
        CallState_* getCallState(const std::string& callID);


        Manager();
        ~Manager();

        static Manager& instance();

        void setAutoAnswer(bool enable);

        /**
         * General preferences configuration
         */
        Preferences preferences;

        /**
         * Voip related preferences
         */
        VoipPreference voipPreferences;

        /**
         * Hook preferences
         */
        HookPreference hookPreference;

        /**
         * Audio preferences
         */
        AudioPreference audioPreference;

        /**
         * Shortcut preferences
         */
        ShortcutPreferences shortcutPreferences;

        /**
         * Video preferences
         */
        VideoPreferences videoPreferences;

        // Manager should not be accessed until initialized.
        // FIXME this is an evil hack!
        static std::atomic_bool initialized;

        /**
         * Initialisation of thread (sound) and map.
         * Init a new VoIPLink, audio codec and audio driver
         */
        void init(const std::string &config_file);

        /*
         * Terminate all threads and exit DBus loop
         */
        void finish() noexcept;

        /**
         * Accessor to audiodriver.
         * it's multi-thread and use mutex internally
         * @return AudioLayer*  The audio layer object
         */
        std::shared_ptr<AudioLayer> getAudioDriver();

        void startAudioDriverStream();

        /**
         * Functions which occur with a user's action
         * Place a new call
         * @param accountId	The account to make the call with
         * @param to  The recipient of the call
         * @param conf_id The conference identifier if any
         * @return id The call ID on success, empty string otherwise
         */
        std::string outgoingCall(const std::string& accountId,
                                 const std::string& to,
                                 const std::string& conf_id = "");

        /**
         * Functions which occur with a user's action
         * Answer the call
         * @param id  The call identifier
         */
        bool answerCall(const std::string& id);

        /**
         * Functions which occur with a user's action
         * Hangup the call
         * @param id  The call identifier
         */
        bool hangupCall(const std::string& id);


        /**
         * Functions which occur with a user's action
         * Hangup the conference (hangup every participants)
         * @param id  The call identifier
         */
        bool hangupConference(const std::string& id);

        /**
         * Functions which occur with a user's action
         * Put the call on hold
         * @param id  The call identifier
         */
        bool onHoldCall(const std::string& id);

        /**
         * Functions which occur with a user's action
         * Put the call off hold
         * @param id  The call identifier
         */
        bool offHoldCall(const std::string& id);

        /**
         * Functions which occur with a user's action
         * Put the media of a call on mute or unmute
         * @param callID  The call identifier
         * @param mediaType The media type; eg : AUDIO or VIDEO
         * @param is_muted true to mute, false to unmute
         */
        bool muteMediaCall(const std::string& callId, const std::string& mediaType, bool is_muted);

        /**
         * Functions which occur with a user's action
         * Transfer the call
         * @param id  The call identifier
         * @param to  The recipient of the transfer
         */
        bool transferCall(const std::string& id, const std::string& to);

        /**
         * Attended transfer
         * @param The call id to be transfered
         * @param The target
         */
        bool attendedTransfer(const std::string& transferID, const std::string& targetID);

        /**
         * Notify the client the transfer is successful
         */
        void transferSucceeded();

        /**
         * Notify the client that the transfer failed
         */
        void transferFailed();

        /**
         * Functions which occur with a user's action
         * Refuse the call
         * @param id  The call identifier
         */
        bool refuseCall(const std::string& id);

        /**
         * Create a new conference given two participant
         * @param the first participant ID
         * @param the second participant ID
         */
        std::shared_ptr<Conference>
        createConference(const std::string& id1, const std::string& id2);

        /**
         * Delete this conference
         * @param the conference ID
         */
        void removeConference(const std::string& conference_id);

        /**
         * Return the conference id for which this call is attached
         * @ param the call id
         */
        std::shared_ptr<Conference>
        getConferenceFromCallID(const std::string& call_id);

        /**
         * Hold every participant to a conference
         * @param the conference id
         */
        bool holdConference(const std::string& conference_id);

        /**
         * Unhold all conference participants
         * @param the conference id
         */
        bool unHoldConference(const std::string& conference_id);

        /**
         * Test if this id is a conference (usefull to test current call)
         * @param the call id
         */
        bool isConference(const std::string& call_id) const;

        /**
         * Test if a call id corresponds to a conference participant
         * @param the call id
         */
        bool isConferenceParticipant(const std::string& call_id);

        /**
         * Add a participant to a conference
         * @param the call id
         * @param the conference id
         */
        bool addParticipant(const std::string& call_id, const std::string& conference_id);

        /**
         * Bind the main participant to a conference (mainly called on a double click action)
         * @param the conference id
         */
        bool addMainParticipant(const std::string& conference_id);

        /**
         * Join two participants to create a conference
         * @param the fist call id
         * @param the second call id
         */
        bool joinParticipant(const std::string& call_id1,
                             const std::string& call_id2);

        /**
         * Create a conference from a list of participant
         * @param A vector containing the list of participant
         */
        void createConfFromParticipantList(const std::vector< std::string > &);

        /**
         * Detach a participant from a conference, put the call on hold, do not hangup it
         * @param call id
         * @param the current call id
         */
        bool detachParticipant(const std::string& call_id);

        /**
         * Remove the conference participant from a conference
         * @param call id
         */
        void removeParticipant(const std::string& call_id);

        /**
         * Join two conference together into one unique conference
         */
        bool joinConference(const std::string& conf_id1, const std::string& conf_id2);

        void addAudio(Call& call);

        void removeAudio(Call& call);

        /**
         * Save config to file
         */
        void saveConfig();

        /**
         * @return true if we tried to register once
         */
        bool hasTriedToRegister_;

        /**
         * Play a ringtone
         */
        void playTone();

        /**
         * Play a special ringtone ( BUSY ) if there's at least one message on the voice mail
         */
        void playToneWithMessage();

        /**
         * Acts on the audio streams and audio files
         */
        void stopTone();

        /**
         * Handle incoming call and notify user
         * @param call A call pointer
         * @param accountId an account id
         */
        void incomingCall(Call &call, const std::string& accountId);

        /**
         * Notify the user that the recipient of the call has answered and the put the
         * call in Current state
         * @param id  The call identifier
         */
        void peerAnsweredCall(Call& call);

        /**
         * Rings back because the outgoing call is ringing and the put the
         * call in Ringing state
         * @param id  The call identifier
         */
        void peerRingingCall(Call& call);

        /**
         * Put the call in Hungup state, remove the call from the list
         * @param id  The call identifier
         */
        void peerHungupCall(Call& call);

        /**
         * Notify the client with an incoming message
         * @param accountId     The account identifier
         * @param messages A map if mime type as key and mime payload as value
         */
        void incomingMessage(const std::string& callID,
                             const std::string& from,
                             const std::map<std::string, std::string>& messages);

        /**
         * Send a new text message to the call, if participate to a conference, send to all participant.
         * @param callID        The call to send the message
         * @param message       A list of pair of mime types and payloads
         * @param from           The sender of this message (could be another participant of a conference)
         */
        void sendCallTextMessage(const std::string& callID,
                                 const std::map<std::string, std::string>& messages,
                                 const std::string& from, bool isMixed);

        /**
         * Notify the client he has voice mails
         * @param accountId	  The account identifier
         * @param nb_msg The number of messages
         */
        void startVoiceMessageNotification(const std::string& accountId, int nb_msg);

        /**
         * ConfigurationManager - Send registration request
         * @param accountId The account to register/unregister
         * @param enable The flag for the type of registration
         *   false for unregistration request
         *   true for registration request
         */
        void sendRegister(const std::string& accountId, bool enable);

        uint64_t sendTextMessage(const std::string& accountID, const std::string& to,
                             const std::map<std::string, std::string>& payloads);

        int getMessageStatus(uint64_t id);

        /**
         * Get account list
         * @return std::vector<std::string> A list of accoundIDs
         */
        std::vector<std::string> getAccountList() const;

        /**
         * Set the account order in the config file
         */
        void setAccountsOrder(const std::string& order);

        /**
         * Retrieve details about a given account
         * @param accountID	  The account identifier
         * @return std::map< std::string, std::string > The account details
         */
        std::map<std::string, std::string> getAccountDetails(const std::string& accountID) const;

        /**
         * Retrieve volatile details such as recent registration errors
         * @param accountID The account identifier
         * @return std::map< std::string, std::string > The account volatile details
         */
        std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountID) const;

        /**
         * Retrieve details about a given call
         * @param callID	  The account identifier
         * @return std::map< std::string, std::string > The call details
         */
        std::map<std::string, std::string> getCallDetails(const std::string& callID);

        /**
         * Get call list
         * @return std::vector<std::string> A list of call IDs
         */
        std::vector<std::string> getCallList() const;

        /**
         * Retrieve details about a given call
         * @param callID	  The account identifier
         * @return std::map< std::string, std::string > The call details
         */
        std::map<std::string, std::string> getConferenceDetails(const std::string& callID) const;

        /**
         * Get call list
         * @return std::vector<std::string> A list of call IDs
         */
        std::vector<std::string> getConferenceList() const;


        /**
         * Get a list of participant to a conference
         * @return std::vector<std::string> A list of call IDs
         */
        std::vector<std::string> getParticipantList(const std::string& confID) const;

        /**
         * Get a list of the display names for everyone in a conference
         * @return std::vector<std::string> A list of display names
         */
        std::vector<std::string> getDisplayNames(const std::string& confID) const;

        std::string getConferenceId(const std::string& callID);

        /**
         * Save the details of an existing account, given the account ID
         * This will load the configuration map with the given data.
         * It will also register/unregister links where the 'Enabled' switched.
         * @param accountID	  The account identifier
         * @param details	  The account parameters
         */
        void setAccountDetails(const std::string& accountID,
                               const std::map<std::string, ::std::string > &details);

        void setAccountActive(const std::string& accountID, bool active);

        std::map<std::string, std::string> testAccountICEInitialization(const std::string& accountID);

        /**
         * Return a new random accountid that is not present in the list
         * @return A brand new accountid
         */
        std::string getNewAccountId();

        /**
         * Add a new account, and give it a new account ID automatically
         * @param details The new account parameters
         * @param accountId optionnal predetermined accountid to use
         * @return The account Id given to the new account
         */
        std::string addAccount(const std::map<std::string, std::string> &details,
                               const std::string& accountId = {});

        /**
         * Delete an existing account, unregister VoIPLink associated, and
         * purge from configuration.
         * If 'flush' argument is true, filesystem entries are also removed.
         * @param accountID	The account unique ID
         */
        void removeAccount(const std::string& accountID, bool flush=false);

        /**
         * Set input audio plugin
         * @param audioPlugin The audio plugin
         */
        void setAudioPlugin(const std::string& audioPlugin);

        /**
             * Set audio device
             * @param index The index of the soundcard
             * @param the type of stream, either PLAYBACK, CAPTURE, RINGTONE
             */
        void setAudioDevice(int index, DeviceType streamType);

        /**
         * Get list of supported audio output device
         * @return std::vector<std::string> A list of the audio devices supporting playback
         */
        std::vector<std::string> getAudioOutputDeviceList();

        /**
         * Get list of supported audio input device
         * @return std::vector<std::string> A list of the audio devices supporting capture
         */
        std::vector<std::string> getAudioInputDeviceList();

        /**
         * Get string array representing integer indexes of output, input, and ringtone device
         * @return std::vector<std::string> A list of the current audio devices
         */
        std::vector<std::string> getCurrentAudioDevicesIndex();

        /**
         * Get index of an audio device
         * @param name The string description of an audio device
         * @return int  His index
         */
        int getAudioInputDeviceIndex(const std::string &name);
        int getAudioOutputDeviceIndex(const std::string &name);

        /**
         * Get current alsa plugin
         * @return std::string  The Alsa plugin
         */
        std::string getCurrentAudioOutputPlugin() const;

        /**
         * Get the noise reduction engin state from
         * the current audio layer.
         */
        bool getNoiseSuppressState() const;

        /**
         * Set the noise reduction engin state in the current
         * audio layer.
         */
        void setNoiseSuppressState(bool state);

        bool isAGCEnabled() const;
        void setAGCState(bool enabled);

        bool switchInput(const std::string& callid, const std::string& res);

        /**
         * Ringtone option.
         * If ringtone is enabled, ringtone on incoming call use custom choice. If not, only standart tone.
         * @return int	1 if enabled
         *	        0 otherwise
         */
        int isRingtoneEnabled(const std::string& id);

        /**
         * Set the ringtone option
         * Inverse current value
         */
        void ringtoneEnabled(const std::string& id);

        /**
         * Get is always recording functionality
         */
        bool getIsAlwaysRecording() const;

        /**
         * Set is always recording functionality, every calls will then be set in RECORDING mode
         * once answered
         */
        void setIsAlwaysRecording(bool isAlwaysRec);

        /**
         * Set recording on / off
         * Start recording
         * @param id  The call identifier
         * Returns true if the call was set to record
         */
        bool toggleRecordingCall(const std::string& id);

        /**
         * Return true if the call is currently recorded
         */
        bool isRecording(const std::string& id);

        /**
         * Start playback fo a recorded file if and only if audio layer is not already started.
         * @param File path of the file to play
             */
        bool startRecordedFilePlayback(const std::string&);

        void recordingPlaybackSeek(const double value);

        /**
         * Stop playback of recorded file
         * @param File of the file to stop
         */
        void stopRecordedFilePlayback(const std::string&);

        /**
         * Set the maximum number of days to keep in the history
         * @param calls The number of days
         */
        void setHistoryLimit(int days);

        /**
         * Get the maximum number of days to keep in the history
         * @return double The number of days
         */
        int getHistoryLimit() const;

        /**
         * Configure the start-up option
         * @return int	1 if Ring should start in the system tray
         *	        0 otherwise
         */
        int isStartHidden();

        /**
         * Configure the start-up option
         * At startup, Ring can be displayed or start hidden in the system tray
         */
        void startHidden();

        /**
         * Get the audio manager
         * @return int The audio manager
         *		    "alsa"
         *		    "pulseaudio"
         */
        std::string getAudioManager() const;

        /**
         * Set the audio manager
         * @return true if api is now in use, false otherwise
         */
        bool setAudioManager(const std::string &api);

        /**
         * Callback called when the audio layer initialised with its
         * preferred format.
         */
        AudioFormat hardwareAudioFormatChanged(AudioFormat format);

        /**
         * Should be called by any component dealing with an external
         * audio source, indicating the format used so the mixer format
         * can be eventually adapted.
         * @returns the new format used by the main buffer.
         */
        AudioFormat audioFormatUsed(AudioFormat format);

        /**
         * Handle audio sounds heard by a caller while they wait for their
         * connection to a called party to be completed.
         */
        void ringback();

        /**
         * Handle played music when an incoming call occurs
         */
        void playRingtone(const std::string& accountID);

        /**
         * Handle played music when a congestion occurs
         */
        void congestion();

        /**
         * Play the dtmf-associated sound
         * @param code  The pressed key
         */
        void playDtmf(char code);

        /**
         * Handle played sound when a call can not be conpleted because of a busy recipient
         */
        void callBusy(Call& call);

        /**
         * Handle played sound when a failure occurs
         */
        void callFailure(Call& call);

        /**
         * Retrieve the current telephone tone
         * @return AudioLoop*   The audio tone or 0 if no tone (init before calling this function)
         */
        AudioLoop* getTelephoneTone();

        /**
         * Retrieve the current telephone file
         * @return AudioLoop* The audio file or 0 if the wav is stopped
         */
        AudioLoop* getTelephoneFile();

        /**
         * @return true is there is one or many incoming call waiting
         * new call, not anwsered or refused
         */
        bool incomingCallsWaiting();

        /**
         * Return a new random callid that is not present in the list
         * @return std::string A brand new callid
         */
        std::string getNewCallID();

        /**
         * Get the current call
         * @return std::shared_ptr<Call> A call shared pointer (could be empty)
         */
        std::shared_ptr<Call> getCurrentCall() const;

        /**
         * Get the current call id
         * @return std::string	The call id or ""
         */
        const std::string getCurrentCallId() const;

        /**
         * Check if a call is the current one
         * @param call the new call
         * @return bool True if the call is the current
         */
        bool isCurrentCall(const Call& call) const;

        void initAudioDriver();

        /**
         * Load the accounts order set by the user from the dringrc config file
         * @return std::vector<std::string> A vector containing the account ID's
         */
        std::vector<std::string> loadAccountOrder() const;

        /**
         * Get the Call referred by callID. If the Call does not exist, return
         * empty std::shared_ptr<Call> instance
         */
        std::shared_ptr<Call> getCallFromCallID(const std::string &callID) const;

    private:
        std::atomic_bool autoAnswer_ {false};

        void removeAccounts();

        bool parseConfiguration();

        // Set the ringtone or recorded call to be played
        void updateAudioFile(const std::string &file, int sampleRate);


        /**
         * Process remaining participant given a conference and the current call id.
         * Mainly called when a participant is detached or hagned up
         * @param current call id
         * @param conference pointer
         */
        void processRemainingParticipants(Conference &conf);

        /**
         * Create config directory in home user and return configuration file path
         */
        std::string retrieveConfigPath() const;

        void unsetCurrentCall();

        void switchCall(const std::string& id);
        void switchCall(std::shared_ptr<Call> call);

        /** Application wide tone controler */
        ToneControl toneCtrl_;

        /*
         * Play one tone
         * @return false if the driver is uninitialize
         */
        void playATone(Tone::TONEID toneId);

        int getCurrentDeviceIndex(DeviceType type);

        /** Current Call ID */
        std::string currentCall_;

        /** Protected current call access */
        std::mutex currentCallMutex_;

        /** Audio layer */
        std::shared_ptr<AudioLayer> audiodriver_{nullptr};

        // Main thread
        std::unique_ptr<DTMF> dtmfKey_;

        /** Buffer to generate DTMF */
        AudioBuffer dtmfBuf_;

        // To handle volume control
        // short speakerVolume_;
        // short micVolume_;
        // End of sound variable

        /**
         * Mutex used to protect audio layer
         */
        std::mutex audioLayerMutex_;

        /**
         * Waiting Call Vectors
         */
        CallIDSet waitingCalls_;

        /**
         * Protect waiting call list, access by many voip/audio threads
         */
        std::mutex waitingCallsMutex_;

        /**
         * Add incoming callid to the waiting list
         * @param id std::string to add
         */
        void addWaitingCall(const std::string& id);

        /**
         * Remove incoming callid to the waiting list
         * @param id std::string to remove
         */
        void removeWaitingCall(const std::string& id);

        /**
         * Path of the ConfigFile
         */
        std::string path_;

        /**
         * Load the account map from configuration
         */
        int loadAccountMap(const YAML::Node &node);

        /**
         * Instance of the RingBufferPool for the whole application
         *
         * In order to send signal to other parts of the application, one must pass through the RingBufferMananger.
         * Audio instances must be registered into the RingBufferMananger and bound together via the Manager.
         *
         */
        std::unique_ptr<RingBufferPool> ringbufferpool_;

    public:

        /**
         * Return a pointer to the instance of the RingBufferPool
         */
        RingBufferPool& getRingBufferPool() { return *ringbufferpool_; }

        /**
         * Tell if there is a current call processed
         * @return bool True if there is a current call
         */
        bool hasCurrentCall() const;

        /**
         * Get an account pointer, looks for account of type T
         * @param accountID account ID to get
         * @return std::shared_ptr<Account> Shared pointer on an Account instance or nullptr if not found
         */
        template <class T=Account>
        std::shared_ptr<T> getAccount(const std::string& accountID) const {
            return accountFactory_.getAccount<T>(accountID);
        }

        /**
         * Get a list of account pointers of type T (baseclass Account)
         * @return a sorted vector of all accounts of type T
         */
        template <class T=Account>
        std::vector<std::shared_ptr<T>> getAllAccounts() const {
            auto account_order = loadAccountOrder();
            std::vector<std::shared_ptr<T>> accountList;

            // If no order has been set, load the default one ie according to the creation date.
            if (account_order.empty()) {
                for (const auto &account : accountFactory_.getAllAccounts<T>())
                    accountList.emplace_back(account);
            } else {
                for (const auto& id : account_order) {
                    if (auto acc = accountFactory_.getAccount<T>(id))
                        accountList.push_back(acc);
                }
            }
            return accountList;
        }

        template <class T=Account>
        bool accountCount() const {
            return accountFactory_.accountCount<T>();
        }

        // only used by test framework
        bool hasAccount(const std::string& accountID) {
            return accountFactory_.hasAccount(accountID);
        }

        /**
         * Send registration for all enabled accounts
         */
        void registerAccounts();

        /**
         * Suspends Ring's audio processing if no calls remain, allowing
         * other applications to resume audio.
         * See:
         * https://projects.savoirfairelinux.com/issues/7037
        */
        void checkAudio();

        /**
         * Call periodically to poll for VoIP events */
        void
        pollEvents();

        /**
         * Create a new outgoing call
         * @param toUrl The address to call
         * @param preferredAccountId The IP of preferred account to use.
         *   This is not necessary the account used.
         * @return Call*  A shared pointer on a valid call.
         * @note This function raises VoipLinkException() on errors.
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl,
                                              const std::string& preferredAccountId);

        CallFactory callFactory;

        using EventHandler = std::function<void()>;

        /**
         * Install an event handler called periodically by pollEvents().
         * @param handlerId an unique identifier for the handler.
         * @param handler the event handler function.
         */
        void registerEventHandler(uintptr_t handlerId, EventHandler handler);

        /**
         * Remove a previously registered event handler.
         * @param handlerId id of handler to remove.
         */
        void unregisterEventHandler(uintptr_t handlerId);

        IceTransportFactory& getIceTransportFactory() { return *ice_tf_; }

        void addTask(const std::function<bool()>&& task);

        struct Runnable {
            std::function<void()> cb;
            Runnable(const std::function<void()>&& t) : cb(std::move(t)) {}
        };
        std::shared_ptr<Runnable> scheduleTask(const std::function<void()>&& task, std::chrono::steady_clock::time_point when);
        void scheduleTask(std::shared_ptr<Runnable> task, std::chrono::steady_clock::time_point when);

#ifdef RING_VIDEO
        /**
         * Create a new SinkClient instance, store it in an internal cache as a weak_ptr
         * and return it as a shared_ptr. If a SinkClient is already stored for the given id,
         * this method returns this instance.
         * @param id SinkClient identifier as a string. Default is empty.
         * @param mixer true if the SinkCient is the sink of a VideoMixer node. Default is false.
         * @return share_ptr<SinkClient> A shared pointer on the created instance.
         */
        std::shared_ptr<video::SinkClient> createSinkClient(const std::string& id="", bool mixer=false);

        /**
         * Return an existing SinkClient instance as a shared_ptr associated to the given identifier.
         * Return an empty shared_ptr (nullptr) if nothing found.
         * @param id SinkClient identifier as a string.
         * @return share_ptr<SinkClient> A shared pointer on the found instance. Empty if not found.
         */
        std::shared_ptr<video::SinkClient> getSinkClient(const std::string& id);

        VideoManager& getVideoManager() const { return *videoManager_; }

        bool getDecodingAccelerated() const;

        void setDecodingAccelerated(bool isAccelerated);
#endif // RING_VIDEO

        std::atomic<unsigned> dhtLogLevel {0}; // default = disable

    private:
        NON_COPYABLE(Manager);

        std::map<uintptr_t, EventHandler> eventHandlerMap_;
        decltype(eventHandlerMap_)::iterator nextEventHandler_;

        std::list<std::function<bool()>> pendingTaskList_;
        std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<Runnable>> scheduledTasks_;
        std::mutex scheduledTasksMutex_;

        /**
         * Test if call is a valid call, i.e. have been created and stored in
         * call-account map
         * @param callID the std::string to be tested
         * @return true if call is created and present in the call-account map
         */
        bool isValidCall(const std::string& callID);

        /**
         * Send unregister for all enabled accounts
         */
        void unregisterAccounts();


        // Map containing conference pointers
        ConferenceMap conferenceMap_;

        std::atomic_bool finished_ {false};

        AccountFactory accountFactory_;

        std::mt19937_64 rand_;

        void loadAccount(const YAML::Node &item, int &errorCount,
                         const std::string &accountOrder);

        /* ICE support */
        std::unique_ptr<IceTransportFactory> ice_tf_;

        /* Sink ID mapping */
        std::map<std::string, std::weak_ptr<video::SinkClient>> sinkMap_;

        void sendTextMessageToConference(const Conference& conf,
                                         const std::map<std::string, std::string>& messages,
                                         const std::string& from) const noexcept;

#ifdef RING_VIDEO
    std::unique_ptr<VideoManager> videoManager_;
#endif
};

// Helper to install a callback to be called once by the main event loop
template<typename Callback>
static void runOnMainThread(Callback&& cb) {
    Manager::instance().addTask([=]() mutable {
        cb();
        return false;
    });
}

} // namespace ring
