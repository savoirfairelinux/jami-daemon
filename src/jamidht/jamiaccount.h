/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "security/tls_session.h"
#include "security/diffie-hellman.h"
#include "sip/sipaccountbase.h"
#include "jami/datatransfer_interface.h"
#include "jamidht/conversation.h"
#include "multiplexed_socket.h"
#include "data_transfer.h"
#include "uri.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "ring_types.h" // enable_if_base_of
#include "security/certstore.h"
#include "scheduled_executor.h"
#include "connectionmanager.h"
#include "gitserver.h"
#include "channel_handler.h"
#include "conversation_module.h"
#include "sync_module.h"
#include "conversationrepository.h"

#include <opendht/dhtrunner.h>
#include <opendht/default_types.h>

#include "upnp/protocol/mapping.h"

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

namespace YAML {
class Node;
class Emitter;
} // namespace YAML

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
class ContactList;
class AccountManager;
struct AccountInfo;
class SipTransport;
class ChanneledOutgoingTransfer;
class SyncModule;

using SipConnectionKey = std::pair<std::string /* accountId */, DeviceId>;
using GitSocketList = std::map<DeviceId,                               /* device Id */
                               std::map<std::string,                   /* conversation */
                                        std::shared_ptr<ChannelSocket> /* related socket */
                                        >>;

/**
 * @brief Ring Account is build on top of SIPAccountBase and uses DHT to handle call connectivity.
 */
class JamiAccount : public SIPAccountBase
{
public:
    constexpr static const char* const ACCOUNT_TYPE = "RING";
    constexpr static const in_port_t DHT_DEFAULT_PORT = 4222;
    constexpr static const char* const DHT_DEFAULT_BOOTSTRAP = "bootstrap.jami.net";
    constexpr static const char* const DHT_DEFAULT_PROXY = "dhtproxy.jami.net:[80-95]";
    constexpr static const char* const DHT_DEFAULT_BOOTSTRAP_LIST_URL
        = "https://config.jami.net/boostrapList";
    constexpr static const char* const DHT_DEFAULT_PROXY_LIST_URL
        = "https://config.jami.net/proxyList";

    /* constexpr */ static const std::pair<uint16_t, uint16_t> DHT_PORT_RANGE;
    constexpr static int ICE_STREAMS_COUNT {1};
    constexpr static int ICE_COMP_COUNT_PER_STREAM {1};

    const char* getAccountType() const override { return ACCOUNT_TYPE; }

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

    /**
     * Constructor
     * @param accountID The account identifier
     */
    JamiAccount(const std::string& accountID, bool presenceEnabled);

    ~JamiAccount() noexcept;

    /**
     * Serialize internal state of this account for configuration
     * @param YamlEmitter the configuration engine which generate the configuration file
     */
    virtual void serialize(YAML::Emitter& out) const override;

    /**
     * Populate the internal state for this account based on info stored in the configuration file
     * @param The configuration node for this account
     */
    virtual void unserialize(const YAML::Node& node) override;

    /**
     * Return an map containing the internal state of this account. Client application can use this
     * method to manage account info.
     * @return A map containing the account information.
     */
    virtual std::map<std::string, std::string> getAccountDetails() const override;

    /**
     * Retrieve volatile details such as recent registration errors
     * @return std::map< std::string, std::string > The account volatile details
     */
    virtual std::map<std::string, std::string> getVolatileAccountDetails() const override;

    /**
     * Actually useless, since config loading is done in init()
     */
    void loadConfig() override {}

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
                              unsigned detail_code = 0,
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
    std::string getContactHeader(SipTransport* transport = nullptr) override;

    /* Returns true if the username and/or hostname match this account */
    MatchRank matches(std::string_view username, std::string_view hostname) const override;

    /**
     * Create outgoing SIPCall.
     * @param[in] toUrl The address to call
     * @return std::shared_ptr<T> A shared pointer on the created call.
     *      The type of this instance is given in template argument.
     *      This type can be any base class of SIPCall class (included).
     */
    std::shared_ptr<Call> newOutgoingCall(
        std::string_view toUrl,
        const std::map<std::string, std::string>& volatileCallDetails = {}) override;

    /**
     * Create outgoing SIPCall.
     * @param[in] toUrl The address to call
     * @param[in] mediaList list of medias
     * @return A shared pointer on the created call.
     */
    std::shared_ptr<Call> newOutgoingCall(std::string_view toUrl,
                                          const std::vector<DRing::MediaMap>& mediaList) override;

    /**
     * Create incoming SIPCall.
     * @param[in] from The origin of the call
     * @param mediaList A list of media
     * @param sipTr: SIP Transport
     * @return A shared pointer on the created call.
     */
    std::shared_ptr<SIPCall> newIncomingCall(
        const std::string& from,
        const std::vector<DRing::MediaMap>& mediaList,
        const std::shared_ptr<SipTransport>& sipTr = {}) override;

    void onTextMessage(const std::string& id,
                       const std::string& from,
                       const std::string& deviceId,
                       const std::map<std::string, std::string>& payloads) override;

    virtual bool isTlsEnabled() const override { return true; }

    bool isSrtpEnabled() const override { return true; }
    KeyExchangeProtocol getSrtpKeyExchange() const { return KeyExchangeProtocol::SDES; }

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
    void sendTextMessage(const std::string& to,
                         const std::map<std::string, std::string>& payloads,
                         uint64_t id,
                         bool retryOnTimeout = true,
                         bool onlyConnected = false) override;
    uint64_t sendTextMessage(const std::string& to,
                             const std::map<std::string, std::string>& payloads) override;
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

    /**
     * Send a E2E connection request to a given peer for the given transfer id
     * @param peer RingID on request's recipient
     * @param tid linked outgoing data transfer
     * @param isVcard if transfer is a vcard transfer
     * @param channeledConnectedCb callback when channel is connected
     * @param onChanneledCancelled callback when channel is canceled
     */
    void requestConnection(
        const DRing::DataTransferInfo& info,
        const DRing::DataTransferId& tid,
        bool isVCard,
        const std::function<void(const std::shared_ptr<ChanneledOutgoingTransfer>&)>&
            channeledConnectedCb,
        const std::function<void(const std::string&)>& onChanneledCancelled);

    ///
    /// Close a E2E connection between a given peer and a given transfer id.
    ///
    /// /// \param[in] peer RingID on request's recipient
    /// /// \param[in] tid linked outgoing data transfer
    ///
    void closePeerConnection(const DRing::DataTransferId& tid);

    /// \return true if the given DHT message identifier has been treated
    /// \note if message has not been treated yet this method store this id and returns true at
    /// further calls
    bool isMessageTreated(std::string_view id);

    std::shared_ptr<dht::DhtRunner> dht() { return dht_; }

    const dht::crypto::Identity& identity() const { return id_; }

    const std::shared_future<tls::DhParams> dhParams() const { return dhParams_; }

    void forEachDevice(const dht::InfoHash& to,
                       std::function<void(const std::shared_ptr<dht::crypto::PublicKey>&)>&& op,
                       std::function<void(bool)>&& end = {});

    /**
     * Start or stop to use the proxy client
     * @param address of the proxy
     * @param deviceKey the device key for push notifications (empty to not use it)
     */
    void enableProxyClient(bool enable);

    void setPushNotificationToken(const std::string& pushDeviceToken = "") override;

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
    std::vector<DRing::Message> getLastMessages(const uint64_t& base_timestamp) override;

    /**
     * Start Publish the Jami Account onto the Network
     */
    void startAccountPublish();

    /**
     * Start Discovery the Jami Account from the Network
     */
    void startAccountDiscovery();

    void saveConfig() const;

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

#ifdef DRING_TESTABLE
    ConnectionManager& connectionManager() { return *connectionManager_; }

    /**
     * Only used for tests, disable sha3sum verification for transfers.
     * @param newValue
     */
    void noSha3sumVerification(bool newValue) { noSha3sumVerification_ = newValue; }
#endif

    /**
     * This should be called before flushing the account.
     * ConnectionManager needs the account to exists
     */
    void shutdownConnections();
    std::optional<std::weak_ptr<ChannelSocket>> gitSocket(const DeviceId& deviceId,
                                                          const std::string& conversationId) const
    {
        auto deviceSockets = gitSocketList_.find(deviceId);
        if (deviceSockets == gitSocketList_.end()) {
            return std::nullopt;
        }
        auto socketIt = deviceSockets->second.find(conversationId);
        if (socketIt == deviceSockets->second.end()) {
            return std::nullopt;
        }
        return socketIt->second;
    }
    bool hasGitSocket(const DeviceId& deviceId, const std::string& conversationId) const
    {
        return gitSocket(deviceId, conversationId) != std::nullopt;
    }

    void addGitSocket(const DeviceId& deviceId,
                      const std::string& conversationId,
                      const std::shared_ptr<ChannelSocket>& socket)
    {
        auto& deviceSockets = gitSocketList_[deviceId];
        deviceSockets[conversationId] = socket;
    }

    void removeGitSocket(const DeviceId& deviceId, const std::string& conversationId)
    {
        auto deviceSockets = gitSocketList_.find(deviceId);
        if (deviceSockets == gitSocketList_.end()) {
            return;
        }
        deviceSockets->second.erase(conversationId);
        if (deviceSockets->second.empty()) {
            gitSocketList_.erase(deviceSockets);
        }
    }

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
                  const std::string& parent);

    // non-swarm version
    DRing::DataTransferId sendFile(const std::string& peer,
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
     * Send Profile via cached SIP connection
     * @param deviceId      Device that will receive the profile
     */
    // Note: when swarm will be merged, this can be moved in transferManager
    bool needToSendProfile(const std::string& deviceId);
    /**
     * Send profile via cached SIP connection
     * @param deviceId      Device that will receive the profile
     */
    void sendProfile(const std::string& deviceId);

    std::string profilePath() const;

    AccountManager* accountManager() { return accountManager_.get(); }

    bool sha3SumVerify() const { return !noSha3sumVerification_; }

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
     * Set the internal state for this account, mainly used to manage account details from the
     * client application.
     * @param The map containing the account information.
     */
    virtual void setAccountDetails(const std::map<std::string, std::string>& details) override;

    /**
     * Start a SIP Call
     * @param call  The current call
     * @return true if all is correct
     */
    bool SIPStartCall(SIPCall& call, const IpAddr& target);

    /**
     * For a call with (from_device, from_account), check the peer certificate chain (cert_list,
     * cert_num) with session check status. Put deserialized certificate to cert_out;
     */
    pj_status_t checkPeerTlsCertificate(dht::InfoHash from_device,
                                        dht::InfoHash from_account,
                                        unsigned status,
                                        const gnutls_datum_t* cert_list,
                                        unsigned cert_num,
                                        std::shared_ptr<dht::crypto::Certificate>& cert_out);

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

    void loadAccount(const std::string& archive_password = {},
                     const std::string& archive_pin = {},
                     const std::string& archive_path = {});
    void loadAccountFromFile(const std::string& archive_path, const std::string& archive_password);
    void loadAccountFromDHT(const std::string& archive_password, const std::string& archive_pin);
    void loadAccountFromArchive(AccountArchive&& archive, const std::string& archive_password);

    bool hasCertificate() const;
    bool hasPrivateKey() const;

    std::string makeReceipt(const dht::crypto::Identity& id);
    void createRingDevice(const dht::crypto::Identity& id);
    void initRingDevice(const AccountArchive& a);
    void migrateAccount(const std::string& pwd, dht::crypto::Identity& device);
    static bool updateCertificates(AccountArchive& archive, dht::crypto::Identity& device);

    void createAccount(const std::string& archive_password, dht::crypto::Identity&& migrate);
    void updateArchive(AccountArchive& content) const;
    void saveArchive(AccountArchive& content, const std::string& pwd);
    AccountArchive readArchive(const std::string& pwd) const;
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
    void newOutgoingCallHelper(const std::shared_ptr<SIPCall>& call, std::string_view toUri);
    std::shared_ptr<SIPCall> createSubCall(const std::shared_ptr<SIPCall>& mainCall);

#if HAVE_RINGNS
    std::string nameServer_;
    std::string registeredName_;
#endif
    std::shared_ptr<dht::Logger> logger_;

    std::shared_ptr<dht::DhtRunner> dht_ {};
    std::unique_ptr<AccountManager> accountManager_;
    dht::crypto::Identity id_ {};

    mutable std::mutex messageMutex_ {};
    std::map<dht::Value::Id, PendingMessage> sentMessages_;
    std::set<std::string, std::less<>> treatedMessages_ {};

    std::string deviceName_ {};
    std::string idPath_ {};
    std::string cachePath_ {};
    std::string dataPath_ {};

    std::string archivePath_ {};
    bool archiveHasPassword_ {true};

    std::string receipt_ {};
    std::vector<uint8_t> receiptSignature_ {};

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

    bool dhtPeerDiscovery_ {false};

    /**
     * Proxy
     */
    std::string proxyListUrl_;
    bool proxyEnabled_ {false};
    std::string proxyServer_ {};
    std::string proxyServerCached_ {};

    std::mutex dhParamsMtx_ {};
    std::shared_future<tls::DhParams> dhParams_;
    std::condition_variable dhParamsCv_;

    bool allowPeersFromHistory_ {true};
    bool allowPeersFromContact_ {true};
    bool allowPeersFromTrusted_ {true};

    std::string managerUri_ {};
    std::string managerUsername_ {};

    /**
     * Optional: via_addr construct from received parameters
     */
    pjsip_host_port via_addr_ {};

    pjsip_transport* via_tp_ {nullptr};

    std::unique_ptr<DhtPeerConnector> dhtPeerConnector_;
    mutable std::mutex connManagerMtx_ {};
    std::unique_ptr<ConnectionManager> connectionManager_;
    GitSocketList gitSocketList_ {};

    std::mutex discoveryMapMtx_;
    std::shared_ptr<dht::PeerDiscovery> peerDiscovery_;
    std::map<dht::InfoHash, DiscoveredPeer> discoveredPeers_;
    std::map<std::string, std::string> discoveredPeerMap_;
    bool accountPeerDiscovery_ {false};
    bool accountPublish_ {false};

    /**
     * Avoid to refresh the cache multiple times
     */
    std::atomic_bool isRefreshing_ {false};
    /**
     * This will cache the turn server resolution each time we launch
     * Jami, or for each connectivityChange()
     */
    void cacheTurnServers();

    std::chrono::duration<int> turnRefreshDelay_ {std::chrono::seconds(10)};

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
     * @param peerId        The contact who owns the device
     * @param deviceId      The device to ask
     * @note triggers cacheSIPConnection
     */
    void requestSIPConnection(const std::string& peerId, const DeviceId& deviceId);
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

    std::mutex gitServersMtx_ {};
    std::map<dht::Value::Id, std::unique_ptr<GitServer>> gitServers_ {};

    std::atomic_bool deviceAnnounced_ {false};

    //// File transfer
    std::shared_ptr<TransferManager> nonSwarmTransferManager_;
    std::mutex transferMutex_ {};
    std::map<std::string, std::shared_ptr<TransferManager>> transferManagers_ {};

    bool noSha3sumVerification_ {false};

    std::map<Uri::Scheme, std::unique_ptr<ChannelHandlerInterface>> channelHandlers_ {};

    std::unique_ptr<ConversationModule> convModule_;
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
