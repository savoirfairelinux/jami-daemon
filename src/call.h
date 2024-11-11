/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logger.h"

#include "conference.h"
#include "media/recordable.h"
#include "media/peerrecorder.h"
#include "media/media_codec.h"
#include "media/media_attribute.h"

#include <dhtnet/ip_utils.h>

#include <atomic>
#include <mutex>
#include <map>
#include <sstream>
#include <memory>
#include <vector>
#include <condition_variable>
#include <set>
#include <list>
#include <functional>

template<typename T>
bool
is_uninitialized(std::weak_ptr<T> const& weak)
{
    using wt = std::weak_ptr<T>;
    return !weak.owner_before(wt {}) && !wt {}.owner_before(weak);
}

namespace jami {

class VoIPLink;
class Account;
class AudioDeviceGuard;

class Call;
class Conference;

using CallMap = std::map<std::string, std::shared_ptr<Call>>;

namespace video {
class VideoGenerator;
}

/*
 * @file call.h
 * @brief A call is the base class for protocol-based calls
 */

class Call : public Recordable, public PeerRecorder, public std::enable_shared_from_this<Call>
{
public:
    /**
     * Tell where we're at with the call. The call gets Connected when we know
     * from the other end what happened with out call. A call can be 'Connected'
     * even if the call state is Busy, or Error.
     *
     * Audio should be transmitted when ConnectionState = Connected AND
     * CallState = Active.
     *
     * \note modify validStateTransition/getStateStr if this enum changes
     */
    enum class ConnectionState : unsigned {
        DISCONNECTED,
        TRYING,
        PROGRESSING,
        RINGING,
        CONNECTED,
        COUNT__
    };

    /**
     * The Call State.
     *
     * \note modify validStateTransition/getStateStr if this enum changes
     */
    enum class CallState : unsigned {
        INACTIVE,
        ACTIVE,
        HOLD,
        BUSY,
        PEER_BUSY,
        MERROR,
        OVER,
        COUNT__
    };

    enum class LinkType { GENERIC, SIP };

    using SubcallSet = std::set<std::shared_ptr<Call>, std::owner_less<std::shared_ptr<Call>>>;
    using OnReadyCb = std::function<void(bool)>;
    using StateListenerCb = std::function<bool(CallState, ConnectionState, int)>;

    /**
     * This determines if the call originated from the local user (OUTGOING)
     * or from some remote peer (INCOMING, MISSED).
     */
    enum class CallType : unsigned { INCOMING, OUTGOING, MISSED };

    virtual ~Call();

    std::weak_ptr<Call> weak() { return std::static_pointer_cast<Call>(shared_from_this()); }

    virtual LinkType getLinkType() const { return LinkType::GENERIC; }

    /**
     * Return a reference on the call id
     * @return call id
     */
    const std::string& getCallId() const { return id_; }

    /**
     * Return a reference on the conference id
     * @return call id
     */
    std::shared_ptr<Conference> getConference() const { return conf_.lock(); }
    bool isConferenceParticipant() const { return not is_uninitialized(conf_); }

    std::weak_ptr<Account> getAccount() const { return account_; }
    std::string getAccountId() const;

    CallType getCallType() const { return type_; }

    /**
     * Set the peer number (destination on outgoing)
     * not protected by mutex (when created)
     * @param number peer number
     */
    void setPeerNumber(const std::string& number) { peerNumber_ = number; }

    /**
     * Get the peer number (destination on outgoing)
     * not protected by mutex (when created)
     * @return std::string The peer number
     */
    const std::string& getPeerNumber() const { return peerNumber_; }
    /**
     * Set the display name (caller in ingoing)
     * not protected by mutex (when created)
     * @return std::string The peer display name
     */
    void setPeerDisplayName(const std::string& name) { peerDisplayName_ = name; }

    /**
     * Get "To" from the invite
     * @note Used to make the difference between incoming calls for accounts and for conversations
     * @return the "To" that was present in the invite
     */
    const std::string& toUsername() const { return toUsername_; }
    /**
     * Updated by sipvoiplink, corresponds to the "To" in the invite
     * @param username      "To"
     */
    void toUsername(const std::string& username) { toUsername_ = username; }

    /**
     * Get the peer display name (caller in ingoing)
     * not protected by mutex (when created)
     * @return std::string The peer name
     */
    const std::string& getPeerDisplayName() const { return peerDisplayName_; }
    /**
     * Tell if the call is incoming
     * @return true if yes false otherwise
     */
    bool isIncoming() const { return type_ == CallType::INCOMING; }

    /**
     * Set the state of the call (protected by mutex)
     * @param call_state The call state
     * @param cnx_state The call connection state
     * @param code Optional error-dependent code (used to report more information)
     * @return true if the requested state change was valid, false otherwise
     */
    bool setState(CallState call_state, signed code = 0);
    bool setState(CallState call_state, ConnectionState cnx_state, signed code = 0);
    bool setState(ConnectionState cnx_state, signed code = 0);

    /**
     * Get the call state of the call (protected by mutex)
     * @return CallState  The call state
     */
    CallState getState() const;

    /**
     * Get the connection state of the call (protected by mutex)
     * @return ConnectionState The connection state
     */
    ConnectionState getConnectionState() const;

    std::string getStateStr() const;

    void setIPToIP(bool IPToIP) { isIPToIP_ = IPToIP; }

    virtual std::map<std::string, std::string> getDetails() const;

    /**
     * Answer the call
     */
    virtual void answer() = 0;

    /**
     * Answer a call with a list of media attributes.
     * @param mediaList The list of the media attributes.
     * The media attributes set by the caller of this method will
     * determine the response sent to the peer and the configuration
     * of the local media.
     * If the media list is empty, the current media set when the call
     * was created will be used.
     */
    virtual void answer(const std::vector<libjami::MediaMap>& mediaList) = 0;

    /**
     * Check the media of an incoming media change request.
     * This method checks the new media against the current media. It
     * determines if the differences are significant enough to require
     * more processing.
     * For instance, this can be used to check if the a change request
     * must be reported to the client for confirmation or can be handled
     * by the daemon.
     * The conditions that cause this method to return true are implementation
     * specific.
     *
     * @param the new media list from the remote
     * @return true if the new media differs from the current media
     **/
    virtual bool checkMediaChangeRequest(const std::vector<libjami::MediaMap>& remoteMediaList) = 0;

    /**
     * Process incoming media change request.
     *
     * @param the new media list from the remote
     */
    virtual void handleMediaChangeRequest(const std::vector<libjami::MediaMap>& remoteMediaList) = 0;

    /**
     * Answer to a media update request.
     * The media attributes set by the caller of this method will
     * determine the response to send to the peer and the configuration
     * of the local media.
     * @param mediaList The list of media attributes. An empty media
     * list means the media update request was not accepted, meaning the
     * call continue with the current media. It's up to the implementation
     * to determine wether an answer will be sent to the peer.
     * @param isRemote      True if the media list is from the remote peer
     */
    virtual void answerMediaChangeRequest(const std::vector<libjami::MediaMap>& mediaList,
                                          bool isRemote = false)
        = 0;
    /**
     * Hang up the call
     * @param reason
     */
    virtual void hangup(int reason) = 0;

    /**
     * Refuse incoming call
     */
    virtual void refuse() = 0;

    /**
     * Transfer a call to specified URI
     * @param to The recipient of the call
     */
    virtual void transfer(const std::string& to) = 0;

    /**
     * Attended transfer
     * @param The target call id
     * @return True on success
     */
    virtual bool attendedTransfer(const std::string& to) = 0;

    /**
     * Put a call on hold
     * @param cb    On hold can be queued if waiting for ICE. This callback will be called when ready
     * @return bool True on success, False if failed or pending
     */
    virtual bool onhold(OnReadyCb&& cb) = 0;

    /**
     * Resume a call from hold state
     * @param cb    On hold can be queued if waiting for ICE. This callback will be called when ready
     * @return bool True on success, False if failed or pending
     */
    virtual bool offhold(OnReadyCb&& cb) = 0;

    virtual void sendKeyframe(int streamIdx = -1) = 0;

    /**
     * Check wether ICE is enabled for media
     */
    virtual bool isIceEnabled() const = 0;

    /**
     * Peer has hung up a call
     */
    virtual void peerHungup();

    virtual void removeCall();

    /**
     * Update recording state. Typically used to send notifications
     * to peers about the local recording session state
     */
    virtual void updateRecState(bool state) = 0;

    void addStateListener(StateListenerCb&& listener)
    {
        std::lock_guard lk {callMutex_};
        stateChangedListeners_.emplace_back(std::move(listener));
    }

    /**
     * Attach subcall to this instance.
     * If this subcall is answered, this subcall and this instance will be merged using merge().
     */
    void addSubCall(Call& call);

    ///
    /// Return true if this call instance is a subcall (internal call for multi-device handling)
    ///
    bool isSubcall() const
    {
        std::lock_guard lk {callMutex_};
        return parent_ != nullptr;
    }

    /**
     * @return Call duration in milliseconds
     */
    std::chrono::milliseconds getCallDuration() const
    {
        return duration_start_ == time_point::min()
                   ? std::chrono::milliseconds::zero()
                   : std::chrono::duration_cast<std::chrono::milliseconds>(clock::now()
                                                                           - duration_start_);
    }

    // media management
    virtual bool toggleRecording();

    virtual std::vector<MediaAttribute> getMediaAttributeList() const = 0;

    virtual std::map<std::string, bool> getAudioStreams() const = 0;

#ifdef ENABLE_VIDEO
    virtual void createSinks(ConfInfo& infos) = 0;
#endif

    virtual void switchInput(const std::string& = {}) {};

    /**
     * mute/unmute a media of a call
     * @param mediaType type of media
     * @param isMuted true for muting, false for unmuting
     */
    virtual void muteMedia(const std::string& mediaType, bool isMuted) = 0;

    /**
     * Send DTMF
     * @param code  The char code
     */
    virtual void carryingDTMFdigits(char code) = 0;

    /**
     * Make a change request of the current media with the provided media
     * @param mediaList the new media list
     * @return true on success
     */
    virtual bool requestMediaChange(const std::vector<libjami::MediaMap>& mediaList) = 0;

    /**
     * Retrieve current medias list
     * @return current medias
     */
    virtual std::vector<libjami::MediaMap> currentMediaList() const = 0;

    /**
     * Send a message to a call identified by its callid
     *
     * @param A list of mimetype/payload pairs
     * @param The sender of this message (could be another participant of a conference)
     */
    virtual void sendTextMessage(const std::map<std::string, std::string>& messages,
                                 const std::string& from)
        = 0;

    void onTextMessage(std::map<std::string, std::string>&& messages);

    virtual std::shared_ptr<SystemCodecInfo> getAudioCodec() const
    {
        return {};
    }
    virtual std::shared_ptr<SystemCodecInfo> getVideoCodec() const
    {
        return {};
    }

    virtual void restartMediaSender() = 0;

    // Media status methods
    virtual bool hasVideo() const = 0;
    virtual bool isCaptureDeviceMuted(const MediaType& mediaType) const = 0;

    /**
     * A Call can be in a conference. If this is the case, the other side
     * will send conference information describing the rendered image
     * @msg     A JSON object describing the conference
     */
    void setConferenceInfo(const std::string& msg);

    virtual void enterConference(std::shared_ptr<Conference> conference) = 0;
    virtual void exitConference() = 0;

    std::vector<std::map<std::string, std::string>> getConferenceInfos() const
    {
        return confInfo_.toVectorMapStringString();
    }

    std::unique_ptr<AudioDeviceGuard> audioGuard;
    void sendConfOrder(const Json::Value& root);
    void sendConfInfo(const std::string& json);
    void resetConfInfo();

    virtual void monitor() const = 0;

    int conferenceProtocolVersion() const
    {
        return peerConfProtocol_;
    }

protected:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    virtual void merge(Call& scall);

    /**
     * Constructor of a call
     * @param id Unique identifier of the call
     * @param type set definitely this call as incoming/outgoing
     * @param details volatile details to customize the call creation
     */
    Call(const std::shared_ptr<Account>& account,
         const std::string& id,
         Call::CallType type,
         const std::map<std::string, std::string>& details = {});

    // TODO all these members are not protected against multi-thread access

    const std::string id_ {};

    ///< MultiDevice: parent call, nullptr otherwise. Access protected by callMutex_.
    mutable std::shared_ptr<Call> parent_;

    ///< MultiDevice: list of attached subcall
    SubcallSet subcalls_;

    using MsgList = std::list<std::pair<std::map<std::string, std::string>, std::string>>;

    ///< MultiDevice: message waiting to be sent (need a valid subcall)
    MsgList pendingOutMessages_;

    /** Protect every attribute that can be changed by two threads */
    mutable std::recursive_mutex callMutex_ {};

    mutable std::mutex confInfoMutex_ {};
    mutable ConfInfo confInfo_ {};
    time_point duration_start_ {time_point::min()};

private:
    bool validStateTransition(CallState newState);

    void checkPendingIM();

    void checkAudio();

    void subcallStateChanged(Call&, Call::CallState, Call::ConnectionState);

    SubcallSet safePopSubcalls();

    std::vector<StateListenerCb> stateChangedListeners_ {};

protected:
    /** Unique conference ID, used exclusively in case of a conference */
    std::weak_ptr<Conference> conf_ {};

    /** Type of the call */
    CallType type_;

    /** Associate account ID */
    std::weak_ptr<Account> account_;

    /** Disconnected/Progressing/Trying/Ringing/Connected */
    ConnectionState connectionState_ {ConnectionState::DISCONNECTED};

    /** Inactive/Active/Hold/Busy/Error */
    CallState callState_ {CallState::INACTIVE};

    std::string reason_ {};

    /** Direct IP-to-IP or classic call */
    bool isIPToIP_ {false};

    /** Number of the peer */
    std::string peerNumber_ {};

    /** Peer Display Name */
    std::string peerDisplayName_ {};

    time_t timestamp_start_ {0};

    ///< MultiDevice: message received by subcall to merged yet
    MsgList pendingInMessages_;

    /// Supported conference protocol version
    int peerConfProtocol_ {0};
    std::string toUsername_ {};
};

// Helpers

/**
 * Obtain a shared smart pointer of instance
 */
inline std::shared_ptr<Call>
getPtr(Call& call)
{
    return call.shared_from_this();
}

} // namespace jami
