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

#include "config/yamlparser.h"
// #include <thread>
// #include <chrono>
// #include <string_view>

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

    if (ctx->credentials->scheme == "p2p") {
        // newDev
        JAMI_DEBUG("[LinkDevice] BP 2 | Importing account via p2p scheme.");
        JAMI_DEBUG("[LinkDevice] Importing account via p2p scheme.");
        startLoadArchiveFromDevice(ctx);
        return;
    }

    dht::ThreadPool::computation().run([ctx = std::move(ctx), wthis = weak_from_this()] {
        auto this_ = std::static_pointer_cast<ArchiveAccountManager>(wthis.lock());
        if (not this_)
            return;
        try {
            if (ctx->credentials->scheme == "file") {
                // Import from external archive
                this_->loadFromFile(*ctx);
            } else {
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
                    this_->onArchiveLoaded(*ctx, std::move(a), false);
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
ArchiveAccountManager::setValidity(std::string_view scheme,
                                   const std::string& password,
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
    onArchiveLoaded(ctx, std::move(a), false);
}

void
ArchiveAccountManager::loadFromFile(AuthContext& ctx)
{
    JAMI_WARNING("[Auth] loading archive from: {}", ctx.credentials->uri);
    AccountArchive archive;
    try {
        archive = AccountArchive(ctx.credentials->uri,
                                 ctx.credentials->password_scheme,
                                 ctx.credentials->password);
    } catch (const std::exception& ex) {
        JAMI_WARNING("[Auth] can't read file: {}", ex.what());
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, ex.what());
        return;
    }
    onArchiveLoaded(ctx, std::move(archive), false);
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
enum class AuthDecodingState : uint8_t {
    HANDSHAKE = 0,
    EST,
    AUTH,
    DATA,
    ERR,
    AUTH_ERROR,
    DONE
};

// enum class AuthDecodingState : uint8_t {
//     HANDSHAKE = 0, // for when devices are first establishing ground rules... should only be used on first msg
//     EST, // replaces SCHEME_KNOWN and is used for general communication
//     // ESTABLISHED = 0,
//     AUTH, // replaces CREDENTIALS and can be used more generically for authenticating a user's will
//     // CREDENTIALS,
//     DATA, // replaces ARCHIVE and can be used for generic transfer of large files... not used for now but can be used later for wireless camera feature
//     // ARCHIVE,
//     // SCHEME_KNOWN, // is not used in v1 but would be useful for supporting multiple protocols
//     ERR, // replaces GENERIC_ERROR
//     // GENERIC_ERROR,
//     AUTH_ERROR,
//     DONE // handle closing channel
// };

// used for status codes on DeviceAuthStateChanged
enum class DeviceAuthState : uint8_t {
    NONE = 0,
    TOKEN_AVAIL = 1,
    CONNECTING = 2,
    AUTH = 3,
    DONE = 4,
    ERROR = 5
};

/** Holds state for NEW device peer import logic */
struct ArchiveAccountManager::LinkDeviceContext
{
    dht::crypto::Identity tmpId;
    uint64_t opId;

    // TODO only use this for newDev and use JamiAccount ptr for oldDev
    dhtnet::ConnectionManager tempConnMgr;
    AuthDecodingState state {AuthDecodingState::HANDSHAKE};

    unsigned numOpenChannels {0};
    unsigned maxOpenChannels {1};

    std::string scheme;
    std::string authScheme {fileutils::ARCHIVE_AUTH_SCHEME_NONE};
    std::string passwordFromUser {""}; // DANGEROUS?
    // std::string scheme {"0"};
    bool authEnabled {false};
    bool archiveTransferredWithoutFailure {false};
    std::string accData; // serialized account data

    LinkDeviceContext(dht::crypto::Identity id)
        : tmpId(std::move(id))
        , tempConnMgr(tmpId)
    {}

    std::shared_ptr<dhtnet::ChannelSocket> channel;

    std::string formattedAuthState()
    {
        switch (this->state) {
        case AuthDecodingState::HANDSHAKE:
            return "HANDSHAKE";
        case AuthDecodingState::EST:
            return "EST";
        case AuthDecodingState::AUTH:
            return "AUTH";
        case AuthDecodingState::DATA:
            return "DATA";
        case AuthDecodingState::ERR:
            return "ERR";
        case AuthDecodingState::AUTH_ERROR:
            return "AUTH_ERROR";
        default:
            return "INVALID ENUM";
        }
    }

    // TODO unify these two structs and implement authmsg and formatauthstate for both in parent struct
};

/** Holds state for OLD device peer import logic */
struct ArchiveAccountManager::PeerLoadContext
{
    uint64_t opId;
    AuthDecodingState state {AuthDecodingState::EST};
    std::string scheme;
    // std::string scheme {"0"};
    bool authEnabled {false};
    bool archiveTransferredWithoutFailure {false};
    std::string accData; // serialized account data
    std::string credentials;

    // TODO pack this into a single int with bitwise arithmetic
    // unsigned failedPasswordAttempts {0};
    unsigned numTries {0};
    unsigned maxTries {3};

    PeerLoadContext(std::shared_ptr<dhtnet::ChannelSocket> c)
        : channel(std::move(c))
    {}

    std::shared_ptr<dhtnet::ChannelSocket> channel;

    std::string formattedAuthState()
    {
        switch (this->state) {
        case AuthDecodingState::HANDSHAKE:
            return "HANDSHAKE";
        case AuthDecodingState::EST:
            return "EST";
        case AuthDecodingState::AUTH:
            return "AUTH";
        case AuthDecodingState::DATA:
            return "DATA";
        case AuthDecodingState::ERR:
            return "ERR";
        case AuthDecodingState::AUTH_ERROR:
            return "AUTH_ERROR";
        default:
            return "INVALID ENUM";
        }
    }
};

struct ArchiveAccountManager::AuthMsg
{
    uint8_t schemeId {0};
    std::map<std::string, std::string> payload;
    // std::vector<uint8_t> accData;
    // std::vector<uint8_t> archive;
    MSGPACK_DEFINE_MAP(schemeId, payload)
};

// linkdevice: newDev: called when user enters their password
// TODO implement with key as bytes instead of string for multiple authentication schemes
bool
ArchiveAccountManager::provideAccountAuthentication(const std::string& passwordFromUser,
                                                    const std::string& scheme)
{
    JAMI_DEBUG("[LinkDevice] ArhiveAccountManager::provideAccountAuthentication");

    if (scheme != fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD) {
        JAMI_ERROR("[LinkDevice] Unsupported account authentication scheme attempted.");
        return false;
    }

    AuthMsg toSend;
    toSend.payload["password"] = std::move(passwordFromUser);
    msgpack::sbuffer buffer(UINT16_MAX);
    msgpack::pack(buffer, toSend);
    std::error_code ec = std::make_error_code(std::errc(AuthDecodingState::ERR));
    bool retVal = false;
    try {
        if (auto channel = authChannel_.lock()) {
            JAMI_DEBUG("[LinkDevice] NEW: Sending password to old device.");
            channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            retVal = true;
        }
    } catch (std::exception e) {
        JAMI_WARNING("[LinkDevice] Failed to send password over auth ChannelSocket.");
    }
    return retVal;
}

// TODO organize ordering of all functions linkdevice
// linkdevice: helper for receiving messages
struct ArchiveAccountManager::DecodingContext
{
    msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                           nullptr,
                           512};
};

// link device: newDev: creates a new temporary account on the DHT for establishing a TLS connection
void
ArchiveAccountManager::startLoadArchiveFromDevice(const std::shared_ptr<AuthContext>& ctx)
{
    JAMI_DEBUG("[LinkDevice] ArchiveAccountManager::startLoadArchiveFromDevice");
    dht::ThreadPool::computation().run([this, ctx = std::move(ctx), wthis = weak_from_this()] {
        JAMI_DEBUG(
            "[LinkDevice] ArchiveAccountManager::startLoadArchiveFromDevice::generateECIdentity");
        auto ca = dht::crypto::generateEcIdentity("Jami Temporary CA");
        if (!ca.first || !ca.second) {
            throw std::runtime_error("[LinkDevice] Can't generate CA for this account.");
        }
        JAMI_DEBUG(
            "[LinkDevice] ArchiveAccountManager::startLoadArchiveFromDevice::generateIdentity");
        auto user = dht::crypto::generateIdentity("Jami Temporary User", ca, 4096, true);
        if (!user.first || !user.second) {
            throw std::runtime_error("[LinkDevice] Can't generate identity for this account.");
        }
        ctx->linkDevCtx = std::make_unique<LinkDeviceContext>(
            dht::crypto::generateIdentity("Jami Temporary device", user));
        // set up auth channel code and also use it as opId
        JAMI_DEBUG("[LinkDevice {}] "
                   "ArchiveAccountManager::startLoadArchiveFromDevice::getSeededRandomEngine",
                   ctx->linkDevCtx->tmpId.second->getId());
        auto gen = Manager::instance().getSeededRandomEngine();
        JAMI_DEBUG("[LinkDevice {}] "
                   "ArchiveAccountManager::startLoadArchiveFromDevice::uniform_int_distribution",
                   ctx->linkDevCtx->tmpId.second->getId());
        ctx->linkDevCtx->opId = std::uniform_int_distribution<uint64_t>(100000, 999999)(gen);
        ctx->linkDevCtx->tempConnMgr.onDhtConnected(ctx->linkDevCtx->tmpId.second->getPublicKey());

        auto accountScheme = fmt::format("jami-auth://{}/{}",
            ctx->linkDevCtx->tmpId.second->getId(),
            ctx->linkDevCtx->opId);
        // makes sure certificate is loaded from the proper
        auto receipt = makeReceipt(user, *ctx->linkDevCtx->tmpId.second, "");
        dht::DhtRunner::Context context {};
        context.identityAnnouncedCb = [=](bool ok) {
            ctx->dhtContext->dht.put(
                ctx->linkDevCtx->tmpId.second->getId(),
                receipt,
                [=](bool ok) {
                    emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                        ctx->accountId,
                        static_cast<uint8_t>(AuthDecodingState::HANDSHAKE),
                        accountScheme);
                },
                {},
                true);
        };

        ctx->dhtContext = std::make_unique<DhtLoadContext>();
        dht::DhtRunner::Config config {};
        config.dht_config.id = user;
        ctx->dhtContext->dht.run(ctx->credentials->dhtPort, config, std::move(context));
        ctx->dhtContext->dht.bootstrap("bootstrap.jami.net", "4222");

        ctx->linkDevCtx->tempConnMgr.onICERequest([&](const DeviceId& deviceId) {
            return true;
        });

        ctx->linkDevCtx->tempConnMgr.onChannelRequest(
            [wthis, ctx](const std::shared_ptr<dht::crypto::Certificate>& cert,
                         const std::string& name) {

                constexpr auto AUTH_SCHEME = "auth:"sv;
                std::string_view url(name);
                auto sep1 = url.find(AUTH_SCHEME);
                if (sep1 == std::string_view::npos) {
                    JAMI_WARNING("[LinkDevice] Temporary connection manager received invalid scheme: {}", name);
                    return false;
                }
                auto opStr = url.substr(sep1 + AUTH_SCHEME.size());
                auto parsedOpId = jami::to_int<uint64_t>(opStr);

                if (ctx->linkDevCtx->opId == parsedOpId
                    && ctx->linkDevCtx->numOpenChannels < ctx->linkDevCtx->maxOpenChannels) {
                    ctx->linkDevCtx->numOpenChannels++;
                    JAMI_DEBUG("[LinkDevice] Opening channel ({}/{}): {}",
                               ctx->linkDevCtx->numOpenChannels,
                               ctx->linkDevCtx->maxOpenChannels,
                               name);
                    return true;
                }
                return false;
            });

        ctx->linkDevCtx->tempConnMgr.onConnectionReady([ctx, accountScheme, wthis](
                                                           const DeviceId& deviceId,
                                                           const std::string& name,
                                                           std::shared_ptr<dhtnet::ChannelSocket> socket) {
            ctx->linkDevCtx->channel = socket;

            socket->onShutdown([ctx, deviceId, name]() {
                JAMI_WARNING("[LinkDevice] Temporary connection manager closing socket: {}", name);
                ctx->linkDevCtx->numOpenChannels--;
            });

            socket->setOnRecv([wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket), ctx, decodingCtx = std::make_shared<DecodingContext>(), wthis](const uint8_t* buf, size_t len) {
                if (!buf) {
                    return len;
                }


                // decode msg
                decodingCtx->pac.reserve_buffer(len);
                std::copy_n(buf, len, decodingCtx->pac.buffer());
                decodingCtx->pac.buffer_consumed(len);
                msgpack::object_handle oh;
                AuthMsg toRecv;
                try {
                    JAMI_DEBUG("[LinkDevice] NEW: Unpacking message.");
                    decodingCtx->pac.next(oh);
                    oh.get().convert(toRecv);
                } catch (std::exception e) {
                    ctx->linkDevCtx->state = AuthDecodingState::ERR; // set the generic error state in the context
                    JAMI_ERROR("[LinkDevice] Error unpacking message from Old Device.");
                }

                {
                    // print out contents of the protocol message for debugging purposes
                    std::string logStr = "[LinkDevice] RECEIVED MSG\nOLD->NEW\n=========\n";
                    logStr += fmt::format("scheme: {}\n", toRecv.schemeId);
                    for (const auto& [msgKey, msgVal] : toRecv.payload) {
                        logStr += fmt::format(" - {}: {}\n", msgKey, msgVal);
                    }
                    if (toRecv.payload["accData"] != "") {
                        logStr += fmt::format(" - accData: {}\n", toRecv.payload["accData"]);
                    }
                    logStr += "=========";
                    JAMI_DEBUG("[LinkDevice] NEW: Successfdully unpacked message from OLD\n{}", logStr);
                }


                // begin new rewrite
                if (toRecv.schemeId != 0) {
                    JAMI_WARNING("[LinkDevice] NEW: Unsupported scheme received from a connection.");
                    ctx->linkDevCtx->state
                        = AuthDecodingState::ERR; // set the generic error state in the context
                }

                if (toRecv.schemeId == 0) {
                    // handle the protocol logic KESS
                    // logic
                    AuthMsg toSend;
                    bool shouldSendMsg = false;
                    bool shouldShutDown = false;

                    JAMI_DEBUG("[LinkDevice] NEW: State is {}:{}", ctx->linkDevCtx->scheme, ctx->linkDevCtx->formattedAuthState());
                    if (ctx->linkDevCtx->state == AuthDecodingState::HANDSHAKE) {
                        ctx->linkDevCtx->state = AuthDecodingState::EST;
                        JAMI_DEBUG("[LinkDevice] NEW: Switching to 0:EST");
                        shouldSendMsg = true;
                    }
                    else if (ctx->linkDevCtx->state == AuthDecodingState::EST) {
                        ctx->linkDevCtx->authScheme = toRecv.payload["authScheme"]; // for loading account archive later
                        // establish password type and either enter AUTH or DATA
                        ctx->linkDevCtx->authEnabled = !toRecv.payload["authScheme"].empty() && toRecv.payload["authScheme"] != "none";
                        if (ctx->linkDevCtx->authEnabled) {
                            JAMI_DEBUG("[LinkDevice] NEW: Auth is enabled.");
                            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                                ctx->accountId,
                                ctx->token,
                                static_cast<uint8_t>(DeviceAuthState::AUTH),
                                "archive_with_auth"
                            );
                        } else {
                            JAMI_DEBUG("[LinkDevice] NEW: auth is NOT enabled.");
                            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                                ctx->accountId,
                                ctx->token,
                                static_cast<uint8_t>(DeviceAuthState::AUTH),
                                "archive_without_auth"
                            );
                            shouldSendMsg = true;
                            toSend.payload["readyToReceiveData"] = "true";
                        }
                        ctx->linkDevCtx->state = ctx->linkDevCtx->authEnabled ? AuthDecodingState::AUTH : AuthDecodingState::DATA;
                    }
                    else if (ctx->linkDevCtx->state == AuthDecodingState::AUTH) {
                        JAMI_DEBUG("[LinkDevice] NEW: auth type is: {}", toRecv.payload["authScheme"]);
                        bool prepareForArchive = toRecv.payload["credentialsValid"] == "true";
                        bool canRetry = toRecv.payload["canRetry"] == "true";
                        ctx->linkDevCtx->state = prepareForArchive ? AuthDecodingState::DATA : (canRetry ? AuthDecodingState::AUTH : AuthDecodingState::AUTH_ERROR);
                        if (prepareForArchive) {
                            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                                ctx->accountId,
                                ctx->token,
                                static_cast<uint8_t>(DeviceAuthState::AUTH),
                                "valid_credentials");
                                shouldSendMsg = true;
                                toSend.payload["readyToReceiveData"] = "true";
                        } else {
                            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                                ctx->accountId,
                                ctx->token,
                                static_cast<uint8_t>(DeviceAuthState::AUTH),
                                "invalid_credentials");
                        }
                    }
                    else if (ctx->linkDevCtx->state == AuthDecodingState::DATA) {
                        // message has arhive... extract it and do jami loading process
                        try {
                            auto archive = AccountArchive(
                                std::string_view(toRecv.payload["accData"])
                            );
                            // auto archive = AccountArchive(static_cast<std::vector<uint8_t>>(toRecv.payload["accData"]), std::vector<uint8_t>());
                            if (auto this_ = std::static_pointer_cast<ArchiveAccountManager>(
                                    wthis.lock())) {
                                JAMI_DEBUG("[LinkDevice] NEW: Loading archive.");
                                // TODO modify this
                                this_->onArchiveLoaded(*ctx, std::move(archive), true);
                                JAMI_DEBUG("[LinkDevice] NEW: Successfully loaded archive.");
                            } else {
                                shouldShutDown = true;
                                JAMI_ERROR("[LinkDevice] NEW: Failed to load account because of null ArchiveAccountManager!");
                            }
                        } catch (const std::exception& e) {
                            shouldShutDown = true;
                            ctx->linkDevCtx->state = AuthDecodingState::ERR;
                            JAMI_WARN("[LinkDevice] NEW: Error reading archive.");
                        }
                    }
                    else {}

                    // END logic
                    if (shouldSendMsg) {

                        {
                            std::string logStr
                                = "[LinkDevice] DEBUGGING NEW->OLD\n=========\n";
                            logStr += fmt::format("scheme: {}\n", toRecv.schemeId);
                            for (const auto& [msgKey, msgVal] : toRecv.payload) {
                                logStr += fmt::format(" - {}: {}\n", msgKey, msgVal);
                            }
                            logStr += "=========";
                            JAMI_DEBUG("[LinkDevice] NEW: trying to write msg: {}", logStr);
                            // you are in startLoadArchiveFromDevice
                        }

                        msgpack::sbuffer buffer(UINT16_MAX);
                        msgpack::pack(buffer, toSend);
                        std::error_code ec;
                        // std::this_thread::sleep_for(std::chrono::seconds(1));
                        ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
                    }

                    if (shouldShutDown) {
                        ctx->linkDevCtx->channel->shutdown();
                    }

                    return len;
                } // END scheme v0

                else {
                    JAMI_WARN("[LinkDevice] NEW: Unsupported linkdev scheme id of {}", toRecv.schemeId);
                    ctx->peerLoadCtx->state = AuthDecodingState::ERR;
                    return (size_t) 0;
                }
                // END new rewrite


                return len;


            }); // !onConnectionReady // TODO emit AuthStateChanged+"connection ready" signal

            ctx->linkDevCtx->state = AuthDecodingState::HANDSHAKE;
            // send first message to establish scheme
            AuthMsg toSend;
            toSend.schemeId = 0; // set latest scheme here
            JAMI_DEBUG("[LinkDevice] NEW: Packing first message for OLD.\nCurrent state is: \n\tauth "
                       "state = {}:{}",
                       toSend.schemeId,
                       ctx->linkDevCtx->formattedAuthState());
            msgpack::sbuffer buffer(UINT16_MAX);
            msgpack::pack(buffer, toSend);
            std::error_code ec;
            ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);


            JAMI_DEBUG(
                "[LinkDevice {}] "
                "ArchiveAccountManager::startLoadArchiveFromDevice::DeviceAuthStateChanged signal",
                ctx->linkDevCtx->tmpId.second->getId());

            JAMI_LOG("[LinkDevice {}] Generated temporary account.",
                     ctx->linkDevCtx->tmpId.second->getId());

            emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                ctx->linkDevCtx->tmpId.second->getId().toString(),
                static_cast<int>(DeviceAuthState::CONNECTING),
                accountScheme);
        });
    });
}

// TODO fix function descriptions and add all as docstring in cxx format
// linkdeviec: oldDev: hook into communication protocol logic once auth_channel_handler::onReady is
// called
// TODO add opId
void
ArchiveAccountManager::addDevice(const std::string& accountId,
                                 uint32_t token,
                                 const std::shared_ptr<dhtnet::ChannelSocket>& channel)
{
    JAMI_DEBUG("[LinkDevice] Setting up addDevice logic on OLD device.");
    // this is where old device context is initiated
    auto ctx = std::make_shared<AuthContext>();
    ctx->peerLoadCtx = std::make_unique<PeerLoadContext>(channel);
    // ctx->peerLoadCtx->state = AuthDecodingState::EST;
    ctx->accountId = accountId;
    ctx->token = token;

    try {
        // send the first message in the TLS connection
        ctx->peerLoadCtx->state = AuthDecodingState::HANDSHAKE;
        ArchiveAccountManager::AuthMsg toSend;
        toSend.schemeId = 0; // set latest scheme here
        JAMI_DEBUG("[LinkDevice] OLD: Packing first message for NEW.\nCurrent state is: \n\tauth "
                   "state = {}:{}",
                   toSend.schemeId,
                   ctx->peerLoadCtx->formattedAuthState());
        msgpack::sbuffer buffer(UINT16_MAX);
        msgpack::pack(buffer, toSend);
        std::error_code ec;
        channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
        JAMI_DEBUG("[LinkDevice] OLD: Successfully wrote first message for NEW.");
    } catch (std::exception e) {
        JAMI_WARNING("[LinkDevice] error sending message on TLS channel.");
    }

    // TODO this may be unecessary duplicate work
    channel->onShutdown([ctx]() {
        JAMI_DEBUG("[LinkDevice] OLD: Shutdown with state {}", ctx->peerLoadCtx->formattedAuthState());
        // check if the archive was successfully loaded and emitSignal
        if (ctx->peerLoadCtx->archiveTransferredWithoutFailure) {
            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                ctx->accountId, ctx->token, static_cast<uint8_t>(DeviceAuthState::DONE), "success");
        } else {
            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                ctx->accountId, ctx->token, static_cast<uint8_t>(DeviceAuthState::DONE), "failure");
        }
    });

    // for now we only have one valid protocol (version is AuthMsg::scheme = 0) but can later
    // add in more schemes inside this callback function
    JAMI_DEBUG("[LinkDevice] Setting up receiving logic callback.");
    channel->setOnRecv([ctx,
                        wthis = weak_from_this(),
                        decodeCtx = std::make_shared<ArchiveAccountManager::DecodingContext>()](
                           const uint8_t* buf, size_t len) {
	JAMI_DEBUG("[LinkDevice] Setting up receiver logic on OLD device.");
        // when archive is sent to newDev we will get back a success or fail response before the
        // connection closes and we need to handle this and pass it to the shutdown callback
        auto this_ = std::static_pointer_cast<ArchiveAccountManager>(wthis.lock());
        if (!this_) {
	    JAMI_ERROR("[LinkDevice] Invalid state for ArchiveAccountManager.");
            return (size_t) 0;
        }

        if (!buf) {
	    JAMI_ERROR("[LinkDevice] Invalid buffer.");
            return (size_t) 0;
            // return len;
        }

        if (ctx->peerLoadCtx->state == AuthDecodingState::ERR) {
	    JAMI_ERROR("[LinkDevice] Error.");
            return (size_t) 0;
            // return len;
        }

        decodeCtx->pac.reserve_buffer(len); // TODO rework like this

        std::copy_n(buf, len, decodeCtx->pac.buffer());
        decodeCtx->pac.buffer_consumed(len);

        // handle unpacking the data from the peer
        JAMI_DEBUG("[LinkDevice] OLD: addDevice: setOnRecv: handling msg from NEW");
        msgpack::object_handle oh;
        AuthMsg toRecv;
        try {
            decodeCtx->pac.next(oh);
            oh.get().convert(toRecv);
            // print out contents of the protocol message for debugging purposes
            {
                std::string logStr = "[LinkDevice] RECEIVED MSG\nNEW->OLD\n=========\n";
                logStr += fmt::format("scheme: {}\n", toRecv.schemeId);
                for (const auto& [msgKey, msgVal] : toRecv.payload) {
                    logStr += fmt::format(" - {}: {}\n", msgKey, msgVal);
                }
                logStr += "=========";
                JAMI_DEBUG("[LinkDevice] OLD: Successfdully unpacked message from NEW\n{}", logStr);
            }
        } catch (std::exception e) {
            ctx->peerLoadCtx->state
                = AuthDecodingState::ERR; // set the generic error state in the context
            // TODO rewrite msg to mirror better example that is more vague
            JAMI_ERROR("[LinkDevice] error unpacking message from msgpack"); // also warn in logs
        }

        // It's possible to start handling different protocol scheme numbers here
        // validate the scheme
        if (toRecv.schemeId != 0) {
            JAMI_WARNING("[LinkDevice] Unsupported scheme received from a connection.");
            ctx->peerLoadCtx->state
                = AuthDecodingState::ERR; // set the generic error state in the context
        }

        if (toRecv.schemeId == 0) {
            AuthMsg toSend;
            bool shouldSendMsg = false;
            bool shouldShutDown = false;

            // logic
            JAMI_DEBUG("[LinkDevice] OLD: State is {}:{}", ctx->peerLoadCtx->scheme, ctx->peerLoadCtx->formattedAuthState());
            if (ctx->peerLoadCtx->state == AuthDecodingState::HANDSHAKE) {
                ctx->peerLoadCtx->state = AuthDecodingState::EST;
                JAMI_DEBUG("[LinkDevice] OLD: Switching to 0:EST");
                shouldSendMsg = true;
            }
            else if (ctx->peerLoadCtx->state == AuthDecodingState::ERR) {
                JAMI_WARNING("[LinkDevice] Undefined behavior encountered during a link auth session.");
                shouldShutDown = true;
            }
            else if (ctx->peerLoadCtx->state == AuthDecodingState::AUTH) {
                // receive the incoming password, check if the password is right, and send back the
                // archive if it is correct
                JAMI_DEBUG("[LinkDevice] OLD: addDevice: setOnRecv: verifying sent credentials from NEW");
                shouldSendMsg = true;
                // REMOVE
                // AccountArchive archive;
                // bool accountDecompressedSuccessfully = false;

                // check if the password is valid... if so then send the unencrypyted archive data over the tls channel as a compressed file
                // this will involve (OLD:) unzip, unencrypt, send, (NEW:) recv, encrypt w new salt, compress, write to system file (ONLY DO OLD HERE)
                // check if password is valid
                bool passValid = this_->isPasswordValid(toRecv.payload["password"]);
                if (passValid) {
                    // try and decompress archive for xfer
                    // bool accountDecompressedSuccessfully = false;
                    // std::vector<uint8_t> dataCheck;
                    try {
                        JAMI_DEBUG("[LinkDevice] Attempting to read archive at path {} for {}.", this_->path_, this_->accountId_);
                        // TODO handle both auth schemes
                        // dataCheck = fileutils::readArchive(fileutils::getFullPath(this_->path_, "archive.gz"), ctx->peerLoadCtx->authScheme, toRecv.payload["password"]).data;
                        JAMI_DEBUG("[LinkDevice] Injecting account archive into outbound message.");
                        toSend.payload["accData"] = fileutils::readArchive(fileutils::getFullPath(this_->path_, "archive.gz"), fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD, toRecv.payload["password"]).data;
                        JAMI_DEBUG("[LinkDevice] Injected account archive into outbound message.");
                        // ctx->peerLoadCtx->archiveTransferredWithoutFailure = true;
                        // accountDecompressedSuccessfully = true;
                        JAMI_DEBUG("[LinkDevice] Finished reading archive: SUCCESS");
                    } catch (...) {
                        JAMI_DEBUG("[LinkDevice] Finished reading archive: FAILURE");
                        shouldShutDown = true;
                    }

                    // // decrypt archive and send it to the new device
                    // if (accountDecompressedSuccessfully) {
                    //     JAMI_DEBUG("[LinkDevice] Injecting account archive into outbound message.");
                    //     // TODO send acc in a second message so that AUTH and DATA are explicit
                    //     // toSend.accData = std::move(dataCheck);
                    //     // ctx->peerLoadCtx->credentials = toRecv.payload["password"];
                    //     JAMI_DEBUG("[LinkDevice] Injected account archive into outbound message.");
                    // }
                }
                else { // pass is not valid
                    if (ctx->peerLoadCtx->numTries < ctx->peerLoadCtx->maxTries) {
                        // can retry auth
                        ctx->peerLoadCtx->numTries++;
                        JAMI_DEBUG("[LinkDevice] Incorrect password was submitted to this server... "
                            "allowing a retry. {}", ctx->peerLoadCtx->numTries);
                            toSend.payload["passwordCorrect"] = "false";
                            toSend.payload["canRetry"] = "true";
                    } else {
                        // cannot retry auth
                        JAMI_DEBUG("[LinkDevice] Incorrect password was submitted to this server... "
                            "NOT allowing a retry because threshold already reached!");
                            toSend.payload["canRetry"] = "false";
                            ctx->peerLoadCtx->state = AuthDecodingState::AUTH_ERROR;
                        shouldShutDown = true;
                    }
                }
            }
            else if (ctx->peerLoadCtx->state == AuthDecodingState::EST) {
                JAMI_DEBUG("[LinkDevice] OLD: Initiating auth.");
                bool hasPassword = false;
                // going to check for password protection here
                {
                    bool configAvail = true;
                    YAML::Node accConfig;
                    try {
                        auto configPath = fileutils::getFullPath(this_->path_, "config.yml");
                        accConfig = YAML::LoadFile(configPath);
                    } catch (...) {
                        configAvail = false;
                        JAMI_WARNING("[LinkDevice] Assuming account has no password because the config does not contain anything. This case is specific to ut_linkdevice.");
                    }
                    // Check if the loaded YAML node is valid
                    if (configAvail && accConfig) {
                        try {
                            hasPassword = accConfig["Account.archiveHasPassword"].as<bool>();
                            // JAMI_DEBUG("[LinkDevice] Account.archiveHasPassword: {}", hasPassword ? "true" : "false");
                        } catch (const YAML::Exception& e) {
                            JAMI_WARNING("[LinkDevice] Error accessing value: {}", e.what());
                            shouldShutDown = true;
                            ctx->peerLoadCtx->state = AuthDecodingState::ERR;
                        }
                    } else {
                        JAMI_ERROR("[LinkDevice] Failed to load config yaml file.");
                        shouldShutDown = true;
                        ctx->peerLoadCtx->state = AuthDecodingState::ERR;
                    }
                }
                JAMI_DEBUG("[LinkDevice] OLD: {} archive detected.", hasPassword ? "Protected" : "Unprotected");
                if (!hasPassword) {
                    toSend.payload["authScheme"] = "none";
                    toSend.payload["credentialsValid"] = "true";
                    // toSend.payload["readyToReceiveData"] = "true";
                    ctx->peerLoadCtx->state = AuthDecodingState::DATA;
                    shouldShutDown = true;
                    try {
                        // TODO KESS: ask Adrien how to handle deserialize and sending account config data
                        toSend.payload["accData"] = /*ctx->peerLoadCtx->accData =*/ fileutils::readArchive(fileutils::getFullPath(this_->path_, "archive.gz"), fileutils::ARCHIVE_AUTH_SCHEME_NONE, "").data;
                        // JAMI_DEBUG("[LinkDevice] OLD: accData is: {}", ctx->peerLoadCtx->accData);
                        ctx->peerLoadCtx->state = AuthDecodingState::DATA;
                        shouldSendMsg = true;
                        // TODO KESS make sure this is good to go
                        // no going to keep AUTH and data separate and have an extra ack msg
                        // fileutils::readArchive(this_->path_, "", "").data
                        // toRecv.accData = ctx->peerLoadCtx->accData;
                        // toRecv.payload["archiveTransferredWithoutFailure"] = true;
                    } catch (const std::exception& e) {
                        JAMI_ERROR("[LinkDevice] OLD: Error preparing archive for xfer: {}", e.what());
                        shouldShutDown = true;
                        shouldSendMsg = true;
                        toSend.payload.clear();
                        toSend.payload["archiveTransferredWithoutFailure"] = "false";
                    }
                } else {
                    // TODO add other auth schemes here
                    toSend.payload["authScheme"] = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;
                    ctx->peerLoadCtx->state = AuthDecodingState::AUTH;
                }
                shouldSendMsg = true;
            }
            else if (ctx->peerLoadCtx->state == AuthDecodingState::AUTH_ERROR) {
                // update the flag and
                // read local flag of whether the operation on newDev succeeded or failed from the
                // payload, then close the channel so that core can emit an onShutdown signal
                JAMI_WARN("[LinkDevice] Isssue with authentication.");
                shouldShutDown = true;
                emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                    ctx->accountId,
                    ctx->token,
                    static_cast<uint8_t>(DeviceAuthState::DONE),
                    "auth_error"); // let the client know that the auth has failed
            }
            else if (ctx->peerLoadCtx->state == AuthDecodingState::DATA) {
                JAMI_WARNING("[LinkDevice] OLD: Sending archive to NEW");
                shouldSendMsg = true;
                // TODO KESS make this send the archive unencrypted
                // find out why this isnt firing!
                JAMI_DEBUG("[LinkDevice] OLD: accData is: {}", ctx->peerLoadCtx->accData);
                toRecv.payload["accData"] = ctx->peerLoadCtx->accData;//fileutils::readArchive(this_->path_, ctx->peerLoadCtx->authEnabled ? fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD : "", ctx->peerLoadCtx->authEnabled ? ctx->peerLoadCtx->credentials : "").data;
                toRecv.payload["archiveTransferredWithoutFailure"] = true;
                shouldShutDown = true;
            }
            else {
                JAMI_WARNING("[LinkDevice] OLD: Exceptional auth state.");
            }
            // END logic

            if (shouldSendMsg) {
                msgpack::sbuffer buffer(UINT16_MAX);
                msgpack::pack(buffer, toSend);
                std::error_code ec;
                // std::this_thread::sleep_for(std::chrono::seconds(1));
                ctx->peerLoadCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size(), ec);
            }

            if (shouldShutDown) {
                ctx->peerLoadCtx->channel->shutdown();
            }

            return len;
        } // END scheme v0

        else {
            JAMI_WARNING("[LinkDevice] Unsupported linkdev scheme id of {}", toRecv.schemeId);
            ctx->peerLoadCtx->state = AuthDecodingState::ERR;
            return (size_t) 0;
        }

    }  // !channel->onRecv callback end
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
            JAMI_LOG("[Auth] trying to load account from DHT with {:s} at {:s}",
                     ctx->credentials->uri, loc.toString());
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
                                                           archive) /*, std::move(contacts)*/,
                                                       false);
                            }
                        } catch (const std::exception& e) {
                            ctx->onFailure(AuthError::UNKNOWN, "");
                        }
                    });
                    return not ctx->dhtContext->found;
                },
                [=, &s](bool ok) {
                    JAMI_LOG("[Auth] DHT archive search ended at {}", loc.toString());
                    s.first = true;
                    s.second = ok;
                    searchEnded();
                });
        } catch (const std::exception& e) {
            // JAMI_ERROR("Error computing keys: {}", e.what());
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
        onArchiveLoaded(ctx, std::move(archive), false);
    } else {
        ctx.onFailure(AuthError::UNKNOWN, "");
    }
}

void
ArchiveAccountManager::onArchiveLoaded(AuthContext& ctx, AccountArchive&& a, bool isLinkDevProtocol)
{
    auto ethAccount = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();
    dhtnet::fileutils::check_dir(path_, 0700);

    if (isLinkDevProtocol) {
        a.save(fileutils::getFullPath(path_, archivePath_),
            ctx.linkDevCtx->authScheme,
            ctx.linkDevCtx->passwordFromUser);
    } else {
        a.save(fileutils::getFullPath(path_, archivePath_),
            ctx.credentials ? ctx.credentials->password_scheme : "",
            ctx.credentials ? ctx.credentials->password : "");
    }

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
        deviceCertificate = std::make_shared<dht::crypto::Certificate>(
            dht::crypto::Certificate::generate(*request, a.id));
        JAMI_WARNING("[Auth] created new device: {}", deviceCertificate->getLongId());
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
ArchiveAccountManager::startSync(const OnNewDeviceCb& cb,
                                 const OnDeviceAnnouncedCb& dcb,
                                 bool publishPresence)
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
ArchiveAccountManager::saveArchive(AccountArchive& archive,
                                   std::string_view scheme,
                                   const std::string& pwd)
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
        [this, scheme = std::string(scheme), password] { return readArchive(scheme, password); });
    findCertificate(DeviceId(device),
                    [fa = std::move(fa),
                     scheme = std::string(scheme),
                     password,
                     device,
                     cb,
                     w = weak_from_this()](
                        const std::shared_ptr<dht::crypto::Certificate>& crt) mutable {
                        if (not crt) {
                            cb(RevokeDeviceResult::ERROR_NETWORK);
                            return;
                        }
                        auto this_ = std::static_pointer_cast<ArchiveAccountManager>(w.lock());
                        if (not this_)
                            return;
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
                        this_->certStore().pinRevocationList(a.id.second->getId().toString(),
                                                             a.revoked);
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
ArchiveAccountManager::exportArchive(const std::string& destinationPath,
                                     std::string_view scheme,
                                     const std::string& password)
{
    try {
        // Save contacts if possible before exporting
        AccountArchive archive = readArchive(scheme, password);
        updateArchive(archive);
        auto archivePath = fileutils::getFullPath(path_, archivePath_);
        archive.save(archivePath, scheme, password);

        // Export the file
        std::error_code ec;
        std::filesystem::copy_file(archivePath,
                                   destinationPath,
                                   std::filesystem::copy_options::overwrite_existing,
                                   ec);
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
