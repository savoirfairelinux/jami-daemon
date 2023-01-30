/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "account.h"

#include <algorithm>
#include <iterator>
#include <mutex>

#ifdef ENABLE_VIDEO
#include "libav_utils.h"
#endif

#include "logger.h"
#include "manager.h"

#include <opendht/rng.h>
using random_device = dht::crypto::random_device;

#include "client/ring_signal.h"
#include "account_schema.h"
#include "jami/account_const.h"
#include "string_utils.h"
#include "fileutils.h"
#include "config/yamlparser.h"
#include "system_codec_container.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include "connectivity/upnp/upnp_control.h"
#include "connectivity/ip_utils.h"
#include "compiler_intrinsics.h"
#include "jami/account_const.h"

#include <fmt/ranges.h>

using namespace std::literals;

namespace jami {

// For portability, do not specify the absolute file name of the ringtone. 
// Instead, specify its base name to be looked in
// JAMI_DATADIR/ringtones/, where JAMI_DATADIR is a preprocessor macro denoting
// the data directory prefix that must be set at build time.
const std::string Account::DEFAULT_USER_AGENT = Account::getDefaultUserAgent();

Account::Account(const std::string& accountID)
    : rand(dht::crypto::getSeededRandomEngine<std::mt19937_64>())
    , accountID_(accountID)
    , registrationState_(RegistrationState::UNREGISTERED)
    , systemCodecContainer_(getSystemCodecContainer())
{
    // Initialize the codec order, used when creating a new account
    loadDefaultCodecs();
}

Account::~Account() {}

void
Account::hangupCalls()
{
    for (const auto& callId : callSet_.getCallIds())
        Manager::instance().hangupCall(getAccountID(), callId);
}

void
Account::updateUpnpController()
{
    std::lock_guard<std::mutex> lk {upnp_mtx};

    if (not config().upnpEnabled or not isUsable()) {
        upnpCtrl_.reset();
        return;
    }

    // UPNP enabled. Create new controller if needed.
    if (not upnpCtrl_) {
        upnpCtrl_.reset(new upnp::Controller());
        if (not upnpCtrl_) {
            throw std::runtime_error("Failed to create a UPNP Controller instance!");
        }
    }
}

void
Account::setRegistrationState(RegistrationState state,
                              int detail_code,
                              const std::string& detail_str)
{
    if (state != registrationState_) {
        registrationState_ = state;
        // Notify the client
        runOnMainThread([accountId = accountID_,
                         state = mapStateNumberToString(registrationState_),
                         detail_code,
                         detail_str,
                         details = getVolatileAccountDetails()] {
            emitSignal<libjami::ConfigurationSignal::RegistrationStateChanged>(accountId,
                                                                             state,
                                                                             detail_code,
                                                                             detail_str);

            emitSignal<libjami::ConfigurationSignal::VolatileDetailsChanged>(accountId, details);
        });
    }
}

void
Account::loadDefaultCodecs()
{
    // default codec are system codecs
    const auto& systemCodecList = systemCodecContainer_->getSystemCodecInfoList();
    accountCodecInfoList_.clear();
    accountCodecInfoList_.reserve(systemCodecList.size());
    for (const auto& systemCodec : systemCodecList) {
        // As defined in SDP RFC, only select a codec if it can encode and decode
        if ((systemCodec->codecType & CODEC_ENCODER_DECODER) != CODEC_ENCODER_DECODER)
            continue;

        if (systemCodec->mediaType & MEDIA_AUDIO) {
            accountCodecInfoList_.emplace_back(std::make_shared<AccountAudioCodecInfo>(
                *std::static_pointer_cast<SystemAudioCodecInfo>(systemCodec)));
        }

        if (systemCodec->mediaType & MEDIA_VIDEO) {
            accountCodecInfoList_.emplace_back(std::make_shared<AccountVideoCodecInfo>(
                *std::static_pointer_cast<SystemVideoCodecInfo>(systemCodec)));
        }
    }
}

void
Account::loadConfig() {
    setActiveCodecs(config_->activeCodecs);
    auto ringtoneDir = fmt::format("{}/{}", JAMI_DATADIR, RINGDIR);
    ringtonePath_ = fileutils::getFullPath(ringtoneDir, config_->ringtonePath);
    // If the user defined a custom ringtone, the file may not exists
    // In this case, fallback on the default ringtone path
    if (!fileutils::isFile(ringtonePath_)) {
        JAMI_WARNING("Ringtone {} is not a valid file", ringtonePath_);
        config_->ringtonePath = DEFAULT_RINGTONE_PATH;
        ringtonePath_ = fileutils::getFullPath(ringtoneDir, config_->ringtonePath);
    }
    updateUpnpController();
}

void
Account::saveConfig() const
{
    Manager::instance().saveConfig();
}

std::map<std::string, std::string>
Account::getVolatileAccountDetails() const
{
    return {{Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS, mapStateNumberToString(registrationState_)},
            {libjami::Account::VolatileProperties::ACTIVE, active_ ? TRUE_STR : FALSE_STR}};
}

bool
Account::hasActiveCodec(MediaType mediaType) const
{
    for (auto& codecIt : accountCodecInfoList_)
        if ((codecIt->systemCodecInfo.mediaType & mediaType) && codecIt->isActive)
            return true;
    return false;
}

void
Account::setActiveCodecs(const std::vector<unsigned>& list)
{
    // first clear the previously stored codecs
    // TODO: mutex to protect isActive
    setAllCodecsActive(MEDIA_ALL, false);

    // list contains the ordered payload of active codecs picked by the user for this account
    // we used the codec vector to save the order.
    uint16_t order = 1;
    for (const auto& item : list) {
        if (auto accCodec = searchCodecById(item, MEDIA_ALL)) {
            accCodec->isActive = true;
            accCodec->order = order;
            ++order;
        }
    }
    sortCodec();
}

void
Account::sortCodec()
{
    std::sort(std::begin(accountCodecInfoList_),
              std::end(accountCodecInfoList_),
              [](const std::shared_ptr<AccountCodecInfo>& a,
                 const std::shared_ptr<AccountCodecInfo>& b) { return a->order < b->order; });
}

std::string
Account::mapStateNumberToString(RegistrationState state)
{
#define CASE_STATE(X) \
    case RegistrationState::X: \
        return #X

    switch (state) {
        CASE_STATE(UNREGISTERED);
        CASE_STATE(TRYING);
        CASE_STATE(REGISTERED);
        CASE_STATE(ERROR_GENERIC);
        CASE_STATE(ERROR_AUTH);
        CASE_STATE(ERROR_NETWORK);
        CASE_STATE(ERROR_HOST);
        CASE_STATE(ERROR_SERVICE_UNAVAILABLE);
        CASE_STATE(ERROR_NEED_MIGRATION);
        CASE_STATE(INITIALIZING);
    default:
        return libjami::Account::States::ERROR_GENERIC;
    }

#undef CASE_STATE
}

std::vector<unsigned>
Account::getDefaultCodecsId()
{
    return getSystemCodecContainer()->getSystemCodecInfoIdList(MEDIA_ALL);
}

std::map<std::string, std::string>
Account::getDefaultCodecDetails(const unsigned& codecId)
{
    auto codec = jami::getSystemCodecContainer()->searchCodecById(codecId, jami::MEDIA_ALL);
    if (codec) {
        if (codec->mediaType & jami::MEDIA_AUDIO) {
            auto audioCodec = std::static_pointer_cast<jami::SystemAudioCodecInfo>(codec);
            return audioCodec->getCodecSpecifications();
        }
        if (codec->mediaType & jami::MEDIA_VIDEO) {
            auto videoCodec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(codec);
            return videoCodec->getCodecSpecifications();
        }
    }
    return {};
}

/**
 * Get the UPnP IP (external router) address.
 * If use UPnP is set to false, the address will be empty.
 */
IpAddr
Account::getUPnPIpAddress() const
{
    std::lock_guard<std::mutex> lk(upnp_mtx);
    if (upnpCtrl_)
        return upnpCtrl_->getExternalIP();
    return {};
}

/**
 * returns whether or not UPnP is enabled and active_
 * ie: if it is able to make port mappings
 */
bool
Account::getUPnPActive() const
{
    std::lock_guard<std::mutex> lk(upnp_mtx);
    if (upnpCtrl_)
        return upnpCtrl_->isReady();
    return false;
}

/*
 * private account codec searching functions
 *
 * */
std::shared_ptr<AccountCodecInfo>
Account::searchCodecById(unsigned codecId, MediaType mediaType)
{
    if (mediaType != MEDIA_NONE) {
        for (auto& codecIt : accountCodecInfoList_) {
            if ((codecIt->systemCodecInfo.id == codecId)
                && (codecIt->systemCodecInfo.mediaType & mediaType))
                return codecIt;
        }
    }
    return {};
}

std::shared_ptr<AccountCodecInfo>
Account::searchCodecByName(const std::string& name, MediaType mediaType)
{
    if (mediaType != MEDIA_NONE) {
        for (auto& codecIt : accountCodecInfoList_) {
            if (codecIt->systemCodecInfo.name == name
                && (codecIt->systemCodecInfo.mediaType & mediaType))
                return codecIt;
        }
    }
    return {};
}

std::shared_ptr<AccountCodecInfo>
Account::searchCodecByPayload(unsigned payload, MediaType mediaType)
{
    if (mediaType != MEDIA_NONE) {
        for (auto& codecIt : accountCodecInfoList_) {
            if ((codecIt->payloadType == payload)
                && (codecIt->systemCodecInfo.mediaType & mediaType))
                return codecIt;
        }
    }
    return {};
}

std::vector<unsigned>
Account::getActiveCodecs(MediaType mediaType) const
{
    if (mediaType == MEDIA_NONE)
        return {};

    std::vector<unsigned> idList;
    for (auto& codecIt : accountCodecInfoList_) {
        if ((codecIt->systemCodecInfo.mediaType & mediaType) && (codecIt->isActive))
            idList.push_back(codecIt->systemCodecInfo.id);
    }
    return idList;
}

std::vector<unsigned>
Account::getAccountCodecInfoIdList(MediaType mediaType) const
{
    if (mediaType == MEDIA_NONE)
        return {};

    std::vector<unsigned> idList;
    for (auto& codecIt : accountCodecInfoList_) {
        if (codecIt->systemCodecInfo.mediaType & mediaType)
            idList.push_back(codecIt->systemCodecInfo.id);
    }

    return idList;
}

void
Account::setAllCodecsActive(MediaType mediaType, bool active)
{
    if (mediaType == MEDIA_NONE)
        return;
    for (auto& codecIt : accountCodecInfoList_) {
        if (codecIt->systemCodecInfo.mediaType & mediaType)
            codecIt->isActive = active;
    }
}

void
Account::setCodecActive(unsigned codecId)
{
    for (auto& codecIt : accountCodecInfoList_) {
        if (codecIt->systemCodecInfo.avcodecId == codecId)
            codecIt->isActive = true;
    }
}

void
Account::setCodecInactive(unsigned codecId)
{
    for (auto& codecIt : accountCodecInfoList_) {
        if (codecIt->systemCodecInfo.avcodecId == codecId)
            codecIt->isActive = false;
    }
}

std::vector<std::shared_ptr<AccountCodecInfo>>
Account::getActiveAccountCodecInfoList(MediaType mediaType) const
{
    if (mediaType == MEDIA_NONE)
        return {};

    std::vector<std::shared_ptr<AccountCodecInfo>> accountCodecList;
    for (auto& codecIt : accountCodecInfoList_) {
        if ((codecIt->systemCodecInfo.mediaType & mediaType) && (codecIt->isActive))
            accountCodecList.push_back(codecIt);
    }

    return accountCodecList;
}

const std::string&
Account::getUserAgentName()
{
    return config_->customUserAgent.empty() ? DEFAULT_USER_AGENT : config_->customUserAgent;
}

std::string
Account::getDefaultUserAgent()
{
    return fmt::format("{:s} {:s} ({:s})", PACKAGE_NAME, libjami::version(), libjami::platform());
}

void
Account::addDefaultModerator(const std::string& uri)
{
    config_->defaultModerators.insert(uri);
}

void
Account::removeDefaultModerator(const std::string& uri)
{
    config_->defaultModerators.erase(uri);
}

bool
Account::meetMinimumRequiredVersion(const std::vector<unsigned>& version,
                                    const std::vector<unsigned>& minRequiredVersion)
{
    for (size_t i = 0; i < minRequiredVersion.size(); i++) {
        if (i == version.size() or version[i] < minRequiredVersion[i])
            return false;
        if (version[i] > minRequiredVersion[i])
            return true;
    }
    return true;
}
} // namespace jami
