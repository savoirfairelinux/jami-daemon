/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef MANAGER_IMPL_H_
#define MANAGER_IMPL_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <mutex>

#include "client/client.h"

#include "config/sfl_config.h"

#include "account.h"

#include "call.h"
#include "conference.h"

#include "audio/audiolayer.h"
#include "audio/sound/tone.h"  // for Tone::TONEID declaration
#include "audio/codecs/audiocodecfactory.h"
#include "audio/mainbuffer.h"

#include "preferences.h"
#include "history/history.h"
#include "noncopyable.h"

namespace Conf {
    class YamlParser;
    class YamlEmitter;
}

class DTMF;
class AudioFile;
class AudioLayer;
class History;
class TelephoneTone;
class VoIPLink;

class Account;
class SIPAccount;
class IAXAccount;

/** To send multiple string */
typedef std::list<std::string> TokenList;

/** To store conference objects by conference ids */
typedef std::map<std::string, Conference*> ConferenceMap;

static const char * const default_conf = "conf";

/** Manager (controller) of sflphone daemon */
class ManagerImpl {
    public:
        ManagerImpl();
        ~ManagerImpl();

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

        // Manager should not be accessed until initialized.
        // FIXME this is an evil hack!
        static std::atomic_bool initialized;

        /**
         * Initialisation of thread (sound) and map.
         * Init a new VoIPLink, audio codec and audio driver
         */
        void init(const std::string &config_file);

        void setPath(const std::string &path);

        /**
         * Enter mainloop
         */
        int run();
        /**
         * Interrupt mainloop
         */
        int interrupt();

        /*
         * Terminate all threads and exit DBus loop
         */
        void finish();

        /**
         * Accessor to audiodriver.
         * it's multi-thread and use mutex internally
         * @return AudioLayer*  The audio layer object
         */
        AudioLayer* getAudioDriver();

        void startAudioDriverStream();

        /**
         * Functions which occur with a user's action
         * Place a new call
         * @param accountId	The account to make tha call with
         * @param call_id  The call identifier
         * @param to  The recipient of the call
         * @param conf_id The conference identifier if any
         * @return bool true on success
         *		  false otherwise
         */
        bool outgoingCall(const std::string&, const std::string&, const std::string&, const std::string& = "");

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
        Conference* createConference(const std::string& id1, const std::string& id2);

        /**
         * Delete this conference
         * @param the conference ID
         */
        void removeConference(const std::string& conference_id);

        /**
         * Return the conference id for which this call is attached
         * @ param the call id
         */
        Conference* getConferenceFromCallID(const std::string& call_id);

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

        void addStream(const std::string& call_id);

        void removeStream(const std::string& call_id);

        /**
         * Save config to file
         */
        void saveConfig();

        /**
         * @return true if we tried to register once
         */
        bool hasTriedToRegister_;

        /**
         * Handle choice of the DTMF-send-way
         * @param   id: callid of the line.
         * @param   code: pressed key.
         */
        void sendDtmf(const std::string& id, char code);

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
        void peerAnsweredCall(const std::string& id);

        /**
         * Rings back because the outgoing call is ringing and the put the
         * call in Ringing state
         * @param id  The call identifier
         */
        void peerRingingCall(const std::string& id);

        /**
         * Put the call in Hungup state, remove the call from the list
         * @param id  The call identifier
         */
        void peerHungupCall(const std::string& id);

#if HAVE_INSTANT_MESSAGING
        /**
         * Notify the client with an incoming message
         * @param accountId	The account identifier
         * @param message The content of the message
         */
        void incomingMessage(const std::string& callID, const std::string& from, const std::string& message);


        /**
         * Send a new text message to the call, if participate to a conference, send to all participant.
         * @param callID	The call to send the message
         * @param message	The content of the message
        * @param from	        The sender of this message (could be another participant of a conference)
         */
        bool sendTextMessage(const std::string& callID, const std::string& message, const std::string& from);
#endif // HAVE_INSTANT_MESSAGING

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
         *		 false for unregistration request
         *		 true for registration request
         */
        void sendRegister(const std::string& accountId, bool enable);

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

        /**
         * Add a new account, and give it a new account ID automatically
         * @param details The new account parameters
         * @return The account Id given to the new account
         */
        std::string addAccount(const std::map<std::string, std::string> &details);

        /**
         * Delete an existing account, unregister VoIPLink associated, and
         * purge from configuration.
         * @param accountID	The account unique ID
         */
        void removeAccount(const std::string& accountID);

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
         * @return int	1 if SFLphone should start in the system tray
         *	        0 otherwise
         */
        int isStartHidden();

        /**
         * Configure the start-up option
         * At startup, SFLphone can be displayed or start hidden in the system tray
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
        void hardwareAudioFormatChanged(AudioFormat format);

        /**
         * Should be called by any component dealing with an external
         * audio source, indicating the format used so the mixer format
         * can be eventually adapted.
         */
        void audioFormatUsed(AudioFormat format);

        /**
         * Change a specific value in the configuration tree.
         * This value will then be saved in the user config file sflphonedrc
         * @param section	The section name
         * @param name	The parameter name
         * @param value	The new string value
         * @return bool	true on success
         *		      false otherwise
         */
        void setConfig(const std::string& section, const std::string& name, const std::string& value);

        /**
         * Change a specific value in the configuration tree.
         * This value will then be saved in the user config file sflphonedrc
         * @param section	The section name
         * @param name	The parameter name
         * @param value	The new int value
         * @return bool	true on success
         *		      false otherwise
         */
        void setConfig(const std::string& section, const std::string& name, int value);

        /**
         * Get a string from the configuration tree
         * Throw an Conf::ConfigTreeItemException if not found
         * @param section The section name to look in
         * @param name    The parameter name
         * @return sdt::string    The string value
         */
        std::string getConfigString(const std::string& section, const std::string& name) const;

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
         * Handle played sound when a call can not be conpleted because of a busy recipient
         */
        void callBusy(const std::string& id);

        /**
         * Handle played sound when a failure occurs
         */
        void callFailure(const std::string& id);

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
         * Get the current call id
         * @return std::string	The call id or ""
         */
        std::string getCurrentCallId() const;

        /**
         * Check if a call is the current one
         * @param callId the new callid
         * @return bool   True if the id is the current call
         */
        bool isCurrentCall(const std::string& callId) const;

        void initAudioDriver();

        /**
         * Load the accounts order set by the user from the sflphonedrc config file
         * @return std::vector<std::string> A vector containing the account ID's
         */
        std::vector<std::string> loadAccountOrder() const;

        // map of codec (for configlist request)
        const AudioCodecFactory audioCodecFactory;

    private:
        void removeAccounts();

        bool parseConfiguration();

        // Set the ringtone or recorded call to be played
        void updateAudioFile(const std::string &file, int sampleRate);

        /**
         * Get the Call referred to by callID. If the Call does not exist, return NULL
         */
        std::shared_ptr<Call> getCallFromCallID(const std::string &callID);

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

        /**
         * Set current call ID to empty string
         */
        void unsetCurrentCall();

        /**
         * Switch of current call id
         * @param id The new callid
         */
        void switchCall(const std::string& id);

        /*
         * Play one tone
         * @return false if the driver is uninitialize
         */
        void playATone(Tone::TONEID toneId);

        Client client_;

        /** The configuration tree. It contains accounts parameters, general user settings ,audio settings, ... */
        Conf::ConfigTree config_;

        /** Current Call ID */
        std::string currentCallId_;

        /** Protected current call access */
        std::mutex currentCallMutex_;

        /** Audio layer */
        AudioLayer* audiodriver_;

        // Main thread
        std::unique_ptr<DTMF> dtmfKey_;

        /** Buffer to generate DTMF */
        AudioBuffer dtmfBuf_;

        /////////////////////
        // Protected by Mutex
        /////////////////////
        std::mutex toneMutex_;
        std::unique_ptr<TelephoneTone> telephoneTone_;
        std::unique_ptr<AudioFile> audiofile_;

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

        std::map<std::string, bool> IPToIPMap_;

        bool isIPToIP(const std::string& callID) const;

        /**
         * Load the account map from configuration
         */
        int loadAccountMap(Conf::YamlParser &parser);

        /**
         * Instance of the MainBuffer for the whole application
         *
         * In order to send signal to other parts of the application, one must pass through the mainbuffer.
         * Audio instances must be registered into the MainBuffer and bound together via the ManagerImpl.
         *
         */
        MainBuffer mainBuffer_;

    public:

        void setIPToIPForCall(const std::string& callID, bool IPToIP);

        /**
         * Test if call is a valid call, i.e. have been created and stored in
         * call-account map
         * @param callID the std::string to be tested
         * @return true if call is created and present in the call-account map
         */
        bool isValidCall(const std::string& callID);

        /**
         * Return a pointer to the  instance of the mainbuffer
         */
        MainBuffer &getMainBuffer();

        /**
         * Tell if there is a current call processed
         * @return bool True if there is a current call
         */
        bool hasCurrentCall() const;

        /**
         * Return the current Client
         * @return A pointer to the Client instance
         */
        Client* getClient();
#ifdef SFL_VIDEO
        VideoManager * getVideoManager();
#endif

        /**
        * Tell if an account exists
        * @param accountID account ID check
        * @return bool True if the account exists
        *		  false otherwise
        */
        bool accountExists(const std::string& accountID);

        std::vector<std::map<std::string, std::string> > getHistory();
        void clearHistory();

        /**
         * Get an account pointer, looks for both SIP and IAX
         * @param accountID account ID to get
         * @return Account*	 The account pointer or 0
         */
        Account* getAccount(const std::string& accountID) const;

        /**
         * Get a SIP account pointer
         * @param accountID account ID to get
         * @return SIPAccount* The account pointer or 0
         */
        SIPAccount *getSipAccount(const std::string& accontID) const;

#if HAVE_IAX
        /**
         * Get an IAX account pointer
         * @param accountID account ID to get
         * @return IAXAccount* The account pointer or 0
         */
        IAXAccount *getIaxAccount(const std::string& accountID) const;
#endif

        /**
         * Get a pointer to the IP2IP account
         * @return SIPAccount * Pointer to the IP2IP account
         */
        SIPAccount *getIP2IPAccount() const;

        /** Return the std::string from a CallID
         * Protected by mutex
         * @param callID the CallID in the list
         * @return std::string  The accountID associated or "" if the callID is not found
         */
        std::string getAccountFromCall(const std::string& callID);

        /**
         * Get the voip link from the account pointer
         * @param accountID	  Account ID to get
         * @return VoIPLink*   The voip link from the account pointer or 0
         */
        VoIPLink* getAccountLink(const std::string& accountID);

        /**
         * Free all ressources related to this account.
         *   ***Current calls using this account are HANG-UP***
         */
        void freeAccount(const std::string& accountID);

        /**
         * Send registration for all enabled accounts
         */
        void registerAccounts();

        void saveHistory();

        /**
         * Suspends SFLphone's audio processing if no calls remain, allowing
         * other applications to resume audio.
         * See:
         * https://projects.savoirfairelinux.com/issues/7037
        */
        void
        checkAudio();

        /**
         * Call periodically to poll for VoIP events */
        void
        pollEvents();

    private:
        NON_COPYABLE(ManagerImpl);

        /**
         * Send unregister for all enabled accounts
         */
        void unregisterAccounts();

        /**
         * Play the dtmf-associated sound
         * @param code  The pressed key
         */
        void playDtmf(char code);


        // Map containing conference pointers
        ConferenceMap conferenceMap_;

        /**
         * Get a map with all the current SIP and IAX accounts
         */
        AccountMap getAllAccounts() const;

        /**
          * To handle the persistent history
          * TODO: move this to ConfigurationManager
          */
        sfl::History history_;
        bool finished_;
};
#endif // MANAGER_IMPL_H_
