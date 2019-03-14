/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Simon Désaulniers <simon.desaulniers@gmail.com>
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

#include "security/tls_session.h"
#include "security/diffie-hellman.h"
#include "sip/sipaccountbase.h"
#include "dring/datatransfer_interface.h"

#include "noncopyable.h"
#include "ip_utils.h"
#include "ring_types.h" // enable_if_base_of
#include "security/certstore.h"
#include "scheduled_executor.h"

#include <opendht/dhtrunner.h>
#include <opendht/default_types.h>

#include <pjsip/sip_types.h>

#include <vector>
#include <map>
#include <chrono>
#include <list>
#include <future>

#if HAVE_RINGNS
#include "namedirectory.h"
#endif

/**
 * @file ringaccount.h
 * @brief Ring Account is build on top of SIPAccountBase and uses DHT to handle call connectivity.
 */

namespace YAML {
class Node;
class Emitter;
}


namespace dev
{
    template <unsigned N> class FixedHash;
    using h160 = FixedHash<20>;
    using Address = h160;
}

namespace ring {

class IceTransport;
struct Contact;
struct AccountArchive;
class DhtPeerConnector;
class PeerConnection;

class RingAccount : public SIPAccountBase {
    private:
        struct PeerConnectionMsg;

    public:
        constexpr static const char* const ACCOUNT_TYPE = "RING";
        constexpr static const in_port_t DHT_DEFAULT_PORT = 4222;
        constexpr static const char* const DHT_DEFAULT_BOOTSTRAP = "bootstrap.jami.net";
        constexpr static const char* const DHT_DEFAULT_PROXY = "dhtproxy.jami.net:[80-100]";
        constexpr static const char* const DHT_TYPE_NS = "cx.ring";

        /* constexpr */ static const std::pair<uint16_t, uint16_t> DHT_PORT_RANGE;

        const char* getAccountType() const override {
            return ACCOUNT_TYPE;
        }

        std::shared_ptr<RingAccount> shared() {
            return std::static_pointer_cast<RingAccount>(shared_from_this());
        }
        std::shared_ptr<RingAccount const> shared() const {
            return std::static_pointer_cast<RingAccount const>(shared_from_this());
        }
        std::weak_ptr<RingAccount> weak() {
            return std::static_pointer_cast<RingAccount>(shared_from_this());
        }
        std::weak_ptr<RingAccount const> weak() const {
            return std::static_pointer_cast<RingAccount const>(shared_from_this());
        }

        const std::string& getPath() const {
            return idPath_;
        }

        /**
         * Constructor
         * @param accountID The account identifier
         */
        RingAccount(const std::string& accountID, bool presenceEnabled);

        ~RingAccount();

        /**
         * Serialize internal state of this account for configuration
         * @param YamlEmitter the configuration engine which generate the configuration file
         */
        virtual void serialize(YAML::Emitter &out) override;

        /**
         * Populate the internal state for this account based on info stored in the configuration file
         * @param The configuration node for this account
         */
        virtual void unserialize(const YAML::Node &node) override;

        /**
         * Return an map containing the internal state of this account. Client application can use this method to manage
         * account info.
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
        std::map<std::string, bool> getTrackedBuddyPresence();

        /**
         * Connect to the DHT.
         */
        void doRegister() override;

        /**
         * Disconnect from the DHT.
         */
        void doUnregister(std::function<void(bool)> cb = {}) override;

        /**
         * @return pj_str_t "From" uri based on account information.
         * From RFC3261: "The To header field first and foremost specifies the desired
         * logical" recipient of the request, or the address-of-record of the
         * user or resource that is the target of this request. [...]  As such, it is
         * very important that the From URI not contain IP addresses or the FQDN
         * of the host on which the UA is running, since these are not logical
         * names."
         */
        std::string getFromUri() const;

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

        /**
         * Get the contact header for
         * @return pj_str_t The contact header based on account information
         */
        pj_str_t getContactHeader(pjsip_transport* = nullptr) override;

        void setReceivedParameter(const std::string &received) {
            receivedParameter_ = received;
            via_addr_.host.ptr = (char *) receivedParameter_.c_str();
            via_addr_.host.slen = receivedParameter_.size();
        }

        std::string getReceivedParameter() const {
            return receivedParameter_;
        }

        pjsip_host_port *
        getViaAddr() {
            return &via_addr_;
        }

        /* Returns true if the username and/or hostname match this account */
        MatchRank matches(const std::string &username, const std::string &hostname) const override;

        /**
         * Implementation of Account::newOutgoingCall()
         * Note: keep declaration before newOutgoingCall template.
         */
        std::shared_ptr<Call> newOutgoingCall(const std::string& toUrl,
                                              const std::map<std::string, std::string>& volatileCallDetails = {}) override;

        /**
         * Create outgoing SIPCall.
         * @param[in] toUrl The address to call
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
#ifndef _MSC_VER
        template <class T=SIPCall>
        std::shared_ptr<enable_if_base_of<T, SIPCall> >
        newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails = {});
#else
        template <class T>
        std::shared_ptr<T>
        newOutgoingCall(const std::string& toUrl, const std::map<std::string, std::string>& volatileCallDetails = {});
#endif

        /**
         * Create incoming SIPCall.
         * @param[in] from The origin of the call
         * @param details use to set some specific details
         * @return std::shared_ptr<T> A shared pointer on the created call.
         *      The type of this instance is given in template argument.
         *      This type can be any base class of SIPCall class (included).
         */
        virtual std::shared_ptr<SIPCall>
        newIncomingCall(const std::string& from, const std::map<std::string, std::string>& details = {}) override;

        virtual bool isTlsEnabled() const override {
            return true;
        }

        virtual bool isSrtpEnabled() const {
            return true;
        }

        virtual sip_utils::KeyExchangeProtocol getSrtpKeyExchange() const override {
            return sip_utils::KeyExchangeProtocol::SDES;
        }

        virtual bool getSrtpFallback() const override {
            return false;
        }

        bool setCertificateStatus(const std::string& cert_id, tls::TrustStore::PermissionStatus status);

        std::vector<std::string> getCertificatesByStatus(tls::TrustStore::PermissionStatus status);

        bool findCertificate(const std::string& id);
        bool findCertificate(const dht::InfoHash& h, std::function<void(const std::shared_ptr<dht::crypto::Certificate>&)>&& cb = {});

        /* contact requests */
        std::vector<std::map<std::string, std::string>> getTrustRequests() const;
        bool acceptTrustRequest(const std::string& from);
        bool discardTrustRequest(const std::string& from);

        /**
         * Add contact to the account contact list.
         * Set confirmed if we know the contact also added us.
         */
        void addContact(const std::string& uri, bool confirmed = false);
        void removeContact(const std::string& uri, bool banned = true);
        std::vector<std::map<std::string, std::string>> getContacts() const;

        ///
        /// Obtain details about one account contact in serializable form.
        ///
        std::map<std::string, std::string> getContactDetails(const std::string& uri) const;

        void sendTrustRequest(const std::string& to, const std::vector<uint8_t>& payload);
        void sendTrustRequestConfirm(const dht::InfoHash& to);
        virtual void sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads, uint64_t id) override;
        virtual uint64_t sendTextMessage(const std::string& to, const std::map<std::string, std::string>& payloads) override;

        /* Devices */
        void addDevice(const std::string& password);
        /**
         * Export the archive to a file
         * @param destinationPath
         * @param (optional) password, if not provided, will update the contacts only if the archive doesn't have a password
         * @return if the archive was exported
         */
        bool exportArchive(const std::string& destinationPath, const std::string& password = {});
        bool revokeDevice(const std::string& password, const std::string& device);
        std::map<std::string, std::string> getKnownDevices() const;

        bool changeArchivePassword(const std::string& password_old, const std::string& password_new);

        void connectivityChanged() override;

        // overloaded methods
        void flush() override;

#if HAVE_RINGNS
        void lookupName(const std::string& name);
        void lookupAddress(const std::string& address);
        void registerName(const std::string& password, const std::string& name);
#endif

        ///
        /// Send a E2E connection request to a given peer for the given transfer id
        ///
        /// /// \param[in] peer RingID on request's recipient
        /// /// \param[in] tid linked outgoing data transfer
        ///
        void requestPeerConnection(const std::string& peer, const DRing::DataTransferId& tid,
                                   std::function<void(PeerConnection*)> connect_cb);

        ///
        /// Close a E2E connection between a given peer and a given transfer id.
        ///
        /// /// \param[in] peer RingID on request's recipient
        /// /// \param[in] tid linked outgoing data transfer
        ///
        void closePeerConnection(const std::string& peer, const DRing::DataTransferId& tid);

        std::vector<std::string> publicAddresses();


        /// \return true if the given DHT message identifier has been treated
        /// \note if message has not been treated yet this method store this id and returns true at further calls
        bool isMessageTreated(unsigned int id) ;

        dht::DhtRunner& dht() { return dht_; }

        const dht::crypto::Identity& identity() const { return identity_; }

        const std::shared_future<tls::DhParams> dhParams() const { return dhParams_; }

        void forEachDevice(const dht::InfoHash& to,
                           std::function<void(const std::shared_ptr<RingAccount>&,
                                              const dht::InfoHash&)> op,
                           std::function<void(const std::shared_ptr<RingAccount>&,
                                              bool)> end = {});

        /**
         * Inform that a potential peer device have been found.
         * Returns true only if the device certificate is a valid device certificate.
         * In that case (true is returned) the account_id parameter is set to the peer account ID.
         */
        static bool foundPeerDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, dht::InfoHash& account_id);

        /**
         * Start or stop to use the proxy client
         * @param address of the proxy
         * @param deviceKey the device key for push notifications (empty to not use it)
         */
        void enableProxyClient(bool enable);

        void setPushNotificationToken(const std::string& pushDeviceToken = "");

        /**
         * To be called by clients with relevant data when a push notification is received.
         */
        void pushNotificationReceived(const std::string& from, const std::map<std::string, std::string>& data);

        std::string getUserUri() const override;

        /**
         * Get last messages (should be used to retrieve messages when launching the client)
         * @param base_timestamp
         */
        std::vector<DRing::Message> getLastMessages(const uint64_t& base_timestamp) override;

    private:
        NON_COPYABLE(RingAccount);

        using clock = std::chrono::system_clock;
        using time_point = clock::time_point;

        /**
         * Private structures
         */
        struct PendingCall;
        struct PendingMessage;
        struct TrustRequest;
        struct KnownDevice;
        struct DeviceAnnouncement;
        struct DeviceSync;
        struct BuddyInfo;

        void syncDevices();
        void onReceiveDeviceSync(DeviceSync&& sync);

#if HAVE_RINGNS
        std::reference_wrapper<NameDirectory> nameDir_;
        std::string nameServer_;
        std::string registeredName_;
#endif

        /**
         * Compute archive encryption key and DHT storage location from password and PIN.
         */
        static std::pair<std::vector<uint8_t>, dht::InfoHash> computeKeys(const std::string& password, const std::string& pin, bool previous=false);

        void trackPresence(const dht::InfoHash& h, BuddyInfo& buddy);

        /**
         * Update tracking info when buddy appears offline.
         */
        void onTrackedBuddyOffline(const dht::InfoHash&);

        /**
         * Update tracking info when buddy appears offline.
         */
        void onTrackedBuddyOnline(const dht::InfoHash&);

        void doRegister_();
        void incomingCall(dht::IceCandidates&& msg, const std::shared_ptr<dht::crypto::Certificate>& from_cert, const dht::InfoHash& from);

        const dht::ValueType USER_PROFILE_TYPE = {9, "User profile", std::chrono::hours(24 * 7)};

        void startOutgoingCall(const std::shared_ptr<SIPCall>& call, const std::string toUri);

        void onConnectedOutgoingCall(SIPCall& call, const std::string& to_id, IpAddr target);

        /**
         * Set the internal state for this account, mainly used to manage account details from the client application.
         * @param The map containing the account information.
         */
        virtual void setAccountDetails(const std::map<std::string, std::string> &details) override;

        /**
         * Start a SIP Call
         * @param call  The current call
         * @return true if all is correct
         */
        bool SIPStartCall(SIPCall& call, IpAddr target);

        /**
         * Inform that a potential account device have been found.
         * Returns true if the device have been validated to be part of this account
         */
        bool foundAccountDevice(const std::shared_ptr<dht::crypto::Certificate>& crt, const std::string& name = {}, const time_point& last_sync = time_point::min());

        /**
         * For a call with (from_device, from_account), check the peer certificate chain (cert_list, cert_num)
         * with session check status.
         * Put deserialized certificate to cert_out;
         */
        static pj_status_t checkPeerTlsCertificate(dht::InfoHash from_device, dht::InfoHash from_account,
                                unsigned status,
                                const gnutls_datum_t* cert_list, unsigned cert_num,
                                std::shared_ptr<dht::crypto::Certificate>& cert_out);

        /**
         * Check that a peer is authorised to talk to us.
         * If everything is in order, calls the callback with the
         * peer certificate chain (down to the peer device certificate),
         * and the peer account id.
         */
        void onPeerMessage(const dht::InfoHash& peer_device, std::function<void(const std::shared_ptr<dht::crypto::Certificate>& crt, const dht::InfoHash& account_id)>);

        void onTrustRequest(const dht::InfoHash& peer_account, const dht::InfoHash& peer_device, time_t received , bool confirm, std::vector<uint8_t>&& payload);

        /**
         * Maps require port via UPnP
         */
        bool mapPortUPnP();

        void igdChanged();

        dht::DhtRunner dht_ {};
        dht::crypto::Identity identity_ {};

        dht::InfoHash callKey_;

        bool handlePendingCallList();
        bool handlePendingCall(PendingCall& pc, bool incoming);

        /**
         * DHT calls waiting for ICE negotiation
         */
        std::list<PendingCall> pendingCalls_;

        /**
         * Incoming DHT calls that are not yet actual SIP calls.
         */
        std::list<PendingCall> pendingSipCalls_;
        std::set<dht::Value::Id> treatedCalls_ {};
        mutable std::mutex callsMutex_ {};

        mutable std::mutex messageMutex_ {};
        std::map<dht::Value::Id, PendingMessage> sentMessages_;
        std::set<dht::Value::Id> treatedMessages_ {};

        std::string ringAccountId_ {};
        std::string ringDeviceId_ {};
        std::string ringDeviceName_ {};
        std::string idPath_ {};
        std::string cachePath_ {};
        std::string dataPath_ {};
        std::string ethPath_ {};
        std::string ethAccount_ {};

        std::string archivePath_ {};
        bool archiveHasPassword_ {true};

        std::string receipt_ {};
        std::vector<uint8_t> receiptSignature_ {};
        dht::Value announceVal_;

        std::map<dht::InfoHash, TrustRequest> trustRequests_;
        void loadTrustRequests();
        void saveTrustRequests() const;

        std::map<dht::InfoHash, Contact> contacts_;
        void loadContacts();
        void saveContacts() const;
        void updateContact(const dht::InfoHash&, const Contact&);
        void addContact(const dht::InfoHash&, bool confirmed = false);

        // Trust store with account main certificate as the only CA
        dht::crypto::TrustList accountTrust_;
        // Trust store for to match peer certificates
        tls::TrustStore trust_;

        std::shared_ptr<dht::Value> announce_;

        /* this ring account associated devices */
        std::map<dht::InfoHash, KnownDevice> knownDevices_;

        /* tracked buddies presence */
        std::mutex buddyInfoMtx;
        std::map<dht::InfoHash, BuddyInfo> trackedBuddies_;

        void loadAccount(const std::string& archive_password = {}, const std::string& archive_pin = {}, const std::string& archive_path = {});
        void loadAccountFromFile(const std::string& archive_path, const std::string& archive_password);
        void loadAccountFromDHT(const std::string& archive_password, const std::string& archive_pin);
        void loadAccountFromArchive(AccountArchive&& archive, const std::string& archive_password);

        bool hasCertificate() const;
        bool hasPrivateKey() const;
        bool useIdentity(const dht::crypto::Identity& id);
        static bool needsMigration(const dht::crypto::Identity& id);

        std::string makeReceipt(const dht::crypto::Identity& id);
        void createRingDevice(const dht::crypto::Identity& id);
        void initRingDevice(const AccountArchive& a);
        void migrateAccount(const std::string& pwd, dht::crypto::Identity& device);
        static bool updateCertificates(AccountArchive& archive, dht::crypto::Identity& device);

        void createAccount(const std::string& archive_password, dht::crypto::Identity&& migrate);
        void updateArchive(AccountArchive& content) const;
        void saveArchive(AccountArchive& content, const std::string& pwd);
        AccountArchive readArchive(const std::string& pwd) const;
        std::vector<dht::SockAddr> loadBootstrap() const;

        static std::pair<std::string, std::string> saveIdentity(const dht::crypto::Identity id, const std::string& path, const std::string& name);
        void saveNodes(const std::vector<dht::NodeExport>&) const;
        void saveValues(const std::vector<dht::ValuesExport>&) const;

        void loadTreatedCalls();
        void saveTreatedCalls() const;

        void loadTreatedMessages();
        void saveTreatedMessages() const;

        void loadKnownDevices();
        void saveKnownDevices() const;

        void replyToIncomingIceMsg(const std::shared_ptr<SIPCall>&,
                                   const std::shared_ptr<IceTransport>&,
                                   const dht::IceCandidates&,
                                   const std::shared_ptr<dht::crypto::Certificate>& from_cert,
                                   const dht::InfoHash& from);

        static tls::DhParams loadDhParams(const std::string path);

        /**
         * If privkeyPath_ is a valid private key file (PEM or DER),
         * and certPath_ a valid certificate file, load and returns them.
         * Otherwise, generate a new identity and returns it.
         */
        dht::crypto::Identity loadIdentity(const std::string& crt_path, const std::string& key_path, const std::string& key_pwd) const;
        std::vector<dht::NodeExport> loadNodes() const;
        std::vector<dht::ValuesExport> loadValues() const;
        mutable std::mutex dhtValuesMtx_;

        bool dhtPublicInCalls_ {true};

        /**
         * DHT port preference
         */
        in_port_t dhtPort_ {};

        /**
         * DHT port actually used,
         * this holds the actual port used for DHT, which may not be the port
         * selected in the configuration in the case that UPnP is used and the
         * configured port is already used by another client
         */
        UsedPort dhtPortUsed_ {};

        /**
         * Proxy
         */
        bool proxyEnabled_;
        std::string proxyServer_;
        std::string proxyServerCached_;
        std::string deviceKey_;
        std::string getDhtProxyServer();

        /**
         * The TLS settings, used only if tls is chosen as a sip transport.
         */
        void generateDhParams();

        std::shared_future<tls::DhParams> dhParams_;
        std::mutex dhParamsMtx_;
        std::condition_variable dhParamsCv_;

        bool allowPeersFromHistory_ {true};
        bool allowPeersFromContact_ {true};
        bool allowPeersFromTrusted_ {true};

        /**
         * Optional: "received" parameter from VIA header
         */
        std::string receivedParameter_ {};

        /**
         * Optional: "rport" parameter from VIA header
         */
        int rPort_ {-1};

        /**
         * Optional: via_addr construct from received parameters
         */
        pjsip_host_port via_addr_ {};

        char contactBuffer_[PJSIP_MAX_URL_SIZE] {};
        pj_str_t contact_ {contactBuffer_, 0};
        pjsip_transport* via_tp_ {nullptr};

        template <class... Args>
        std::shared_ptr<IceTransport> createIceTransport(const Args&... args);

        void registerDhtAddress(IceTransport&);

        std::unique_ptr<DhtPeerConnector> dhtPeerConnector_;

        std::shared_ptr<RepeatedTask> eventHandler {};
        void checkPendingCallsTask();
};

static inline std::ostream& operator<< (std::ostream& os, const RingAccount& acc)
{
    os << "[Account " << acc.getAccountID() << "] ";
    return os;
}

} // namespace ring
