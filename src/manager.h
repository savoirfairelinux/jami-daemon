/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#include "account_factory.h"
#include "call_factory.h"
#include "preferences.h"
#include "media/audio/audiolayer.h"
#include "scheduled_executor.h"
#include "gittransport.h"
#include <dhtnet/certstore.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "trace-tools.h"

namespace asio {
class io_context;
}

namespace dhtnet {
class ChannelSocket;
class IceTransportFactory;
}

namespace jami {
namespace video {
class SinkClient;
class VideoGenerator;
} // namespace video
class RingBufferPool;
struct VideoManager;
class Conference;
class AudioLoop;
class JamiAccount;
class SIPVoIPLink;
class JamiPluginManager;

/** Manager (controller) of daemon */
// TODO LIBJAMI_PUBLIC only if tests
class LIBJAMI_TESTABLE Manager
{
private:
    std::mt19937_64 rand_;

public:
    // TODO LIBJAMI_PUBLIC only if tests
    static LIBJAMI_TESTABLE Manager& instance();

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
     * Audio preferences
     */
    AudioPreference audioPreference;

#ifdef ENABLE_PLUGIN
    /**
     * Plugin preferences
     */
    PluginPreferences pluginPreferences;
#endif

#ifdef ENABLE_VIDEO
    /**
     * Video preferences
     */
    VideoPreferences videoPreferences;
#endif

    // Manager should not be accessed until initialized.
    // FIXME this is an evil hack!
    static std::atomic_bool initialized;

#if TARGET_OS_IOS
    static bool isIOSExtension;
#endif

    static bool syncOnRegister;

    /**
     * Initialisation of thread (sound) and map.
     * Init a new VoIPLink, audio codec and audio driver
     */
    void init(const std::filesystem::path& config_file, libjami::InitFlag flags);

    /*
     * Terminate all threads and exit DBus loop
     */
    void finish() noexcept;

    void monitor(bool continuous);

    std::vector<std::map<std::string, std::string>> getConnectionList(const std::string& accountId, const std::string& conversationId);
    std::vector<std::map<std::string, std::string>> getChannelList(const std::string& accountId, const std::string& connectionId);

    /**
     * Accessor to audiodriver.
     * it's multi-thread and use mutex internally
     * @return AudioLayer*  The audio layer object
     */
    std::shared_ptr<AudioLayer> getAudioDriver();

    inline std::unique_ptr<AudioDeviceGuard> startAudioStream(AudioDeviceType stream)
    {
        return std::make_unique<AudioDeviceGuard>(*this, stream);
    }

    /**
     * Place a new call
     * @param accountId the user's account ID
     * @param callee the callee's ID/URI. Depends on the account type.
     * Refer to placeCall/placeCallWithMedia documentations.
     * @param mediaList a list of medias to include
     * @return the call ID on success, empty string otherwise
     */
    std::string outgoingCall(const std::string& accountId,
                             const std::string& callee,
                             const std::vector<libjami::MediaMap>& mediaList = {});

    /**
     * Functions which occur with a user's action
     * Answer the call
     * @param callId
     */
    bool answerCall(const std::string& accountId,
                    const std::string& callId,
                    const std::vector<libjami::MediaMap>& mediaList = {});
    bool answerCall(Call& call, const std::vector<libjami::MediaMap>& mediaList = {});

    /**
     * Handle incoming call and notify user
     * @param accountId an account id
     * @param call A call pointer
     */
    void incomingCall(const std::string& accountId, Call& call);

    /**
     * Functions which occur with a user's action
     * Hangup the call
     * @param accountId
     * @param callId  The call identifier
     */
    bool hangupCall(const std::string& accountId, const std::string& callId);

    /**
     * Functions which occur with a user's action
     * Hangup the conference (hangup every participants)
     * @param id  The call identifier
     */
    bool hangupConference(const std::string& accountId, const std::string& confId);

    /**
     * Functions which occur with a user's action
     * Put the call on hold
     * @param accountId
     * @param callId  The call identifier
     */
    bool onHoldCall(const std::string& accountId, const std::string& callId);

    /**
     * Functions which occur with a user's action
     * Put the call off hold
     * @param accountId
     * @param id  The call identifier
     */
    bool offHoldCall(const std::string& accountId, const std::string& callId);

    /**
     * Functions which occur with a user's action
     * Transfer the call
     * @param id  The call identifier
     * @param to  The recipient of the transfer
     */
    bool transferCall(const std::string& accountId, const std::string& id, const std::string& to);

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
    bool refuseCall(const std::string& accountId, const std::string& id);

    /**
     * Hold every participant to a conference
     * @param the conference id
     */
    bool holdConference(const std::string& accountId, const std::string& confId);

    /**
     * Unhold all conference participants
     * @param the conference id
     */
    bool unHoldConference(const std::string& accountId, const std::string& confId);

    /**
     * Add a participant to a conference
     * @param the call id
     * @param the conference id
     */
    bool addParticipant(const std::string& accountId,
                        const std::string& callId,
                        const std::string& account2Id,
                        const std::string& confId);
    bool addParticipant(const std::shared_ptr<SIPCall>& call, Conference& conference);

    /**
     * Bind the main participant to a conference (mainly called on a double click action)
     * @param the conference id
     */
    bool addMainParticipant(const std::string& accountId, const std::string& confId);

    /**
     * Join two participants to create a conference
     * @param the fist call id
     * @param the second call id
     */
    bool joinParticipant(const std::string& accountId,
                         const std::string& callId1,
                         const std::string& account2Id,
                         const std::string& callId2,
                         bool attached = true);

    /**
     * Create a conference from a list of participant
     * @param A vector containing the list of participant
     */
    void createConfFromParticipantList(const std::string& accountId,
                                       const std::vector<std::string>&);

    /**
     * Detach a participant from a conference, put the call on hold, do not hangup it
     * @param call id
     * @param the current call id
     */
    bool detachParticipant(const std::string& callId);

    /**
     * Detach the local participant from curent conference.
     * Remote participants are placed in hold.
     */
    bool detachLocal(const std::shared_ptr<Conference>& conf = {});

    /**
     * Remove a call from a conference
     * @param call id
     */
    void removeCall(const std::shared_ptr<Call>& call);

    /**
     * Join two conference together into one unique conference
     */
    bool joinConference(const std::string& accountId,
                        const std::string& confId1,
                        const std::string& account2Id,
                        const std::string& confId2);

    void addAudio(Call& call);

    void removeAudio(const std::shared_ptr<Call>& call);

    /**
     * Save config to file
     */
    void saveConfig();
    void saveConfig(const std::shared_ptr<Account>& acc);

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
    void peerHungupCall(const std::shared_ptr<Call>& call);

    /**
     * Notify the client with an incoming message
     * @param accountId     The account identifier
     * @param callId        The call to send the message
     * @param messages A map if mime type as key and mime payload as value
     */
    void incomingMessage(const std::string& accountId,
                         const std::string& callId,
                         const std::string& from,
                         const std::map<std::string, std::string>& messages);

    /**
     * Send a new text message to the call, if participate to a conference, send to all participant.
     * @param accountId
     * @param callId        The call to send the message
     * @param message       A list of pair of mime types and payloads
     * @param from           The sender of this message (could be another participant of a conference)
     */
    void sendCallTextMessage(const std::string& accountId,
                             const std::string& callID,
                             const std::map<std::string, std::string>& messages,
                             const std::string& from,
                             bool isMixed);

    /**
     * ConfigurationManager - Send registration request
     * @param accountId The account to register/unregister
     * @param enable The flag for the type of registration
     *   false for unregistration request
     *   true for registration request
     */
    void sendRegister(const std::string& accountId, bool enable);

    bool isPasswordValid(const std::string& accountID, const std::string& password);

    uint64_t sendTextMessage(const std::string& accountID,
                             const std::string& to,
                             const std::map<std::string, std::string>& payloads,
                             bool fromPlugin = false,
                             bool onlyConnected = false);

    int getMessageStatus(uint64_t id) const;
    int getMessageStatus(const std::string& accountID, uint64_t id) const;

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
     * @param accountID   The account identifier
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
     * Get list of calls (internal subcalls are filter-out)
     * @return std::vector<std::string> A list of call IDs (without subcalls)
     */
    std::vector<std::string> getCallList() const;

    /**
     * Save the details of an existing account, given the account ID
     * This will load the configuration map with the given data.
     * It will also register/unregister links where the 'Enabled' switched.
     * @param accountID   The account identifier
     * @param details     The account parameters
     */
    void setAccountDetails(const std::string& accountID,
                           const std::map<std::string, ::std::string>& details);

    void setAccountActive(const std::string& accountID, bool active, bool shutdownConnections);

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
    std::string addAccount(const std::map<std::string, std::string>& details,
                           const std::string& accountId = {});

    /**
     * Delete an existing account, unregister VoIPLink associated, and
     * purge from configuration.
     * If 'flush' argument is true, filesystem entries are also removed.
     * @param accountID The account unique ID
     */
    void removeAccount(const std::string& accountID, bool flush = false);

    void removeAccounts();

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
    void setAudioDevice(int index, AudioDeviceType streamType);

    void startAudio();

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
    int getAudioInputDeviceIndex(const std::string& name);
    int getAudioOutputDeviceIndex(const std::string& name);

    /**
     * Get current alsa plugin
     * @return std::string  The Alsa plugin
     */
    std::string getCurrentAudioOutputPlugin() const;

    /**
     * Get the noise reduction engine state from
     * the current audio layer.
     */
    std::string getNoiseSuppressState() const;

    /**
     * Set the noise reduction engine state in the current
     * audio layer.
     */
    void setNoiseSuppressState(const std::string& state);

    bool isAGCEnabled() const;
    void setAGCState(bool enabled);

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
    bool toggleRecordingCall(const std::string& accountId, const std::string& id);

    /**
     * Start playback fo a recorded file if and only if audio layer is not already started.
     * @param File path of the file to play
     */
    bool startRecordedFilePlayback(const std::string&);

    void recordingPlaybackSeek(const double value);

    /**
     * Stop playback of recorded file
     */
    void stopRecordedFilePlayback();

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
     * Set ringing timeout (number of seconds after which a call will
     * enter BUSY state if not answered).
     * @param timeout in seconds
     */
    void setRingingTimeout(int timeout);

    /**
     * Get ringing timeout (number of seconds after which a call will
     * enter BUSY state if not answered).
     * @return timeout in seconds
     */
    int getRingingTimeout() const;

    /**
     * Get the audio manager
     * @return int The audio manager
     *          "alsa"
     *          "pulseaudio"
     */
    std::string getAudioManager() const;

    /**
     * Set the audio manager
     * @return true if api is now in use, false otherwise
     */
    bool setAudioManager(const std::string& api);

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
    void callFailure(const std::shared_ptr<Call>& call);

    /**
     * Retrieve the current telephone tone
     * @return AudioLoop*   The audio tone or 0 if no tone (init before calling this function)
     */
    std::shared_ptr<AudioLoop> getTelephoneTone();

    /**
     * Retrieve the current telephone file
     * @return AudioLoop* The audio file or 0 if the wav is stopped
     */
    std::shared_ptr<AudioLoop> getTelephoneFile();

    /**
     * @return true is there is one or many incoming call waiting
     * new call, not answered or refused
     */
    bool incomingCallsWaiting();

    /**
     * Get the current call
     * @return std::shared_ptr<Call> A call shared pointer (could be empty)
     */
    std::shared_ptr<Call> getCurrentCall() const;

    /**
     * Get the current call id
     * @return std::string  The call id or ""
     */
    const std::string& getCurrentCallId() const;

    /**
     * Check if a call is the current one
     * @param call the new call
     * @return bool True if the call is the current
     */
    bool isCurrentCall(const Call& call) const;

    /**
     * Load the accounts order set by the user from the jamirc config file
     * @return std::vector<std::string> A vector containing the account ID's
     */
    std::vector<std::string_view> loadAccountOrder() const;

    /**
     * Load the account map from configuration
     */
    int loadAccountMap(const YAML::Node& node);

    /**
     * Get the Call referred by callID. If the Call does not exist, return
     * empty std::shared_ptr<Call> instance
     */
    std::shared_ptr<Call> getCallFromCallID(const std::string& callID) const;

    /**
     * Return a pointer to the instance of the RingBufferPool
     */
    RingBufferPool& getRingBufferPool();

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
    template<class T = Account>
    inline std::shared_ptr<T> getAccount(std::string_view accountId) const
    {
        return accountFactory.getAccount<T>(accountId);
    }

    /**
     * Get a list of account pointers of type T (baseclass Account)
     * @return a sorted vector of all accounts of type T
     */
    template<class T = Account>
    std::vector<std::shared_ptr<T>> getAllAccounts() const
    {
        const auto& account_order = loadAccountOrder();
        const auto& all_accounts = accountFactory.getAllAccounts<T>();
        std::vector<std::shared_ptr<T>> accountList;
        accountList.reserve(all_accounts.size());
        for (const auto& id : account_order) {
            if (auto acc = accountFactory.getAccount<T>(id))
                accountList.emplace_back(std::move(acc));
        }
        for (auto& account : all_accounts) {
            if (std::find(accountList.begin(), accountList.end(), account) == accountList.end())
                accountList.emplace_back(std::move(account));
        }
        return accountList;
    }

    template<class T = Account>
    std::size_t accountCount() const
    {
        return accountFactory.accountCount<T>();
    }

    template<class T>
    inline std::shared_ptr<T> findAccount(const std::function<bool(const std::shared_ptr<T>&)>& pred)
    {
        for (const auto& account : getAllAccounts<T>()) {
            if (pred(account))
                return account;
        }
        return {};
    }

    // only used by test framework
    bool hasAccount(const std::string& accountID);

    /**
     * Send registration for all enabled accounts
     */
    void registerAccounts();

    /**
     * Send unregister for all enabled accounts
     */
    void unregisterAccounts();

    /**
     * Create a new outgoing call
     * @param toUrl Destination address
     * @param accountId local account
     * @param mediaList the list of medias
     * @return A (shared) pointer of Call class type.
     * @note This function raises VoipLinkException() on error.
     */
    std::shared_ptr<Call> newOutgoingCall(std::string_view toUrl,
                                          const std::string& accountId,
                                          const std::vector<libjami::MediaMap>& mediaList);

    CallFactory callFactory;

    const std::shared_ptr<dhtnet::IceTransportFactory>& getIceTransportFactory();

    std::shared_ptr<asio::io_context> ioContext() const;
    std::shared_ptr<dhtnet::upnp::UPnPContext> upnpContext() const;

    ScheduledExecutor& scheduler();
    std::shared_ptr<Task> scheduleTask(std::function<void()>&& task,
                                       std::chrono::steady_clock::time_point when,
                                       const char* filename = CURRENT_FILENAME(),
                                       uint32_t linum = CURRENT_LINE());

    std::shared_ptr<Task> scheduleTaskIn(std::function<void()>&& task,
                                         std::chrono::steady_clock::duration timeout,
                                         const char* filename = CURRENT_FILENAME(),
                                         uint32_t linum = CURRENT_LINE());

    std::map<std::string, std::string> getNearbyPeers(const std::string& accountID);

#ifdef ENABLE_VIDEO
    /**
     * Create a new SinkClient instance, store it in an internal cache as a weak_ptr
     * and return it as a shared_ptr. If a SinkClient is already stored for the given id,
     * this method returns this instance.
     * @param id SinkClient identifier as a string. Default is empty.
     * @param mixer true if the SinkCient is the sink of a VideoMixer node. Default is false.
     * @return share_ptr<SinkClient> A shared pointer on the created instance.
     */
    std::shared_ptr<video::SinkClient> createSinkClient(const std::string& id = "",
                                                        bool mixer = false);

    /**
     * Create a SinkClient instance for each participant in a conference, store it in an internal
     * cache as a weak_ptr and populates sinksMap with sink ids and shared_ptrs.
     * @param callId
     * @param infos ConferenceInfos that will create the sinks
     * @param videoStream the the VideoFrameActiveWriter to which the sinks should be attached
     * @param sinksMap A map between sink ids and the respective shared pointer.
     */
    void createSinkClients(
        const std::string& callId,
        const ConfInfo& infos,
        const std::vector<std::shared_ptr<video::VideoFrameActiveWriter>>& videoStreams,
        std::map<std::string, std::shared_ptr<video::SinkClient>>& sinksMap,
        const std::string& accountId = "");

    /**
     * Return an existing SinkClient instance as a shared_ptr associated to the given identifier.
     * Return an empty shared_ptr (nullptr) if nothing found.
     * @param id SinkClient identifier as a string.
     * @return share_ptr<SinkClient> A shared pointer on the found instance. Empty if not found.
     */
    std::shared_ptr<video::SinkClient> getSinkClient(const std::string& id);

#endif // ENABLE_VIDEO
    VideoManager& getVideoManager() const;

    std::atomic<unsigned> dhtLogLevel {0}; // default = disable
    AccountFactory accountFactory;

    std::vector<libjami::Message> getLastMessages(const std::string& accountID,
                                                  const uint64_t& base_timestamp);

    SIPVoIPLink& sipVoIPLink() const;
#ifdef ENABLE_PLUGIN
    JamiPluginManager& getJamiPluginManager() const;
#endif
    /**
     * Return current git socket used for a conversation
     * @param accountId         Related account
     * @param deviceId          Related device
     * @param conversationId    Related conversation
     * @return std::optional<std::weak_ptr<ChannelSocket>> the related socket
     */
    std::shared_ptr<dhtnet::ChannelSocket> gitSocket(const std::string_view accountId,
                                             const std::string_view deviceId,
                                             const std::string_view conversationId);

    void setDefaultModerator(const std::string& accountID, const std::string& peerURI, bool state);
    std::vector<std::string> getDefaultModerators(const std::string& accountID);
    void enableLocalModerators(const std::string& accountID, bool state);
    bool isLocalModeratorsEnabled(const std::string& accountID);
    void setAllModerators(const std::string& accountID, bool allModerators);
    bool isAllModerators(const std::string& accountID);

    void insertGitTransport(git_smart_subtransport* tr, std::unique_ptr<P2PSubTransport>&& sub);
    void eraseGitTransport(git_smart_subtransport* tr);

    dhtnet::tls::CertificateStore& certStore(const std::string& accountId) const;

private:
    Manager();
    ~Manager();
    friend class AudioDeviceGuard;

    // Data members
    struct ManagerPimpl;
    std::unique_ptr<ManagerPimpl> pimpl_;
};

class AudioDeviceGuard
{
public:
    AudioDeviceGuard(Manager& manager, AudioDeviceType type);
    ~AudioDeviceGuard();

private:
    Manager& manager_;
    const AudioDeviceType type_;
};

// Helper to install a callback to be called once by the main event loop
template<typename Callback>
static void
runOnMainThread(Callback&& cb,
                const char* filename = CURRENT_FILENAME(),
                uint32_t linum = CURRENT_LINE())
{
    Manager::instance().scheduler().run([cb = std::forward<Callback>(cb)]() mutable { cb(); },
                                        filename,
                                        linum);
}

} // namespace jami
