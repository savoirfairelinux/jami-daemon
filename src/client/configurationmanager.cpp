/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configurationmanager_interface.h"
#include "account_schema.h"
#include "manager.h"
#if HAVE_TLS && HAVE_DHT
#include "security/tlsvalidator.h"
#include "security/certstore.h"
#endif
#include "logger.h"
#include "fileutils.h"
#include "ip_utils.h"
#include "sip/sipaccount.h"
#include "ringdht/ringaccount.h"
#if HAVE_IAX
#include "iax/iaxaccount.h"
#endif
#include "audio/audiolayer.h"
#include "system_codec_container.h"
#include "account_const.h"
#include "client/ring_signal.h"

#include <dirent.h>

#include <cerrno>
#include <cstring>
#include <sstream>

namespace DRing {

constexpr unsigned CODECS_NOT_LOADED = 0x1000; /** Codecs not found */

using ring::SIPAccount;
using ring::tls::TlsValidator;
using ring::tls::CertificateStore;
using ring::DeviceType;
using ring::HookPreference;

void
registerConfHandlers(const std::map<std::string,
                     std::shared_ptr<CallbackWrapperBase>>& handlers)
{
    auto& handlers_ = ring::getSignalHandlers();
    for (auto& item : handlers) {
        auto iter = handlers_.find(item.first);
        if (iter == handlers_.end()) {
            RING_ERR("Signal %s not supported", item.first.c_str());
            continue;
        }

        iter->second = std::move(item.second);
    }
}

std::map<std::string, std::string>
getIp2IpDetails()
{
    auto account = ring::Manager::instance().getIP2IPAccount();
    if (auto sipaccount = static_cast<SIPAccount*>(account.get()))
        return sipaccount->getIp2IpDetails();
    RING_ERR("Could not find IP2IP account");
    return std::map<std::string, std::string>();
}

std::map<std::string, std::string>
getAccountDetails(const std::string& accountID)
{
    return ring::Manager::instance().getAccountDetails(accountID);
}

std::map<std::string, std::string>
getVolatileAccountDetails(const std::string& accountID)
{
    return ring::Manager::instance().getVolatileAccountDetails(accountID);
}

std::map<std::string, std::string>
getTlsDefaultSettings()
{
    std::stringstream portstr;
    portstr << ring::sip_utils::DEFAULT_SIP_TLS_PORT;

    return {
        {ring::Conf::CONFIG_TLS_LISTENER_PORT, portstr.str()},
        {ring::Conf::CONFIG_TLS_CA_LIST_FILE, ""},
        {ring::Conf::CONFIG_TLS_CERTIFICATE_FILE, ""},
        {ring::Conf::CONFIG_TLS_PRIVATE_KEY_FILE, ""},
        {ring::Conf::CONFIG_TLS_PASSWORD, ""},
        {ring::Conf::CONFIG_TLS_METHOD, "Default"},
        {ring::Conf::CONFIG_TLS_CIPHERS, ""},
        {ring::Conf::CONFIG_TLS_SERVER_NAME, ""},
        {ring::Conf::CONFIG_TLS_VERIFY_SERVER, "true"},
        {ring::Conf::CONFIG_TLS_VERIFY_CLIENT, "true"},
        {ring::Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, "true"},
        {ring::Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, "2"}
    };
}

std::map<std::string, std::string>
validateCertificate(const std::string&,
                    const std::string& certificate)
{
#if HAVE_TLS && HAVE_DHT
    try {
        return TlsValidator{CertificateStore::instance().getCertificate(certificate)}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
        return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
    }
#else
    RING_WARN("TLS not supported");
    return {};
#endif
}

std::map<std::string, std::string>
validateCertificatePath(const std::string&,
                    const std::string& certificate,
                    const std::string& privateKey,
                    const std::string& caList)
{
#if HAVE_TLS && HAVE_DHT
    try {
        return TlsValidator{certificate, privateKey, caList}.getSerializedChecks();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
        return {{Certificate::ChecksNames::EXIST, Certificate::CheckValuesNames::FAILED}};
    }
#else
    RING_WARN("TLS not supported");
    return {};
#endif
}

std::map<std::string, std::string>
getCertificateDetails(const std::string& certificate)
{
#if HAVE_TLS && HAVE_DHT
    try {
        return TlsValidator{CertificateStore::instance().getCertificate(certificate)}.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed: %s", e.what());
    }
#else
    RING_WARN("TLS not supported");
#endif
    return {};
}

std::map<std::string, std::string>
getCertificateDetailsPath(const std::string& certificate)
{
#if HAVE_TLS && HAVE_DHT
    try {
        auto crt = std::make_shared<dht::crypto::Certificate>(ring::fileutils::loadFile(certificate));
        CertificateStore::instance().pinCertificate(crt, false);
        return TlsValidator{crt}.getSerializedDetails();
    } catch(const std::runtime_error& e) {
        RING_WARN("Certificate loading failed");
    }
#else
    RING_WARN("TLS not supported");
#endif
    return {};
}

std::vector<std::string>
getPinnedCertificates()
{
#if HAVE_TLS && HAVE_DHT
    return ring::tls::CertificateStore::instance().getPinnedCertificates();
#else
    RING_WARN("TLS not supported");
#endif
    return {};
}

std::vector<std::string>
pinCertificate(const std::vector<uint8_t>& certificate, bool local)
{
    return ring::tls::CertificateStore::instance().pinCertificate(certificate, local);
}

void
pinCertificatePath(const std::string& path)
{
    ring::tls::CertificateStore::instance().pinCertificatePath(path);
}

bool
unpinCertificate(const std::string& certId)
{
    return ring::tls::CertificateStore::instance().unpinCertificate(certId);
}

unsigned
unpinCertificatePath(const std::string& path)
{
    return ring::tls::CertificateStore::instance().unpinCertificatePath(path);
}

bool
pinRemoteCertificate(const std::string& accountId, const std::string& certId)
{
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->findCertificate(certId);
    return false;
}

bool
setCertificateStatus(const std::string& accountId, const std::string& certId, const std::string& ststr)
{
    auto status = ring::tls::TrustStore::statusFromStr(ststr.c_str());
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->setCertificateStatus(certId, status);
    return false;
}

std::vector<std::string>
getCertificatesByStatus(const std::string& accountId, const std::string& ststr)
{
     auto status = ring::tls::TrustStore::statusFromStr(ststr.c_str());
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->getCertificatesByStatus(status);
    return {};
}

void
setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
    ring::Manager::instance().setAccountDetails(accountID, details);
}

void
sendRegister(const std::string& accountID, bool enable)
{
    ring::Manager::instance().sendRegister(accountID, enable);
}

void
registerAllAccounts()
{
    ring::Manager::instance().registerAccounts();
}

void
sendAccountTextMessage(const std::string& accountID, const std::string& to, const std::string& message)
{
    ring::Manager::instance().sendTextMessage(accountID, to, message);
}

/* contact requests */
std::map<std::string, std::string>
getTrustRequests(const std::string& accountId)
{
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->getTrustRequests();
    return {};
}

bool
acceptTrustRequest(const std::string& accountId, const std::string& from)
{
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->acceptTrustRequest(from);
    return false;
}

bool
discardTrustRequest(const std::string& accountId, const std::string& from)
{
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        return acc->discardTrustRequest(from);
    return false;
}

void
sendTrustRequest(const std::string& accountId, const std::string& to)
{
    if (auto acc = ring::Manager::instance().getAccount<ring::RingAccount>(accountId))
        acc->sendTrustRequest(to);
}

///This function is used as a base for new accounts for clients that support it
std::map<std::string, std::string>
getAccountTemplate(const std::string& accountType)
{
    if (accountType == Account::ProtocolNames::RING)
        return ring::RingAccount("dummy", false).getAccountDetails();
    else if (accountType == Account::ProtocolNames::SIP)
        return ring::SIPAccount("dummy", false).getAccountDetails();
#if HAVE_IAX
    else if (accountType == Account::ProtocolNames::IAX)
        return ring::IAXAccount("dummy").getAccountDetails();
#endif
    return {};
}

std::string
addAccount(const std::map<std::string, std::string>& details)
{
    return ring::Manager::instance().addAccount(details);
}

void
removeAccount(const std::string& accountID)
{
    return ring::Manager::instance().removeAccount(accountID);
}

std::vector<std::string>
getAccountList()
{
    return ring::Manager::instance().getAccountList();
}

/**
 * Send the list of all codecs loaded to the client through DBus.
 * Can stay global, as only the active codecs will be set per accounts
 */
std::vector<unsigned>
getCodecList()
{
    std::vector<unsigned> list {ring::getSystemCodecContainer()->getSystemCodecInfoIdList(ring::MEDIA_ALL)};
    if (list.empty())
        ring::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
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
#if HAVE_TLS
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID))
        return SIPAccount::getSupportedTlsCiphers();
    RING_ERR("SIP account %s doesn't exist", accountID.c_str());
#endif
    return {};
}


bool
setCodecDetails(const std::string& accountID,
                const unsigned& codecId,
                const  std::map<std::string, std::string>& details)
{
    auto acc = ring::Manager::instance().getAccount(accountID);
    if (!acc) {
        RING_ERR("Could not find account %s. can not set codec details"
                , accountID.c_str());
        return false;
    }

    auto codec = acc->searchCodecById(codecId, ring::MEDIA_ALL);
    if (!codec) {
        RING_ERR("can not find codec %d", codecId);
        return false;

    }
    if (codec->systemCodecInfo.mediaType & ring::MEDIA_AUDIO) {
        if (auto foundCodec = std::static_pointer_cast<ring::AccountAudioCodecInfo>(codec)) {
            foundCodec->setCodecSpecifications(details);
            ring::emitSignal<ConfigurationSignal::MediaParametersChanged>(accountID);
            return true;
        }
    }

    if (codec->systemCodecInfo.mediaType & ring::MEDIA_VIDEO) {
        if (auto foundCodec = std::static_pointer_cast<ring::AccountVideoCodecInfo>(codec)) {
            foundCodec->setCodecSpecifications(details);
            RING_WARN("parameters for %s changed ",
                    foundCodec->systemCodecInfo.name.c_str());
            if (foundCodec->isRunning) {
                RING_WARN("%s running. Need to restart encoding",
                        foundCodec->systemCodecInfo.name.c_str());
                auto call = ring::Manager::instance().getCurrentCall();
                call->restartMediaSender();
            }
            ring::emitSignal<ConfigurationSignal::MediaParametersChanged>(accountID);
            return true;
        }
    }
    return false;
}

std::map<std::string, std::string>
getCodecDetails(const std::string& accountID, const unsigned& codecId)
{
    auto acc = ring::Manager::instance().getAccount(accountID);
    if (!acc)
    {
        RING_ERR("Could not find account %s return default codec details"
                , accountID.c_str());
        return ring::Account::getDefaultCodecDetails(codecId);
    }

    auto codec = acc->searchCodecById(codecId, ring::MEDIA_ALL);
    if (!codec)
    {
        ring::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
        return {};
    }

    if (codec->systemCodecInfo.mediaType & ring::MEDIA_AUDIO)
        if (auto foundCodec = std::static_pointer_cast<ring::AccountAudioCodecInfo>(codec))
            return foundCodec->getCodecSpecifications();

    if (codec->systemCodecInfo.mediaType & ring::MEDIA_VIDEO)
        if (auto foundCodec = std::static_pointer_cast<ring::AccountVideoCodecInfo>(codec))
            return foundCodec->getCodecSpecifications();

    ring::emitSignal<ConfigurationSignal::Error>(CODECS_NOT_LOADED);
    return {};
}

std::vector<unsigned>
getActiveCodecList(const std::string& accountID)
{
    if (auto acc = ring::Manager::instance().getAccount(accountID))
        return acc->getActiveCodecs();
    RING_ERR("Could not find account %s, returning default", accountID.c_str());
    return ring::Account::getDefaultCodecsId();
}

void
setActiveCodecList(const std::string& accountID
        , const std::vector<unsigned>& list)
{
    if (auto acc = ring::Manager::instance().getAccount(accountID))
    {
        acc->setActiveCodecs(list);
        ring::Manager::instance().saveConfig();
    } else {
        RING_ERR("Could not find account %s", accountID.c_str());
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
    return ring::Manager::instance().setAudioPlugin(audioPlugin);
}

std::vector<std::string>
getAudioOutputDeviceList()
{
    return ring::Manager::instance().getAudioOutputDeviceList();
}

std::vector<std::string>
getAudioInputDeviceList()
{
    return ring::Manager::instance().getAudioInputDeviceList();
}

void
setAudioOutputDevice(int32_t index)
{
    return ring::Manager::instance().setAudioDevice(index, DeviceType::PLAYBACK);
}

void
setAudioInputDevice(int32_t index)
{
    return ring::Manager::instance().setAudioDevice(index, DeviceType::CAPTURE);
}

void
setAudioRingtoneDevice(int32_t index)
{
    return ring::Manager::instance().setAudioDevice(index, DeviceType::RINGTONE);
}

std::vector<std::string>
getCurrentAudioDevicesIndex()
{
    return ring::Manager::instance().getCurrentAudioDevicesIndex();
}

int32_t
getAudioInputDeviceIndex(const std::string& name)
{
    return ring::Manager::instance().getAudioInputDeviceIndex(name);
}

int32_t
getAudioOutputDeviceIndex(const std::string& name)
{
    return ring::Manager::instance().getAudioOutputDeviceIndex(name);
}

std::string
getCurrentAudioOutputPlugin()
{
    auto plugin = ring::Manager::instance().getCurrentAudioOutputPlugin();
    RING_DBG("Get audio plugin %s", plugin.c_str());
    return plugin;
}

bool
getNoiseSuppressState()
{
    return ring::Manager::instance().getNoiseSuppressState();
}

void
setNoiseSuppressState(bool state)
{
    ring::Manager::instance().setNoiseSuppressState(state);
}

bool
isAgcEnabled()
{
    return ring::Manager::instance().isAGCEnabled();
}

void
setAgcState(bool enabled)
{
    ring::Manager::instance().setAGCState(enabled);
}

int32_t
isIax2Enabled()
{
    return HAVE_IAX;
}

std::string
getRecordPath()
{
    return ring::Manager::instance().audioPreference.getRecordPath();
}

void
setRecordPath(const std::string& recPath)
{
    ring::Manager::instance().audioPreference.setRecordPath(recPath);
}

bool
getIsAlwaysRecording()
{
    return ring::Manager::instance().getIsAlwaysRecording();
}

void
setIsAlwaysRecording(bool rec)
{
    ring::Manager::instance().setIsAlwaysRecording(rec);
}

int32_t
getHistoryLimit()
{
    return ring::Manager::instance().getHistoryLimit();
}

void
setHistoryLimit(int32_t days)
{
    ring::Manager::instance().setHistoryLimit(days);
}

bool
setAudioManager(const std::string& api)
{
    return ring::Manager::instance().setAudioManager(api);
}

std::string
getAudioManager()
{
    return ring::Manager::instance().getAudioManager();
}

void
setVolume(const std::string& device, double value)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver()) {
        RING_DBG("set volume for %s: %f", device.c_str(), value);

        if (device == "speaker")
            audiolayer->setPlaybackGain(value);
        else if (device == "mic")
            audiolayer->setCaptureGain(value);

        ring::emitSignal<ConfigurationSignal::VolumeChanged>(device, value);
    } else {
        RING_ERR("Audio layer not valid while updating volume");
    }
}

double
getVolume(const std::string& device)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver()) {
        if (device == "speaker")
            return audiolayer->getPlaybackGain();
        if (device == "mic")
            return audiolayer->getCaptureGain();
    }

    RING_ERR("Audio layer not valid while updating volume");
    return 0.0;
}

// FIXME: we should store "muteDtmf" instead of "playDtmf"
// in config and avoid negating like this
bool
isDtmfMuted()
{
    return not ring::Manager::instance().voipPreferences.getPlayDtmf();
}

void
muteDtmf(bool mute)
{
    ring::Manager::instance().voipPreferences.setPlayDtmf(not mute);
}

bool
isCaptureMuted()
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->isCaptureMuted();

    RING_ERR("Audio layer not valid");
    return false;
}

void
muteCapture(bool mute)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->muteCapture(mute);

    RING_ERR("Audio layer not valid");
    return;
}

bool
isPlaybackMuted()
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->isPlaybackMuted();

    RING_ERR("Audio layer not valid");
    return false;
}

void
mutePlayback(bool mute)
{
    if (auto audiolayer = ring::Manager::instance().getAudioDriver())
        return audiolayer->mutePlayback(mute);

    RING_ERR("Audio layer not valid");
    return;
}

std::map<std::string, std::string>
getHookSettings()
{
    return ring::Manager::instance().hookPreference.toMap();
}

void
setHookSettings(const std::map<std::string,
                std::string>& settings)
{
    ring::Manager::instance().hookPreference = HookPreference(settings);
}

void setAccountsOrder(const std::string& order)
{
    ring::Manager::instance().setAccountsOrder(order);
}

std::string
getAddrFromInterfaceName(const std::string& interface)
{
    return ring::ip_utils::getInterfaceAddr(interface);
}

std::vector<std::string>
getAllIpInterface()
{
    return ring::ip_utils::getAllIpInterface();
}

std::vector<std::string>
getAllIpInterfaceByName()
{
    return ring::ip_utils::getAllIpInterfaceByName();
}

std::map<std::string, std::string>
getShortcuts()
{
    return ring::Manager::instance().shortcutPreferences.getShortcuts();
}

void
setShortcuts(const std::map<std::string, std::string>& shortcutsMap)
{
    ring::Manager::instance().shortcutPreferences.setShortcuts(shortcutsMap);
    ring::Manager::instance().saveConfig();
}

std::vector<std::map<std::string, std::string>>
getCredentials(const std::string& accountID)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID))
        return sipaccount->getCredentials();
    return {};
}

void
setCredentials(const std::string& accountID,
               const std::vector<std::map<std::string, std::string>>& details)
{
    if (auto sipaccount = ring::Manager::instance().getAccount<SIPAccount>(accountID)) {
        sipaccount->doUnregister([&](bool /* transport_free */) {
            sipaccount->setCredentials(details);
            if (sipaccount->isEnabled())
                sipaccount->doRegister();
        });
    }
}

} // namespace DRing
