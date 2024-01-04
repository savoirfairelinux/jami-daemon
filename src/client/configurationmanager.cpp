/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configurationmanager_interface.h"
#include "account_schema.h"
#include "manager.h"
#include "logger.h"
#include "fileutils.h"
#include "archiver.h"
#include "sip/sipaccount.h"
#include "jamidht/jamiaccount.h"
#include "sip/sipaccount_config.h"
#include "jamidht/jamiaccount_config.h"
#include "audio/audiolayer.h"
#include "system_codec_container.h"
#include "account_const.h"
#include "client/ring_signal.h"
#include "audio/ringbufferpool.h"
#include "connectivity/security/tlsvalidator.h"

#include <dhtnet/ip_utils.h>
#include <dhtnet/upnp/upnp_context.h>
#include <dhtnet/certstore.h>


#include <regex>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef _MSC_VER
#include "windirent.h"
#else
#include <dirent.h>
#endif

#include <cerrno>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#undef interface
#endif

namespace libjami {

constexpr unsigned CODECS_NOT_LOADED = 0x1000; /** Codecs not found */

using jami::SIPAccount;
using jami::JamiAccount;
using jami::tls::TlsValidator;
using jami::AudioDeviceType;

void
registerConfHandlers(const std::map<std::string, std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    registerSignalHandlers(handlers);
}

std::map<std::string, std::string>
getAccountDetails(const std::string& accountId)
{
    return jami::Manager::instance().getAccountDetails(accountId);
}

std::map<std::string, std::string>
getVolatileAccountDetails(const std::string& accountId)
{
    return jami::Manager::instance().getVolatileAccountDetails(accountId);
}

std::map<std::string, std::string>
validateCertificate(const std::string& accountId, const std::string& certificate)
{
    try {
        if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
            return TlsValidator {acc->certStore(), acc->certStore().getCertificate(certificate)}
                .getSerializedChecks();
    } catch (const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
    }
    return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
}

std::map<std::string, std::string>
validateCertificatePath(const std::string& accountId,
                        const std::string& certificate,
                        const std::string& privateKey,
                        const std::string& privateKeyPass,
                        const std::string& caList)
{
    try {
        if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
            return TlsValidator {acc->certStore(), certificate, privateKey, privateKeyPass, caList}
                .getSerializedChecks();
    } catch (const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
        return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
    }
    return {};
}

std::map<std::string, std::string>
getCertificateDetails(const std::string& accountId, const std::string& certificate)
{
    try {
        if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
            return TlsValidator {acc->certStore(), acc->certStore().getCertificate(certificate)}
                .getSerializedDetails();
    } catch (const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
    }
    return {};
}

std::map<std::string, std::string>
getCertificateDetailsPath(const std::string& accountId,
                          const std::string& certificate,
                          const std::string& privateKey,
                          const std::string& privateKeyPassword)
{
    try {
        auto crt = std::make_shared<dht::crypto::Certificate>(
            jami::fileutils::loadFile(certificate));
        if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
            TlsValidator validator {acc->certStore(), certificate, privateKey, privateKeyPassword};
            acc->certStore().pinCertificate(validator.getCertificate(), false);
            return validator.getSerializedDetails();
        }
    } catch (const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
    }
    return {};
}

std::vector<std::string>
getPinnedCertificates(const std::string& accountId)
{
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->certStore().getPinnedCertificates();
    return {};
}

std::vector<std::string>
pinCertificate(const std::string& accountId, const std::vector<uint8_t>& certificate, bool local)
{
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->certStore().pinCertificate(certificate, local);
    return {};
}

void
pinCertificatePath(const std::string& accountId, const std::string& path)
{
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->certStore().pinCertificatePath(path);
}

bool
unpinCertificate(const std::string& accountId, const std::string& certId)
{
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->certStore().unpinCertificate(certId);
    return {};
}

unsigned
unpinCertificatePath(const std::string& accountId, const std::string& path)
{
    if (const auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->certStore().unpinCertificatePath(path);
    return {};
}

bool
pinRemoteCertificate(const std::string& accountId, const std::string& certId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        acc->dht()->findCertificate(dht::InfoHash(certId),
                                    [](const std::shared_ptr<dht::crypto::Certificate>& crt) {});
        return true;
    }
    return false;
}

bool
setCertificateStatus(const std::string& accountId,
                     const std::string& certId,
                     const std::string& ststr)
{
    try {
        if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
            auto status = dhtnet::tls::TrustStore::statusFromStr(ststr.c_str());
            return acc->setCertificateStatus(certId, status);
        }
    } catch (const std::out_of_range&) {
    }
    return false;
}

std::vector<std::string>
getCertificatesByStatus(const std::string& accountId, const std::string& ststr)
{
    auto status = dhtnet::tls::TrustStore::statusFromStr(ststr.c_str());
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getCertificatesByStatus(status);
    return {};
}

void
setAccountDetails(const std::string& accountId, const std::map<std::string, std::string>& details)
{
    jami::Manager::instance().setAccountDetails(accountId, details);
}

void
setAccountActive(const std::string& accountId, bool enable, bool shutdownConnections)
{
    jami::Manager::instance().setAccountActive(accountId, enable, shutdownConnections);
}

void
sendRegister(const std::string& accountId, bool enable)
{
    jami::Manager::instance().sendRegister(accountId, enable);
}

bool
isPasswordValid(const std::string& accountId, const std::string& password)
{
    if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(accountId))
        return acc->isPasswordValid(password);
    return false;
}

std::vector<uint8_t>
getPasswordKey(const std::string& accountID, const std::string& password)
{
    if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(accountID))
        return acc->getPasswordKey(password);
    return {};
}


void
registerAllAccounts()
{
    jami::Manager::instance().registerAccounts();
}

uint64_t
sendAccountTextMessage(const std::string& accountId,
                       const std::string& to,
                       const std::map<std::string, std::string>& payloads,
                       int32_t flags)
{
    bool onlyConnected = flags & 0x1;
    return jami::Manager::instance().sendTextMessage(accountId, to, payloads, onlyConnected);
}

std::vector<Message>
getLastMessages(const std::string& accountId, const uint64_t& base_timestamp)
{
    if (const auto acc = jami::Manager::instance().getAccount(accountId))
        return acc->getLastMessages(base_timestamp);
    return {};
}

std::map<std::string, std::string>
getNearbyPeers(const std::string& accountId)
{
    return jami::Manager::instance().getNearbyPeers(accountId);
}

int
getMessageStatus(uint64_t messageId)
{
    return jami::Manager::instance().getMessageStatus(messageId);
}

int
getMessageStatus(const std::string& accountId, uint64_t messageId)
{
    return jami::Manager::instance().getMessageStatus(accountId, messageId);
}

bool
cancelMessage(const std::string& accountId, uint64_t messageId)
{
    if (const auto acc = jami::Manager::instance().getAccount(accountId))
        return acc->cancelMessage(messageId);
    return {};
}

void
setIsComposing(const std::string& accountId, const std::string& conversationUri, bool isWriting)
{
    if (const auto acc = jami::Manager::instance().getAccount(accountId))
        acc->setIsComposing(conversationUri, isWriting);
}

bool
setMessageDisplayed(const std::string& accountId,
                    const std::string& conversationUri,
                    const std::string& messageId,
                    int status)
{
    if (const auto acc = jami::Manager::instance().getAccount(accountId))
        return acc->setMessageDisplayed(conversationUri, messageId, status);
    return false;
}

// deprecated
bool
exportOnRing(const std::string& accountId, const std::string& password)
{
    return false;
}


const std::regex AUTH_URI_VALIDATOR {
    "jami-auth:\/\/([-a-zA-Z0-9@:%._\+~#=]{40}\b)\/([0-9]{6}\b)"
};

// TODO make this function and JamiAccount::addDevice unified in return type either void or uint64_t... and also add the appropriate callback listeners to unitTest/linkdevice
uint32_t // TODO change uint8_t to error code and cast it where needed for java/qt bindings
exportToPeer(const std::string& accountId, const std::string& uri)
{
    JAMI_DEBUG("[LinkDevice {}] exportToPeer called.", accountId);
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
            // TODO validate with regex in the future
            // bool hasCorrectScheme = false;
            // std::cmatch urlMatches;
            // if (std::regex_match(uri, pieces_match, URI_VALIDATOR)) {
            //     if (pieces_match.size() == 4) {
            //         if (pieces_match[2].length() == 0)
            //             instance(default_ns).lookupName(pieces_match[3], std::move(cb));
            //         else
            //             instance(pieces_match[3].str()).lookupName(pieces_match[2], std::move(cb));
            //         return;
            //     }
            // }
            // JAMI_ERR("Can't parse URI: %.*s", (int) uri.size(), uri.data());
        bool uriValid = false;
        try {
            const std::string prefix = "jami-auth://";
            const std::string uriPrefix = uri.substr(0, prefix.length());
            const std::string accountUsername = uri.substr(prefix.length(), 40);
            const std::string accountCodeStr = uri.substr(prefix.length()+41, 6);
            uint64_t accountCode = std::stoull(accountCodeStr);
            if (uriPrefix == prefix) {
                uriValid = true;
            }
        } catch (const std::exception& e) {
            JAMI_ERROR("[LinkDevice] Error: invalid jami-auth url: {}", uri);
        }
        return account->addDevice(accountId, uri);
    }
    // TODO standardize error codes
    return 0;
}

bool
exportToFile(const std::string& accountId,
            const std::string& destinationPath,
            const std::string& scheme,
            const std::string& password)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        return account->exportArchive(destinationPath, scheme, password);
    }
    return false;
}

bool
revokeDevice(const std::string& accountId, const std::string& deviceId, const std::string& scheme, const std::string& password)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        return account->revokeDevice(deviceId, scheme, password);
    }
    return false;
}

std::map<std::string, std::string>
getKnownRingDevices(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getKnownDevices();
    return {};
}

bool
changeAccountPassword(const std::string& accountId,
                      const std::string& password_old,
                      const std::string& password_new)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->changeArchivePassword(password_old, password_new);
    return false;
}

/* contacts */

void
addContact(const std::string& accountId, const std::string& uri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->addContact(uri);
}

void
removeContact(const std::string& accountId, const std::string& uri, bool ban)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->removeContact(uri, ban);
}

std::map<std::string, std::string>
getContactDetails(const std::string& accountId, const std::string& uri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getContactDetails(uri);
    return {};
}

std::vector<std::map<std::string, std::string>>
getContacts(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getContacts();
    return {};
}

/* contact requests */
std::vector<std::map<std::string, std::string>>
getTrustRequests(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getTrustRequests();
    return {};
}

bool
acceptTrustRequest(const std::string& accountId, const std::string& from)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->acceptTrustRequest(from);
    return false;
}

bool
discardTrustRequest(const std::string& accountId, const std::string& from)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->discardTrustRequest(from);
    return false;
}

void
sendTrustRequest(const std::string& accountId,
                 const std::string& to,
                 const std::vector<uint8_t>& payload)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->sendTrustRequest(to, payload);
}

/// This function is used as a base for new accounts for clients that support it
std::map<std::string, std::string>
getAccountTemplate(const std::string& accountType)
{
    if (accountType == Account::ProtocolNames::RING)
        return jami::JamiAccountConfig().toMap();
    else if (accountType == Account::ProtocolNames::SIP)
        return jami::SipAccountConfig().toMap();
    return {};
}

std::string
addAccount(const std::map<std::string, std::string>& details, const std::string& accountId)
{
    return jami::Manager::instance().addAccount(details, accountId);
}

void
monitor(bool continuous)
{
    return jami::Manager::instance().monitor(continuous);
}

std::vector<std::map<std::string, std::string>>
getConnectionList(const std::string& accountId, const std::string& conversationId)
{
    return jami::Manager::instance().getConnectionList(accountId, conversationId);
}

std::vector<std::map<std::string, std::string>>
getChannelList(const std::string& accountId, const std::string& connectionId)
{
    return jami::Manager::instance().getChannelList(accountId, connectionId);
}

void
removeAccount(const std::string& accountId)
{
    return jami::Manager::instance().removeAccount(accountId, true); // with 'flush' enabled
}

std::vector<std::string>
getAccountList()
{
    return jami::Manager::instance().getAccountList();
}

/**
 * Send the list of all codecs loaded to the client through DBus.
 * Can stay global, as only the active codecs will be set per accounts
 */
std::vector<unsigned>
getCodecList()
{
    std::vector<unsigned> list {
        jami::getSystemCodecContainer()->getSystemCodecInfoIdList(jami::MEDIA_ALL)};
    if (list.empty())
        jami::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
    return list;
}

std::vector<std::string>
getSupportedTlsMethod()
{
    return SIPAccount::getSupportedTlsProtocols();
}

std::vector<std::string>
getSupportedCiphers(const std::string& accountId)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountId))
        return SIPAccount::getSupportedTlsCiphers();
    JAMI_ERR("SIP account %s doesn't exist", accountId.c_str());
    return {};
}

bool
setCodecDetails(const std::string& accountId,
                const unsigned& codecId,
                const std::map<std::string, std::string>& details)
{
    auto acc = jami::Manager::instance().getAccount(accountId);
    if (!acc) {
        JAMI_ERR("Could not find account %s. can not set codec details", accountId.c_str());
        return false;
    }

    auto codec = acc->searchCodecById(codecId, jami::MEDIA_ALL);
    if (!codec) {
        JAMI_ERR("can not find codec %d", codecId);
        return false;
    }
    try {
        if (codec->mediaType & jami::MEDIA_AUDIO) {
            if (auto foundCodec = std::static_pointer_cast<jami::SystemAudioCodecInfo>(codec)) {
                foundCodec->setCodecSpecifications(details);
                jami::emitSignal<ConfigurationSignal::MediaParametersChanged>(accountId);
                return true;
            }
        }

        if (codec->mediaType & jami::MEDIA_VIDEO) {
            if (auto foundCodec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(codec)) {
                foundCodec->setCodecSpecifications(details);
                JAMI_WARN("parameters for %s changed ", foundCodec->name.c_str());
                if (auto call = jami::Manager::instance().getCurrentCall()) {
                    if (call->getVideoCodec() == foundCodec) {
                        JAMI_WARN("%s running. Need to restart encoding",
                                  foundCodec->name.c_str());
                        call->restartMediaSender();
                    }
                }
                jami::emitSignal<ConfigurationSignal::MediaParametersChanged>(accountId);
                return true;
            }
        }
    } catch (const std::exception& e) {
        JAMI_ERR("Cannot set codec specifications: %s", e.what());
    }

    return false;
}

std::map<std::string, std::string>
getCodecDetails(const std::string& accountId, const unsigned& codecId)
{
    auto acc = jami::Manager::instance().getAccount(accountId);
    if (!acc) {
        JAMI_ERR("Could not find account %s return default codec details", accountId.c_str());
        return jami::Account::getDefaultCodecDetails(codecId);
    }

    auto codec = acc->searchCodecById(codecId, jami::MEDIA_ALL);
    if (!codec) {
        jami::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
        return {};
    }

    if (codec->mediaType & jami::MEDIA_AUDIO)
        if (auto foundCodec = std::static_pointer_cast<jami::SystemAudioCodecInfo>(codec))
            return foundCodec->getCodecSpecifications();

    if (codec->mediaType & jami::MEDIA_VIDEO)
        if (auto foundCodec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(codec))
            return foundCodec->getCodecSpecifications();

    jami::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
    return {};
}

std::vector<unsigned>
getActiveCodecList(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount(accountId))
        return acc->getActiveCodecs();
    JAMI_ERR("Could not find account %s, returning default", accountId.c_str());
    return jami::Account::getDefaultCodecsId();
}

void
setActiveCodecList(const std::string& accountId, const std::vector<unsigned>& list)
{
    if (auto acc = jami::Manager::instance().getAccount(accountId)) {
        acc->setActiveCodecs(list);
        jami::Manager::instance().saveConfig(acc);
    } else {
        JAMI_ERR("Could not find account %s", accountId.c_str());
    }
}

std::vector<std::string>
getAudioPluginList()
{
    return {PCM_DEFAULT, PCM_DMIX_DSNOOP};
}

void
setAudioPlugin(const std::string& audioPlugin)
{
    return jami::Manager::instance().setAudioPlugin(audioPlugin);
}

std::vector<std::string>
getAudioOutputDeviceList()
{
    return jami::Manager::instance().getAudioOutputDeviceList();
}

std::vector<std::string>
getAudioInputDeviceList()
{
    return jami::Manager::instance().getAudioInputDeviceList();
}

void
setAudioOutputDevice(int32_t index)
{
    return jami::Manager::instance().setAudioDevice(index, AudioDeviceType::PLAYBACK);
}

void
setAudioInputDevice(int32_t index)
{
    return jami::Manager::instance().setAudioDevice(index, AudioDeviceType::CAPTURE);
}

void
startAudio()
{
    jami::Manager::instance().startAudio();
}

void
setAudioRingtoneDevice(int32_t index)
{
    return jami::Manager::instance().setAudioDevice(index, AudioDeviceType::RINGTONE);
}

std::vector<std::string>
getCurrentAudioDevicesIndex()
{
    return jami::Manager::instance().getCurrentAudioDevicesIndex();
}

int32_t
getAudioInputDeviceIndex(const std::string& name)
{
    return jami::Manager::instance().getAudioInputDeviceIndex(name);
}

int32_t
getAudioOutputDeviceIndex(const std::string& name)
{
    return jami::Manager::instance().getAudioOutputDeviceIndex(name);
}

std::string
getCurrentAudioOutputPlugin()
{
    auto plugin = jami::Manager::instance().getCurrentAudioOutputPlugin();
    JAMI_DBG("Get audio plugin %s", plugin.c_str());
    return plugin;
}

std::string
getNoiseSuppressState()
{
    return jami::Manager::instance().getNoiseSuppressState();
}

void
setNoiseSuppressState(const std::string& state)
{
    jami::Manager::instance().setNoiseSuppressState(state);
}

bool
isAgcEnabled()
{
    return jami::Manager::instance().isAGCEnabled();
}

void
setAgcState(bool enabled)
{
    jami::Manager::instance().setAGCState(enabled);
}

std::string
getRecordPath()
{
    return jami::Manager::instance().audioPreference.getRecordPath();
}

void
setRecordPath(const std::string& recPath)
{
    jami::Manager::instance().audioPreference.setRecordPath(recPath);
}

bool
getIsAlwaysRecording()
{
    return jami::Manager::instance().getIsAlwaysRecording();
}

void
setIsAlwaysRecording(bool rec)
{
    jami::Manager::instance().setIsAlwaysRecording(rec);
}

bool
getRecordPreview()
{
#ifdef ENABLE_VIDEO
    return jami::Manager::instance().videoPreferences.getRecordPreview();
#else
    return false;
#endif
}

void
setRecordPreview(bool rec)
{
#ifdef ENABLE_VIDEO
    jami::Manager::instance().videoPreferences.setRecordPreview(rec);
    jami::Manager::instance().saveConfig();
#endif
}

int32_t
getRecordQuality()
{
#ifdef ENABLE_VIDEO
    return jami::Manager::instance().videoPreferences.getRecordQuality();
#else
    return 0;
#endif
}

void
setRecordQuality(int32_t quality)
{
#ifdef ENABLE_VIDEO
    jami::Manager::instance().videoPreferences.setRecordQuality(quality);
    jami::Manager::instance().saveConfig();
#endif
}

int32_t
getHistoryLimit()
{
    return jami::Manager::instance().getHistoryLimit();
}

void
setHistoryLimit(int32_t days)
{
    jami::Manager::instance().setHistoryLimit(days);
}

int32_t
getRingingTimeout()
{
    return jami::Manager::instance().getRingingTimeout();
}

void
setRingingTimeout(int32_t timeout)
{
    jami::Manager::instance().setRingingTimeout(timeout);
}

std::vector<std::string>
getSupportedAudioManagers()
{
    return jami::AudioPreference::getSupportedAudioManagers();
}

bool
setAudioManager(const std::string& api)
{
    return jami::Manager::instance().setAudioManager(api);
}

std::string
getAudioManager()
{
    return jami::Manager::instance().getAudioManager();
}

void
setVolume(const std::string& device, double value)
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver()) {
        JAMI_DBG("set volume for %s: %f", device.c_str(), value);

        if (device == "speaker")
            audiolayer->setPlaybackGain(value);
        else if (device == "mic")
            audiolayer->setCaptureGain(value);

        jami::emitSignal<ConfigurationSignal::VolumeChanged>(device, value);
    } else {
        JAMI_ERR("Audio layer not valid while updating volume");
    }
}

double
getVolume(const std::string& device)
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver()) {
        if (device == "speaker")
            return audiolayer->getPlaybackGain();
        if (device == "mic")
            return audiolayer->getCaptureGain();
    }

    JAMI_ERR("Audio layer not valid while updating volume");
    return 0.0;
}

// FIXME: we should store "muteDtmf" instead of "playDtmf"
// in config and avoid negating like this
bool
isDtmfMuted()
{
    return not jami::Manager::instance().voipPreferences.getPlayDtmf();
}

void
muteDtmf(bool mute)
{
    jami::Manager::instance().voipPreferences.setPlayDtmf(not mute);
}

bool
isCaptureMuted()
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver())
        return audiolayer->isCaptureMuted();

    JAMI_ERR("Audio layer not valid");
    return false;
}

void
muteCapture(bool mute)
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver())
        return audiolayer->muteCapture(mute);

    JAMI_ERR("Audio layer not valid");
    return;
}

bool
isPlaybackMuted()
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver())
        return audiolayer->isPlaybackMuted();

    JAMI_ERR("Audio layer not valid");
    return false;
}

void
mutePlayback(bool mute)
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver())
        return audiolayer->mutePlayback(mute);

    JAMI_ERR("Audio layer not valid");
    return;
}

bool
isRingtoneMuted()
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver())
        return audiolayer->isRingtoneMuted();

    JAMI_ERR("Audio layer not valid");
    return false;
}

void
muteRingtone(bool mute)
{
    if (auto audiolayer = jami::Manager::instance().getAudioDriver())
        return audiolayer->muteRingtone(mute);

    JAMI_ERR("Audio layer not valid");
    return;
}

void
setAccountsOrder(const std::string& order)
{
    jami::Manager::instance().setAccountsOrder(order);
}

std::string
getAddrFromInterfaceName(const std::string& interface)
{
    return dhtnet::ip_utils::getInterfaceAddr(interface, AF_INET);
}

std::vector<std::string>
getAllIpInterface()
{
    return dhtnet::ip_utils::getAllIpInterface();
}

std::vector<std::string>
getAllIpInterfaceByName()
{
    return dhtnet::ip_utils::getAllIpInterfaceByName();
}

std::vector<std::map<std::string, std::string>>
getCredentials(const std::string& accountId)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountId))
        return sipaccount->getCredentials();
    return {};
}

void
setCredentials(const std::string& accountId,
               const std::vector<std::map<std::string, std::string>>& details)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountId)) {
        sipaccount->doUnregister([&](bool /* transport_free */) {
            sipaccount->editConfig(
                [&](jami::SipAccountConfig& config) { config.setCredentials(details); });
            sipaccount->loadConfig();
            if (sipaccount->isEnabled())
                sipaccount->doRegister();
        });
        jami::Manager::instance().saveConfig(sipaccount);
    }
}

void
connectivityChanged()
{
    JAMI_WARN("received connectivity changed - trying to re-connect enabled accounts");

    // reset the UPnP context
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    try {
        jami::Manager::instance().upnpContext()->connectivityChanged();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
#endif

    for (const auto& account : jami::Manager::instance().getAllAccounts()) {
        account->connectivityChanged();
    }
}

bool
lookupName(const std::string& account, const std::string& nameserver, const std::string& name)
{
#if HAVE_RINGNS
    if (account.empty()) {
        auto cb = [name](const std::string& result, jami::NameDirectory::Response response) {
            jami::emitSignal<libjami::ConfigurationSignal::RegisteredNameFound>("",
                                                                                (int) response,
                                                                                result,
                                                                                name);
        };
        if (nameserver.empty())
            jami::NameDirectory::lookupUri(name, "", cb);
        else
            jami::NameDirectory::instance(nameserver).lookupName(name, cb);
        return true;
    } else if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(account)) {
        acc->lookupName(name);
        return true;
    }
#endif
    return false;
}

bool
lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address)
{
#if HAVE_RINGNS
    if (account.empty()) {
        jami::NameDirectory::instance(nameserver)
            .lookupAddress(address,
                           [address](const std::string& result,
                                     jami::NameDirectory::Response response) {
                               jami::emitSignal<libjami::ConfigurationSignal::RegisteredNameFound>(
                                   "", (int) response, address, result);
                           });
        return true;
    } else if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(account)) {
        acc->lookupAddress(address);
        return true;
    }
#endif
    return false;
}

bool
searchUser(const std::string& account, const std::string& query)
{
    if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(account)) {
        return acc->searchUser(query);
    }
    return false;
}

bool
registerName(const std::string& account, const std::string& name, const std::string& scheme, const std::string& password)
{
#if HAVE_RINGNS
    if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(account)) {
        acc->registerName(name, scheme, password);
        return true;
    }
#endif
    return false;
}

void
setPushNotificationToken(const std::string& token)
{
    for (const auto& account : jami::Manager::instance().getAllAccounts()) {
        account->setPushNotificationToken(token);
    }
}

void
setPushNotificationTopic(const std::string& topic)
{
    for (const auto& account : jami::Manager::instance().getAllAccounts()) {
        account->setPushNotificationTopic(topic);
    }
}

void
setPushNotificationConfig(const std::map<std::string, std::string>& data)
{
    for (const auto& account : jami::Manager::instance().getAllAccounts()) {
        account->setPushNotificationConfig(data);
    }
}

void
pushNotificationReceived(const std::string& from, const std::map<std::string, std::string>& data)
{
    try {
        auto it = data.find("to");
        if (it != data.end()) {
            if (auto account = jami::Manager::instance().getAccount<JamiAccount>(it->second))
                account->pushNotificationReceived(from, data);
        }
#if defined(__ANDROID__) || defined(ANDROID) || defined(__Apple__)
        else {
            for (const auto& sipAccount : jami::Manager::instance().getAllAccounts<SIPAccount>()) {
                sipAccount->pushNotificationReceived(from, data);
            }
        }
#endif
    } catch (const std::exception& e) {
        JAMI_ERR("Error processing push notification: %s", e.what());
    }
}

bool
isAudioMeterActive(const std::string& id)
{
    return jami::Manager::instance().getRingBufferPool().isAudioMeterActive(id);
}

void
setAudioMeterState(const std::string& id, bool state)
{
    jami::Manager::instance().getRingBufferPool().setAudioMeterState(id, state);
}

void
setDefaultModerator(const std::string& accountId, const std::string& peerURI, bool state)
{
    jami::Manager::instance().setDefaultModerator(accountId, peerURI, state);
}

std::vector<std::string>
getDefaultModerators(const std::string& accountId)
{
    return jami::Manager::instance().getDefaultModerators(accountId);
}

void
enableLocalModerators(const std::string& accountId, bool isModEnabled)
{
    jami::Manager::instance().enableLocalModerators(accountId, isModEnabled);
}

bool
isLocalModeratorsEnabled(const std::string& accountId)
{
    return jami::Manager::instance().isLocalModeratorsEnabled(accountId);
}

void
setAllModerators(const std::string& accountId, bool allModerators)
{
    jami::Manager::instance().setAllModerators(accountId, allModerators);
}

bool
isAllModerators(const std::string& accountId)
{
    return jami::Manager::instance().isAllModerators(accountId);
}

} // namespace libjami
