/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "dring/account_const.h"
#include "string_utils.h"
#include "fileutils.h"
#include "config/yamlparser.h"
#include "system_codec_container.h"
#include "ice_transport.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

#include "upnp/upnp_control.h"
#include "ip_utils.h"
#include "compiler_intrinsics.h"
#include "dring/account_const.h"

#include <fmt/ranges.h>

using namespace std::literals;

namespace jami {

const char* const Account::ALL_CODECS_KEY = "allCodecs";
const char* const Account::VIDEO_CODEC_ENABLED = "enabled";
const char* const Account::VIDEO_CODEC_NAME = "name";
const char* const Account::VIDEO_CODEC_PARAMETERS = "parameters";
const char* const Account::VIDEO_CODEC_BITRATE = "bitrate";
const char* const Account::RINGTONE_PATH_KEY = "ringtonePath";
const char* const Account::RINGTONE_ENABLED_KEY = "ringtoneEnabled";
const char* const Account::VIDEO_ENABLED_KEY = "videoEnabled";
const char* const Account::DISPLAY_NAME_KEY = "displayName";
const char* const Account::ALIAS_KEY = "alias";
const char* const Account::TYPE_KEY = "type";
const char* const Account::ID_KEY = "id";
const char* const Account::USERNAME_KEY = "username";
const char* const Account::AUTHENTICATION_USERNAME_KEY = "authenticationUsername";
const char* const Account::PASSWORD_KEY = "password";
const char* const Account::HOSTNAME_KEY = "hostname";
const char* const Account::ACCOUNT_ENABLE_KEY = "enable";
const char* const Account::ACCOUNT_AUTOANSWER_KEY = "autoAnswer";
const char* const Account::ACCOUNT_ISRENDEZVOUS_KEY = "rendezVous";
const char* const Account::ACCOUNT_ACTIVE_CALL_LIMIT_KEY = "activeCallLimit";
const char* const Account::MAILBOX_KEY = "mailbox";
const char* const Account::USER_AGENT_KEY = "useragent";
const char* const Account::HAS_CUSTOM_USER_AGENT_KEY = "hasCustomUserAgent";
const char* const Account::PRESENCE_MODULE_ENABLED_KEY = "presenceModuleEnabled";
const char* const Account::UPNP_ENABLED_KEY = "upnpEnabled";
const char* const Account::ACTIVE_CODEC_KEY = "activeCodecs";
const std::string Account::DEFAULT_USER_AGENT = Account::setDefaultUserAgent();
const char* const Account::DEFAULT_MODERATORS_KEY = "defaultModerators";
const char* const Account::LOCAL_MODERATORS_ENABLED_KEY = "localModeratorsEnabled";
const char* const Account::ALL_MODERATORS_ENABLED_KEY = "allModeratorsEnabled";

#ifdef __ANDROID__
constexpr const char* const DEFAULT_RINGTONE_PATH
    = "/data/data/cx.ring/files/ringtones/default.opus";
#else
constexpr const char* const DEFAULT_RINGTONE_PATH = "/usr/share/ring/ringtones/default.opus";
#endif

Account::Account(const std::string& accountID)
    : rand(dht::crypto::getSeededRandomEngine<std::mt19937_64>())
    , accountID_(accountID)
    , username_()
    , hostname_()
    , alias_()
    , enabled_(true)
    , autoAnswerEnabled_(false)
    , isRendezVous_(false)
    , registrationState_(RegistrationState::UNREGISTERED)
    , systemCodecContainer_(getSystemCodecContainer())
    , accountCodecInfoList_()
    , ringtonePath_(DEFAULT_RINGTONE_PATH)
    , ringtoneEnabled_(true)
    , displayName_("")
    , customUserAgent_("")
    , hasCustomUserAgent_(false)
    , mailBox_()
    , upnpEnabled_(true)
    , localModeratorsEnabled_(true)
    , allModeratorsEnabled_(true)
    , multiStreamEnabled_(false)
{
    // Initialize the codec order, used when creating a new account
    loadDefaultCodecs();
}

Account::~Account() {}

void
Account::attachCall(const std::string& id)
{
    std::lock_guard<std::mutex> lk {callIDSetMtx_};
    callIDSet_.insert(id);
}

void
Account::detachCall(const std::string& id)
{
    std::lock_guard<std::mutex> lk {callIDSetMtx_};
    callIDSet_.erase(id);
}

void
Account::hangupCalls()
{
    decltype(callIDSet_) calls;
    {
        std::lock_guard<std::mutex> lk {callIDSetMtx_};
        calls = callIDSet_;
    }
    for (const auto& id : calls)
        Manager::instance().hangupCall(id);
}

void
Account::updateUpnpController()
{
    std::lock_guard<std::mutex> lk {upnp_mtx};

    if (upnpEnabled_ and not upnpCtrl_) {
        upnpCtrl_.reset(new upnp::Controller());
        if (not upnpCtrl_) {
            throw std::runtime_error("Failed to create a UPNP Controller instance!");
        }
    } else if (not upnpEnabled_) {
        upnpCtrl_.reset();
    }
}

void
Account::setRegistrationState(RegistrationState state,
                              unsigned detail_code,
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
            emitSignal<DRing::ConfigurationSignal::RegistrationStateChanged>(accountId,
                                                                             state,
                                                                             detail_code,
                                                                             detail_str);

            emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(accountId, details);
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
Account::serialize(YAML::Emitter& out) const
{
    const auto& activeCodecs = fmt::format("{}", fmt::join(getActiveCodecs(MEDIA_ALL), "/"sv));

    out << YAML::Key << ID_KEY << YAML::Value << accountID_;
    out << YAML::Key << ALIAS_KEY << YAML::Value << alias_;
    out << YAML::Key << ACCOUNT_ENABLE_KEY << YAML::Value << enabled_;
    out << YAML::Key << TYPE_KEY << YAML::Value << getAccountType();
    out << YAML::Key << ACTIVE_CODEC_KEY << YAML::Value << activeCodecs;
    out << YAML::Key << MAILBOX_KEY << YAML::Value << mailBox_;
    out << YAML::Key << ACCOUNT_AUTOANSWER_KEY << YAML::Value << autoAnswerEnabled_;
    out << YAML::Key << ACCOUNT_ISRENDEZVOUS_KEY << YAML::Value << isRendezVous_;
    out << YAML::Key << ACCOUNT_ACTIVE_CALL_LIMIT_KEY << YAML::Value << activeCallLimit_;
    out << YAML::Key << RINGTONE_ENABLED_KEY << YAML::Value << ringtoneEnabled_;
    out << YAML::Key << RINGTONE_PATH_KEY << YAML::Value << ringtonePath_;
    out << YAML::Key << HAS_CUSTOM_USER_AGENT_KEY << YAML::Value << hasCustomUserAgent_;
    out << YAML::Key << USER_AGENT_KEY << YAML::Value << customUserAgent_;
    out << YAML::Key << DISPLAY_NAME_KEY << YAML::Value << displayName_;
    out << YAML::Key << HOSTNAME_KEY << YAML::Value << hostname_;
    out << YAML::Key << UPNP_ENABLED_KEY << YAML::Value << upnpEnabled_;
    out << YAML::Key << DEFAULT_MODERATORS_KEY << YAML::Value << string_join(defaultModerators_);
    out << YAML::Key << LOCAL_MODERATORS_ENABLED_KEY << YAML::Value << localModeratorsEnabled_;
    out << YAML::Key << ALL_MODERATORS_ENABLED_KEY << YAML::Value << allModeratorsEnabled_;
}

void
Account::unserialize(const YAML::Node& node)
{
    using yaml_utils::parseValue;
    using yaml_utils::parseValueOptional;

    parseValue(node, ALIAS_KEY, alias_);
    parseValue(node, ACCOUNT_ENABLE_KEY, enabled_);
    parseValue(node, ACCOUNT_AUTOANSWER_KEY, autoAnswerEnabled_);
    parseValueOptional(node, ACCOUNT_ISRENDEZVOUS_KEY, isRendezVous_);
    parseValue(node, ACCOUNT_ACTIVE_CALL_LIMIT_KEY, activeCallLimit_);
    // parseValue(node, PASSWORD_KEY, password_);

    parseValue(node, MAILBOX_KEY, mailBox_);

    std::string activeCodecs;
    if (parseValueOptional(node, ACTIVE_CODEC_KEY, activeCodecs))
        setActiveCodecs(split_string_to_unsigned(activeCodecs, '/'));
    else {
        std::string allCodecs;
        if (parseValueOptional(node, ALL_CODECS_KEY, allCodecs)) {
            JAMI_WARN("Converting deprecated codec list");
            auto list = convertIdToAVId(split_string_to_unsigned(allCodecs, '/'));
            auto codec = searchCodecByName("H265", MEDIA_ALL);
            // set H265 as first active codec if found
            if (codec)
                list.emplace(list.begin(), codec->systemCodecInfo.id);
            setActiveCodecs(list);
            runOnMainThread([id = getAccountID()] {
                if (auto sthis = Manager::instance().getAccount(id))
                    Manager::instance().saveConfig(sthis);
            });
        }
    }

    parseValue(node, DISPLAY_NAME_KEY, displayName_);
    parseValue(node, HOSTNAME_KEY, hostname_);

    parseValue(node, HAS_CUSTOM_USER_AGENT_KEY, hasCustomUserAgent_);
    parseValue(node, USER_AGENT_KEY, customUserAgent_);
    parseValue(node, RINGTONE_PATH_KEY, ringtonePath_);
    parseValue(node, RINGTONE_ENABLED_KEY, ringtoneEnabled_);
    if (ringtonePath_.empty()) {
        ringtonePath_ = DEFAULT_RINGTONE_PATH;
    }

    parseValue(node, UPNP_ENABLED_KEY, upnpEnabled_);
    updateUpnpController();

    std::string defMod;
    parseValueOptional(node, DEFAULT_MODERATORS_KEY, defMod);
    defaultModerators_ = string_split_set(defMod);
    parseValueOptional(node, LOCAL_MODERATORS_ENABLED_KEY, localModeratorsEnabled_);
    parseValueOptional(node, ALL_MODERATORS_ENABLED_KEY, allModeratorsEnabled_);
}

void
Account::setAccountDetails(const std::map<std::string, std::string>& details)
{
    // Account setting common to any account type
    parseString(details, Conf::CONFIG_ACCOUNT_ALIAS, alias_);
    parseString(details, Conf::CONFIG_ACCOUNT_DISPLAYNAME, displayName_);
    parseBool(details, Conf::CONFIG_ACCOUNT_ENABLE, enabled_);
    parseString(details, Conf::CONFIG_ACCOUNT_HOSTNAME, hostname_);
    parseString(details, Conf::CONFIG_ACCOUNT_MAILBOX, mailBox_);
    parseBool(details, Conf::CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled_);
    parseBool(details, Conf::CONFIG_ACCOUNT_ISRENDEZVOUS, isRendezVous_);
    parseInt(details, DRing::Account::ConfProperties::ACTIVE_CALL_LIMIT, activeCallLimit_);
    parseBool(details, Conf::CONFIG_RINGTONE_ENABLED, ringtoneEnabled_);
    parseString(details, Conf::CONFIG_RINGTONE_PATH, ringtonePath_);
    if (ringtonePath_.empty()) {
        ringtonePath_ = DEFAULT_RINGTONE_PATH;
    }
    parseBool(details, Conf::CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT, hasCustomUserAgent_);
    if (hasCustomUserAgent_)
        parseString(details, Conf::CONFIG_ACCOUNT_USERAGENT, customUserAgent_);

    parseBool(details, Conf::CONFIG_UPNP_ENABLED, upnpEnabled_);
    updateUpnpController();

    std::string defMod;
    parseString(details, Conf::CONFIG_DEFAULT_MODERATORS, defMod);
    defaultModerators_ = string_split_set(defMod);
    parseBool(details, Conf::CONFIG_LOCAL_MODERATORS_ENABLED, localModeratorsEnabled_);
    parseBool(details, Conf::CONFIG_ALL_MODERATORS_ENABLED, allModeratorsEnabled_);
}

std::map<std::string, std::string>
Account::getAccountDetails() const
{
    return {{Conf::CONFIG_ACCOUNT_ALIAS, alias_},
            {Conf::CONFIG_ACCOUNT_DISPLAYNAME, displayName_},
            {Conf::CONFIG_ACCOUNT_ENABLE, enabled_ ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ACCOUNT_TYPE, getAccountType()},
            {Conf::CONFIG_ACCOUNT_HOSTNAME, hostname_},
            {Conf::CONFIG_ACCOUNT_USERNAME, username_},
            {Conf::CONFIG_ACCOUNT_MAILBOX, mailBox_},
            {Conf::CONFIG_ACCOUNT_USERAGENT, customUserAgent_},
            {Conf::CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT, hasCustomUserAgent_ ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled_ ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ACCOUNT_ISRENDEZVOUS, isRendezVous_ ? TRUE_STR : FALSE_STR},
            {DRing::Account::ConfProperties::ACTIVE_CALL_LIMIT, std::to_string(activeCallLimit_)},
            {Conf::CONFIG_RINGTONE_ENABLED, ringtoneEnabled_ ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_RINGTONE_PATH, ringtonePath_},
            {Conf::CONFIG_UPNP_ENABLED, upnpEnabled_ ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_DEFAULT_MODERATORS, string_join(defaultModerators_)},
            {Conf::CONFIG_LOCAL_MODERATORS_ENABLED, localModeratorsEnabled_ ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ALL_MODERATORS_ENABLED, allModeratorsEnabled_ ? TRUE_STR : FALSE_STR}};
}

std::map<std::string, std::string>
Account::getVolatileAccountDetails() const
{
    return {{Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS, mapStateNumberToString(registrationState_)},
            {DRing::Account::VolatileProperties::ACTIVE, active_ ? TRUE_STR : FALSE_STR}};
}

void
Account::onIsComposing(const std::string& conversationId, const std::string& peer, bool isComposing)
{
    emitSignal<DRing::ConfigurationSignal::ComposingStatusChanged>(accountID_,
                                                                   conversationId,
                                                                   peer,
                                                                   isComposing ? 1 : 0);
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

std::vector<unsigned>
Account::convertIdToAVId(const std::vector<unsigned>& list)
{
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    constexpr size_t CODEC_NUM = 12;
#else
    constexpr size_t CODEC_NUM = 10;
#endif

    static constexpr std::array<unsigned, CODEC_NUM> CODEC_ID_MAPPING
        = { AV_CODEC_ID_NONE,
            AV_CODEC_ID_H264,
            AV_CODEC_ID_VP8,
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
            AV_CODEC_ID_MPEG4,
            AV_CODEC_ID_H263,
#endif
            AV_CODEC_ID_OPUS,
            AV_CODEC_ID_ADPCM_G722,
            AV_CODEC_ID_SPEEX | 0x20000000,
            AV_CODEC_ID_SPEEX | 0x10000000,
            AV_CODEC_ID_SPEEX,
            AV_CODEC_ID_PCM_ALAW,
            AV_CODEC_ID_PCM_MULAW };

    std::vector<unsigned> av_list;
    av_list.reserve(list.size());
    for (auto& item : list) {
        if (item > 0 and item < CODEC_ID_MAPPING.size())
            av_list.emplace_back(CODEC_ID_MAPPING[item]);
    }
    return av_list;
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
        CASE_STATE(ERROR_EXIST_STUN);
        CASE_STATE(ERROR_NOT_ACCEPTABLE);
        CASE_STATE(ERROR_NEED_MIGRATION);
        CASE_STATE(INITIALIZING);
    default:
        return DRing::Account::States::ERROR_GENERIC;
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

#define find_iter() \
    const auto& iter = details.find(key); \
    if (iter == details.end()) { \
        JAMI_ERR("Couldn't find key \"%s\"", key); \
        return; \
    }

void
Account::parseString(const std::map<std::string, std::string>& details,
                     const char* key,
                     std::string& s)
{
    find_iter();
    s = iter->second;
}

void
Account::parsePath(const std::map<std::string, std::string>& details,
                   const char* key,
                   std::string& s,
                   const std::string& base)
{
    find_iter();
    s = fileutils::getCleanPath(base, iter->second);
}

void
Account::parseBool(const std::map<std::string, std::string>& details, const char* key, bool& b)
{
    find_iter();
    b = iter->second == TRUE_STR;
}

#undef find_iter

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
    if (hasCustomUserAgent_ and not customUserAgent_.empty())
        return customUserAgent_;
    return DEFAULT_USER_AGENT;
}

std::string
Account::setDefaultUserAgent()
{
    // Build the default user-agent string
    std::string defaultUA;
    defaultUA.append(PACKAGE_NAME);
    defaultUA.append(" ");
    defaultUA.append(DRing::version());
    defaultUA.append(" (");
    defaultUA.append(DRing::platform());
    defaultUA.append(")");

    return defaultUA;
}

void
Account::addDefaultModerator(const std::string& uri)
{
    defaultModerators_.insert(uri);
}

void
Account::removeDefaultModerator(const std::string& uri)
{
    defaultModerators_.erase(uri);
}

} // namespace jami
