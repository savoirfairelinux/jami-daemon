/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "archive_account_manager.h"
#include "accountarchive.h"
#include "fileutils.h"
#include "libdevcrypto/Common.h"
#include "archiver.h"
#include "base64.h"
#include "jami/account_const.h"
#include "account_schema.h"
#include "jamidht/conversation_module.h"
#include "manager.h"
#include "jamidht/auth_channel_handler.h"
#include "client/ring_signal.h"

#include <dhtnet/multiplexed_socket.h>
#include <opendht/dhtrunner.h>
#include <opendht/thread_pool.h>

#include <memory>
#include <fstream>

namespace jami {

const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);
static const uint8_t MAX_OPEN_CHANNELS {1}; // TODO enforce this in ::connect

void
ArchiveAccountManager::initAuthentication(const std::string& accountId,
                                          PrivateKey key,
                                          std::string deviceName,
                                          std::unique_ptr<AccountCredentials> credentials,
                                          AuthSuccessCallback onSuccess,
                                          AuthFailureCallback onFailure,
                                          const OnChangeCallback& onChange)
                                          {
    auto ctx = std::make_shared<AuthContext>();
    ctx->accountId = accountId;
    ctx->key = key;
    ctx->request = buildRequest(key);
    ctx->deviceName = std::move(deviceName);
    ctx->credentials = dynamic_unique_cast<ArchiveAccountCredentials>(std::move(credentials));
    ctx->onSuccess = std::move(onSuccess);
    ctx->onFailure = std::move(onFailure);

    if (not ctx->credentials) {
        ctx->onFailure(AuthError::INVALID_ARGUMENTS, "invalid credentials");
        return;
    }

    onChange_ = std::move(onChange);

    // if (ctx->credentials->scheme == "dht") {
    //     loadFromDHT(ctx);
    //     return;
    // } else if (ctx->credentials->scheme == "p2p") {
    JAMI_DEBUG("[LinkDevice] BP 1 | Importing account via p2p scheme.");
    JAMI_DEBUG("[LinkDevice] BP 1 | Importing account via p2p scheme. {}", ctx->credentials->scheme);
    JAMI_DEBUG("[LinkDevice] BP 1 | Importing account via p2p scheme. {}", ctx->credentials->uri);
    // if (ctx->credentials->uri == "jami-auth") {
    if (ctx->credentials->scheme == "p2p") {
        // newDev
        JAMI_DEBUG("[LinkDevice] BP 2 | Importing account via p2p scheme.");
        JAMI_DEBUG("[LinkDevice] Importing account via p2p scheme.");
        startLoadArchiveFromDevice(ctx);
        return;
    }

    dht::ThreadPool::computation().run([ctx = std::move(ctx), w = weak_from_this()] {
        auto this_ = std::static_pointer_cast<ArchiveAccountManager>(w.lock());
        if (not this_)
            return;
        try {
            if (ctx->credentials->scheme == "file") {
                // Import from external archive
                this_->loadFromFile(*ctx);
            }
            else {
                // Create/migrate local account
                bool hasArchive = not ctx->credentials->uri.empty()
                                  and std::filesystem::is_regular_file(ctx->credentials->uri);
                if (hasArchive) {
                    // Create/migrate from local archive
                    if (ctx->credentials->updateIdentity.first
                        and ctx->credentials->updateIdentity.second
                        and needsMigration(ctx->credentials->updateIdentity)) {
                        this_->migrateAccount(*ctx);
                    } else {
                        this_->loadFromFile(*ctx);
                    }
                } else if (ctx->credentials->updateIdentity.first
                           and ctx->credentials->updateIdentity.second) {
                    auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(
                        &dev::KeyPair::create);
                    AccountArchive a;
                    JAMI_WARN("[Auth] converting certificate from old account %s",
                              ctx->credentials->updateIdentity.first->getPublicKey()
                                  .getId()
                                  .toString()
                                  .c_str());
                    a.id = std::move(ctx->credentials->updateIdentity);
                    try {
                        a.ca_key = std::make_shared<dht::crypto::PrivateKey>(
                            fileutils::loadFile("ca.key", this_->path_));
                    } catch (...) {
                    }
                    this_->updateCertificates(a, ctx->credentials->updateIdentity);
                    a.eth_key = future_keypair.get().secret().makeInsecure().asBytes();
                    this_->onArchiveLoaded(*ctx, std::move(a));
                } else {
                    this_->createAccount(*ctx);
                }
            }
        } catch (const std::exception& e) {
            ctx->onFailure(AuthError::UNKNOWN, e.what());
        }
    });
}

bool
ArchiveAccountManager::updateCertificates(AccountArchive& archive, dht::crypto::Identity& device)
{
    JAMI_WARNING("Updating certificates");
    using Certificate = dht::crypto::Certificate;

    // We need the CA key to resign certificates
    if (not archive.id.first or not *archive.id.first or not archive.id.second or not archive.ca_key
        or not *archive.ca_key)
        return false;

    // Currently set the CA flag and update expiration dates
    bool updated = false;

    auto& cert = archive.id.second;
    auto ca = cert->issuer;
    // Update CA if possible and relevant
    if (not ca or (not ca->issuer and (not ca->isCA() or ca->getExpiration() < clock::now()))) {
        ca = std::make_shared<Certificate>(
            Certificate::generate(*archive.ca_key, "Jami CA", {}, true));
        updated = true;
        JAMI_DBG("CA CRT re-generated");
    }

    // Update certificate
    if (updated or not cert->isCA() or cert->getExpiration() < clock::now()) {
        cert = std::make_shared<Certificate>(
            Certificate::generate(*archive.id.first,
                                  "Jami",
                                  dht::crypto::Identity {archive.ca_key, ca},
                                  true));
        updated = true;
        JAMI_DBG("Jami CRT re-generated");
    }

    if (updated and device.first and *device.first) {
        // update device certificate
        device.second = std::make_shared<Certificate>(
            Certificate::generate(*device.first, "Jami device", archive.id));
        JAMI_DBG("device CRT re-generated");
    }

    return updated;
}

bool
ArchiveAccountManager::setValidity(std::string_view scheme, const std::string& password,
                                   dht::crypto::Identity& device,
                                   const dht::InfoHash& id,
                                   int64_t validity)
{
    auto archive = readArchive(scheme, password);
    // We need the CA key to resign certificates
    if (not archive.id.first or not *archive.id.first or not archive.id.second or not archive.ca_key
        or not *archive.ca_key)
        return false;

    auto updated = false;

    if (id)
        JAMI_WARN("Updating validity for certificate with id: %s", id.to_c_str());
    else
        JAMI_WARN("Updating validity for certificates");

    auto& cert = archive.id.second;
    auto ca = cert->issuer;
    if (not ca)
        return false;

    // using Certificate = dht::crypto::Certificate;
    //  Update CA if possible and relevant
    if (not id or ca->getId() == id) {
        ca->setValidity(*archive.ca_key, validity);
        updated = true;
        JAMI_DBG("CA CRT re-generated");
    }

    // Update certificate
    if (updated or not id or cert->getId() == id) {
        cert->setValidity(dht::crypto::Identity {archive.ca_key, ca}, validity);
        device.second->issuer = cert;
        updated = true;
        JAMI_DBG("Jami CRT re-generated");
    }

    if (updated) {
        archive.save(fileutils::getFullPath(path_, archivePath_), scheme, password);
    }

    if (updated or not id or device.second->getId() == id) {
        // update device certificate
        device.second->setValidity(archive.id, validity);
        updated = true;
    }

    return updated;
}

void
ArchiveAccountManager::createAccount(AuthContext& ctx)
{
    AccountArchive a;
    auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(&dev::KeyPair::create);
    auto ca = dht::crypto::generateIdentity("Jami CA");
    if (!ca.first || !ca.second) {
        throw std::runtime_error("Can't generate CA for this account.");
    }
    a.id = dht::crypto::generateIdentity("Jami", ca, 4096, true);
    if (!a.id.first || !a.id.second) {
        throw std::runtime_error("Can't generate identity for this account.");
    }
    JAMI_WARN("[Auth] new account: CA: %s, RingID: %s",
              ca.second->getId().toString().c_str(),
              a.id.second->getId().toString().c_str());
    a.ca_key = ca.first;
    auto keypair = future_keypair.get();
    a.eth_key = keypair.secret().makeInsecure().asBytes();
    onArchiveLoaded(ctx, std::move(a));
}

void
ArchiveAccountManager::loadFromFile(AuthContext& ctx)
{
    JAMI_WARNING("[Auth] loading archive from: {}", ctx.credentials->uri);
    AccountArchive archive;
    try {
        archive = AccountArchive(ctx.credentials->uri, ctx.credentials->password_scheme, ctx.credentials->password);
    } catch (const std::exception& ex) {
        JAMI_WARNING("[Auth] can't read file: {}", ex.what());
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, ex.what());
        return;
    }
    onArchiveLoaded(ctx, std::move(archive));
}

// TODO remove?
struct ArchiveAccountManager::DhtLoadContext
{
    dht::DhtRunner dht;
    std::pair<bool, bool> stateOld {false, true};
    std::pair<bool, bool> stateNew {false, true};
    bool found {false};
};

// this enum is for the states of add device TLS protocol
// used for LinkDeviceProtocolStateChanged = AddDeviceStateChanged
enum class AuthDecodingState : uint8_t {ESTABLISHED=0, SCHEME_SENT, CREDENTIALS, ARCHIVE, SCHEME_KNOWN, REQUEST_TRANSMITTED, ARCHIVE_SENT, ARCHIVE_RECEIVED, GENERIC_ERROR, AUTH_ERROR};

// used for status codes on DeviceAuthStateChanged
enum class DeviceAuthState: uint8_t {NONE=0, TOKEN_AVAIL=1, CONNECTING=2, AUTH=3, DONE=4, ERROR=5};

/** Holds state for NEW device peer import logic */
struct ArchiveAccountManager::LinkDeviceContext
{
    dht::crypto::Identity tmpId;
    uint64_t opId;

    // TODO only use this for newDev and use JamiAccount ptr for oldDev
    dhtnet::ConnectionManager tempConnMgr;
    AuthDecodingState state; // TODO use this in the function calls

    unsigned numOpenChannels {0};
    unsigned maxOpenChannels {1};

    AuthDecodingState deviceState {AuthDecodingState::ESTABLISHED};
    std::string scheme;
    bool authEnabled {false};
    bool archiveTransferredWithoutFailure {false};
    std::vector<uint8_t> accData;

    LinkDeviceContext(dht::crypto::Identity id)
    : tmpId(std::move(id))
    , tempConnMgr(tmpId)
    {}

    std::shared_ptr<dhtnet::ChannelSocket> channel;
};

/** Holds state for OLD device peer import logic */
struct ArchiveAccountManager::PeerLoadContext
{
    uint64_t opId;
    AuthDecodingState state;
    std::string scheme;
    bool authEnabled {false};
    bool archiveTransferredWithoutFailure {false};
    std::vector<uint8_t> accData;

    // TODO pack this into a single int with bitwise arithmetic
    unsigned failedPasswordAttempts {0};
    unsigned numTries {0};
    unsigned maxTries {0};
    std::shared_ptr<dhtnet::ChannelSocket> channel;

    PeerLoadContext(std::shared_ptr<dhtnet::ChannelSocket> c)
    : channel(std::move(c))
    {}

};

struct ArchiveAccountManager::AuthMsg {
    uint8_t schemeId {0};
    std::map<std::string, std::string> payload;
    std::vector<uint8_t> archive;
    MSGPACK_DEFINE_MAP(schemeId, payload, archive)
};

// ArchiveAccountManager::ChannelModule(std::weak_ptr<JamiAccount>&& account)
//     : pimpl_ {std::make_shared<Impl>(std::move(account))}
// {}

// this is for signals and not the channel op state
// aka this is for jami-client
enum LinkDeviceState {SUCCESS=0, ERROR=1, CONTINUE=2}; //scheme is the password type, attempt is the cli/serv exchange, archive is the account download phase



// linkdevice: oldDev: setup handler for receiving messages from newDev
void
ArchiveAccountManager::onAuthReady(const std::string& deviceId, std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    /*JAMI_DEBUG("[LinkDevice] ArhiveAccountManager::onAuthReady");
    auto ctx = std::make_shared<AuthContext>();
    auto decodeCtx = std::make_shared<DecodingContext>();

    ctx->linkDevCtx->channel = std::move(channel);
    
    channel->setOnRecv(
        [
            w = weak_from_this(),
            ctx,
            decodeCtx
        ] (const uint8_t* buf, size_t len) {
            auto this_ = std::static_pointer_cast<ArchiveAccountManager>(w.lock());
            if (not this_) return (ssize_t)-1;
            this_->onAuthRecv(ctx, decodeCtx, buf, len);
            return (ssize_t)len;
        }
    );*/
}

// linkdevice: newDev: called when user enters their password
// void
// TODO implement with key as bytes instead of string for multiple authentication schemes
bool
ArchiveAccountManager::provideAccountAuthentication(const std::string& passwordFromUser, const std::string& scheme) {
    // ArchiveAccountManager::onPasswordProvided(const std::string& passwordFromUser) {

    JAMI_DEBUG("[LinkDevice] ArhiveAccountManager::provideAccountAuthentication");

    if (scheme != fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD) {
        JAMI_ERROR("[LinkDevice] Unsupported account authentication scheme attempted.");
        return false;
    }

    AuthMsg toSend;
    toSend.payload["password"] = std::move(passwordFromUser);
    toSend.payload["name"] = "requestTransmitted";
    msgpack::sbuffer buffer(UINT16_MAX);
    msgpack::pack(buffer, toSend);
    std::error_code ec = std::make_error_code(std::errc(AuthDecodingState::GENERIC_ERROR));
    bool retVal = false;
    try {
        if (auto channel = authChannel_.lock()) {
            channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            retVal = true;
        }
    }
    catch (std::exception e) {
        JAMI_WARNING("[LinkDevice] Failed to send password over auth ChannelSocket.");
    }
    return retVal;
}

// TODO organize ordering of all functions linkdevice
// linkdevice: helper for receiving messages
struct ArchiveAccountManager::DecodingContext
{
    msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) {return true;}, nullptr, 512};
};

// // linkdevice: helper function for sending protocol messages
// void
// linkDevMsg(const dhtnet::ChannelSocket& channel, const ArchiveAccountManager::AuthMsg& msg, const std::string& failureAlert, const bool& isCritical)
// {
//     msgpack::sbuffer buffer(UINT16_MAX);
//     msgpack::pack(buffer, msg);
//     std::error_code ec;
//     channel.write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
//     if (!ec)
//         return;
//     // TODO implement ec handling
//     switch (isCritical) {
//         case true:
//             JAMI_ERROR("{}", failureAlert);
//             break;
//         case false:
//             JAMI_WARNING("{}", failureAlert);
//             break;
//     }
// };

// void
// linkDevMsg(const std::shared_ptr<dhtnet::ChannelSocket>& channelPtr, const ArchiveAccountManager::AuthMsg& msg, const std::string& failureAlert, const bool isCritical)
// {
//     msgpack::sbuffer buffer(UINT16_MAX);
//     msgpack::pack(buffer, msg);
//     std::error_code ec;
//     channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
//     if (!ec)
//         return;
//     // TODO implement ec handling
//     switch isCritical {
//         case true:
//             JAMI_ERROR("{}", failureAlert);
//             break;
//         case false:
//             JAMI_WARNING("{}", failureAlert);
//             break;
//     }
// };
// // linkdevice: helper function for sending protocol messages
// void
// // linkDevMsg(const std::shared_ptr<dhtnet::ChannelSocket>* channelPtr, const ArchiveAccountManager::AuthMsg& msg) {
// linkDevMsg(const std::shared_ptr<dhtnet::ChannelSocket> channelPtr, const ArchiveAccountManager::AuthMsg& msg, const std::string& failureAlert, const bool isCritical)
// {
//     linkDevMsg(channelPtr, msg, failureAlert, isCritical);
//     msgpack::sbuffer buffer(UINT16_MAX);
// };


// callback for NewLinkDevImpl
// oldDev: handle incoming messages for Link Device protocol
void
ArchiveAccountManager::onAuthRecv(const std::shared_ptr<AuthContext>& ctx, const std::shared_ptr<DecodingContext>& decodeCtx, const uint8_t* buf, size_t len)
{
    JAMI_DEBUG("[LinkDevice] ArhiveAccountManager::onAuthRecv");

    if (!buf) {
        return;
    }
    if (ctx->linkDevCtx->deviceState == AuthDecodingState::GENERIC_ERROR) {
        JAMI_ERROR("[LinkDevice] An authentication error occured.");
        return; // TODO shutdown channel in this state
    }

    decodeCtx->pac.reserve_buffer(len);

    std::copy_n(buf, len, decodeCtx->pac.buffer());
    decodeCtx->pac.buffer_consumed(len);

    // handle unpacking the data from the peer
    msgpack::object_handle oh;
    AuthMsg msg;
    try {
        decodeCtx->pac.next(oh);
        oh.get().convert(msg);
    } catch (std::exception e) {
        ctx->linkDevCtx->deviceState = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
        JAMI_WARNING("[LinkDevice] An error occured while unpacking a message from msgpack.");
        return;
    }

    if (msg.schemeId != 0) {
        JAMI_WARNING("[LinkDevice] Unsupported scheme received from a connection.");
        ctx->linkDevCtx->deviceState = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
    }


    AuthMsg toSend;
    if (ctx->linkDevCtx->state == AuthDecodingState::ESTABLISHED) {
        // TODO send msg with scheme id=0 to set protocol
        toSend.schemeId = 0;
        msgpack::sbuffer buffer(UINT16_MAX);
        msgpack::pack(buffer, toSend);
        std::error_code ec;
        ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    }
    else if (ctx->linkDevCtx->state == AuthDecodingState::CREDENTIALS) {
        // TODO
        // toSend.payload["authScheme"] = ctx->linkDevCtx->scheme;
        msgpack::sbuffer buffer(UINT16_MAX);
        msgpack::pack(buffer, toSend);
        std::error_code ec;
        ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    }
    else if (ctx->linkDevCtx->state == AuthDecodingState::SCHEME_SENT) {
        // TODO if scheme was sent & expect to receive credentials... send archive as msg if correct otherwise empty with incorrect flag and increment numtries
        // toSend.archive = ctx->linkDevCtx->accData;
        msgpack::sbuffer buffer(UINT16_MAX);
        msgpack::pack(buffer, toSend);
        std::error_code ec;
        ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
    }
    else if (ctx->linkDevCtx->state == AuthDecodingState::ARCHIVE) {
        // TODO if archive already sent close channel
        ctx->linkDevCtx->channel->shutdown();
    }
    else {
        JAMI_WARNING("[Link Device] channel operation mode is unspecified (unspecified auth protocol)");
    }
}

// // callback for NewLinkDevImpl
// // oldDev: handle incoming messages for Link Device protocol
// // TODO move newDev logic to newDev function
// void
// ArchiveAccountManager::onAuthRecv(const std::shared_ptr<AuthContext>& ctx, const std::shared_ptr<DecodingContext>& decodeCtx, const uint8_t* buf, size_t len)
// {
//     JAMI_DEBUG("[LinkDevice] ArhiveAccountManager::onAuthRecv");
//
//     if (!buf)
//         return;
//     if (ctx->linkDevCtx->deviceState == AuthDecodingState::GENERIC_ERROR) {
//         JAMI_ERROR("[LinkDevice] An authentication error occured.")
//         return; // TODO shutdown channel in this state
//     }
//     // int opId = ctx->linkDevCtx->opId;
//     // TODO change all channel to ctx->linkDevCtx->channel
//
//     decodeCtx->pac.reserve_buffer(len); // TODO rework like this
//
//     std::copy_n(buf, len, decodeCtx->pac.buffer());
//     decodeCtx->pac.buffer_consumed(len);
//
//     // handle unpacking the data from the peer
//     msgpack::object_handle oh;
//     AuthMsg msg;
//     try {
//         decodeCtx->pac.next(oh);
//         oh.get().convert(msg);
//     } catch (std::exception e) {
//         ctx->linkDevCtx->deviceState = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
//         JAMI_WARNING("[LinkDevice] An error occured while unpacking a message from msgpack.");
//         return;
//     }
//
//     if (msg.schemeId != 0) {
//         JAMI_WARNING("[LinkDevice] Unsupported scheme received from a connection.");
//         ctx->linkDevCtx->deviceState = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
//     }
//
//     AuthMsg toSend;
//     if (ctx->linkDevCtx->deviceState == AuthDecodingState::GENERIC_ERROR) {
//         JAMI_WARNING("[LinkDevice] Undefined behavior encountered during a link auth session.");
//         ctx->linkDevCtx->channel->shutdown();
//     }
//     else if (ctx->linkDevCtx->deviceState == AuthDecodingState::ESTABLISHED) {
//         // send back protocolId
//         toSend.schemeId = 0;
//         linkDevMsg(ctx->linkDevCtx->channel, &toSend);
//         ctx->linkDevCtx->deviceState = AuthDecodingState::CREDENTIALS;
//     }
//     else if (ctx->linkDevCtx->deviceState == AuthDecodingState::ARCHIVE) {
//         bool credentialsValid = msg.payload["credentialsValid"] == "true" ? true : false;
//         if (credentialsValid) {
//             // save archive
//             AccountArchive a;
//             a.deserialize(msg.archive, {});
//             saveArchive(a, fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD, std::move(msg.payload["password"]));
//             // write the success status
//             toSend.payload["transferSuccessful"] = "true";
//             linkDevMsg(ctx->linkDevCtx->channel, &toSend);
//             ctx->linkDevCtx->channel->shutdown();
//             // break;
//         } else {
//             if (ctx->linkDevCtx->numTries > ctx->linkDevCtx->maxTries){
//                 // set error state and let old device know via error msg toSend
//                 ctx->linkDevCtx->deviceState = AuthDecodingState::AUTH_ERROR;
//                 // break;
//             } else {
//                 // increment numTries and continue back to CREDENTIALS state
//                 ctx->linkDevCtx->numTries++;
//                 // break;
//             }
//         }
//     }
//     else if (ctx->linkDevCtx->deviceState == AuthDecodingState::CREDENTIALS) {
//         if (msg.payload["hasPassword"] == "true") {
//             if (ctx->linkDevCtx->numTries < ctx->linkDevCtx->maxTries) {
//                 // ctx->channel = std::move(channel);
//                 authChannel_ = std::weak_ptr(ctx->linkDevCtx->channel);
//                 emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
//                     ctx->accountId,
//                     ctx->token,
//                     static_cast<uint8_t>(AuthState::AUTH),
//                     "archive_authentication_required"
//                 );
//             } else {
//                 // TODO
//                 // emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
//                 //     // ctx->linkDevCtx->tmpId.second->getId().toString(),
//                 //     // static_cast<uint8_t>(AuthDecodingState::GENERIC_ERROR),
//                 //     std::static_cast<uint8_t>(AuthState::AUTH),
//                 //     "archive_authentication_failed"
//                 // );
//             }
//         } else /* no password enabled for account*/ {
//             // save archive
//             AccountArchive a;
//             a.deserialize(msg.archive, {});
//             saveArchive(a, fileutils::ARCHIVE_AUTH_SCHEME_NONE, ""/* because no password */);
//             // write the success status
//             toSend.payload["transferSuccessful"] = "true";
//             packAndWrite(ctx->linkDevCtx->channel, toSend);
//             ctx->linkDevCtx->channel->shutdown();
//         }
//     }
//     else if (ctx->linkDevCtx->deviceState == AuthDecodingState::AUTH_ERROR) {
//         JAMI_WARN("[LinkDevice] Isssue with authentication");
//         emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
//             ctx->accountId,
//             ctx->token,
//             static_cast<uint8_t>(AuthState::AUTH),
//             "requestFailed"
//         ); // let the client know that the auth has failed
//         // write the failure status
//         toSend.payload["transferSuccessful"] = "false";
//         packAndWrite(ctx->linkDevCtx->channel, toSend);
//         ctx->linkDevCtx->channel->shutdown();
//     } else {
//         // do nothing / default
//     }
//
// }


// link device: newDev: creates a new temporary account on the DHT for establishing a TLS connection
// TODO fix bracket matching / alignment
void
ArchiveAccountManager::startLoadArchiveFromDevice(const std::shared_ptr<AuthContext>& ctx)
{
    JAMI_DEBUG("[LinkDevice] ArchiveAccountManager::startLoadArchiveFromDevice");
    // JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice", ctx->linkDevCtx->tmpId.second->getId());
    dht::ThreadPool::computation().run([ctx = std::move(ctx), w=weak_from_this()] {

        JAMI_DEBUG("[LinkDevice] ArchiveAccountManager::startLoadArchiveFromDevice::generateECIdentity");
        auto ca = dht::crypto::generateEcIdentity("Jami Temporary CA");
        JAMI_DEBUG("[LinkDevice] ArchiveAccountManager::startLoadArchiveFromDevice::generateIdentity");
        auto user = dht::crypto::generateIdentity("Jami Temporary User", ca, 4096, true);
        ctx->linkDevCtx = std::make_unique<LinkDeviceContext>(dht::crypto::generateIdentity("Jami Temporary device", user));
        // set up auth channel code and also use it as opId
        JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice::getSeededRandomEngine", ctx->linkDevCtx->tmpId.second->getId());
        auto gen = Manager::instance().getSeededRandomEngine();
        JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice::uniform_int_distribution", ctx->linkDevCtx->tmpId.second->getId());
        ctx->linkDevCtx->opId = std::uniform_int_distribution<uint64_t>(100000, 999999)(gen);
        ctx->linkDevCtx->tempConnMgr.onDhtConnected(ctx->linkDevCtx->tmpId.second->getPublicKey());
        auto accountScheme = fmt::format("jami-auth://{}/{}", ctx->linkDevCtx->tmpId.second->getId(), ctx->linkDevCtx->opId);
        // ctx->linkDevCtx->scheme = accountScheme;
        emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            ctx->accountId,
            static_cast<uint8_t>(AuthDecodingState::SCHEME_KNOWN),
            accountScheme
        );
        JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice::onICERequest", ctx->linkDevCtx->tmpId.second->getId());
        ctx->linkDevCtx->tempConnMgr.onICERequest([](const DeviceId& deviceId) {
            return true;
        });

        JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice::onChannelRequest", ctx->linkDevCtx->tmpId.second->getId());
        ctx->linkDevCtx->tempConnMgr.onChannelRequest([ctx](const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name) {
            JAMI_WARNING("[LinkDevice] onChannelRequest {} {}", cert->getId(), name);
            constexpr auto AUTH_SCHEME = "auth:"sv;
            std::string_view url(name);
            auto sep1 = url.find(AUTH_SCHEME);
            if (sep1 == std::string_view::npos) {
                JAMI_WARNING("[LinkDevice] onChannelRequest: wrong scheme {}", name);
                return false;
            }
            auto opStr = url.substr(sep1 + AUTH_SCHEME.size());
            auto parsedOpId = jami::to_int<uint64_t>(opStr);

            JAMI_WARNING("[LinkDevice] onChannelRequest: opStr = url={} parsedOpId={} open channel: {}/{}", opStr, parsedOpId, ctx->linkDevCtx->numOpenChannels, ctx->linkDevCtx->maxOpenChannels);
	        if (ctx->linkDevCtx->opId == parsedOpId && ctx->linkDevCtx->numOpenChannels < ctx->linkDevCtx->maxOpenChannels) {
    	        ctx->linkDevCtx->numOpenChannels++;
	            return true;
  	        }
	        return false;
        });

        JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice::onConnectionReady", ctx->linkDevCtx->tmpId.second->getId());
        ctx->linkDevCtx->tempConnMgr.onConnectionReady([ctx, accountScheme](const DeviceId& deviceId, const std::string& name, std::shared_ptr<dhtnet::ChannelSocket> socket) {

            socket->onShutdown([ctx,deviceId, name]() {
                JAMI_WARNING("[LinkDevice] Socket closed: {} | {}", deviceId, name);
                ctx->linkDevCtx->numOpenChannels--;
            });

            // TODO implement packAndWrite here
            socket->setOnRecv([wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket), ctx, decodingCtx = std::make_shared<DecodingContext>()](const uint8_t* buf, size_t len) {

                if (!buf) {
                    return len;
                }

                decodingCtx->pac.reserve_buffer(len);
                std::copy_n(buf, len, decodingCtx->pac.buffer());
                decodingCtx->pac.buffer_consumed(len);
                msgpack::object_handle oh;
                AuthMsg toRecv;
                try {
                    decodingCtx->pac.next(oh);
                    oh.get().convert(toRecv);
                } catch (std::exception e) {
                    ctx->linkDevCtx->state = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
                    JAMI_WARNING("[Link Device] Error unpacking message from Old Device.");
                }

                AuthMsg toSend;
                if (ctx->linkDevCtx->state == AuthDecodingState::SCHEME_KNOWN) {
                    // CLEANUP if SCHEME: scheme is known and should check for password and prompt for password then send it back
                    // TODO rewrite structure for singular message type
                    // - don't have AuthMsg passwordScheme just read the message and if in scheme mode take passwordEnabled and password fields from payload
                    if (toRecv.payload["authScheme"] == "none" || toRecv.payload["authScheme"].empty()) {
                        // no password required
                        ctx->linkDevCtx->authEnabled = false;
                        JAMI_WARNING("[Link Device] Warning: receiving unencrypted archive from another device. Please make sure you intended for this to happen.");
                    } else if (toRecv.payload["authScheme"] == "password") {
                        ctx->linkDevCtx->authEnabled = true;
                        JAMI_DEBUG("[Link Device] Connection asserting a scheme of type {}.", toRecv.payload["authScheme"]);
                    } else {
                        JAMI_ERROR("[LinkDevice] Unsupported scheme with alias {}.", toRecv.payload["authScheme"]);
                        emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                            ctx->accountId,
                            static_cast<uint8_t>(AuthDecodingState::AUTH_ERROR),
                            ctx->linkDevCtx->scheme
                        );
                        return len;
                    }
                    ctx->linkDevCtx->scheme = toRecv.payload["authScheme"];
                }
                else if (ctx->linkDevCtx->state == AuthDecodingState::ARCHIVE) {
                    if (!toRecv.archive.empty()) {
                        auto archive = toRecv.archive;
                        auto decrypted = archiver::decompress(dht::crypto::aesDecrypt(archive, toRecv.payload["password"]));
                        ctx->linkDevCtx->accData = std::move(decrypted); // TODO
                        // ctx->linkDevCtx->transferredAccArchive = std::move(decrypted); // TODO
                        emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                            ctx->accountId,
                            ctx->linkDevCtx->opId,
                            static_cast<uint8_t>(DeviceAuthState::DONE),
                            "archive_loaded"
                        );
                        JAMI_LOG("[LinkDevice] Archive successfully transferred.");
                    } else {
                        JAMI_ERROR("[LinkDevice] Account archive is empty.");
                        emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                            ctx->accountId,
                            ctx->linkDevCtx->opId,
                            static_cast<uint8_t>(DeviceAuthState::ERROR),
                            "archive_load_failed"
                        );
                    }
                }
                // else if (ctx->linkDevCtx->state == AuthDecodingState::ASK) {
                //     JAMI_WARNING("[Link Device] unimplemented ASK request");
                // }
                else {
                    JAMI_WARNING("[Link Device] channel operation mode is unspecified (unspecified auth protocol)");
                }
            }); // !onConnectionReady // TODO emit AuthStateChanged+"connection ready" signal

            JAMI_DEBUG("[LinkDevice {}] ArchiveAccountManager::startLoadArchiveFromDevice::DeviceAuthStateChanged signal", ctx->linkDevCtx->tmpId.second->getId());
            JAMI_LOG("[LinkDevice {}] Generated temporary account.", ctx->linkDevCtx->tmpId.second->getId());
            emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                ctx->linkDevCtx->tmpId.second->getId().toString(),
                static_cast<uint8_t>(DeviceAuthState::CONNECTING),
                accountScheme
            );
        });
    });
}


// TODO fix function descriptions and add all as docstring in cxx format
// linkdeviec: oldDev: hook into communication protocol logic once auth_channel_handler::onReady is called
// TODO add opId
void
ArchiveAccountManager::addDevice(const std::string& accountId, uint32_t token, const std::shared_ptr<dhtnet::ChannelSocket>& channel)
{
    auto ctx = std::make_shared<AuthContext>();
    ctx->addDevCtx = std::make_unique<PeerLoadContext>(channel);
    ctx->accountId = accountId;
    ctx->token = token;

    try {
        // send the first message in the TLS connection
        // TODO KESS use this as the example for rewriting msgpack messaging system in all parts of linkdevice
        ArchiveAccountManager::AuthMsg msg;
        msgpack::sbuffer buffer(UINT16_MAX);
        msgpack::pack(buffer, msg);
        std::error_code ec;
        channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        ctx->addDevCtx->state = AuthDecodingState::ESTABLISHED;
    } catch (std::exception e) {
        JAMI_WARNING("[LinkDevice] error sending message on TLS channel.");
    }

    // TODO move this logic to oldDev::addDevice::onRecv::onArchiveSend
    channel->onShutdown([ctx] () {
        // check if the archive was successfully loaded and emitSignal
        if (ctx->addDevCtx->archiveTransferredWithoutFailure) {
            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                ctx->accountId,
                ctx->token,
                static_cast<uint8_t>(DeviceAuthState::DONE),
                "success"
            );
        } else {
            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                ctx->accountId,
                ctx->token,
                static_cast<uint8_t>(DeviceAuthState::DONE),
                "failure"
            );
        }
    });

    // for now we assume that the only valid protocol version is AuthMsg::scheme = 0 but can later add in more schemes inside this callback function
    ctx->addDevCtx->state = AuthDecodingState::CREDENTIALS;
    channel->setOnRecv([
                ctx,
                wthis = weak_from_this(),
                decodeCtx = std::make_shared<ArchiveAccountManager::DecodingContext>()
            ]
            (const uint8_t* buf, size_t len) {
                // when archive is sent to newDev we will get back a success or fail response before the connection closes and we need to handle this and pass it to the shutdown callback
                auto this_ = std::static_pointer_cast<ArchiveAccountManager>(wthis.lock());
                if (not this_) {
                    return (size_t)0;
                }

                if (!buf) {
                    return len;
                }

                // TODO ->deviceState to .state
                if (ctx->addDevCtx->state == AuthDecodingState::GENERIC_ERROR) {
                    return len;
                }

                decodeCtx->pac.reserve_buffer(len); // TODO rework like this

                std::copy_n(buf, len, decodeCtx->pac.buffer());
                decodeCtx->pac.buffer_consumed(len);

                // handle unpacking the data from the peer
                msgpack::object_handle oh;
                AuthMsg msg;
                try {
                    decodeCtx->pac.next(oh);
                    oh.get().convert(msg);
                } catch (std::exception e) {
                    ctx->addDevCtx->state = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
                    // TODO rewrite msg to mirror better example that is more vague
                    JAMI_WARNING("[LinkDevice] error unpacking message from msgpack"); // also warn in logs
                }

                if (msg.schemeId != 0) {
                    JAMI_WARNING("[LinkDevice] Unsupported scheme received from a connection.");
                    ctx->addDevCtx->state = AuthDecodingState::GENERIC_ERROR; // set the generic error state in the context
                }

                if (ctx->addDevCtx->state == AuthDecodingState::GENERIC_ERROR) {
                    JAMI_WARNING("[LinkDevice] Undefined behavior encountered during a link auth session.");
                    ctx->addDevCtx->channel->shutdown();
                    emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                        ctx->accountId,
                        ctx->token,
                        static_cast<uint8_t>(DeviceAuthState::DONE),
                        "failure"
                    ); // let the client know that the auth has failed
                }
                else if (ctx->addDevCtx->state == AuthDecodingState::CREDENTIALS) {
                    // receive the incoming password, check if the password is right, and send back the archive if it is correct
                    AuthMsg toSend;
                    AccountArchive archive;
                    try {
                        toSend.archive = fileutils::readArchive(this_->path_, fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD, msg.payload["password"]).data;
                        ctx->addDevCtx->archiveTransferredWithoutFailure = true;
                    } catch (...) {
                        if (ctx->addDevCtx->numTries < ctx->addDevCtx->maxTries) {
                            JAMI_DEBUG("[LinkDevice] Incorrect password was submitted to this server... allowing a retry.");
                            toSend.payload["passwordCorrect"] = "false";
                            toSend.payload["canRetry"] = "true";
                        } else {
                            JAMI_DEBUG("[LinkDevice] Incorrect password was submitted to this server... NOT allowing a retry because threshold already reached!");
                            toSend.payload["canRetry"] = "false";
                        }
                    }
                    // linkDevMsg(ctx->addDevCtx->channel, &toSend);
                    msgpack::sbuffer buffer(UINT16_MAX);
                    msgpack::pack(buffer, toSend);
                    std::error_code ec;
                    ctx->addDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
                }
                else if (ctx->addDevCtx->state == AuthDecodingState::ARCHIVE) {
                    // get whether the operation on newDev succeeded or failed from the payload and set the flag, then close the channel so that core can emit an onShutdown signal
                    ctx->addDevCtx->channel->shutdown();
                }
                else if (ctx->addDevCtx->state == AuthDecodingState::AUTH_ERROR) {
                    // update the flag and
                    // read local flag of whether the operation on newDev succeeded or failed from the payload, then close the channel so that core can emit an onShutdown signal
                    JAMI_WARN("[LinkDevice] Isssue with authentication.");
                    ctx->addDevCtx->channel->shutdown();
                    emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                        ctx->accountId,
                        ctx->token,
                        static_cast<uint8_t>(DeviceAuthState::DONE),
                        "auth_error"
                    ); // let the client know that the auth has failed
                }
                else {}
            return len;
        } // !channel->onRecv callback end
    ); // !channel onRecv closure
}

void
ArchiveAccountManager::loadFromDHT(const std::shared_ptr<AuthContext>& ctx)
{
    ctx->dhtContext = std::make_unique<DhtLoadContext>();
    ctx->dhtContext->dht.run(ctx->credentials->dhtPort, {}, true);
    for (const auto& bootstrap : ctx->credentials->dhtBootstrap) {
        ctx->dhtContext->dht.bootstrap(bootstrap);
    }

    auto searchEnded = [ctx]() {
        if (not ctx->dhtContext or ctx->dhtContext->found) {
            return;
        }
        auto& s = *ctx->dhtContext;
        if (s.stateOld.first && s.stateNew.first) {
            dht::ThreadPool::computation().run(
                [ctx, network_error = !s.stateOld.second && !s.stateNew.second] {
                    ctx->dhtContext.reset();
                    JAMI_WARNING("[Auth] failure looking for archive on DHT: %s",
                              /**/ network_error ? "network error" : "not found");
                    ctx->onFailure(network_error ? AuthError::NETWORK : AuthError::UNKNOWN, "");
                });
        }
    };

    auto search = [ctx, searchEnded, w = weak_from_this()](bool previous) {
        std::vector<uint8_t> key;
        dht::InfoHash loc;
        auto& s = previous ? ctx->dhtContext->stateOld : ctx->dhtContext->stateNew;

        // compute archive location and decryption keys
        try {
            std::tie(key, loc) = computeKeys(ctx->credentials->password,
                                             ctx->credentials->uri,
                                             previous);
            JAMI_DBG("[Auth] trying to load account from DHT with %s at %s",
                     /**/ ctx->credentials->uri.c_str(),
                     loc.toString().c_str());
            if (not ctx->dhtContext or ctx->dhtContext->found) {
                return;
            }
            ctx->dhtContext->dht.get(
                loc,
                [ctx, key = std::move(key), w](const std::shared_ptr<dht::Value>& val) {
                    std::vector<uint8_t> decrypted;
                    try {
                        decrypted = archiver::decompress(dht::crypto::aesDecrypt(val->data, key));
                    } catch (const std::exception& ex) {
                        return true;
                    }
                    JAMI_DBG("[Auth] found archive on the DHT");
                    ctx->dhtContext->found = true;
                    dht::ThreadPool::computation().run([ctx, decrypted = std::move(decrypted), w] {
                        try {
                            auto archive = AccountArchive(decrypted);
                            if (auto sthis = std::static_pointer_cast<ArchiveAccountManager>(
                                    w.lock())) {
                                if (ctx->dhtContext) {
                                    ctx->dhtContext->dht.join();
                                    ctx->dhtContext.reset();
                                }
                                sthis->onArchiveLoaded(*ctx,
                                                       std::move(
                                                           archive) /*, std::move(contacts)*/);
                            }
                        } catch (const std::exception& e) {
                            ctx->onFailure(AuthError::UNKNOWN, "");
                        }
                    });
                    return not ctx->dhtContext->found;
                },
                [=, &s](bool ok) {
                    JAMI_DBG("[Auth] DHT archive search ended at %s", /**/ loc.toString().c_str());
                    s.first = true;
                    s.second = ok;
                    searchEnded();
                });
        } catch (const std::exception& e) {
            // JAMI_ERR("Error computing kedht::ThreadPool::computation().run(ys: %s", e.what());
            s.first = true;
            s.second = true;
            searchEnded();
            return;
        }
    };
    dht::ThreadPool::computation().run(std::bind(search, true));
    dht::ThreadPool::computation().run(std::bind(search, false));
}

void
ArchiveAccountManager::migrateAccount(AuthContext& ctx)
{
    JAMI_WARN("[Auth] account migration needed");
    AccountArchive archive;
    try {
        archive = readArchive(ctx.credentials->password_scheme, ctx.credentials->password);
    } catch (...) {
        JAMI_DBG("[Auth] Can't load archive");
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, "");
        return;
    }

    updateArchive(archive);

    if (updateCertificates(archive, ctx.credentials->updateIdentity)) {
        // because updateCertificates already regenerate a device, we do not need to
        // regenerate one in onArchiveLoaded
        onArchiveLoaded(ctx, std::move(archive));
    } else {
        ctx.onFailure(AuthError::UNKNOWN, "");
    }
}

// KESS is it necessary to call this for linkdevice?
void
ArchiveAccountManager::onArchiveLoaded(AuthContext& ctx,
                                       AccountArchive&& a)
{
    auto ethAccount = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();
    dhtnet::fileutils::check_dir(path_, 0700);

    a.save(fileutils::getFullPath(path_, archivePath_), ctx.credentials ? ctx.credentials->password_scheme : "", ctx.credentials ? ctx.credentials->password : "");

    if (not a.id.second->isCA()) {
        JAMI_ERR("[Auth] trying to sign a certificate with a non-CA.");
    }

    std::shared_ptr<dht::crypto::Certificate> deviceCertificate;
    std::unique_ptr<ContactList> contacts;
    auto usePreviousIdentity = false;
    // If updateIdentity got a valid certificate, there is no need for a new cert
    if (auto oldId = ctx.credentials->updateIdentity.second) {
        contacts = std::make_unique<ContactList>(ctx.accountId, oldId, path_, onChange_);
        if (contacts->isValidAccountDevice(*oldId) && ctx.credentials->updateIdentity.first) {
            deviceCertificate = oldId;
            usePreviousIdentity = true;
            JAMI_WARN("[Auth] Using previously generated certificate %s",
                                          deviceCertificate->getLongId().toString().c_str());
        } else {
            contacts.reset();
        }
    }

    // Generate a new device if needed
    if (!deviceCertificate) {
        JAMI_WARN("[Auth] creating new device certificate");
        auto request = ctx.request.get();
        if (not request->verify()) {
            JAMI_ERR("[Auth] Invalid certificate request.");
            ctx.onFailure(AuthError::INVALID_ARGUMENTS, "");
            return;
        }
        deviceCertificate = std::make_shared<dht::crypto::Certificate>(dht::crypto::Certificate::generate(*request, a.id));
        JAMI_WARNING("[Auth] created new device: {}",
                  deviceCertificate->getLongId());
    }

    auto receipt = makeReceipt(a.id, *deviceCertificate, ethAccount);
    auto receiptSignature = a.id.first->sign({receipt.first.begin(), receipt.first.end()});

    auto info = std::make_unique<AccountInfo>();
    auto pk = usePreviousIdentity ? ctx.credentials->updateIdentity.first : ctx.key.get();
    auto sharedPk = pk->getSharedPublicKey();
    info->identity.first = pk;
    info->identity.second = deviceCertificate;
    info->accountId = a.id.second->getId().toString();
    info->devicePk = sharedPk;
    info->deviceId = info->devicePk->getLongId().toString();
    if (ctx.deviceName.empty())
        ctx.deviceName = info->deviceId.substr(8);

    if (!contacts) {
        contacts = std::make_unique<ContactList>(ctx.accountId, a.id.second, path_, onChange_);
    }
    info->contacts = std::move(contacts);
    info->contacts->setContacts(a.contacts);
    info->contacts->foundAccountDevice(deviceCertificate, ctx.deviceName, clock::now());
    info->ethAccount = ethAccount;
    info->announce = std::move(receipt.second);
    ConversationModule::saveConvInfosToPath(path_, a.conversations);
    ConversationModule::saveConvRequestsToPath(path_, a.conversationsRequests);
    info_ = std::move(info);

    ctx.onSuccess(*info_,
                  std::move(a.config),
                  std::move(receipt.first),
                  std::move(receiptSignature));
}

std::pair<std::vector<uint8_t>, dht::InfoHash>
ArchiveAccountManager::computeKeys(const std::string& password,
                                   const std::string& pin,
                                   bool previous)
{
    // Compute time seed
    auto now = std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch());
    auto tseed = now.count() / std::chrono::seconds(EXPORT_KEY_RENEWAL_TIME).count();
    if (previous)
        tseed--;
    std::ostringstream ss;
    ss << std::hex << tseed;
    auto tseed_str = ss.str();

    // Generate key for archive encryption, using PIN as the salt
    std::vector<uint8_t> salt_key;
    salt_key.reserve(pin.size() + tseed_str.size());
    salt_key.insert(salt_key.end(), pin.begin(), pin.end());
    salt_key.insert(salt_key.end(), tseed_str.begin(), tseed_str.end());
    auto key = dht::crypto::stretchKey(password, salt_key, 256 / 8);

    // Generate public storage location as SHA1(key).
    auto loc = dht::InfoHash::get(key);

    return {key, loc};
}

std::pair<std::string, std::shared_ptr<dht::Value>>
ArchiveAccountManager::makeReceipt(const dht::crypto::Identity& id,
                                   const dht::crypto::Certificate& device,
                                   const std::string& ethAccount)
{
    JAMI_DBG("[Auth] signing device receipt");
    auto devId = device.getId();
    DeviceAnnouncement announcement;
    announcement.dev = devId;
    announcement.pk = device.getSharedPublicKey();
    dht::Value ann_val {announcement};
    ann_val.sign(*id.first);

    auto packedAnnoucement = ann_val.getPacked();
    JAMI_DBG("[Auth] device announcement size: %zu", packedAnnoucement.size());

    std::ostringstream is;
    is << "{\"id\":\"" << id.second->getId() << "\",\"dev\":\"" << devId << "\",\"eth\":\""
       << ethAccount << "\",\"announce\":\"" << base64::encode(packedAnnoucement) << "\"}";

    // auto announce_ = ;
    return {is.str(), std::make_shared<dht::Value>(std::move(ann_val))};
}

bool
ArchiveAccountManager::needsMigration(const dht::crypto::Identity& id)
{
    if (not id.second)
        return false;
    auto cert = id.second->issuer;
    while (cert) {
        if (not cert->isCA()) {
            JAMI_WARN("certificate %s is not a CA, needs update.", cert->getId().toString().c_str());
            return true;
        }
        if (cert->getExpiration() < clock::now()) {
            JAMI_WARN("certificate %s is expired, needs update.", cert->getId().toString().c_str());
            return true;
        }
        cert = cert->issuer;
    }
    return false;
}

void
ArchiveAccountManager::syncDevices()
{
    if (!dht_ or !dht_->isRunning()) {
        JAMI_WARNING("Not syncing devices: DHT is not running");
        return;
    }
    JAMI_DBG("Building device sync from %s", info_->deviceId.c_str());
    auto sync_data = info_->contacts->getSyncData();

    for (const auto& dev : getKnownDevices()) {
        // don't send sync data to ourself
        if (dev.first.toString() == info_->deviceId) {
            continue;
        }
        if (!dev.second.certificate) {
            JAMI_WARNING("Cannot find certificate for {}", dev.first);
            continue;
        }
        auto pk = dev.second.certificate->getSharedPublicKey();
        JAMI_DBG("sending device sync to %s %s",
                 dev.second.name.c_str(),
                 dev.first.toString().c_str());
        auto syncDeviceKey = dht::InfoHash::get("inbox:" + pk->getId().toString());
        dht_->putEncrypted(syncDeviceKey, pk, sync_data);
    }
}

void
ArchiveAccountManager::startSync(const OnNewDeviceCb& cb, const OnDeviceAnnouncedCb& dcb, bool publishPresence)
{
    AccountManager::startSync(std::move(cb), std::move(dcb), publishPresence);

    dht_->listen<DeviceSync>(
        dht::InfoHash::get("inbox:" + info_->devicePk->getId().toString()),
        [this](DeviceSync&& sync) {
            // Received device sync data.
            // check device certificate
            findCertificate(sync.from,
                            [this,
                             sync](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                                if (!cert or cert->getId() != sync.from) {
                                    JAMI_WARN("Can't find certificate for device %s",
                                              sync.from.toString().c_str());
                                    return;
                                }
                                if (not foundAccountDevice(cert))
                                    return;
                                onSyncData(std::move(sync));
                            });

            return true;
        });
}

AccountArchive
ArchiveAccountManager::readArchive(std::string_view scheme, const std::string& pwd) const
{
    JAMI_DBG("[Auth] reading account archive");
    return AccountArchive(fileutils::getFullPath(path_, archivePath_), scheme, pwd);
}

void
ArchiveAccountManager::updateArchive(AccountArchive& archive) const
{
    using namespace libjami::Account::ConfProperties;

    // Keys not exported to archive
    static const auto filtered_keys = {Ringtone::PATH,
                                       ARCHIVE_PATH,
                                       DEVICE_ID,
                                       DEVICE_NAME,
                                       Conf::CONFIG_DHT_PORT,
                                       DHT_PROXY_LIST_URL,
                                       AUTOANSWER,
                                       PROXY_ENABLED,
                                       PROXY_SERVER,
                                       PROXY_PUSH_TOKEN};

    // Keys with meaning of file path where the contents has to be exported in base64
    static const auto encoded_keys = {TLS::CA_LIST_FILE,
                                      TLS::CERTIFICATE_FILE,
                                      TLS::PRIVATE_KEY_FILE};

    JAMI_DBG("[Auth] building account archive");
    for (const auto& it : onExportConfig_()) {
        // filter-out?
        if (std::any_of(std::begin(filtered_keys), std::end(filtered_keys), [&](const auto& key) {
                return key == it.first;
            }))
            continue;

        // file contents?
        if (std::any_of(std::begin(encoded_keys), std::end(encoded_keys), [&](const auto& key) {
                return key == it.first;
            })) {
            try {
                archive.config.emplace(it.first, base64::encode(fileutils::loadFile(it.second)));
            } catch (...) {
            }
        } else
            archive.config[it.first] = it.second;
    }
    if (info_) {
        // If migrating from same archive, info_ will be null
        archive.contacts = info_->contacts->getContacts();
        // Note we do not know accountID_ here, use path
        archive.conversations = ConversationModule::convInfosFromPath(path_);
        archive.conversationsRequests = ConversationModule::convRequestsFromPath(path_);
    }
}

void
ArchiveAccountManager::saveArchive(AccountArchive& archive, std::string_view scheme, const std::string& pwd)
{
    try {
        updateArchive(archive);
        if (archivePath_.empty())
            archivePath_ = "export.gz";
        archive.save(fileutils::getFullPath(path_, archivePath_), scheme, pwd);
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("[Auth] Can't export archive: %s", ex.what());
        return;
    }
}

bool
ArchiveAccountManager::changePassword(const std::string& password_old,
                                      const std::string& password_new)
{
    try {
        auto path = fileutils::getFullPath(path_, archivePath_);
        AccountArchive(path, fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD, password_old)
            .save(path, fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD, password_new);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<uint8_t>
ArchiveAccountManager::getPasswordKey(const std::string& password)
{
    try {
        auto data = dhtnet::fileutils::loadFile(fileutils::getFullPath(path_, archivePath_));
        // Try to decrypt to check if password is valid
        auto key = dht::crypto::aesGetKey(data, password);
        auto decrypted = dht::crypto::aesDecrypt(dht::crypto::aesGetEncrypted(data), key);
        return key;
    } catch (const std::exception& e) {
        JAMI_ERR("Error loading archive: %s", e.what());
    }
    return {};
}

bool
ArchiveAccountManager::revokeDevice(const std::string& device,
                                    std::string_view scheme,
                                    const std::string& password,
                                    RevokeDeviceCallback cb)
{
    auto fa = dht::ThreadPool::computation().getShared<AccountArchive>(
        [this, scheme=std::string(scheme), password] { return readArchive(scheme, password); });
    findCertificate(DeviceId(device),
        [fa = std::move(fa), scheme=std::string(scheme), password, device, cb, w=weak_from_this()](
            const std::shared_ptr<dht::crypto::Certificate>& crt) mutable {
                if (not crt) {
                    cb(RevokeDeviceResult::ERROR_NETWORK);
                    return;
                }
                auto this_ = std::static_pointer_cast<ArchiveAccountManager>(w.lock());
                if (not this_) return;
                this_->info_->contacts->foundAccountDevice(crt);
                AccountArchive a;
                try {
                    a = fa.get();
                } catch (...) {
                    cb(RevokeDeviceResult::ERROR_CREDENTIALS);
                    return;
                }
                // Add revoked device to the revocation list and resign it
                if (not a.revoked)
                    a.revoked = std::make_shared<decltype(a.revoked)::element_type>();
                a.revoked->revoke(*crt);
                a.revoked->sign(a.id);
                // add to CRL cache
                this_->certStore().pinRevocationList(a.id.second->getId().toString(), a.revoked);
                this_->certStore().loadRevocations(*a.id.second);

                // Announce CRL immediately
                auto h = a.id.second->getId();
                this_->dht_->put(h, a.revoked, dht::DoneCallback {}, {}, true);

                this_->saveArchive(a, scheme, password);
                this_->info_->contacts->removeAccountDevice(crt->getLongId());
                cb(RevokeDeviceResult::SUCCESS);
                this_->syncDevices();
            });
    return false;
}

bool
ArchiveAccountManager::exportArchive(const std::string& destinationPath, std::string_view scheme, const std::string& password)
{
    try {
        // Save contacts if possible before exporting
        AccountArchive archive = readArchive(scheme, password);
        updateArchive(archive);
        auto archivePath = fileutils::getFullPath(path_, archivePath_);
        archive.save(archivePath, scheme, password);

        // Export the file
        std::error_code ec;
        std::filesystem::copy_file(archivePath, destinationPath, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec;
    } catch (const std::runtime_error& ex) {
        JAMI_ERR("[Auth] Can't export archive: %s", ex.what());
        return false;
    } catch (...) {
        JAMI_ERR("[Auth] Can't export archive: can't read archive");
        return false;
    }
}

bool
ArchiveAccountManager::isPasswordValid(const std::string& password)
{
    try {
        readArchive(fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD, password);
        return true;
    } catch (...) {
        return false;
    }
}

#if HAVE_RINGNS
void
ArchiveAccountManager::registerName(const std::string& name,
                                    std::string_view scheme,
                                    const std::string& password,
                                    RegistrationCallback cb)
{
    std::string signedName;
    auto nameLowercase {name};
    std::transform(nameLowercase.begin(), nameLowercase.end(), nameLowercase.begin(), ::tolower);
    std::string publickey;
    std::string accountId;
    std::string ethAccount;

    try {
        auto archive = readArchive(scheme, password);
        auto privateKey = archive.id.first;
        const auto& pk = privateKey->getPublicKey();
        publickey = pk.toString();
        accountId = pk.getId().toString();
        signedName = base64::encode(
            privateKey->sign(std::vector<uint8_t>(nameLowercase.begin(), nameLowercase.end())));
        ethAccount = dev::KeyPair(dev::Secret(archive.eth_key)).address().hex();
    } catch (const std::exception& e) {
        // JAMI_ERR("[Auth] can't export account: %s", e.what());
        cb(NameDirectory::RegistrationResponse::invalidCredentials);
        return;
    }

    nameDir_.get().registerName(accountId, nameLowercase, ethAccount, cb, signedName, publickey);
}
#endif

} // namespace jami
