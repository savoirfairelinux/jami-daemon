/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
#include "security/tlsvalidator.h"
#include "security/certstore.h"
#include "logger.h"
#include "fileutils.h"
#include "archiver.h"
#include "ip_utils.h"
#include "sip/sipaccount.h"
#include "jamidht/jamiaccount.h"
#include "audio/audiolayer.h"
#include "system_codec_container.h"
#include "account_const.h"
#include "client/ring_signal.h"
#include "upnp/upnp_context.h"
#include "audio/ringbufferpool.h"

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

namespace DRing {

constexpr unsigned CODECS_NOT_LOADED = 0x1000; /** Codecs not found */

using jami::SIPAccount;
using jami::JamiAccount;
using jami::tls::TlsValidator;
using jami::tls::CertificateStore;
using jami::DeviceType;
using jami::HookPreference;

void
registerConfHandlers(const std::map<std::string,
    std::shared_ptr<CallbackWrapperBase>>&handlers)
{
    registerSignalHandlers(handlers);
}

std::map<std::string, std::string>
getAccountDetails(const std::string& accountID)
{
    return jami::Manager::instance().getAccountDetails(accountID);
}

std::map<std::string, std::string>
getVolatileAccountDetails(const std::string& accountID)
{
    return jami::Manager::instance().getVolatileAccountDetails(accountID);
}

std::map<std::string, std::string>
testAccountICEInitialization(const std::string& accountID)
{
    return jami::Manager::instance().testAccountICEInitialization(accountID);
}

std::map<std::string, std::string>
getTlsDefaultSettings()
{
    std::stringstream portstr;
    portstr << jami::sip_utils::DEFAULT_SIP_TLS_PORT;

    return {
        {jami::Conf::CONFIG_TLS_LISTENER_PORT, portstr.str()},
        {jami::Conf::CONFIG_TLS_CA_LIST_FILE, ""},
        {jami::Conf::CONFIG_TLS_CERTIFICATE_FILE, ""},
        {jami::Conf::CONFIG_TLS_PRIVATE_KEY_FILE, ""},
        {jami::Conf::CONFIG_TLS_PASSWORD, ""},
        {jami::Conf::CONFIG_TLS_METHOD, "Default"},
        {jami::Conf::CONFIG_TLS_CIPHERS, ""},
        {jami::Conf::CONFIG_TLS_SERVER_NAME, ""},
        {jami::Conf::CONFIG_TLS_VERIFY_SERVER, "true"},
        {jami::Conf::CONFIG_TLS_VERIFY_CLIENT, "true"},
        {jami::Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, "true"},
        {jami::Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, "2"}
    };
}

std::map<std::string, std::string>
validateCertificate(const std::string&,
                    const std::string& certificate)
{
    try {
        return TlsValidator{CertificateStore::instance().getCertificate(certificate)}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
        return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
    }
}

std::map<std::string, std::string>
validateCertificatePath(const std::string&,
                    const std::string& certificate,
                    const std::string& privateKey,
                    const std::string& privateKeyPass,
                    const std::string& caList)
{
    try {
        return TlsValidator{certificate, privateKey, privateKeyPass, caList}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
        return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
    }
}

std::map<std::string, std::string>
getCertificateDetails(const std::string& certificate)
{
    try {
        return TlsValidator{CertificateStore::instance().getCertificate(certificate)}.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
    }
    return {};
}

std::map<std::string, std::string>
getCertificateDetailsPath(const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPassword)
{
    try {
        auto crt = std::make_shared<dht::crypto::Certificate>(jami::fileutils::loadFile(certificate));
        TlsValidator validator {certificate, privateKey, privateKeyPassword};
        CertificateStore::instance().pinCertificate(validator.getCertificate(), false);
        return validator.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        JAMI_WARN("Certificate loading failed: %s", e.what());
    }
    return {};
}

std::vector<std::string>
getPinnedCertificates()
{
    return jami::tls::CertificateStore::instance().getPinnedCertificates();
}

std::vector<std::string>
pinCertificate(const std::vector<uint8_t>& certificate, bool local)
{
    return jami::tls::CertificateStore::instance().pinCertificate(certificate, local);
}

void
pinCertificatePath(const std::string& path)
{
    jami::tls::CertificateStore::instance().pinCertificatePath(path);
}

bool
unpinCertificate(const std::string& certId)
{
    return jami::tls::CertificateStore::instance().unpinCertificate(certId);
}

unsigned
unpinCertificatePath(const std::string& path)
{
    return jami::tls::CertificateStore::instance().unpinCertificatePath(path);
}

bool
pinRemoteCertificate(const std::string& accountId, const std::string& certId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->findCertificate(certId);
    return false;
}

bool
setCertificateStatus(const std::string& accountId, const std::string& certId, const std::string& ststr)
{
    try {
        if (accountId.empty()) {
            jami::tls::CertificateStore::instance().setTrustedCertificate(certId, jami::tls::trustStatusFromStr(ststr.c_str()));
        } else if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
            auto status = jami::tls::TrustStore::statusFromStr(ststr.c_str());
            return acc->setCertificateStatus(certId, status);
        }
    } catch (const std::out_of_range&) {}
    return false;
}

std::vector<std::string>
getCertificatesByStatus(const std::string& accountId, const std::string& ststr)
{
     auto status = jami::tls::TrustStore::statusFromStr(ststr.c_str());
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getCertificatesByStatus(status);
    return {};
}

void
setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    jami::Manager::instance().setAccountDetails(accountID, details);
}

void
setAccountActive(const std::string& accountID, bool enable)
{
    jami::Manager::instance().setAccountActive(accountID, enable);
}

void
sendRegister(const std::string& accountID, bool enable)
{
    jami::Manager::instance().sendRegister(accountID, enable);
}

void
registerAllAccounts()
{
    jami::Manager::instance().registerAccounts();
}

uint64_t
sendAccountTextMessage(const std::string& accountID, const std::string& to, const std::map<std::string, std::string>& payloads)
{
    return jami::Manager::instance().sendTextMessage(accountID, to, payloads);
}

std::vector<Message>
getLastMessages(const std::string& accountID, const uint64_t& base_timestamp)
{
    if (const auto acc = jami::Manager::instance().getAccount(accountID))
        return acc->getLastMessages(base_timestamp);
    return {};
}

std::map<std::string, std::string>
getNearbyPeers(const std::string& accountID)
{
    return jami::Manager::instance().getNearbyPeers(accountID);
}


int
getMessageStatus(uint64_t messageId)
{
    return jami::Manager::instance().getMessageStatus(messageId);
}

int
getMessageStatus(const std::string& accountID, uint64_t messageId)
{
    return jami::Manager::instance().getMessageStatus(accountID, messageId);
}

bool
cancelMessage(const std::string& accountID, uint64_t messageId)
{
    if (const auto acc = jami::Manager::instance().getAccount(accountID))
        return acc->cancelMessage(messageId);
    return {};
}

bool
exportOnRing(const std::string& accountID, const std::string& password)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountID)) {
        account->addDevice(password);
        return true;
    }
    return false;
}

bool
exportToFile(const std::string& accountID, const std::string& destinationPath, const std::string& password)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountID)) {
        return account->exportArchive(destinationPath, password);
    }
    return false;
}

bool
revokeDevice(const std::string& accountID, const std::string& password, const std::string& deviceID)
{
    if (const auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountID)) {
        return account->revokeDevice(password, deviceID);
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
changeAccountPassword(const std::string& accountID, const std::string& password_old, const std::string& password_new)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountID))
        if (acc->changeArchivePassword(password_old, password_new)) {
            jami::Manager::instance().saveConfig(acc);
            return true;
        }
    return false;
}

/* contacts */

void addContact(const std::string& accountId, const std::string& uri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->addContact(uri);
}

void removeContact(const std::string& accountId, const std::string& uri, bool ban)
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
sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->sendTrustRequest(to, payload);
}

/*
 * Import/Export accounts
 */
int
exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password)
{
    return jami::archiver::exportAccounts(accountIDs, filepath, password);
}

int
importAccounts(const std::string& archivePath, const std::string& password)
{
    return jami::archiver::importAccounts(archivePath, password);
}

///This function is used as a base for new accounts for clients that support it
std::map<std::string, std::string>
getAccountTemplate(const std::string& accountType)
{
    if (accountType == Account::ProtocolNames::RING)
        return jami::JamiAccount("dummy", false).getAccountDetails();
    else if (accountType == Account::ProtocolNames::SIP)
        return jami::SIPAccount("dummy", false).getAccountDetails();
    return {};
}

std::string
addAccount(const std::map<std::string, std::string>& details)
{
    return jami::Manager::instance().addAccount(details);
}

void
removeAccount(const std::string& accountID)
{
    return jami::Manager::instance().removeAccount(accountID, true); // with 'flush' enabled
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
    std::vector<unsigned> list {jami::getSystemCodecContainer()->getSystemCodecInfoIdList(jami::MEDIA_ALL)};
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
getSupportedCiphers(const std::string& accountID)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID))
        return SIPAccount::getSupportedTlsCiphers();
    JAMI_ERR("SIP account %s doesn't exist", accountID.c_str());
    return {};
}


bool
setCodecDetails(const std::string& accountID,
                const unsigned& codecId,
                const  std::map<std::string, std::string>& details)
{
    auto acc = jami::Manager::instance().getAccount(accountID);
    if (!acc) {
        JAMI_ERR("Could not find account %s. can not set codec details"
                , accountID.c_str());
        return false;
    }

    auto codec = acc->searchCodecById(codecId, jami::MEDIA_ALL);
    if (!codec) {
        JAMI_ERR("can not find codec %d", codecId);
        return false;

    }
    try {
        if (codec->systemCodecInfo.mediaType & jami::MEDIA_AUDIO) {
            if (auto foundCodec = std::static_pointer_cast<jami::AccountAudioCodecInfo>(codec)) {
                foundCodec->setCodecSpecifications(details);
                jami::emitSignal<ConfigurationSignal::MediaParametersChanged>(accountID);
                return true;
            }
        }

        if (codec->systemCodecInfo.mediaType & jami::MEDIA_VIDEO) {
            if (auto foundCodec = std::static_pointer_cast<jami::AccountVideoCodecInfo>(codec)) {
                if(foundCodec->isAutoQualityActivated(details)){
                    foundCodec->setCodecSpecifications(details);
                    if (auto call = jami::Manager::instance().getCurrentCall()) {
                        if (call->getVideoCodec() == foundCodec) {
                            JAMI_WARN("%s running. Need to restart encoding",
                                    foundCodec->systemCodecInfo.name.c_str());
                            call->activeAutoAdapt();
                        }
                    }
                }
                else
                {
                    foundCodec->setCodecSpecifications(details);
                    JAMI_WARN("parameters for %s changed ",
                    foundCodec->systemCodecInfo.name.c_str());
                    if (auto call = jami::Manager::instance().getCurrentCall()) {
                        if (call->getVideoCodec() == foundCodec) {
                            JAMI_WARN("%s running. Need to restart encoding",
                                    foundCodec->systemCodecInfo.name.c_str());
                            call->restartMediaSender();
                        }
                    }
                    jami::emitSignal<ConfigurationSignal::MediaParametersChanged>(accountID);
                }
                return true;
            }
        }
    } catch (const std::exception& e) {
        JAMI_ERR("Cannot set codec specifications: %s", e.what());
    }

    return false;
}

std::map<std::string, std::string>
getCodecDetails(const std::string& accountID, const unsigned& codecId)
{
    auto acc = jami::Manager::instance().getAccount(accountID);
    if (!acc)
    {
        JAMI_ERR("Could not find account %s return default codec details"
                , accountID.c_str());
        return jami::Account::getDefaultCodecDetails(codecId);
    }

    auto codec = acc->searchCodecById(codecId, jami::MEDIA_ALL);
    if (!codec)
    {
        jami::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
        return {};
    }

    if (codec->systemCodecInfo.mediaType & jami::MEDIA_AUDIO)
        if (auto foundCodec = std::static_pointer_cast<jami::AccountAudioCodecInfo>(codec))
            return foundCodec->getCodecSpecifications();

    if (codec->systemCodecInfo.mediaType & jami::MEDIA_VIDEO)
        if (auto foundCodec = std::static_pointer_cast<jami::AccountVideoCodecInfo>(codec))
            return foundCodec->getCodecSpecifications();

    jami::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
    return {};
}

std::vector<unsigned>
getActiveCodecList(const std::string& accountID)
{
    if (auto acc = jami::Manager::instance().getAccount(accountID))
        return acc->getActiveCodecs();
    JAMI_ERR("Could not find account %s, returning default", accountID.c_str());
    return jami::Account::getDefaultCodecsId();
}

void
setActiveCodecList(const std::string& accountID
        , const std::vector<unsigned>& list)
{
    if (auto acc = jami::Manager::instance().getAccount(accountID))
    {
        acc->setActiveCodecs(list);
        jami::Manager::instance().saveConfig(acc);
    } else {
        JAMI_ERR("Could not find account %s", accountID.c_str());
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
    return jami::Manager::instance().setAudioDevice(index, DeviceType::PLAYBACK);
}

void
setAudioInputDevice(int32_t index)
{
    return jami::Manager::instance().setAudioDevice(index, DeviceType::CAPTURE);
}

void
setAudioRingtoneDevice(int32_t index)
{
    return jami::Manager::instance().setAudioDevice(index, DeviceType::RINGTONE);
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

bool
getNoiseSuppressState()
{
    return jami::Manager::instance().getNoiseSuppressState();
}

void
setNoiseSuppressState(bool state)
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

std::map<std::string, std::string>
getHookSettings()
{
    return jami::Manager::instance().hookPreference.toMap();
}

void
setHookSettings(const std::map<std::string,
                std::string>& settings)
{
    jami::Manager::instance().hookPreference = HookPreference(settings);
}

void setAccountsOrder(const std::string& order)
{
    jami::Manager::instance().setAccountsOrder(order);
}

std::string
getAddrFromInterfaceName(const std::string& interface)
{
    return jami::ip_utils::getInterfaceAddr(interface);
}

std::vector<std::string>
getAllIpInterface()
{
    return jami::ip_utils::getAllIpInterface();
}

std::vector<std::string>
getAllIpInterfaceByName()
{
    return jami::ip_utils::getAllIpInterfaceByName();
}

std::map<std::string, std::string>
getShortcuts()
{
    return jami::Manager::instance().shortcutPreferences.getShortcuts();
}

void
setShortcuts(const std::map<std::string, std::string>& shortcutsMap)
{
    jami::Manager::instance().shortcutPreferences.setShortcuts(shortcutsMap);
    jami::Manager::instance().saveConfig();
}

std::vector<std::map<std::string, std::string>>
getCredentials(const std::string& accountID)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID))
        return sipaccount->getCredentials();
    return {};
}

void
setCredentials(const std::string& accountID,
               const std::vector<std::map<std::string, std::string>>& details)
{
    if (auto sipaccount = jami::Manager::instance().getAccount<SIPAccount>(accountID)) {
        sipaccount->doUnregister([&](bool /* transport_free */) {
            sipaccount->setCredentials(details);
            if (sipaccount->isEnabled())
                sipaccount->doRegister();
        });
    }
}

void
connectivityChanged()
{
    JAMI_WARN("received connectivity changed - trying to re-connect enabled accounts");

    // reset the UPnP context
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    try {
        jami::upnp::getUPnPContext()->connectivityChanged();
    } catch (std::runtime_error& e) {
        JAMI_ERR("UPnP context error: %s", e.what());
    }
#endif

    for (const auto &account : jami::Manager::instance().getAllAccounts()) {
        account->connectivityChanged();
    }
}

bool lookupName(const std::string& account, const std::string& nameserver, const std::string& name)
{
#if HAVE_RINGNS
    if (account.empty()) {
        auto cb = [name](const std::string& result, jami::NameDirectory::Response response) {
            jami::emitSignal<DRing::ConfigurationSignal::RegisteredNameFound>("", (int)response, result, name);
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

bool lookupAddress(const std::string& account, const std::string& nameserver, const std::string& address)
{
#if HAVE_RINGNS
    if (account.empty()) {
        jami::NameDirectory::instance(nameserver).lookupAddress(address, [address](const std::string& result, jami::NameDirectory::Response response) {
            jami::emitSignal<DRing::ConfigurationSignal::RegisteredNameFound>("", (int)response, address, result);
        });
        return true;
    } else if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(account)) {
        acc->lookupAddress(address);
        return true;
    }
#endif
    return false;
}

bool registerName(const std::string& account, const std::string& password, const std::string& name)
{
#if HAVE_RINGNS
    if (auto acc = jami::Manager::instance().getAccount<JamiAccount>(account)) {
        acc->registerName(password, name);
        return true;
    }
#endif
    return false;
}

void enableProxyClient(const std::string& accountID, bool enable)
{
    if (auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountID))
        account->enableProxyClient(enable);
}

void setPushNotificationToken(const std::string& token)
{
    for (const auto &account : jami::Manager::instance().getAllAccounts<JamiAccount>())
        account->setPushNotificationToken(token);
}

void pushNotificationReceived(const std::string& from, const std::map<std::string, std::string>& data)
{
    try {
        if (auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(data.at("to")))
            account->pushNotificationReceived(from, data);
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

} // namespace DRing
