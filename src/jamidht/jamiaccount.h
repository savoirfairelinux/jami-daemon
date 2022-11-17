/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Simon Désaulniers <simon.desaulniers@gmail.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "connectivity/security/tls_session.h"
#include "connectivity/security/diffie-hellman.h"
#include "sip/sipaccountbase.h"
#include "jami/datatransfer_interface.h"
#include "jamidht/conversation.h"
#include "connectivity/multiplexed_socket.h"
#include "data_transfer.h"
#include "uri.h"
#include "jamiaccount_config.h"
#include "connectivity/peer_connection.h"

#include "noncopyable.h"
#include "connectivity/ip_utils.h"
#include "ring_types.h" // enable_if_base_of
#include "connectivity/security/certstore.h"
#include "scheduled_executor.h"
#include "connectivity/connectionmanager.h"
#include "gitserver.h"
#include "channel_handler.h"
#include "conversation_module.h"
#include "sync_module.h"
#include "conversationrepository.h"

#include <opendht/dhtrunner.h>
#include <opendht/default_types.h>

#include "connectivity/upnp/protocol/mapping.h"

#include <pjsip/sip_types.h>

#include <chrono>
#include <future>
#include <json/json.h>
#include <list>
#include <map>
#include <optional>
#include <vector>

#if HAVE_RINGNS
#include "namedirectory.h"
#endif

namespace dev {
template<unsigned N>
class FixedHash;
using h160 = FixedHash<20>;
using Address = h160;
} // namespace dev

namespace jami {

class IceTransport;
struct Contact;
struct AccountArchive;
class DhtPeerConnector;
class AccountManager;
struct AccountInfo;
class SipTransport;
class ChanneledOutgoingTransfer;
class SyncModule;
struct TextMessageCtx;

using SipConnectionKey = std::pair<std::string /* accountId */, DeviceId>;

/**
 * @brief Ring Account is build on top of SIPAccountBase and uses DHT to handle call connectivity.
 */
class JamiAccount : public SIPAccountBase
{
public:
    constexpr static auto ACCOUNT_TYPE = ACCOUNT_TYPE_JAMI;
    constexpr static const std::pair<uint16_t, uint16_t> DHT_PORT_RANGE {4000, 8888};
    constexpr static int ICE_STREAMS_COUNT {1};
    constexpr static int ICE_COMP_COUNT_PER_STREAM {1};

    std::string_view getAccountType() const override { return ACCOUNT_TYPE; }

    std::shared_ptr<JamiAccount> shared()
    {
        return std::static_pointer_cast<JamiAccount>(shared_from_this());
    }
    std::shared_ptr<JamiAccount const> shared() const
    {
        return std::static_pointer_cast<JamiAccount const>(shared_from_this());
    }
    std::weak_ptr<JamiAccount> weak()
    {
        return std::static_pointer_cast<JamiAccount>(shared_from_this());
    }
    std::weak_ptr<JamiAccount const> weak() const
    {
        return std::static_pointer_cast<JamiAccount const>(shared_from_this());
    }

    const std::string& getPath() const { return idPath_; }

    const JamiAccountConfig& config() const
    {
        return *static_cast<const JamiAccountConfig*>(&Account::config());
    }

    void loadConfig() override;

    /**
     * Constructor
     * @param accountID The account identifier
     */
    JamiAccount(const std::string& accountId);

    ~JamiAccount() noexcept;

    /**
     * Retrieve volatile details such as recent registration errors
     * @return std::map< std::string, std::string > The account volatile details
     */
    virtual std::map<std::string, std::string> getVolatileAccountDetails() const override;

    std::unique_ptr<AccountConfig> buildConfig() const override
    {
        return std::make_unique<JamiAccountConfig>(getAccountID(), idPath_);
    }

    /**
     * Adds an account id to the list of accounts to track on the DHT for
     * buddy presence.
     *
     * @param buddy_id  The buddy id.
     */
    void trackBuddyPresence(const std::string& buddy_id, bool track);

    /**
     * Tells for each tracked account id if it has been seen online so far
     * in the last DeviceAnnouncement::TYPE.expiration minutes.
     *
     * @return map of buddy_uri to bool (online or not)
     */
    std::map<std::string, bool> getTrackedBuddyPresence() const;

    void setActiveCodecs(const std::vector<unsigned>& list) override;

    /**
     * Connect to the DHT.
     */
    void doRegister() override;

    /**
     * Disconnect from the DHT.
     */
    void doUnregister(std::function<void(bool)> cb = {}) override;

    /**
     * Set the registration state of the specified link
     * @param state The registration state of underlying VoIPLink
     */
    void setRegistrationState(RegistrationState state,
                              int detail_code = 0,
                              const std::string& detail_str = {}) override;

    /**
     * @return pj_str_t "From" uri based on account information.
     * From RFC3261: "The To header field first and foremost specifies the desired
     * logical" recipient of the request, or the address-of-record of the
     * user or resource that is the target of this request. [...]  As such, it is
     * very important that the From URI not contain IP addresses or the FQDN
     * of the host on which the UA is running, since these are not logical
     * names."
     */
    std::string getFromUri() const override;

    /**
     * This method adds the correct scheme, hostname and append
     * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
     * It is expected that "port" is present in the internal hostname_.
     *
     * @return pj_str_t "To" uri based on @param username
     * @param username A string formatted as : "username"
     */
    std::string getToUri(const std::string& username) const override;

    /**
     * In the current version, "srv" uri is obtained in the preformated
     * way: hostname:port. This method adds the correct scheme and append
     * the ;transport= parameter at the end of the uri, in accordance with RFC3261.
     *
     * @return pj_str_t "server" uri based on @param hostPort
     * @param hostPort A string formatted as : "hostname:port"
     */
    std::string getServerUri() const { return ""; };

    void setIsComposing(const std::string& conversationUri, bool isWriting) override;

    bool setMessageDisplayed(const std::string& conversationUri,
                             const std::string& messageId,
                             int status) override;

    /**
     * Get the contact header for
     * @return The contact header based on account information
     */
    std::string getContactHeader(const std::shared_ptr<SipTransport>& sipTransport);

    /* Returns true if the username and/or hostname match this account */
    MatchRank matches(std::string_view username, std::string_view hostname) const override;

    /**
     * Create outgoing SIPCall.
     * @note Accepts several urls:
     *          + jami:uri for calling someone
     *          + swarm:id for calling a group (will host or join if an active call is detected)
     *          + rdv:id/uri/device/confId to join a specific conference hosted on (uri, device)
     * @param[in] toUrl The address to call
     * @param[in] mediaList list of medias
     * @return A shared pointer on the created call.
     */
    std::shared_ptr<Call> newOutgoingCall(std::string_view toUrl,
                                          const std::vector<libjami::MediaMap>& mediaList) override;

    /**
     * Create incoming SIPCall.
     * @param[in] from The origin of the call
     * @param mediaList A list of media
     * @param sipTr: SIP Transport
     * @return A shared pointer on the created call.
     */
    std::shared_ptr<SIPCall> newIncomingCall(
        const std::string& from,
        const std::vector<libjami::MediaMap>& mediaList,
        const std::shared_ptr<SipTransport>& sipTr = {}) override;

    void onTextMessage(const std::string& id,
                       const std::string& from,
                       const std::string& deviceId,
                       const std::map<std::string, std::string>& payloads) override;

    virtual bool isTlsEnabled() const override { return true; }
    bool isSrtpEnabled() const override { return true; }

    virtual bool getSrtpFallback() const override { return false; }

    bool setCertificateStatus(const std::string& cert_id, tls::TrustStore::PermissionStatus status);
    std::vector<std::string> getCertificatesByStatus(tls::TrustStore::PermissionStatus status);

    bool findCertificate(const std::string& id);
    bool findCertificate(
        const dht::InfoHash& h,
        std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {});
    bool findCertificate(
        const dht::PkId& h,
        std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {});

    /* contact requests */
    std::vector<std::map<std::string, std::string>> getTrustRequests() const;
    // Note: includeConversation used for compatibility test. Do not change
    bool acceptTrustRequest(const std::string& from, bool includeConversation = true);
    bool discardTrustRequest(const std::string& from);
    void declineConversationRequest(const std::string& conversationId);

    /**
     * Add contact to the account contact list.
     * Set confirmed if we know the contact also added us.
     */
    void addContact(const std::string& uri, bool confirmed = false);
    void removeContact(const std::string& uri, bool banned = true);
    std::vector<std::map<std::string, std::string>> getContacts() const;

    /**
     * Replace in contact's details related conversation
     * @param uri           Of the contact
     * @param oldConv       Current conversation
     * @param newConv
     * @return if replaced
     */
    bool updateConvForContact(const std::string& uri,
                              const std::string& oldConv,
                              const std::string& newConv);

    ///
    /// Obtain details about one account contact in serializable form.
    ///
    std::map<std::string, std::string> getContactDetails(const std::string& uri) const;

    void sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload);
    void sendMessage(const std::string& to,
                     const std::map<std::string, std::string>& payloads,
                     uint64_t id,
                     bool retryOnTimeout = true,
                     bool onlyConnected = false) override;
    uint64_t sendTextMessage(const std::string& to,
                             const std::map<std::string, std::string>& payloads,
                             uint64_t refreshToken = 0,
                             bool onlyConnected = false) override;
    void sendInstantMessage(const std::string& convId,
                            const std::map<std::string, std::string>& msg);
    void onIsComposing(const std::string& conversationId, const std::string& peer, bool isWriting);

    /* Devices */
    void addDevice(const std::string& password);
    /**
     * Export the archive to a file
     * @param destinationPath
     * @param (optional) password, if not provided, will update the contacts only if the archive
     * doesn't have a password
     * @return if the archive was exported
     */
    bool exportArchive(const std::string& destinationPath, const std::string& password = {});
    bool revokeDevice(const std::string& password, const std::string& device);
    std::map<std::string, std::string> getKnownDevices() const;

    bool isPasswordValid(const std::string& password);

    bool changeArchivePassword(const std::string& password_old, const std::string& password_new);

    void connectivityChanged() override;

    // overloaded methods
    void flush() override;

#if HAVE_RINGNS
    void lookupName(const std::string& name);
    void lookupAddress(const std::string& address);
    void registerName(const std::string& password, const std::string& name);
#endif
    bool searchUser(const std::string& nameQuery);

    /// \return true if the given DHT message identifier has been treated
    /// \note if message has not been treated yet this method store this id and returns true at
    /// further calls
    bool isMessageTreated(std::string_view id);

    std::shared_ptr<dht::DhtRunner> dht()
    {
        return dht_;
    }

    const dht::crypto::Identity& identity() const
    {
        return id_;
    }

    const std::shared_future<tls::DhParams> dhParams() const
    {
        return dhParams_;
    }

    void forEachDevice(const dht::InfoHash& to,
                       std::function<void(const std::shared_ptr<dht::crypto::PublicKey>&)>&& op,
                       std::function<void(bool)>&& end = {});

    bool setPushNotificationToken(const std::string& pushDeviceToken = "") override;
    bool setPushNotificationTopic(const std::string& topic) override;
    bool setPushNotificationConfig(const std::map<std::string, std::string>& data) override;

    /**
     * To be called by clients with relevant data when a push notification is received.
     */
    void pushNotificationReceived(const std::string& from,
                                  const std::map<std::string, std::string>& data);

    std::string getUserUri() const override;

    /**
     * Get last messages (should be used to retrieve messages when launching the client)
     * @param base_timestamp
     */
    std::vector<libjami::Message> getLastMessages(const uint64_t& base_timestamp) override;

    /**
     * Start Publish the Jami Account onto the Network
     */
    void startAccountPublish();

    /**
     * Start Discovery the Jami Account from the Network
     */
    void startAccountDiscovery();

    void saveConfig() const override;

    inline void editConfig(std::function<void(JamiAccountConfig& conf)>&& edit)
    {
        Account::editConfig(
            [&](AccountConfig& conf) { edit(*static_cast<JamiAccountConfig*>(&conf)); });
    }

    /**
     * Get current discovered peers account id and display name
     */
    std::map<std::string, std::string> getNearbyPeers() const override;

    /**
     * Store the local/public addresses used to register
     */
    void storeActiveIpAddress(std::function<void()>&& cb = {});

    /**
     * Create and return ICE options.
     */
    void getIceOptions(std::function<void(IceTransportOptions&&)> cb) noexcept;

#ifdef LIBJAMI_TESTABLE
    ConnectionManager& connectionManager()
    {
        return *connectionManager_;
    }

    /**
     * Only used for tests, disable sha3sum verification for transfers.
     * @param newValue
     */
    void noSha3sumVerification(bool newValue)
    {
        noSha3sumVerification_ = newValue;
    }
#endif

    /**
     * This should be called before flushing the account.
     * ConnectionManager needs the account to exists
     */
    void shutdownConnections();

    std::string_view currentDeviceId() const;

    // Member management
    void saveMembers(const std::string& convId,
                     const std::vector<std::string>& members); // Save confInfos

    // Received a new commit notification

    bool handleMessage(const std::string& from,
                       const std::pair<std::string, std::string>& message) override;

    void monitor() const;

    // File transfer
    void sendFile(const std::string& conversationId,
                  const std::string& path,
                  const std::string& name,
                  const std::string& replyTo);
    // non-swarm version
    libjami::DataTransferId sendFile(const std::string& peer,
                                     const std::string& path,
                                     const InternalCompletionCb& icb = {});

    void transferFile(const std::string& conversationId,
                      const std::string& path,
                      const std::string& deviceId,
                      const std::string& fileId,
                      const std::string& interactionId,
                      size_t start = 0,
                      size_t end = 0);

    void askForFileChannel(const std::string& conversationId,
                           const std::string& deviceId,
                           const std::string& interactionId,
                           const std::string& fileId,
                           size_t start = 0,
                           size_t end = 0);

    void askForProfile(const std::string& conversationId,
                       const std::string& deviceId,
                       const std::string& memberUri);

    /**
     * Retrieve linked transfer manager
     * @param id    conversationId or empty for fallback
     * @return linked transfer manager
     */
    std::shared_ptr<TransferManager> dataTransfer(const std::string& id = "");

    ConversationModule* convModule();
    SyncModule* syncModule();

    /**
     * Check (via the cache) if we need to send our profile to a specific device
     * @param peerUri       Uri that will receive the profile
     * @param deviceId      Device that will receive the profile
     */
    // Note: when swarm will be merged, this can be moved in transferManager
    bool needToSendProfile(const std::string& peerUri, const std::string& deviceId);
    /**
     * Send Profile via cached SIP connection
     * @param convId        Conversation's identifier (can be empty for self profile on sync)
     * @param peerUri       Uri that will receive the profile
     * @param deviceId      Device that will receive the profile
     */
    void sendProfile(const std::string& convId, const std::string& peerUri, const std::string& deviceId);
    /**
     * Send profile via cached SIP connection
     * @param peerUri       Uri that will receive the profile
     * @param deviceId      Device that will receive the profile
     */
    void sendProfile(const std::string& peerUri, const std::string& deviceId);
    /**
     * Clear sent profiles (because of a removed contact or new trust request)
     * @param peerUri       Uri used to clear cache
     */
    void clearProfileCache(const std::string& peerUri);

    std::string profilePath() const;

    AccountManager* accountManager()
    {
        return accountManager_.get();
    }

    bool sha3SumVerify() const
    {
        return !noSha3sumVerification_;
    }

    /**
     * Change certificate's validity period
     * @param pwd       Password for the archive
     * @param id        Certificate to update ({} for updating the whole chain)
     * @param validity  New validity
     * @note forceReloadAccount may be necessary to retrigger the migration
     */
    bool setValidity(const std::string& pwd, const dht::InfoHash& id, int64_t validity);
    /**
     * Try to reload the account to force the identity to be updated
     */
    void forceReloadAccount();

    /**
     * Make sure appdata/contacts.yml contains correct informations
     * @param removedConv   The current removed conversations
     */
    void unlinkConversations(const std::set<std::string>& removedConv);

    bool isValidAccountDevice(const dht::crypto::Certificate& cert) const;

    /**
     * Join incoming call to hosted conference
     * @param callId        The call to join
     * @param destination   conversation/uri/device/confId to join
     */
    void handleIncomingConversationCall(const std::string& callId, const std::string& destination);

    /**
     * The DRT component is composed on some special nodes, that are usually present but not connected.
     * This kind of node corresponds to devices with push notifications & proxy and are
     * stored in the mobile nodes
     */
    bool isMobile() const {
        return config().proxyEnabled and not config().deviceKey.empty();
    }

private:
    NON_COPYABLE(JamiAccount);

    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;

    /**
     * Private structures
     */
    struct PendingCall;
    struct PendingMessage;
    struct BuddyInfo;
    struct DiscoveredPeer;

    inline std::string getProxyConfigKey() const
    {
        const auto& conf = config();
        return dht::InfoHash::get(conf.proxyServer + conf.proxyListUrl).toString();
    }

    /**
     * Compute archive encryption key and DHT storage location from password and PIN.
     */
    static std::pair<std::vector<uint8_t>, dht::InfoHash> computeKeys(const std::string& password,
                                                                      const std::string& pin,
                                                                      bool previous = false);

    void trackPresence(const dht::InfoHash& h, BuddyInfo& buddy);

    void doRegister_();

    const dht::ValueType USER_PROFILE_TYPE = {9, "User profile", std::chrono::hours(24 * 7)};

    void startOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string& toUri);

    void onConnectedOutgoingCall(const std::shared_ptr<SIPCall>& call,
                                 const std::string& to_id,
                                 IpAddr target);

    /**
     * Start a SIP Call
     * @param call  The current call
     * @return true if all is correct
     */
    bool SIPStartCall(SIPCall& call, const IpAddr& target);

    /**
     * Update tracking info when buddy appears offline.
     */
    void onTrackedBuddyOffline(const dht::InfoHash&);

    /**
     * Update tracking info when buddy appears offline.
     */
    void onTrackedBuddyOnline(const dht::InfoHash&);

    /**
     * Maps require port via UPnP and other async ops
     */
    void registerAsyncOps();
    /**
     * Add port mapping callback function.
     */
    void onPortMappingAdded(uint16_t port_used, bool success);
    void forEachPendingCall(const DeviceId& deviceId,
                            const std::function<void(const std::shared_ptr<SIPCall>&)>& cb);

    void loadAccountFromFile(const std::string& archive_path, const std::string& archive_password);
    void loadAccountFromDHT(const std::string& archive_password, const std::string& archive_pin);
    void loadAccountFromArchive(AccountArchive&& archive, const std::string& archive_password);
    void loadAccount(const std::string& archive_password = {},
                     const std::string& archive_pin = {},
                     const std::string& archive_path = {});

    std::vector<std::string> loadBootstrap() const;

    static std::pair<std::string, std::string> saveIdentity(const dht::crypto::Identity id,
                                                            const std::string& path,
                                                            const std::string& name);

    void loadTreatedMessages();
    void saveTreatedMessages() const;

    void replyToIncomingIceMsg(const std::shared_ptr<SIPCall>&,
                               const std::shared_ptr<IceTransport>&,
                               const std::shared_ptr<IceTransport>&,
                               const dht::IceCandidates&,
                               const std::shared_ptr<dht::crypto::Certificate>& from_cert,
                               const dht::InfoHash& from);

    static tls::DhParams loadDhParams(std::string path);

    void loadCachedUrl(const std::string& url,
                       const std::string& cachePath,
                       const std::chrono::seconds& cacheDuration,
                       std::function<void(const dht::http::Response& response)>);

    std::string getDhtProxyServer(const std::string& serverList);
    void loadCachedProxyServer(std::function<void(const std::string&)> cb);

    /**
     * The TLS settings, used only if tls is chosen as a sip transport.
     */
    void generateDhParams();

    template<class... Args>
    std::shared_ptr<IceTransport> createIceTransport(const Args&... args);
    void newOutgoingCallHelper(const std::shared_ptr<SIPCall>& call, const Uri& uri);
    void newSwarmOutgoingCallHelper(const std::shared_ptr<SIPCall>& call, const Uri& uri);
    std::shared_ptr<SIPCall> createSubCall(const std::shared_ptr<SIPCall>& mainCall);

#if HAVE_RINGNS
    std::string registeredName_;
#endif
    std::shared_ptr<dht::Logger> logger_;

    std::shared_ptr<dht::DhtRunner> dht_ {};
    std::unique_ptr<AccountManager> accountManager_;
    dht::crypto::Identity id_ {};

    mutable std::mutex messageMutex_ {};
    std::map<dht::Value::Id, PendingMessage> sentMessages_;
    std::set<std::string, std::less<>> treatedMessages_ {};

    std::string idPath_ {};
    std::string cachePath_ {};
    std::string dataPath_ {};

    /* tracked buddies presence */
    mutable std::mutex buddyInfoMtx;
    std::map<dht::InfoHash, BuddyInfo> trackedBuddies_;

    mutable std::mutex dhtValuesMtx_;
    bool dhtPublicInCalls_ {true};

    std::string bootstrapListUrl_;

    /**
     * DHT port preference
     */
    in_port_t dhtDefaultPort_ {0};

    /**
     * DHT port actually used.
     * This holds the actual DHT port, which might different from the port
     * set in the configuration. This can be the case if UPnP is used.
     */
    in_port_t dhtPortUsed()
    {
        return (upnpCtrl_ and dhtUpnpMapping_.isValid()) ? dhtUpnpMapping_.getExternalPort()
                                                         : dhtDefaultPort_;
    }

    /* Current UPNP mapping */
    upnp::Mapping dhtUpnpMapping_ {upnp::PortType::UDP};

    /**
     * Proxy
     */
    std::string proxyServerCached_ {};

    std::mutex dhParamsMtx_ {};
    std::shared_future<tls::DhParams> dhParams_;
    std::condition_variable dhParamsCv_;

    /**
     * Optional: via_addr construct from received parameters
     */
    pjsip_host_port via_addr_ {};

    pjsip_transport* via_tp_ {nullptr};

    mutable std::mutex connManagerMtx_ {};
    std::unique_ptr<ConnectionManager> connectionManager_;

    std::mutex discoveryMapMtx_;
    std::shared_ptr<dht::PeerDiscovery> peerDiscovery_;
    std::map<dht::InfoHash, DiscoveredPeer> discoveredPeers_;
    std::map<std::string, std::string> discoveredPeerMap_;

    /**
     * Avoid to refresh the cache multiple times
     */
    std::atomic_bool isRefreshing_ {false};
    /**
     * This will cache the turn server resolution each time we launch
     * Jami, or for each connectivityChange()
     */
    // TODO move in separate class
    void testTurn(IpAddr server);
    void cacheTurnServers();
    std::unique_ptr<TurnTransport> testTurnV4_;
    std::unique_ptr<TurnTransport> testTurnV6_;
    void refreshTurnDelay(bool scheduleNext);

    std::chrono::seconds turnRefreshDelay_ {std::chrono::seconds(10)};

    std::set<std::shared_ptr<dht::http::Request>> requests_;

    std::mutex sipConnsMtx_ {};
    struct SipConnection
    {
        std::shared_ptr<SipTransport> transport;
        // Needs to keep track of that channel to access underlying ICE
        // informations, as the SipTransport use a generic transport
        std::shared_ptr<ChannelSocket> channel;
    };
    // NOTE: here we use a vector to avoid race conditions. In fact the contact
    // can ask for a SIP channel when we are creating a new SIP Channel with this
    // peer too.
    std::map<SipConnectionKey, std::vector<SipConnection>> sipConns_;

    std::mutex pendingCallsMutex_;
    std::map<DeviceId, std::vector<std::shared_ptr<SIPCall>>> pendingCalls_;

    std::mutex onConnectionClosedMtx_ {};
    std::map<DeviceId, std::function<void(const DeviceId&, bool)>> onConnectionClosed_ {};
    /**
     * onConnectionClosed contains callbacks that need to be called if a sub call is failing
     * @param deviceId      The device we are calling
     * @param eraseDummy    Erase the dummy call (a temporary subcall that must be stop when we will
     * not create new subcalls)
     */
    void callConnectionClosed(const DeviceId& deviceId, bool eraseDummy);

    /**
     * Ask a device to open a channeled SIP socket
     * @param peerId             The contact who owns the device
     * @param deviceId           The device to ask
     * @param forceNewConnection If we want a new SIP connection
     * @param pc                 A pending call to stop if the request fails
     * @note triggers cacheSIPConnection
     */
    void requestSIPConnection(const std::string& peerId,
                              const DeviceId& deviceId,
                              const std::string& connectionType,
                              bool forceNewConnection = false,
                              const std::shared_ptr<SIPCall>& pc = {});
    /**
     * Store a new SIP connection into sipConnections_
     * @param channel   The new sip channel
     * @param peerId    The contact who owns the device
     * @param deviceId  Device linked to that transport
     */
    void cacheSIPConnection(std::shared_ptr<ChannelSocket>&& channel,
                            const std::string& peerId,
                            const DeviceId& deviceId);
    /**
     * Shutdown a SIP connection
     * @param channel   The channel to close
     * @param peerId    The contact who owns the device
     * @param deviceId  Device linked to that transport
     */
    void shutdownSIPConnection(const std::shared_ptr<ChannelSocket>& channel,
                               const std::string& peerId,
                               const DeviceId& deviceId);

    // File transfers
    std::mutex transfersMtx_ {};
    std::set<std::string> incomingFileTransfers_ {};

    /**
     * Helper used to send SIP messages on a channeled connection
     * @param conn      The connection used
     * @param to        Peer URI
     * @param ctx       Context passed to the send request
     * @param token     Token used
     * @param data      Message to send
     * @param cb        Callback to trigger when message is sent
     * @throw runtime_error if connection is invalid
     * @return if the request will be sent
     */
    bool sendSIPMessage(SipConnection& conn,
                        const std::string& to,
                        void* ctx,
                        uint64_t token,
                        const std::map<std::string, std::string>& data,
                        pjsip_endpt_send_callback cb);
    void onSIPMessageSent(const std::shared_ptr<TextMessageCtx>& ctx, int code);

    std::mutex gitServersMtx_ {};
    std::map<dht::Value::Id, std::unique_ptr<GitServer>> gitServers_ {};

    //// File transfer (for profiles)
    std::shared_ptr<TransferManager> nonSwarmTransferManager_;

    std::atomic_bool deviceAnnounced_ {false};

    bool noSha3sumVerification_ {false};

    std::map<Uri::Scheme, std::unique_ptr<ChannelHandlerInterface>> channelHandlers_ {};

    std::unique_ptr<ConversationModule> convModule_;
    std::mutex moduleMtx_;
    std::unique_ptr<SyncModule> syncModule_;

    void initConnectionManager();
};

static inline std::ostream&
operator<<(std::ostream& os, const JamiAccount& acc)
{
    os << "[Account " << acc.getAccountID() << "] ";
    return os;
}

} // namespace jami
