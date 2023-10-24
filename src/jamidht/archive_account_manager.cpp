/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include <opendht/dhtrunner.h>
#include <opendht/thread_pool.h>

#include <memory>
#include <fstream>

namespace jami {

const constexpr auto EXPORT_KEY_RENEWAL_TIME = std::chrono::minutes(20);

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

    if (ctx->credentials->scheme == "dht") {
        loadFromDHT(ctx);
        return;
    } else if (ctx->credentials->scheme == "p2p") {
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
            //             else if (ctx->credentials->scheme == "p2p") {
            //
            //
            //
            //             }
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
    JAMI_WARN("Updating certificates");
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
ArchiveAccountManager::setValidity(const std::string& password,
                                   dht::crypto::Identity& device,
                                   const dht::InfoHash& id,
                                   int64_t validity)
{
    auto archive = readArchive(password);
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
        auto path = fileutils::getFullPath(path_, archivePath_);
        archive.save(path.string(), password);
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
    JAMI_WARN("[Auth] loading archive from: %s", ctx.credentials->uri.c_str());
    AccountArchive archive;
    try {
        archive = AccountArchive(ctx.credentials->uri, ctx.credentials->password);
    } catch (const std::exception& ex) {
        JAMI_WARN("[Auth] can't read file: %s", ex.what());
        ctx.onFailure(AuthError::INVALID_ARGUMENTS, ex.what());
        return;
    }
    onArchiveLoaded(ctx, std::move(archive));
}

struct ArchiveAccountManager::DhtLoadContext
{
    dht::DhtRunner dht;
    std::pair<bool, bool> stateOld {false, true};
    std::pair<bool, bool> stateNew {false, true};
    bool found {false};
};

// TODO potentially move all NewLinkDevImpl stuff to separate class called ImportManager but probably not since still fits ArchiveAccountManager scope
// all data related to NewLinkDevImpl
struct ArchiveAccountManager::LinkDeviceContext
{
    dht::crypto::Identity tmpId;
    dhtnet::ConnectionManager connectionManager;
    // std::map<std::string, std::string> passwordAttemptState or authDecodingState; // will store archiveReceived=yes/no, passwordEnabled=yes/no, numTries=[0-3]
    // std::pair<bool, bool, uint8_t> authDecodingState {false, false, 0}; // stores archiveReceived=yes/no, passwordEnabled=yes/no, numTries=[0-3]
    uint8_t numOpenChannels;
    uint8_t maxOpenChannels;
    AuthDecodingState authDecodingState;
    LinkDeviceContext(dht::crypto::Identity id)
        : tmpId(std::move(id))
        , connectionManager(tmpId)
    {}
};

// used for establishing operation state in TLS communication
enum AuthDecodingStateOpId {SCHEME=0, ATTEMPT, ARCHIVE, ASK}; //scheme is the password type, attempt is the cli/serv exchange, archive is the account download phase, ask is for robust synchronization and is a bonus feature

// submodule for LinkDeviceContext
struct ArchiveAccountManager::AuthDecodingState
{
    // TODO KESS Adrien warned about having to lock when UI interaction is reading/writing data that is being communicated over the stream
    // uint8_t channelOpId {0}; // 0: scheme, 1: attempt loop, 2+: close
    AuthDecodingStateOpId channelOpId {SCHEME}; // 0: scheme, 1: attempt loop, 2+: close
    bool archiveReceived {false};
    bool passwordEnabled {false};
    AccountArchive* transferredAccArchive; // TODO
    uint8_t failedAttempts {0};
}

// UNUSED
// communicates which of the three channels should be used to parse the next incoming message
struct LinkChannelIdMessage
{
   // uint8_t operationId {0};
   AuthDecodingStateOpId opId {0};
   MSGPACK_DEFINE_MAP(opId)
   // MSGPACK_DEFINE_MAP(operationId)
}

// stores AccountArchive in a msgpack-able format
struct PackableArchiveMsg
{
    AccountArchive archive;
    MSGPACK_DEFINE_MAP(archive)
}

// stores the password for NewLinkDevImpl
struct PasswordSchemeMsg // used for authenticating
{
    bool passwordEnabled {false};
    std::string password;
    MSGPACK_DEFINE_MAP(passwordEnabled, password)
};

struct CredentialStatusMsg {
    bool passwordCorrect {false};
    MSGPACK_DEFINE_MAP(passwordCorrect)
}

// KESS TODO ask Adrien about this
ArchiveAccountManager::ChannelModule(std::weak_ptr<JamiAccount>&& account)
    : pimpl_ {std::make_shared<Impl>(std::move(account))}
{}

enum DeviceStadeId {SUCCESS=0, ERROR=1, CONTINUE=2}; //scheme is the password type, attempt is the cli/serv exchange, archive is the account download phase, ask is for robust synchronization and is a bonus feature

// called on newDevice
// starts the inter-device communication for linking a device

void setupArchiveTlsChannel(const std::shared_ptr<AuthContext>& ctx, const std::function recvCb, bool oldDevice)
{
    // wait for first incoming ICE connection
    ctx->linkDevCtx->connectionManager.onICERequest([this](const DeviceId& deviceId) { return true; });

    // open the first jami-auth chanenl
    ctx->linkDevCtx->connectionManager->onChannelRequest(
        [this](const std::shared_ptr<dht::crypto::Certificate>& cert, const std::string& name) {
	      if (ctx->linkDevCtx->numOpenChannels < ctx->linkDevCtx->maxOpenChannels) {
	          if (name == uri::Uri::AUTH.schemeToString()) {
	    	        ctx->linkDevCtx->numOpenChannels++;
		            return true;
		        } else {
	              ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged(ctx->accountId, 1, "too_many_channels"); // TODO use some string from JAMI_WARN, etc., that is standardized across the project
		            return false;
		        }
	      }
	      return false;
    });

    struct AuthDecodingCtx
    {
        msgpack::unpacker pac {[](msgpack::type::object_type, std::size_t, void*) { return true; },
                               nullptr,
                               64}; )
    };

    // TODO set shutdown callback before setting connectionReady in case channel closes first

    ctx->linkDevCtx->connectionManager->onShutDown([]() {
      	// need to handle a few things in this function
      	// // if the channel shuts down and the archive was not successfully loaded yet, we need to communicate this to the client in order to present a failure status
      	// 1. if the channel shuts down and (the arhcive is not loaded) but (!password || password && no password) we need to emit error signal with error state
      	if (!ctx->linkDevCtx->authDecodingState->archiveReceived) {
      	    // let client know there was an error
      	    ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged(ctx->accountId, 1, "channel_closed_errror"); // TODO use some string from JAMI_WARN, etc., that is standardized across the project
      	} // this may be a duplicate message if the socket encounters errors during setOnRecv
      	if (ctx->linkDevCtx->numOpenChannels > ctx->linkDevCtx->maxOpenChannels) {
      	    // let client know there was an error
      	    ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged(ctx->accountId, 1, "too_many_channels"); // TODO use some string from JAMI_WARN, etc., that is standardized across the project
      	} // this may be a duplicate message if the socket encounters errors during onChannelRequest
    }); // TODO

    ctx->linkDevCtx->connectionManager->onConnectionReady([ctx = std::make_shared<AuthContext>(ctx)](const DeviceId& deviceId, const std::string& name, std::shared_ptr(dhtnet::ChannelSocket> channel)) {
      	socket->setOnRecv(recvCb}); // !setOnRecv
    }); // !onConnectionReady // TODO emit AuthStateChanged+"connection ready" signal
}

void
ArchiveAccountManager::startLoadArchiveFromDevice(const std::shared_ptr<AuthContext>& ctx)
{


    dht::ThreadPool::computation().run([password, cb = std::move(cb), w=weak_from_this()] {
        // TODO move this code to a separate thread to improve performance


        // create a temporary Jami account for negotioating a TLS connection via DhtNet
        auto ca = dht::crypto::generateEcIdentity("Jami Temporary CA");
        ctx->linkDevCtx = std::make_unique<LinkDeviceContext>(dht::crypto::generateIdentity("Jami", ca));

        const std::string AUTH_URI = "jami-auth://";
        const std::string accountScheme = AUTH_URI + ctx->linkDevCtx->tmpId.second->getId();
        // tell UI toshow the qr code
        // TODO define what the int states are in this protocol
        // // IDEA v1 could be used to indicate 0=account not loaded and 1=account is loaded + provide path to archive!
        // // IDEA v2: 0=successful load, 1=failed load, 2=operations in progress so keep waiting??
        ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(), ctx->accountId, DeviceStateId::CONTINUE, std::move(accountScheme));
        ///////ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(ctx->accountId, std::move(accountScheme), 2, "auth_session_ready");

        setupArchiveTlsChannel(
            ctx,
            std::move([wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket), ctx = std::make_shared<AuthContext>(ctx), decodingCtx = std::make_shared<AuthDecodingCtx>()](const uint8_t* buf, size_t len) {
          	    // if socket weak ptr is null then return
                if (!buf || !acc)
                    return len;

                decodingCtx->pac.reserve_buffer(len);

                std::copy_n(buf, len, decodingCtx->pac.buffer());
                decodingCtx->pac.buffer_consumed(len);

                msgpack::object_handle oh;
                try {
          	        decodingCtx->pac.next(oh); // TODO move try catch to just this line in order to handle errors more narrowly
                } catch (std::exception e) {
                    JAMI_WARN("[Link Device] error unpacking message from msgpack");
                }

          	    if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::SCHEME) {
                		PassowrdSchemeMsg passwordSchemeMsg;
            	    	oh.get().convert(passwordSchemeMsg);

                    if (!passwordSchemeMsg.passwordEnabled.empty()) {
              		    auto pwEnbl = passwordSchemeMsg.passwordEnabled;
              		    ctx->linkDevCtx->passwordEnabled = pwEnbl; //passwordSchemeMsg.passwordEnabled;
                        if (pwEnbl) {
          	                ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged(ctx->accountId, 2, "password_required");
                        }
                   	} else {
            		    JAMI_WARN("[Link Device] passwordEnabled flag is empty!");
                  	}

            	    if (!passwordSchemeMsg.password.empty()) {
            	        auto pw = passwordSchemeMsg.password;
            	    } else {
          		        JAMI_WARN("[Link Device] password flag is empty!");
          	        }
          	    } else if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::ATTEMPT) {
                		// do nothing
          	    }
          	    else if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::ARCHIVE) {
                    PackableArchiveMsg archiveSchemeMsg;
                	oh.get().convert(archiveSchemeMsg);
                	if (!archiveSchemeMsg.archive.empty()) {
                    	auto archive = archiveSchemeMsg.archive;
                        decrypted = archiver::decompress(dht::crypto::aesDecrypt(archive, ctx->linkDevCtx->password));
                    	ctx->linkDevCtx->transferredAccArchive = std::move(archive || decrypted); // TODO
                    	ctx->emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged(ctx->accountId, 0, "archive_loaded"); // TODO use some string from JAMI_WARN, etc., that is standardized across the project
                	} else {
                    	JAMI_WARN("[Link Device] archive is empty!");
              	        ctx->emitSignal<libjami::ConfigurationSignal::AddDeviceStateChanged(ctx->accountId, 1, "archive_load_failed"); // TODO use some string from JAMI_WARN, etc., that is standardized across the project
            	    }
          	    }
          	    else if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::ASK) {
          	        JAMI_WARN("[Link Device] unimplemented ASK request");
                }
          	    else {
            		JAMI_WARN("[Link Device] channel operation mode is unspecified (unspecified auth protocol)");
                }
          	}), // !handle different operation states
            false // oldDevice = false because this is the new device
        });


    });


}

void
ArchiveAccountManager::loadFromDHT(const std::shared_ptr<AuthContext>& ctx)
{
    ctx->dhtContext = std::make_unique<DhtLoadContext>();
    ctx->dhtContext->dht.run(ctx->credentials->dhtPort, {}, true);
    for (const auto& bootstrap : ctx->credentials->dhtBootstrap)
        ctx->dhtContext->dht.bootstrap(bootstrap);
    auto searchEnded = [ctx]() {
        if (not ctx->dhtContext or ctx->dhtContext->found) {
            return;
        }
        auto& s = *ctx->dhtContext;
        if (s.stateOld.first && s.stateNew.first) {
            dht::ThreadPool::computation().run(
                [ctx, network_error = !s.stateOld.second && !s.stateNew.second] {
                    ctx->dhtContext.reset();
                    JAMI_WARN("[Auth] failure looking for archive on DHT: %s",
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
        archive = readArchive(ctx.credentials->password);
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
    } else
        ctx.onFailure(AuthError::UNKNOWN, "");
}

void
ArchiveAccountManager::onArchiveLoaded(AuthContext& ctx,
                                       AccountArchive&& a)
{
    auto ethAccount = dev::KeyPair(dev::Secret(a.eth_key)).address().hex();
    dhtnet::fileutils::check_dir(path_, 0700);

    auto path = fileutils::getFullPath(path_, archivePath_);
    a.save(path.string(), ctx.credentials ? ctx.credentials->password : "");

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

    if (!contacts)
        contacts = std::make_unique<ContactList>(ctx.accountId, a.id.second, path_, onChange_);
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
    if (not dht_ or not dht_->isRunning()) {
        JAMI_WARN("Not syncing devices: DHT is not running");
        return;
    }
    JAMI_DBG("Building device sync from %s", info_->deviceId.c_str());
    auto sync_data = info_->contacts->getSyncData();

    for (const auto& dev : getKnownDevices()) {
        // don't send sync data to ourself
        if (dev.first.toString() == info_->deviceId)
            continue;
        auto pk = dev.second.certificate->getSharedPublicKey();
        JAMI_DBG("sending device sync to %s %s",
                 dev.second.name.c_str(),
                 dev.first.toString().c_str());
        auto syncDeviceKey = dht::InfoHash::get("inbox:" + pk->getId().toString());
        dht_->putEncrypted(syncDeviceKey, pk, sync_data);
    }
}

void
ArchiveAccountManager::startSync(const OnNewDeviceCb& cb, const OnDeviceAnnouncedCb& dcb)
{
    AccountManager::startSync(std::move(cb), std::move(dcb));

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
ArchiveAccountManager::readArchive(const std::string& pwd) const
{
    JAMI_DBG("[Auth] reading account archive");
    return AccountArchive(fileutils::getFullPath(path_, archivePath_).string(), pwd);
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
ArchiveAccountManager::saveArchive(AccountArchive& archive, const std::string& pwd)
{
    try {
        updateArchive(archive);
        if (archivePath_.empty())
            archivePath_ = "export.gz";
        archive.save(fileutils::getFullPath(path_, archivePath_).string(), pwd);
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
        auto path = fileutils::getFullPath(path_, archivePath_).string();
        AccountArchive(path, password_old).save(path, password_new);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// TODO KESS remove this from arch acc man
// std::string
// generatePIN(size_t length = 16, size_t split = 8)
// {
//     static constexpr const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
//     dht::crypto::random_device rd;
//     std::uniform_int_distribution<size_t> dis(0, sizeof(alphabet) - 2);
//     std::string ret;
//     ret.reserve(length);
//     for (size_t i = 0; i < length; i++) {
//         ret.push_back(alphabet[dis(rd)]);
//         if (i % split == split - 1 and i != length - 1)
//             ret.push_back('-');
//     }
//     return ret;
// }

// KESS hook in here for old device
// end goal is to callback connectDevice
void
ArchiveAccountManager::addDevice(/*std::shared_ptr<AuthContext> ctx*/, const std::string& password, AddDeviceCallback cb)
{
    dht::ThreadPool::computation().run([password, cb = std::move(cb), w=weak_from_this()] {
        auto this_ = std::static_pointer_cast<ArchiveAccountManager>(w.lock());
        if (not this_) return;

        // std::vector<uint8_t> key;
        // dht::InfoHash loc;
        // std::string pin_str;
        AccountArchive a;
        try {
            JAMI_DBG("[Link Device / Auth] exporting account");

            a = this_->readArchive(password);

            // Generate random PIN
            // TODO KESS can generate an EcAccount here for later when displaying a QR code on the reverse implementation
            // pin_str = generatePIN();

            // std::tie(key, loc) = computeKeys(password, pin_str);
        } catch (const std::exception& e) {
            JAMI_ERR("[Auth] can't export account: %s", e.what());
            cb(AddDeviceResult::ERROR_CREDENTIALS, {});
            return;
        }
        // // now that key and loc are computed, display to user in lowercase
        // std::transform(pin_str.begin(), pin_str.end(), pin_str.begin(), ::tolower);
        try {
            this_->updateArchive(a);
            auto encrypted = dht::crypto::aesEncrypt(archiver::compress(a.serialize()), password);
            // auto encrypted = dht::crypto::aesEncrypt(archiver::compress(a.serialize()), key);
            if (not this_->dht_ or not this_->dht_->isRunning())
                throw std::runtime_error("DHT is not running..");
            // TODO switch all these JAMI_WARN tags to JAMI_WARNING tags
            JAMI_WARN("[Link Device / Auth] exporting account without pin of size %zu", encrypted.size());
            cb(AddDeviceResult::SUCCESS_SHOW_QR);

        } catch (const std::exception& e) {
            JAMI_ERR("[Auth] can't export account: %s", e.what());
            cb(AddDeviceResult::ERROR_NETWORK, {});
            return;
        }

        auto ctx = std::shared_ptr<AuthContext>(); // make new device AuthContext

        // makes the link impl unified to reuse code
        setupArchiveTlsChannel(
            std::make_shared<AuthContext>(ctx),
            [ctx = std::make_shared<AuthContext>(ctx)](const DeviceId& deviceId, const std::string& name, std::shared_ptr(dhtnet::ChannelSocket> channel)) {
          	    // if socket weak ptr is null then return
                if (!buf || !acc)
                    return len;

                decodingCtx->pac.reserve_buffer(len);

                std::copy_n(buf, len, decodingCtx->pac.buffer());
                decodingCtx->pac.buffer_consumed(len);

                msgpack::object_handle oh;
                try {
          	        decodingCtx->pac.next(oh); // TODO move try catch to just this line in order to handle errors more narrowly
                } catch (std::exception e) {
                    JAMI_WARN("[Link Device] error unpacking message from msgpack");
                }

          	    if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::SCHEME) {
                    // do nothing
                    JAMI_WARN("[Link Device] this scheme should not be used on this device in the Link Device mode");
          	    } else if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::ATTEMPT) {
                    // handle receiving passwords and checking them against local password
                    if (!passwordSchemeMsg.passwordEnabled.emtpy() && passwordSchemeMsg.passwordEnabled) {
                        std::string pw = passwordSchememsg.passwordEnabled;
                        CredentialStatusMsg msg;
                        if (pw == password) {
                            // send back true in CredentialStatusMsg
                            // msg.passwordCorrect = true;
                        } else {
                            // update message failed counter i
                            // and send back a false CredentialStatusMsg
                            msg.passwordCorrect = false;
                        }
                        // SEND MSG HERE
                    }
          	    }
          	    else if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::ARCHIVE) {
                    // do nothing
                    JAMI_WARN("[Link Device] this scheme should not be used on this device in the Link Device mode");
          	    }
          	    else if (ctx->linkDevCtx->authDecodingState->channelOpId == AuthDecodingStateOpId::ASK) {
          	        JAMI_WARN("[Link Device] unimplemented ASK request");
                }
          	    else {
            		JAMI_WARN("[Link Device] channel operation mode is unspecified (unspecified auth protocol)");
                }
            },
            true // oldDevice = true because this is the old device
        );

        // KEEP until done
        // JAMI_WARN("[Auth] exporting account with PIN: %s at %s (size %zu)",
        //             pin_str.c_str(),
        //             loc.toString().c_str(),
        //             encrypted.size());
        // this_->dht_->put(loc, encrypted, [cb, pin = std::move(pin_str)](bool ok) {
        //     JAMI_DBG("[Auth] account archive published: %s", ok ? "success" : "failure");
        //     if (ok)
        //         cb(AddDeviceResult::SUCCESS_SHOW_PIN, pin);
        //     else
        //         cb(AddDeviceResult::ERROR_NETWORK, {});
        // });

        // TODO get ctx
        // startSendArchiveToDevice(std::move(ctx)); // don't outsource here




        // // THIS happens before this point!
        // // // IDEA v2: 0=successful load, 1=failed load, 2=operations in progress so keep waiting??
        // ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(), ctx->accountId, 2, "scan_a_qr");
        // ///////ctx->emitSignal<libjami::ConfigurationSignal::DeviceAuthStateChanged>(ctx->accountId, std::move(accountScheme), 2, "auth_session_ready");


            // ctx->linkDevCtx->connectionManager->onConnectionReady([ctx = std::make_shared<AuthContext>(ctx)](const DeviceId& deviceId, const std::string& name, std::shared_ptr(dhtnet::ChannelSocket> channel)) {
            //   	// TODO setOnRecv
            //   	socket->setOnRecv([wsocket = std::weak_ptr<dhtnet::ChannelSocketInterface>(socket), ctx = std::make_shared<AuthContext>(ctx), decodingCtx = std::make_shared<AuthDecodingCtx>()](const uint8_t* buf, size_t len) {
            //   	} // !handle different operation states
            // //} // !try
            // // catch {
            // // } // !catch
            //   	}); // !setOnRecv
            // }); // !onConnectionReady // TODO emit AuthStateChanged+"connection ready" signal
            //

        // TODO
        // before this function:
        // // emit signal to scan device and wait for callback containing tmpId

        // // find account on DHT

        // // establish P2P TLS

        // in this function
        // // send password type for archive (going to need the archive so just do this in the other function)

        // // receive password from new device (in hash form?? ask Adrien) and check it against local device password
        // // 1. check password against local
        // // 2. update failure counter
        // // 3. close everything if failure counter is > 3 (set some bool for session_terminated in the context)







    });
}

bool
ArchiveAccountManager::revokeDevice(
                                    const std::string& password,
                                    const std::string& device,
                                    RevokeDeviceCallback cb)
{
    auto fa = dht::ThreadPool::computation().getShared<AccountArchive>(
        [this, password] { return readArchive(password); });
    findCertificate(DeviceId(device),
        [fa = std::move(fa), password, device, cb, w=weak_from_this()](
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

                this_->saveArchive(a, password);
                this_->info_->contacts->removeAccountDevice(crt->getLongId());
                cb(RevokeDeviceResult::SUCCESS);
                this_->syncDevices();
            });
    return false;
}

bool
ArchiveAccountManager::exportArchive(const std::string& destinationPath, const std::string& password)
{
    try {
        // Save contacts if possible before exporting
        AccountArchive archive = readArchive(password);
        updateArchive(archive);
        archive.save(fileutils::getFullPath(path_, archivePath_).string(), password);

        // Export the file
        auto sourcePath = fileutils::getFullPath(path_, archivePath_);
        std::ifstream src(sourcePath, std::ios::in | std::ios::binary);
        if (!src)
            return false;
        std::ofstream dst(destinationPath, std::ios::out | std::ios::binary);
        dst << src.rdbuf();
        return true;
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
        readArchive(password);
        return true;
    } catch (...) {
        return false;
    }
}

#if HAVE_RINGNS
void
ArchiveAccountManager::registerName(const std::string& password,
                                    const std::string& name,
                                    RegistrationCallback cb)
{
    std::string signedName;
    auto nameLowercase {name};
    std::transform(nameLowercase.begin(), nameLowercase.end(), nameLowercase.begin(), ::tolower);
    std::string publickey;
    std::string accountId;
    std::string ethAccount;

    try {
        auto archive = readArchive(password);
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
