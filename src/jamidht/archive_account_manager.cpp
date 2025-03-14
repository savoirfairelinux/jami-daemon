/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

namespace jami {

const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);
constexpr auto AUTH_URI_SCHEME = "jami-auth://"sv;
constexpr auto CHANNEL_SCHEME = "auth:"sv;
constexpr auto OP_TIMEOUT = 5min;

void
ArchiveAccountManager::initAuthentication(PrivateKey key,
                                          std::string deviceName,
                                          std::unique_ptr<AccountCredentials> credentials,
                                          AuthSuccessCallback onSuccess,
                                          AuthFailureCallback onFailure,
                                          const OnChangeCallback& onChange)
{
    JAMI_WARNING("[Account {}] [Auth] starting authentication with scheme '{}'",
                 accountId_,
                 credentials->scheme);
    auto ctx = std::make_shared<AuthContext>();
    ctx->accountId = accountId_;
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
        JAMI_DEBUG("[LinkDevice] Importing account via p2p scheme.");
        startLoadArchiveFromDevice(ctx);
        return;
    }

    dht::ThreadPool::computation().run([ctx = std::move(ctx), wthis = weak()] {
        auto this_ = wthis.lock();
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
                        and needsMigration(this_->accountId_, ctx->credentials->updateIdentity)) {
                        this_->migrateAccount(*ctx);
                    } else {
                        this_->loadFromFile(*ctx);
                    }
                } else if (ctx->credentials->updateIdentity.first
                           and ctx->credentials->updateIdentity.second) {
                    auto future_keypair = dht::ThreadPool::computation().get<dev::KeyPair>(
                        &dev::KeyPair::create);
                    AccountArchive a;
                    JAMI_WARNING("[Account {}] [Auth] Converting certificate from old account {}",
                                 this_->accountId_,
                                 ctx->credentials->updateIdentity.first->getPublicKey()
                                     .getId()
                                     .to_view());
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
    JAMI_WARNING("[Account {}] [Auth] Updating certificates", accountId_);
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
        JAMI_LOG("[Account {}] [Auth] CA certificate re-generated", accountId_);
    }

    // Update certificate
    if (updated or not cert->isCA() or cert->getExpiration() < clock::now()) {
        cert = std::make_shared<Certificate>(
            Certificate::generate(*archive.id.first,
                                  "Jami",
                                  dht::crypto::Identity {archive.ca_key, ca},
                                  true));
        updated = true;
        JAMI_LOG("[Account {}] [Auth] Account certificate for {} re-generated",
                 accountId_,
                 cert->getId());
    }

    if (updated and device.first and *device.first) {
        // update device certificate
        device.second = std::make_shared<Certificate>(
            Certificate::generate(*device.first, "Jami device", archive.id));
        JAMI_LOG("[Account {}] [Auth] Device certificate re-generated", accountId_);
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
        JAMI_WARNING("[Account {}] [Auth] Updating validity for certificate with id: {}",
                     accountId_,
                     id);
    else
        JAMI_WARNING("[Account {}] [Auth] Updating validity for certificates", accountId_);

    auto& cert = archive.id.second;
    auto ca = cert->issuer;
    if (not ca)
        return false;

    // using Certificate = dht::crypto::Certificate;
    //  Update CA if possible and relevant
    if (not id or ca->getId() == id) {
        ca->setValidity(*archive.ca_key, validity);
        updated = true;
        JAMI_LOG("[Account {}] [Auth] CA certificate re-generated", accountId_);
    }

    // Update certificate
    if (updated or not id or cert->getId() == id) {
        cert->setValidity(dht::crypto::Identity {archive.ca_key, ca}, validity);
        device.second->issuer = cert;
        updated = true;
        JAMI_LOG("[Account {}] [Auth] Jami certificate re-generated", accountId_);
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
    auto ca = dht::crypto::generateIdentity("Jami CA");
    if (!ca.first || !ca.second) {
        throw std::runtime_error("Unable to generate CA for this account.");
    }
    a.id = dht::crypto::generateIdentity("Jami", ca, 4096, true);
    if (!a.id.first || !a.id.second) {
        throw std::runtime_error("Unable to generate identity for this account.");
    }
    JAMI_WARNING("[Account {}] [Auth] New account: CA: {}, ID: {}",
                 accountId_,
                 ca.second->getId(),
                 a.id.second->getId());
    a.ca_key = ca.first;
    auto keypair = dev::KeyPair::create();
    a.eth_key = keypair.secret().makeInsecure().asBytes();
    onArchiveLoaded(ctx, std::move(a), false);
}

void
ArchiveAccountManager::loadFromFile(AuthContext& ctx)
{
    JAMI_WARNING("[Account {}] [Auth] Loading archive from: {}",
                 accountId_,
                 ctx.credentials->uri.c_str());
    AccountArchive archive;
    try {
        archive = AccountArchive(ctx.credentials->uri,
                                 ctx.credentials->password_scheme,
                                 ctx.credentials->password);
    } catch (const std::exception& ex) {
        JAMI_WARNING("[Account {}] [Auth] Unable to read archive file: {}", accountId_, ex.what());
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
    DONE,
    TIMEOUT,
    CANCELED
};

static constexpr std::string_view
toString(AuthDecodingState state)
{
    switch (state) {
    case AuthDecodingState::HANDSHAKE:
        return "HANDSHAKE"sv;
    case AuthDecodingState::EST:
        return "EST"sv;
    case AuthDecodingState::AUTH:
        return "AUTH"sv;
    case AuthDecodingState::DATA:
        return "DATA"sv;
    case AuthDecodingState::ERR:
        return "ERR"sv;
    case AuthDecodingState::AUTH_ERROR:
        return "AUTH_ERROR"sv;
    case AuthDecodingState::DONE:
        return "DONE"sv;
    case AuthDecodingState::TIMEOUT:
        return "TIMEOUT"sv;
    case AuthDecodingState::CANCELED:
        return "CANCELED"sv;
    }
}

namespace PayloadKey {
static constexpr auto passwordCorrect = "passwordCorrect"sv;
static constexpr auto canRetry = "canRetry"sv;
static constexpr auto accData = "accData"sv;
static constexpr auto authScheme = "authScheme"sv;
static constexpr auto password = "password"sv;
static constexpr auto stateMsg = "stateMsg"sv;
}

struct ArchiveAccountManager::AuthMsg
{
    uint8_t schemeId {0};
    std::map<std::string, std::string> payload;
    MSGPACK_DEFINE_MAP(schemeId, payload)

    void set(std::string_view key, std::string_view value) {
        payload.emplace(std::string(key), std::string(value));
    }

    auto find(std::string_view key) const { return payload.find(std::string(key)); }

    auto at(std::string_view key) const { return payload.at(std::string(key)); }

    void logMsg() { JAMI_DEBUG("[LinkDevice]\nLinkDevice::logMsg:\n{}", formatMsg()); }

    std::string formatMsg() {
        std::string logStr = "=========\n";
        logStr += fmt::format("scheme: {}\n", schemeId);
        for (const auto& [msgKey, msgVal] : payload) {
            logStr += fmt::format(" - {}: {}\n", msgKey, msgVal);
        }
        logStr += "=========";
        return logStr;
    }

    static AuthMsg timeout() {
        AuthMsg timeoutMsg;
        timeoutMsg.set(PayloadKey::stateMsg, toString(AuthDecodingState::TIMEOUT));
        return timeoutMsg;
    }
};

struct ArchiveAccountManager::DeviceAuthInfo : public std::map<std::string, std::string>
{
    // Static key definitions
    static constexpr auto token = "token"sv;
    static constexpr auto error = "error"sv;
    static constexpr auto auth_scheme = "auth_scheme"sv;
    static constexpr auto peer_id = "peer_id"sv;
    static constexpr auto auth_error = "auth_error"sv;
    static constexpr auto peer_address = "peer_address"sv;

    // Add error enum
    enum class Error { NETWORK, TIMEOUT, AUTH_ERROR, CANCELED, UNKNOWN, NONE };

    using Map = std::map<std::string, std::string>;

    DeviceAuthInfo() = default;
    DeviceAuthInfo(const Map& map)
        : Map(map)
    {}
    DeviceAuthInfo(Map&& map)
        : Map(std::move(map))
    {}

    void set(std::string_view key, std::string_view value) {
        emplace(std::string(key), std::string(value));
    }

    static DeviceAuthInfo createError(Error err)
    {
        std::string errStr;
        switch (err) {
        case Error::NETWORK:
            errStr = "network";
            break;
        case Error::TIMEOUT:
            errStr = "timeout";
            break;
        case Error::AUTH_ERROR:
            errStr = "auth_error";
            break;
        case Error::CANCELED:
            errStr = "canceled";
            break;
        case Error::UNKNOWN:
            errStr = "unknown";
            break;
        case Error::NONE:
            errStr = "";
            break;
        }
        return DeviceAuthInfo {Map {{std::string(error), errStr}}};
    }
};

struct ArchiveAccountManager::DeviceContextBase
{
    uint64_t opId;
    AuthDecodingState state {AuthDecodingState::EST};
    std::string scheme;
    bool authEnabled {false};
    bool archiveTransferredWithoutFailure {false};
    std::string accData;

    DeviceContextBase(uint64_t operationId, AuthDecodingState initialState)
        : opId(operationId)
        , state(initialState)
    {}

    constexpr std::string_view formattedAuthState() const { return toString(state); }

    bool handleTimeoutMessage(const AuthMsg& msg)
    {
        auto stateMsgIt = msg.find(PayloadKey::stateMsg);
        if (stateMsgIt != msg.payload.end()) {
            if (stateMsgIt->second == toString(AuthDecodingState::TIMEOUT)) {
                this->state = AuthDecodingState::TIMEOUT;
                return true;
            }
        }
        return false;
    }

    bool handleCanceledMessage(const AuthMsg& msg)
    {
        auto stateMsgIt = msg.find(PayloadKey::stateMsg);
        if (stateMsgIt != msg.payload.end()) {
            if (stateMsgIt->second == toString(AuthDecodingState::CANCELED)) {
                this->state = AuthDecodingState::CANCELED;
                return true;
            }
        }
        return false;
    }

    DeviceAuthInfo::Error getErrorState() const
    {
        if (state == AuthDecodingState::AUTH_ERROR) {
            return DeviceAuthInfo::Error::AUTH_ERROR;
        } else if (state == AuthDecodingState::TIMEOUT) {
            return DeviceAuthInfo::Error::TIMEOUT;
        } else if (state == AuthDecodingState::CANCELED) {
            return DeviceAuthInfo::Error::CANCELED;
        } else if (state == AuthDecodingState::ERR) {
            return DeviceAuthInfo::Error::UNKNOWN;
        } else if (archiveTransferredWithoutFailure) {
            return DeviceAuthInfo::Error::NONE;
        }
        return DeviceAuthInfo::Error::NETWORK;
    }

    bool isCompleted() const
    {
        return state == AuthDecodingState::DONE || state == AuthDecodingState::ERR
               || state == AuthDecodingState::AUTH_ERROR || state == AuthDecodingState::TIMEOUT
               || state == AuthDecodingState::CANCELED;
    }
};

struct ArchiveAccountManager::LinkDeviceContext : public DeviceContextBase
{
    dht::crypto::Identity tmpId;
    dhtnet::ConnectionManager tempConnMgr;
    unsigned numOpenChannels {0};
    unsigned maxOpenChannels {1};
    std::shared_ptr<dhtnet::ChannelSocket> channel;
    msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                           nullptr,
                           512};
    std::string authScheme {fileutils::ARCHIVE_AUTH_SCHEME_NONE};
    std::string credentialsFromUser {""};

    LinkDeviceContext(dht::crypto::Identity id)
        : DeviceContextBase(0, AuthDecodingState::HANDSHAKE)
        , tmpId(std::move(id))
        , tempConnMgr(tmpId)
    {}
};

struct ArchiveAccountManager::AddDeviceContext : public DeviceContextBase
{
    unsigned numTries {0};
    unsigned maxTries {3};
    std::shared_ptr<dhtnet::ChannelSocket> channel;
    std::string_view authScheme;
    std::string credentials;

    AddDeviceContext(std::shared_ptr<dhtnet::ChannelSocket> c)
        : DeviceContextBase(0, AuthDecodingState::EST)
        , channel(std::move(c))
    {}

    AuthMsg createCanceledMsg() const
    {
        AuthMsg timeoutMsg;
        timeoutMsg.set(PayloadKey::stateMsg, toString(AuthDecodingState::CANCELED));
        return timeoutMsg;
    }
};

bool
ArchiveAccountManager::provideAccountAuthentication(const std::string& key,
                                                    const std::string& scheme)
{
    if (scheme != fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD) {
        JAMI_ERROR("[LinkDevice] Unsupported account authentication scheme attempted.");
        return false;
    }
    auto ctx = authCtx_;
    if (!ctx) {
        JAMI_WARNING("[LinkDevice] No auth context found.");
        return false;
    }

    if (ctx->linkDevCtx->state != AuthDecodingState::AUTH) {
        JAMI_WARNING("[LinkDevice] Invalid state for providing account authentication.");
        return false;
    }
    // After authentication, the next step is to receive the account archive from the exporting device
    ctx->linkDevCtx->state = AuthDecodingState::DATA;
    emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
        ctx->accountId, static_cast<uint8_t>(DeviceAuthState::IN_PROGRESS), DeviceAuthInfo {});

    dht::ThreadPool::io().run([key = std::move(key), scheme, ctx]() mutable {
        AuthMsg toSend;
        toSend.set(PayloadKey::password, std::move(key));
        msgpack::sbuffer buffer(UINT16_MAX);
        toSend.logMsg();
        msgpack::pack(buffer, toSend);
        std::error_code ec;
        try {
            ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()),
                                            buffer.size(),
                                            ec);
        } catch (const std::exception& e) {
            JAMI_WARNING("[LinkDevice] Failed to send password over auth ChannelSocket. Channel "
                         "may be invalid.");
        }
    });

    return true;
}

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
    if (authCtx_) {
        JAMI_WARNING("[LinkDevice] Already loading archive from device.");
        ctx->onFailure(AuthError::INVALID_ARGUMENTS, "Already loading archive from device.");
        return;
    }
    JAMI_DEBUG("[LinkDevice] Starting load archive from device {} {}.",
               fmt::ptr(this),
               fmt::ptr(ctx));
    authCtx_ = ctx;
    // move the account creation to another thread
    dht::ThreadPool::computation().run([ctx, wthis = weak()] {
        auto ca = dht::crypto::generateEcIdentity("Jami Temporary CA");
        if (!ca.first || !ca.second) {
            throw std::runtime_error("[LinkDevice] Can't generate CA for this account.");
        }
        // temporary user for bootstrapping p2p connection is created here
        auto user = dht::crypto::generateIdentity("Jami Temporary User", ca, 4096, true);
        if (!user.first || !user.second) {
            throw std::runtime_error("[LinkDevice] Can't generate identity for this account.");
        }

        auto this_ = wthis.lock();
        if (!this_) {
            JAMI_WARNING("[LinkDevice] Failed to get the ArchiveAccountManager.");
            return;
        }

        // establish linkDevCtx
        ctx->linkDevCtx = std::make_shared<LinkDeviceContext>(
            dht::crypto::generateIdentity("Jami Temporary device", user));
        JAMI_LOG("[LinkDevice] Established linkDevCtx. {} {} {}.",
                 fmt::ptr(this_),
                 fmt::ptr(ctx),
                 fmt::ptr(ctx->linkDevCtx));

        // set up auth channel code and also use it as opId
        auto gen = Manager::instance().getSeededRandomEngine();
        ctx->linkDevCtx->opId = std::uniform_int_distribution<uint64_t>(100000, 999999)(gen);
#if TARGET_OS_IOS
        ctx->linkDevCtx->tempConnMgr.oniOSConnected(
            [&](const std::string& connType, dht::InfoHash peer_h) { return false; });
#endif
        ctx->linkDevCtx->tempConnMgr.onDhtConnected(ctx->linkDevCtx->tmpId.second->getPublicKey());

        auto accountScheme = fmt::format("{}{}/{}",
                                         AUTH_URI_SCHEME,
                                         ctx->linkDevCtx->tmpId.second->getId(),
                                         ctx->linkDevCtx->opId);
        JAMI_LOG("[LinkDevice] auth scheme will be: {}", accountScheme);

        DeviceAuthInfo info;
        info.set(DeviceAuthInfo::token, accountScheme);

        emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
            ctx->accountId, static_cast<uint8_t>(DeviceAuthState::TOKEN_AVAILABLE), info);

        ctx->linkDevCtx->tempConnMgr.onICERequest(
            [wctx = std::weak_ptr(ctx)](const DeviceId& deviceId) {
                if (auto ctx = wctx.lock()) {
                    emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                        ctx->accountId,
                        static_cast<uint8_t>(DeviceAuthState::CONNECTING),
                        DeviceAuthInfo {});
                    return true;
                }
                return false;
            });

        ctx->linkDevCtx->tempConnMgr.onChannelRequest(
            [wthis, ctx](const std::shared_ptr<dht::crypto::Certificate>& cert,
                         const std::string& name) {
                std::string_view url(name);
                if (!starts_with(url, CHANNEL_SCHEME)) {
                    JAMI_WARNING(
                        "[LinkDevice] Temporary connection manager received invalid scheme: {}",
                        name);
                    return false;
                }
                auto opStr = url.substr(CHANNEL_SCHEME.size());
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

        ctx->linkDevCtx->tempConnMgr.onConnectionReady([ctx,
                                                        accountScheme,
                                                        wthis](const DeviceId& deviceId,
                                                               const std::string& name,
                                                               std::shared_ptr<dhtnet::ChannelSocket>
                                                                   socket) {
            ctx->linkDevCtx->channel = socket;

            ctx->timeout = std::make_unique<asio::steady_timer>(*Manager::instance().ioContext());
            ctx->timeout->expires_from_now(OP_TIMEOUT);
            ctx->timeout->async_wait([c = std::weak_ptr(ctx), socket](const std::error_code& ec) {
                if (ec) {
                    return;
                }
                if (auto ctx = c.lock()) {
                    if (!ctx->linkDevCtx->isCompleted()) {
                        ctx->linkDevCtx->state = AuthDecodingState::TIMEOUT;
                        JAMI_WARNING("[LinkDevice] timeout: {}", socket->name());

                        // Create and send timeout message
                        msgpack::sbuffer buffer(UINT16_MAX);
                        msgpack::pack(buffer, AuthMsg::timeout());
                        std::error_code ec;
                        socket->write(reinterpret_cast<const unsigned char*>(buffer.data()),
                                      buffer.size(),
                                      ec);
                        socket->shutdown();
                    }
                }
            });

            socket->onShutdown([ctx, name, wthis]() {
                JAMI_WARNING("[LinkDevice] Temporary connection manager closing socket: {}", name);
                if (ctx->timeout)
                    ctx->timeout->cancel();
                ctx->timeout.reset();
                ctx->linkDevCtx->numOpenChannels--;
                ctx->linkDevCtx->channel.reset();
                if (auto sthis = wthis.lock())
                    sthis->authCtx_.reset();

                DeviceAuthInfo::Error error = ctx->linkDevCtx->getErrorState();
                emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                    ctx->accountId,
                    static_cast<uint8_t>(DeviceAuthState::DONE),
                    DeviceAuthInfo::createError(error));
            });

            socket->setOnRecv([ctx,
                               decodingCtx = std::make_shared<DecodingContext>(),
                               wthis](const uint8_t* buf, size_t len) {
                if (!buf) {
                    return len;
                }

                decodingCtx->pac.reserve_buffer(len);
                std::copy_n(buf, len, decodingCtx->pac.buffer());
                decodingCtx->pac.buffer_consumed(len);
                AuthMsg toRecv;
                try {
                    msgpack::object_handle oh;
                    if (decodingCtx->pac.next(oh)) {
                        JAMI_DEBUG("[LinkDevice] NEW: Unpacking message.");
                        oh.get().convert(toRecv);
                    } else {
                        return len;
                    }
                } catch (const std::exception& e) {
                    ctx->linkDevCtx->state = AuthDecodingState::ERR;
                    JAMI_ERROR("[LinkDevice] Error unpacking message from source device: {}", e.what());
                    return len;
                }

                JAMI_DEBUG("[LinkDevice] NEW: Successfully unpacked message from source\n{}",
                           toRecv.formatMsg());
                JAMI_DEBUG("[LinkDevice] NEW: State is {}:{}",
                           ctx->linkDevCtx->scheme,
                           ctx->linkDevCtx->formattedAuthState());

                // check if scheme is supported
                if (toRecv.schemeId != 0) {
                    JAMI_WARNING("[LinkDevice] NEW: Unsupported scheme received from source");
                    ctx->linkDevCtx->state = AuthDecodingState::ERR;
                    return len;
                }

                // handle the protocol logic
                if (ctx->linkDevCtx->handleCanceledMessage(toRecv)) {
                    // import canceled. Will be handeled onShutdown
                    return len;
                }
                AuthMsg toSend;
                bool shouldShutdown = false;
                auto accDataIt = toRecv.find(PayloadKey::accData);
                bool shouldLoadArchive = accDataIt != toRecv.payload.end();

                if (ctx->linkDevCtx->state == AuthDecodingState::HANDSHAKE) {
                    auto peerCert = ctx->linkDevCtx->channel->peerCertificate();
                    auto authScheme = toRecv.at(PayloadKey::authScheme);
                    ctx->linkDevCtx->authEnabled = authScheme
                                                   != fileutils::ARCHIVE_AUTH_SCHEME_NONE;

                    JAMI_DEBUG("[LinkDevice] NEW: Auth scheme from payload is '{}'", authScheme);
                    ctx->linkDevCtx->state = AuthDecodingState::AUTH;
                    DeviceAuthInfo info;
                    info.set(DeviceAuthInfo::auth_scheme, authScheme);
                    info.set(DeviceAuthInfo::peer_id, peerCert->issuer->getId().toString());
                    emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                        ctx->accountId, static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING), info);
                } else if (ctx->linkDevCtx->state == AuthDecodingState::DATA) {
                    auto passwordCorrectIt = toRecv.find(PayloadKey::passwordCorrect);
                    auto canRetry = toRecv.find(PayloadKey::canRetry);

                    // If we've reached the maximum number of retry attempts
                    if (canRetry != toRecv.payload.end() && canRetry->second == "false") {
                        JAMI_DEBUG("[LinkDevice] Authentication failed: maximum retry attempts "
                                   "reached");
                        ctx->linkDevCtx->state = AuthDecodingState::AUTH_ERROR;
                        return len;
                    }

                    // If the password was incorrect but we can still retry
                    if (passwordCorrectIt != toRecv.payload.end()
                        && passwordCorrectIt->second == "false") {
                        ctx->linkDevCtx->state = AuthDecodingState::AUTH;

                        JAMI_DEBUG("[LinkDevice] NEW: Password incorrect.");
                        auto peerCert = ctx->linkDevCtx->channel->peerCertificate();
                        auto peer_id = peerCert->issuer->getId().toString();
                        // We received a password incorrect response, so we know we're using
                        // password authentication
                        auto authScheme = fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD;

                        DeviceAuthInfo info;
                        info.set(DeviceAuthInfo::auth_scheme, authScheme);
                        info.set(DeviceAuthInfo::peer_id, peer_id);
                        info.set(DeviceAuthInfo::auth_error, "invalid_credentials");

                        emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                            ctx->accountId,
                            static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING),
                            info);
                        return len;
                    }

                    if (!shouldLoadArchive) {
                        JAMI_DEBUG("[LinkDevice] NEW: no archive received.");
                        // at this point we suppose to have archive. If not, export failed.
                        // Update state and signal will be handeled onShutdown
                        ctx->linkDevCtx->state = AuthDecodingState::ERR;
                        shouldShutdown = true;
                    }
                }

                // check if an account archive is ready to be loaded
                if (shouldLoadArchive) {
                    emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(
                        ctx->accountId,
                        static_cast<uint8_t>(DeviceAuthState::IN_PROGRESS),
                        DeviceAuthInfo {});
                    try {
                        auto archive = AccountArchive(std::string_view(accDataIt->second));
                        if (auto this_ = wthis.lock()) {
                            JAMI_DEBUG("[LinkDevice] NEW: Reading archive from peer.");
                            this_->onArchiveLoaded(*ctx, std::move(archive), true);
                            JAMI_DEBUG("[LinkDevice] NEW: Successfully loaded archive.");
                            ctx->linkDevCtx->archiveTransferredWithoutFailure = true;
                        } else {
                            ctx->linkDevCtx->archiveTransferredWithoutFailure = false;
                            JAMI_ERROR("[LinkDevice] NEW: Failed to load account because of "
                                       "null ArchiveAccountManager!");
                        }
                    } catch (const std::exception& e) {
                        ctx->linkDevCtx->state = AuthDecodingState::ERR;
                        ctx->linkDevCtx->archiveTransferredWithoutFailure = false;
                        JAMI_WARNING("[LinkDevice] NEW: Error reading archive.");
                    }
                    shouldShutdown = true;
                }

                if (shouldShutdown) {
                    ctx->linkDevCtx->channel->shutdown();
                }

                return len;
            }); // !onConnectionReady // TODO emit AuthStateChanged+"connection ready" signal

            ctx->linkDevCtx->state = AuthDecodingState::HANDSHAKE;
            // send first message to establish scheme
            AuthMsg toSend;
            toSend.schemeId = 0; // set latest scheme here
            JAMI_DEBUG("[LinkDevice] NEW: Packing first message for SOURCE.\nCurrent state is: "
                       "\n\tauth "
                       "state = {}:{}",
                       toSend.schemeId,
                       ctx->linkDevCtx->formattedAuthState());
            msgpack::sbuffer buffer(UINT16_MAX);
            msgpack::pack(buffer, toSend);
            std::error_code ec;
            ctx->linkDevCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()),
                                            buffer.size(),
                                            ec);

            JAMI_LOG("[LinkDevice {}] Generated temporary account.",
                     ctx->linkDevCtx->tmpId.second->getId());
        });
    });
    JAMI_DEBUG("[LinkDevice] Starting load archive from device END {} {}.",
               fmt::ptr(this),
               fmt::ptr(ctx));
}

int32_t
ArchiveAccountManager::addDevice(const std::string& uriProvided,
                                 std::string_view auth_scheme,
                                 AuthChannelHandler* channelHandler)
{
    if (authCtx_) {
        JAMI_WARNING("[LinkDevice] addDevice: auth context already exists.");
        return static_cast<int32_t>(AccountManager::AddDeviceError::ALREADY_LINKING);
    }
    JAMI_LOG("[LinkDevice] ArchiveAccountManager::addDevice({}, {})", accountId_, uriProvided);
    try {
        std::string_view url(uriProvided);
        if (!starts_with(url, AUTH_URI_SCHEME)) {
            JAMI_ERROR("[LinkDevice] Invalid uri provided: {}", uriProvided);
            return static_cast<int32_t>(AccountManager::AddDeviceError::INVALID_URI);
        }
        auto peerTempAcc = url.substr(AUTH_URI_SCHEME.length(), 40);
        auto peerCodeS = url.substr(AUTH_URI_SCHEME.length() + peerTempAcc.length() + 1, 6);
        JAMI_LOG("[LinkDevice] ======\n * tempAcc =  {}\n * code = {}", peerTempAcc, peerCodeS);

        auto gen = Manager::instance().getSeededRandomEngine();
        std::uniform_int_distribution<int32_t> dist(1, INT32_MAX);
        auto token = dist(gen);
        JAMI_WARNING("[LinkDevice] SOURCE: Creating auth context, token: {}.", token);
        auto ctx = std::make_shared<AuthContext>();
        ctx->accountId = accountId_;
        ctx->token = token;
        ctx->credentials = std::make_unique<ArchiveAccountCredentials>();
        authCtx_ = ctx;

        channelHandler->connect(
            dht::InfoHash(peerTempAcc),
            fmt::format("{}{}", CHANNEL_SCHEME, peerCodeS),
            [wthis = weak(), auth_scheme, ctx, accountId=accountId_](std::shared_ptr<dhtnet::ChannelSocket> socket,
                                     const dht::InfoHash& infoHash) {
                auto this_ = wthis.lock();
                if (!socket || !this_) {
                    JAMI_WARNING(
                        "[LinkDevice] Invalid socket event while AccountManager connecting.");
                    if (this_)
                        this_->authCtx_.reset();
                    emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                        accountId,
                        ctx->token,
                        static_cast<uint8_t>(DeviceAuthState::DONE),
                        DeviceAuthInfo::createError(DeviceAuthInfo::Error::NETWORK));
                } else {
                    if (!this_->doAddDevice(auth_scheme, ctx, socket))
                        emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                            accountId,
                            ctx->token,
                            static_cast<uint8_t>(DeviceAuthState::DONE),
                            DeviceAuthInfo::createError(DeviceAuthInfo::Error::UNKNOWN));
                }
            });
        runOnMainThread([token, id = accountId_] {
            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                id, token, static_cast<uint8_t>(DeviceAuthState::CONNECTING), DeviceAuthInfo {});
        });
        return token;
    } catch (const std::exception& e) {
        JAMI_ERROR("[LinkDevice] Parsing uri failed: {}", uriProvided);
        return static_cast<int32_t>(AccountManager::AddDeviceError::GENERIC);
    }
}

bool
ArchiveAccountManager::doAddDevice(std::string_view scheme,
                                   const std::shared_ptr<AuthContext>& ctx,
                                   const std::shared_ptr<dhtnet::ChannelSocket>& channel)
{
    if (ctx->canceled) {
        JAMI_WARNING("[LinkDevice] SOURCE: addDevice canceled.");
        channel->shutdown();
        return false;
    }
    JAMI_DEBUG("[LinkDevice] Setting up addDevice logic on SOURCE device.");
    JAMI_DEBUG("[LinkDevice] SOURCE: Creating addDeviceCtx.");
    ctx->addDeviceCtx = std::make_unique<AddDeviceContext>(channel);
    ctx->addDeviceCtx->authScheme = scheme;
    ctx->addDeviceCtx->state = AuthDecodingState::HANDSHAKE;

    ctx->timeout = std::make_unique<asio::steady_timer>(*Manager::instance().ioContext());
    ctx->timeout->expires_from_now(OP_TIMEOUT);
    ctx->timeout->async_wait(
        [wthis = weak(), wctx = std::weak_ptr(ctx)](const std::error_code& ec) {
            if (ec) {
                return;
            }
            if (auto ctx = wctx.lock()) {
                if (!ctx->addDeviceCtx->isCompleted()) {
                    if (auto this_ = wthis.lock()) {
                        ctx->addDeviceCtx->state = AuthDecodingState::TIMEOUT;
                        JAMI_WARNING("[LinkDevice] Timeout for addDevice.");

                        // Create and send timeout message
                        msgpack::sbuffer buffer(UINT16_MAX);
                        msgpack::pack(buffer, AuthMsg::timeout());
                        std::error_code ec;
                        ctx->addDeviceCtx->channel->write(reinterpret_cast<const unsigned char*>(
                                                              buffer.data()),
                                                          buffer.size(),
                                                          ec);
                        ctx->addDeviceCtx->channel->shutdown();
                    }
                }
            }
        });

    JAMI_DEBUG("[LinkDevice] SOURCE: Creating callbacks.");
    channel->onShutdown([ctx, w = weak()]() {
        JAMI_DEBUG("[LinkDevice] SOURCE: Shutdown with state {}... xfer {}uccessful",
                   ctx->addDeviceCtx->formattedAuthState(),
                   ctx->addDeviceCtx->archiveTransferredWithoutFailure ? "s" : "uns");
        // check if the archive was successfully loaded and emitSignal
        if (ctx->timeout)
            ctx->timeout->cancel();
        ctx->timeout.reset();

        if (auto this_ = w.lock()) {
            this_->authCtx_.reset();
        }

        DeviceAuthInfo::Error error = ctx->addDeviceCtx->getErrorState();
        emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(ctx->accountId,
                                                                        ctx->token,
                                                                        static_cast<uint8_t>(
                                                                            DeviceAuthState::DONE),
                                                                        DeviceAuthInfo::createError(
                                                                            error));
    });

    // for now we only have one valid protocol (version is AuthMsg::scheme = 0) but can later
    // add in more schemes inside this callback function
    JAMI_DEBUG("[LinkDevice] Setting up receiving logic callback.");
    channel->setOnRecv([ctx,
                        wthis = weak(),
                        decodeCtx = std::make_shared<ArchiveAccountManager::DecodingContext>()](
                           const uint8_t* buf, size_t len) {
        JAMI_DEBUG("[LinkDevice] Setting up receiver callback for communication logic on SOURCE "
                   "device.");
        // when archive is sent to newDev we will get back a success or fail response before the
        // connection closes and we need to handle this and pass it to the shutdown callback
        auto this_ = wthis.lock();
        if (!this_) {
            JAMI_ERROR("[LinkDevice] Invalid state for ArchiveAccountManager.");
            return (size_t) 0;
        }

        if (!buf) {
            JAMI_ERROR("[LinkDevice] Invalid buffer.");
            return (size_t) 0;
        }

        if (ctx->canceled || ctx->addDeviceCtx->state == AuthDecodingState::ERR) {
            JAMI_ERROR("[LinkDevice] Error.");
            return (size_t) 0;
        }

        decodeCtx->pac.reserve_buffer(len);
        std::copy_n(buf, len, decodeCtx->pac.buffer());
        decodeCtx->pac.buffer_consumed(len);

        // handle unpacking the data from the peer
        JAMI_DEBUG("[LinkDevice] SOURCE: addDevice: setOnRecv: handling msg from NEW");
        msgpack::object_handle oh;
        AuthMsg toRecv;
        try {
            if (decodeCtx->pac.next(oh)) {
                oh.get().convert(toRecv);
                JAMI_DEBUG("[LinkDevice] SOURCE: Successfully unpacked message from NEW "
                           "(NEW->SOURCE)\n{}",
                           toRecv.formatMsg());
            } else {
                return len;
            }
        } catch (const std::exception& e) {
            // set the generic error state in the context
            ctx->addDeviceCtx->state = AuthDecodingState::ERR;
            JAMI_ERROR("[LinkDevice] error unpacking message from new device: {}", e.what()); // also warn in logs
        }

        JAMI_DEBUG("[LinkDevice] SOURCE: State is '{}'", ctx->addDeviceCtx->formattedAuthState());

        // It's possible to start handling different protocol scheme numbers here
        // one possibility is for multi-account xfer in the future
        // validate the scheme
        if (toRecv.schemeId != 0) {
            ctx->addDeviceCtx->state = AuthDecodingState::ERR;
            JAMI_WARNING("[LinkDevice] Unsupported scheme received from a connection.");
        }

        if (ctx->addDeviceCtx->state == AuthDecodingState::ERR
            || ctx->addDeviceCtx->state == AuthDecodingState::AUTH_ERROR) {
            JAMI_WARNING("[LinkDevice] Undefined behavior encountered during a link auth session.");
            ctx->addDeviceCtx->channel->shutdown();
        }
        // Check for timeout message
        if (ctx->addDeviceCtx->handleTimeoutMessage(toRecv)) {
            return len;
        }
        AuthMsg toSend;
        bool shouldSendMsg = false;
        bool shouldShutdown = false;
        bool shouldSendArchive = false;

        // we expect to be receiving credentials in this state and we know the archive is encrypted
        if (ctx->addDeviceCtx->state == AuthDecodingState::AUTH) {
            // receive the incoming password, check if the password is right, and send back the
            // archive if it is correct
            JAMI_DEBUG("[LinkDevice] SOURCE: addDevice: setOnRecv: verifying sent "
                       "credentials from NEW");
            shouldSendMsg = true;
            const auto& passwordIt = toRecv.find(PayloadKey::password);
            if (passwordIt != toRecv.payload.end()) {
                // try and decompress archive for xfer
                try {
                    JAMI_DEBUG("[LinkDevice] Injecting account archive into outbound message.");
                    ctx->addDeviceCtx->accData
                        = this_
                              ->readArchive(fileutils::ARCHIVE_AUTH_SCHEME_PASSWORD,
                                            passwordIt->second)
                              .serialize();
                    shouldSendArchive = true;
                    JAMI_DEBUG("[LinkDevice] Sending account archive.");
                } catch (const std::exception& e) {
                    ctx->addDeviceCtx->state = AuthDecodingState::ERR;
                    JAMI_DEBUG("[LinkDevice] Finished reading archive: FAILURE: {}", e.what());
                    shouldSendArchive = false;
                }
            }
            if (!shouldSendArchive) {
                // pass is not valid
                if (ctx->addDeviceCtx->numTries < ctx->addDeviceCtx->maxTries) {
                    // can retry auth
                    ctx->addDeviceCtx->numTries++;
                    JAMI_DEBUG("[LinkDevice] Incorrect password received. "
                               "Attempt {} out of {}.",
                               ctx->addDeviceCtx->numTries,
                               ctx->addDeviceCtx->maxTries);
                    toSend.set(PayloadKey::passwordCorrect, "false");
                    toSend.set(PayloadKey::canRetry, "true");
                } else {
                    // cannot retry auth
                    JAMI_WARNING("[LinkDevice] Incorrect password received, maximum attempts reached.");
                    toSend.set(PayloadKey::canRetry, "false");
                    ctx->addDeviceCtx->state = AuthDecodingState::AUTH_ERROR;
                    shouldShutdown = true;
                }
            }
        }

        if (shouldSendArchive) {
            JAMI_DEBUG("[LinkDevice] SOURCE: Archive in message has encryption scheme '{}'",
                       ctx->addDeviceCtx->authScheme);
            emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
                ctx->accountId,
                ctx->token,
                static_cast<uint8_t>(DeviceAuthState::IN_PROGRESS),
                DeviceAuthInfo {});
            shouldShutdown = true;
            shouldSendMsg = true;
            ctx->addDeviceCtx->archiveTransferredWithoutFailure = true;
            toSend.set(PayloadKey::accData, ctx->addDeviceCtx->accData);
        }
        if (shouldSendMsg) {
            JAMI_DEBUG("[LinkDevice] SOURCE: Sending msg to NEW:\n{}", toSend.formatMsg());
            msgpack::sbuffer buffer(UINT16_MAX);
            msgpack::pack(buffer, toSend);
            std::error_code ec;
            ctx->addDeviceCtx->channel->write(reinterpret_cast<const unsigned char*>(buffer.data()),
                                              buffer.size(),
                                              ec);
        }

        if (shouldShutdown) {
            ctx->addDeviceCtx->channel->shutdown();
        }

        return len;
    }); // !channel onRecv closure

    if (ctx->addDeviceCtx->state == AuthDecodingState::HANDSHAKE) {
        ctx->addDeviceCtx->state = AuthDecodingState::EST;
        DeviceAuthInfo info;
        info.set(DeviceAuthInfo::peer_address, channel->getRemoteAddress().toString(true));
        emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged>(
            ctx->accountId, ctx->token, static_cast<uint8_t>(DeviceAuthState::AUTHENTICATING), info);
    }

    return true;
}

bool
ArchiveAccountManager::cancelAddDevice(uint32_t token)
{
    if (auto ctx = authCtx_) {
        if (ctx->token == token) {
            ctx->canceled = true;
            if (ctx->addDeviceCtx) {
                ctx->addDeviceCtx->state = AuthDecodingState::CANCELED;
                if (ctx->addDeviceCtx->channel) {
                    // Create and send canceled message
                    auto canceledMsg = ctx->addDeviceCtx->createCanceledMsg();
                    msgpack::sbuffer buffer(UINT16_MAX);
                    msgpack::pack(buffer, canceledMsg);
                    std::error_code ec;
                    ctx->addDeviceCtx->channel->write(reinterpret_cast<const unsigned char*>(
                                                          buffer.data()),
                                                      buffer.size(),
                                                      ec);
                    ctx->addDeviceCtx->channel->shutdown();
                }
            }
            if (ctx->onFailure)
                ctx->onFailure(AuthError::UNKNOWN, "");
            authCtx_.reset();
            return true;
        }
    }
    return false;
}

bool
ArchiveAccountManager::confirmAddDevice(uint32_t token)
{
    if (auto ctx = authCtx_) {
        if (ctx->token == token && ctx->addDeviceCtx
            && ctx->addDeviceCtx->state == AuthDecodingState::EST) {
            dht::ThreadPool::io().run([ctx] {
                ctx->addDeviceCtx->state = AuthDecodingState::AUTH;
                AuthMsg toSend;
                JAMI_DEBUG("[LinkDevice] SOURCE: Packing first message for NEW and switching to "
                           "state: {}",
                           ctx->addDeviceCtx->formattedAuthState());
                toSend.set(PayloadKey::authScheme, ctx->addDeviceCtx->authScheme);
                msgpack::sbuffer buffer(UINT16_MAX);
                msgpack::pack(buffer, toSend);
                std::error_code ec;
                ctx->addDeviceCtx->channel->write(reinterpret_cast<const unsigned char*>(
                                                      buffer.data()),
                                                  buffer.size(),
                                                  ec);
            });
            return true;
        }
    }
    return false;
}

void
ArchiveAccountManager::loadFromDHT(const std::shared_ptr<AuthContext>& ctx)
{
    ctx->dhtContext = std::make_unique<DhtLoadContext>();
    ctx->dhtContext->dht.run(ctx->credentials->dhtPort, {}, true);
    for (const auto& bootstrap : ctx->credentials->dhtBootstrap) {
        ctx->dhtContext->dht.bootstrap(bootstrap);
        auto searchEnded = [ctx, accountId = accountId_]() {
            if (not ctx->dhtContext or ctx->dhtContext->found) {
                return;
            }
            auto& s = *ctx->dhtContext;
            if (s.stateOld.first && s.stateNew.first) {
                dht::ThreadPool::computation().run(
                    [ctx,
                     network_error = !s.stateOld.second && !s.stateNew.second,
                     accountId = std::move(accountId)] {
                        ctx->dhtContext.reset();
                        JAMI_WARNING("[Account {}] [Auth] Failure looking for archive on DHT: {}",
                                     accountId,
                                     network_error ? "network error" : "not found");
                        ctx->onFailure(network_error ? AuthError::NETWORK : AuthError::UNKNOWN, "");
                    });
            }
        };

        auto search = [ctx, searchEnded, w = weak()](bool previous) {
            std::vector<uint8_t> key;
            dht::InfoHash loc;
            auto& s = previous ? ctx->dhtContext->stateOld : ctx->dhtContext->stateNew;

            // compute archive location and decryption keys
            try {
                std::tie(key, loc) = computeKeys(ctx->credentials->password,
                                                 ctx->credentials->uri,
                                                 previous);
                JAMI_LOG("[Auth] Attempting to load account from DHT with {:s} at {:s}",
                         ctx->credentials->uri,
                         loc.toString());
                if (not ctx->dhtContext or ctx->dhtContext->found) {
                    return;
                }
                ctx->dhtContext->dht.get(
                    loc,
                    [ctx, key = std::move(key), w](const std::shared_ptr<dht::Value>& val) {
                        std::vector<uint8_t> decrypted;
                        try {
                            decrypted = archiver::decompress(
                                dht::crypto::aesDecrypt(val->data, key));
                        } catch (const std::exception& ex) {
                            return true;
                        }
                        JAMI_DBG("[Auth] Found archive on the DHT");
                        ctx->dhtContext->found = true;
                        dht::ThreadPool::computation().run(
                            [ctx, decrypted = std::move(decrypted), w] {
                                try {
                                    auto archive = AccountArchive(decrypted);
                                    if (auto sthis = w.lock()) {
                                        if (ctx->dhtContext) {
                                            ctx->dhtContext->dht.join();
                                            ctx->dhtContext.reset();
                                        }
                                        sthis->onArchiveLoaded(*ctx, std::move(archive), false);
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
}

void
ArchiveAccountManager::migrateAccount(AuthContext& ctx)
{
    JAMI_WARN("[Auth] Account migration needed");
    AccountArchive archive;
    try {
        archive = readArchive(ctx.credentials->password_scheme, ctx.credentials->password);
    } catch (...) {
        JAMI_DBG("[Auth] Unable to load archive");
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
               ctx.linkDevCtx->credentialsFromUser);
    } else {
        a.save(fileutils::getFullPath(path_, archivePath_),
               ctx.credentials ? ctx.credentials->password_scheme : "",
               ctx.credentials ? ctx.credentials->password : "");
    }

    if (not a.id.second->isCA()) {
        JAMI_ERROR("[Account {}] [Auth] Attempting to sign a certificate with a non-CA.",
                   accountId_);
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
            JAMI_WARNING("[Account {}] [Auth] Using previously generated device certificate {}",
                         accountId_,
                         deviceCertificate->getLongId());
        } else {
            contacts.reset();
        }
    }

    // Generate a new device if needed
    if (!deviceCertificate) {
        JAMI_WARNING("[Account {}] [Auth] Creating new device certificate", accountId_);
        auto request = ctx.request.get();
        if (not request->verify()) {
            JAMI_ERROR("[Account {}] [Auth] Invalid certificate request.", accountId_);
            ctx.onFailure(AuthError::INVALID_ARGUMENTS, "");
            return;
        }
        deviceCertificate = std::make_shared<dht::crypto::Certificate>(
            dht::crypto::Certificate::generate(*request, a.id));
        JAMI_WARNING("[Account {}] [Auth] Created new device: {}",
                     accountId_,
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
    JAMI_LOG("[Account {}] [Auth] Signing receipt for device {}", accountId_, device.getLongId());
    auto devId = device.getId();
    DeviceAnnouncement announcement;
    announcement.dev = devId;
    announcement.pk = device.getSharedPublicKey();
    dht::Value ann_val {announcement};
    ann_val.sign(*id.first);

    auto packedAnnoucement = ann_val.getPacked();
    JAMI_LOG("[Account {}] [Auth] Device announcement size: {}",
             accountId_,
             packedAnnoucement.size());

    std::ostringstream is;
    is << "{\"id\":\"" << id.second->getId() << "\",\"dev\":\"" << devId << "\",\"eth\":\""
       << ethAccount << "\",\"announce\":\"" << base64::encode(packedAnnoucement) << "\"}";

    // auto announce_ = ;
    return {is.str(), std::make_shared<dht::Value>(std::move(ann_val))};
}

bool
ArchiveAccountManager::needsMigration(const std::string& accountId, const dht::crypto::Identity& id)
{
    if (not id.second)
        return true;
    auto cert = id.second->issuer;
    while (cert) {
        if (not cert->isCA()) {
            JAMI_WARNING("[Account {}] [Auth] certificate {} is not a CA, needs update.",
                         accountId,
                         cert->getId());
            return true;
        }
        if (cert->getExpiration() < clock::now()) {
            JAMI_WARNING("[Account {}] [Auth] certificate {} is expired, needs update.",
                         accountId,
                         cert->getId());
            return true;
        }
        cert = cert->issuer;
    }
    return false;
}

void
ArchiveAccountManager::syncDevices()
{
    if (not dht_ or not dht_->isRunning()) {
        JAMI_WARNING("[Account {}] Not syncing devices: DHT is not running", accountId_);
        return;
    }
    JAMI_LOG("[Account {}] Building device sync from {}", accountId_, info_->deviceId);
    auto sync_data = info_->contacts->getSyncData();

    for (const auto& dev : getKnownDevices()) {
        // don't send sync data to ourself
        if (dev.first.toString() == info_->deviceId) {
            continue;
        }
        if (!dev.second.certificate) {
            JAMI_WARNING("[Account {}] Unable to find certificate for {}", accountId_, dev.first);
            continue;
        }
        auto pk = dev.second.certificate->getSharedPublicKey();
        JAMI_LOG("[Account {}] Sending device sync to {} {}",
                 accountId_,
                 dev.second.name,
                 dev.first.toString());
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
            findCertificate(
                sync.from,
                [this, sync](const std::shared_ptr<dht::crypto::Certificate>& cert) mutable {
                    if (!cert or cert->getId() != sync.from) {
                        JAMI_WARNING("[Account {}] Unable to find certificate for device {}",
                                     accountId_,
                                     sync.from.toString());
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
    JAMI_LOG("[Account {}] [Auth] Reading account archive", accountId_);
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

    JAMI_LOG("[Account {}] [Auth] Building account archive", accountId_);
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
        JAMI_ERROR("[Account {}] [Auth] Unable to export archive: {}", accountId_, ex.what());
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
        JAMI_ERROR("[Account {}] Error loading archive: {}", accountId_, e.what());
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
                     w = weak()](
                        const std::shared_ptr<dht::crypto::Certificate>& crt) mutable {
                        if (not crt) {
                            cb(RevokeDeviceResult::ERROR_NETWORK);
                            return;
                        }
                        auto this_ = w.lock();
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
        JAMI_ERR("[Auth] Unable to export archive: %s", ex.what());
        return false;
    } catch (...) {
        JAMI_ERR("[Auth] Unable to export archive: Unable to read archive");
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
        // JAMI_ERR("[Auth] Unable to export account: %s", e.what());
        cb(NameDirectory::RegistrationResponse::invalidCredentials, name);
        return;
    }

    nameDir_.get().registerName(accountId, nameLowercase, ethAccount, cb, signedName, publickey);
}
#endif

} // namespace jami
