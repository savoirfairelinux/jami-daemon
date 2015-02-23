/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#include "account.h"

#include <algorithm>
#include <iterator>
#include <mutex>

#ifdef RING_VIDEO
#include "libav_utils.h"
#endif

#include "logger.h"
#include "manager.h"

#include "client/signal.h"
#include "account_schema.h"
#include "string_utils.h"
#include "config/yamlparser.h"
#include "system_codec_container.h"

#include <yaml-cpp/yaml.h>

#include "upnp/upnp_control.h"
#include "ip_utils.h"
#include "intrin.h"
#include "dring/account_const.h"

namespace ring {

const char * const Account::ALL_CODECS_KEY              = "allCodecs";
const char * const Account::VIDEO_CODEC_ENABLED         = "enabled";
const char * const Account::VIDEO_CODEC_NAME            = "name";
const char * const Account::VIDEO_CODEC_PARAMETERS      = "parameters";
const char * const Account::VIDEO_CODEC_BITRATE         = "bitrate";
const char * const Account::RINGTONE_PATH_KEY           = "ringtonePath";
const char * const Account::RINGTONE_ENABLED_KEY        = "ringtoneEnabled";
const char * const Account::VIDEO_ENABLED_KEY           = "videoEnabled";
const char * const Account::DISPLAY_NAME_KEY            = "displayName";
const char * const Account::ALIAS_KEY                   = "alias";
const char * const Account::TYPE_KEY                    = "type";
const char * const Account::ID_KEY                      = "id";
const char * const Account::USERNAME_KEY                = "username";
const char * const Account::AUTHENTICATION_USERNAME_KEY = "authenticationUsername";
const char * const Account::PASSWORD_KEY                = "password";
const char * const Account::HOSTNAME_KEY                = "hostname";
const char * const Account::ACCOUNT_ENABLE_KEY          = "enable";
const char * const Account::ACCOUNT_AUTOANSWER_KEY      = "autoAnswer";
const char * const Account::MAILBOX_KEY                 = "mailbox";
const char * const Account::DEFAULT_USER_AGENT          = PACKAGE_NAME "/" PACKAGE_VERSION;
const char * const Account::USER_AGENT_KEY              = "useragent";
const char * const Account::HAS_CUSTOM_USER_AGENT_KEY   = "hasCustomUserAgent";
const char * const Account::PRESENCE_MODULE_ENABLED_KEY = "presenceModuleEnabled";
const char * const Account::UPNP_ENABLED_KEY            = "upnpEnabled";

Account::Account(const std::string &accountID)
    : accountID_(accountID)
    , username_()
    , hostname_()
    , alias_()
    , enabled_(true)
    , autoAnswerEnabled_(false)
    , registrationState_(RegistrationState::UNREGISTERED)
    , systemCodecContainer_(getSystemCodecContainer())
    , accountCodecInfoList_()
    , allCodecStr_()
    , ringtonePath_("")
    , ringtoneEnabled_(true)
    , displayName_("")
    , userAgent_(DEFAULT_USER_AGENT)
    , hasCustomUserAgent_(false)
    , mailBox_()
    , upnp_(new upnp::Controller())
{
    std::random_device rdev;
    std::seed_seq seed {rdev(), rdev()};
    rand_.seed(seed);

    // Initialize the codec order, used when creating a new account
    loadDefaultCodecs();
    #ifdef __ANDROID__
        ringtonePath_ = "/data/data/cx.ring/files/ringtones/konga.ul";
    #else
        ringtonePath_ = "/usr/share/ring/ringtones/konga.ul";
    #endif
}

Account::~Account()
{}

void
Account::attachCall(const std::string& id)
{
    callIDSet_.insert(id);
}

void
Account::detachCall(const std::string& id)
{
    callIDSet_.erase(id);
}

void
Account::freeAccount()
{
    for (const auto& id : callIDSet_)
        Manager::instance().hangupCall(id);
    doUnregister();
}

void
Account::setRegistrationState(RegistrationState state)
{
    if (state != registrationState_) {
        registrationState_ = state;
        // Notify the client
        emitSignal<DRing::ConfigurationSignal::RegistrationStateChanged>(accountID_,
                static_cast<int32_t>(registrationState_));

        emitSignal<DRing::ConfigurationSignal::VolatileDetailsChanged>(accountID_,
                getVolatileAccountDetails());
    }
}

void
Account::loadDefaultCodecs()
{
    // default codec are system codecs
    auto systemCodecList = systemCodecContainer_->getSystemCodecInfoList();

    for (const auto& systemCodec: systemCodecList) {
        // only take encoders and or decoders
        if (systemCodec->codecType & CODEC_UNDEFINED)
            continue;

        if (systemCodec->mediaType & MEDIA_AUDIO) {
            // we are sure of our downcast type : use static_pointer_cast
            auto audioCodec = std::static_pointer_cast<SystemAudioCodecInfo>(systemCodec);
            // instantiate AccountAudioCodecInfo initialized with our system codec
            auto codec = std::make_shared <AccountAudioCodecInfo>(*audioCodec);
            accountCodecInfoList_.push_back(codec);
            RING_DBG("[%s] loading audio codec = %s", accountID_.c_str(),
                     codec->systemCodecInfo.name.c_str());
        }

        if (systemCodec->mediaType & MEDIA_VIDEO) {
            // we are sure of our downcast type : use static_pointer_cast
            auto videoCodec = std::static_pointer_cast<SystemVideoCodecInfo>(systemCodec);
            // instantiate AccountVideoCodecInfo initialized with our system codec
            auto codec = std::make_shared<AccountVideoCodecInfo>(*videoCodec);
            accountCodecInfoList_.push_back(codec);
            RING_DBG("[%s] loading video codec = %s", accountID_.c_str(),
                     codec->systemCodecInfo.name.c_str());
        }
    }
}

void
Account::serialize(YAML::Emitter &out)
{
    out << YAML::Key << ID_KEY << YAML::Value << accountID_;
    out << YAML::Key << ALIAS_KEY << YAML::Value << alias_;
    out << YAML::Key << ACCOUNT_ENABLE_KEY << YAML::Value << enabled_;
    out << YAML::Key << TYPE_KEY << YAML::Value << getAccountType();
    out << YAML::Key << ALL_CODECS_KEY << YAML::Value << allCodecStr_;
    out << YAML::Key << MAILBOX_KEY << YAML::Value << mailBox_;
    out << YAML::Key << ACCOUNT_AUTOANSWER_KEY << YAML::Value << autoAnswerEnabled_;
    out << YAML::Key << RINGTONE_ENABLED_KEY << YAML::Value << ringtoneEnabled_;
    out << YAML::Key << RINGTONE_PATH_KEY << YAML::Value << ringtonePath_;
    out << YAML::Key << HAS_CUSTOM_USER_AGENT_KEY << YAML::Value << hasCustomUserAgent_;
    out << YAML::Key << USER_AGENT_KEY << YAML::Value << userAgent_;
    out << YAML::Key << USERNAME_KEY << YAML::Value << username_;
    out << YAML::Key << DISPLAY_NAME_KEY << YAML::Value << displayName_;
    out << YAML::Key << HOSTNAME_KEY << YAML::Value << hostname_;
    out << YAML::Key << UPNP_ENABLED_KEY << YAML::Value << upnpEnabled_;
}

void
Account::unserialize(const YAML::Node &node)
{
    using yaml_utils::parseValue;

    parseValue(node, ALIAS_KEY, alias_);
    parseValue(node, ACCOUNT_ENABLE_KEY, enabled_);
    parseValue(node, USERNAME_KEY, username_);
    parseValue(node, ACCOUNT_AUTOANSWER_KEY, autoAnswerEnabled_);
    //parseValue(node, PASSWORD_KEY, password_);

    parseValue(node, MAILBOX_KEY, mailBox_);
    parseValue(node, ALL_CODECS_KEY, allCodecStr_);

    // Update codec list which one is used for SDP offer
    setActiveCodecs(split_string_to_unsigned(allCodecStr_, '/'));
    parseValue(node, DISPLAY_NAME_KEY, displayName_);
    parseValue(node, HOSTNAME_KEY, hostname_);

    parseValue(node, HAS_CUSTOM_USER_AGENT_KEY, hasCustomUserAgent_);
    parseValue(node, USER_AGENT_KEY, userAgent_);
    parseValue(node, RINGTONE_PATH_KEY, ringtonePath_);
    parseValue(node, RINGTONE_ENABLED_KEY, ringtoneEnabled_);

    bool enabled;
    parseValue(node, UPNP_ENABLED_KEY, enabled);
    upnpEnabled_.store(enabled);
}

void
Account::setAccountDetails(const std::map<std::string, std::string> &details)
{
    // Account setting common to SIP and IAX
    parseString(details, Conf::CONFIG_ACCOUNT_ALIAS, alias_);
    parseBool(details, Conf::CONFIG_ACCOUNT_ENABLE, enabled_);
    parseString(details, Conf::CONFIG_ACCOUNT_USERNAME, username_);
    parseString(details, Conf::CONFIG_ACCOUNT_HOSTNAME, hostname_);
    parseString(details, Conf::CONFIG_ACCOUNT_MAILBOX, mailBox_);
    parseString(details, Conf::CONFIG_ACCOUNT_USERAGENT, userAgent_);
    parseBool(details, Conf::CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled_);
    parseBool(details, Conf::CONFIG_RINGTONE_ENABLED, ringtoneEnabled_);
    parseString(details, Conf::CONFIG_RINGTONE_PATH, ringtonePath_);
    parseBool(details, Conf::CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT, hasCustomUserAgent_);
    if (hasCustomUserAgent_)
        parseString(details, Conf::CONFIG_ACCOUNT_USERAGENT, userAgent_);
    else
        userAgent_ = DEFAULT_USER_AGENT;
    bool enabled;
    parseBool(details, Conf::CONFIG_UPNP_ENABLED, enabled);
    upnpEnabled_.store(enabled);
}

std::map<std::string, std::string>
Account::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[Conf::CONFIG_ACCOUNT_ALIAS] = alias_;
    a[Conf::CONFIG_ACCOUNT_ENABLE] = enabled_ ? "true" : "false";
    a[Conf::CONFIG_ACCOUNT_TYPE] = getAccountType();
    a[Conf::CONFIG_ACCOUNT_HOSTNAME] = hostname_;
    a[Conf::CONFIG_ACCOUNT_USERNAME] = username_;
    a[Conf::CONFIG_ACCOUNT_MAILBOX] = mailBox_;

    RegistrationState state(registrationState_);

    // This method should only stores user-settable fields
    // For legacy reasons, the STATUS will be kept for some time
    a[Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS] = mapStateNumberToString(state);
    a[Conf::CONFIG_ACCOUNT_USERAGENT] = hasCustomUserAgent_ ? userAgent_ : DEFAULT_USER_AGENT;
    a[Conf::CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT] = hasCustomUserAgent_ ? TRUE_STR : FALSE_STR;
    a[Conf::CONFIG_ACCOUNT_AUTOANSWER] = autoAnswerEnabled_ ? TRUE_STR : FALSE_STR;
    a[Conf::CONFIG_RINGTONE_ENABLED] = ringtoneEnabled_ ? TRUE_STR : FALSE_STR;
    a[Conf::CONFIG_RINGTONE_PATH] = ringtonePath_;
    a[Conf::CONFIG_UPNP_ENABLED] = upnpEnabled_ ? TRUE_STR : FALSE_STR;

    return a;
}

std::map<std::string, std::string>
Account::getVolatileAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS] = mapStateNumberToString(registrationState_);
    return a;
}

#ifdef RING_VIDEO
/*
* TODO ebail : *
* video codec information format changed (merged with audio mechanism)
* we need thus to adapt serialization / unserialization to it
*
*/
#if 0
static bool
isPositiveInteger(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() and std::isdigit(*it))
        ++it;
    return not s.empty() and it == s.end();
}

static bool
isBoolean(const std::string& s)
{
    return s == "true" or s == "false";
}

template <typename Predicate>
static bool
isFieldValid(const std::map<std::string, std::string>& codec, const char* field, Predicate p)
{
    std::map<std::string, std::string>::const_iterator key(codec.find(field));
    return key != codec.end() and p(key->second);
}

static bool
isCodecValid(const std::map<std::string, std::string>& codec, const std::vector<std::map<std::string, std::string>>& defaults)
{
    const std::map<std::string, std::string>::const_iterator name(codec.find(Account::VIDEO_CODEC_NAME));
    if (name == codec.end()) {
        RING_ERR("Field \"name\" missing in codec specification");
        return false;
    }

    // check that it's in the list of valid codecs and that it has all the required fields
    for (const auto &i : defaults) {
        const auto defaultName = i.find(Account::VIDEO_CODEC_NAME);
        if (defaultName->second == name->second) {
            return isFieldValid(codec, Account::VIDEO_CODEC_BITRATE, isPositiveInteger)
                and isFieldValid(codec, Account::VIDEO_CODEC_ENABLED, isBoolean);
        }
    }
    RING_ERR("Codec %s not supported", name->second.c_str());
    return false;
}
#endif // ebail

static bool
isCodecListValid(const std::vector<unsigned>& list UNUSED)
{
/*
* TODO ebail : *
* video codec information format changed (merged with audio mechanism)
* we need thus to adapt serialization / unserialization to it
*
*/
#if 1 /* ebail : for the moment always return true */
    return true;
#else
    const auto defaults(libav_utils::getDefaultVideoCodecs());
    if (list.size() != defaults.size()) {
        RING_ERR("New codec list has a different length than the list of supported codecs");
        return false;
    }

    // make sure that all codecs are present
    for (const auto &i : list) {
        if (not isCodecValid(i, defaults))
            return false;
    }
    return true;
#endif // ebail
}
#endif // RING_VIDEO


// Convert a list of payloads in a special format, readable by the server.
// Required format: payloads separated by slashes.
// @return std::string The serializable string
static std::string
join_string(const std::vector<unsigned> &v)
{
    std::ostringstream os;
    std::copy(v.begin(), v.end(), std::ostream_iterator<unsigned>(os, "/"));
    return os.str();
}

std::vector<unsigned>
Account::getActiveCodecs() const
{
    return getActiveAccountCodecInfoIdList(MEDIA_ALL);
}

void
Account::setActiveCodecs(const std::vector<unsigned>& list)
{
    // first clear the previously stored codecs
    // TODO: mutex to protect isActive
    desactivateAllMedia(MEDIA_ALL);

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

    std::sort(std::begin(accountCodecInfoList_),
              std::end  (accountCodecInfoList_),
              [](std::shared_ptr<AccountCodecInfo> a,
                 std::shared_ptr<AccountCodecInfo> b) {
                  return a->order < b->order;
              });

    allCodecStr_ = join_string(getActiveAccountCodecInfoIdList(MEDIA_ALL));

    for (const auto& item : accountCodecInfoList_)
        RING_DBG("[%s] order:%d,  isActive=%s, codec=%s", accountID_.c_str(),
                 item->order, (item->isActive ? "true" : "false"),
                 item->systemCodecInfo.to_string().c_str());
}

std::string
Account::mapStateNumberToString(RegistrationState state)
{
#define CASE_STATE(X) case RegistrationState::X: \
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
        default:
            return DRing::Account::States::ERROR_GENERIC;
    }

#undef CASE_STATE
}

std::vector<unsigned>
Account::getDefaultCodecs()
{
    return getSystemCodecContainer()->getSystemCodecInfoIdList(MEDIA_ALL);
}

#define find_iter()                                    \
    const auto& iter = details.find(key);               \
    if (iter == details.end()) {                       \
        RING_ERR("Couldn't find key \"%s\"", key);     \
        return;                                        \
    }

void
Account::parseString(const std::map<std::string, std::string>& details,
                     const char* key, std::string& s)
{
    find_iter();
    s = iter->second;
}

void
Account::parseBool(const std::map<std::string, std::string>& details,
                   const char* key, bool &b)
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
    if (upnpEnabled_)
        return upnp_->getExternalIP();
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
    if (upnpEnabled_)
        return upnp_->hasValidIGD();
    return false;
}

/*
 * private account codec searching functions
 *
 * */
std::shared_ptr<AccountCodecInfo>
Account::searchCodecById(unsigned codecId, MediaType mediaType)
{
    for (auto& codecIt: accountCodecInfoList_) {
        if ((codecIt->systemCodecInfo.id == codecId) &&
            (codecIt->systemCodecInfo.mediaType & mediaType ))
                return codecIt;
    }
    return {};
}

std::shared_ptr<AccountCodecInfo>
Account::searchCodecByName(std::string name, MediaType mediaType)
{
    for (auto& codecIt: accountCodecInfoList_) {
        if ((codecIt->systemCodecInfo.name.compare(name) == 0) &&
            (codecIt->systemCodecInfo.mediaType & mediaType ))
            return codecIt;
    }
    return {};
}

std::shared_ptr<AccountCodecInfo>
Account::searchCodecByPayload(unsigned payload, MediaType mediaType)
{
    for (auto& codecIt: accountCodecInfoList_) {
        if ((codecIt->payloadType == payload ) &&
            (codecIt->systemCodecInfo.mediaType & mediaType ))
            return codecIt;
    }
    return {};
}

std::vector<unsigned>
Account::getActiveAccountCodecInfoIdList(MediaType mediaType) const
{
    std::vector<unsigned> idList;
    for (auto& codecIt: accountCodecInfoList_) {
        if ((codecIt->systemCodecInfo.mediaType & mediaType) &&
            (codecIt->isActive))
            idList.push_back(codecIt->systemCodecInfo.id);
    }
    return idList;
}

std::vector<unsigned>
Account::getAccountCodecInfoIdList(MediaType mediaType) const
{
    std::vector<unsigned> idList;
    for (auto& codecIt: accountCodecInfoList_) {
        if (codecIt->systemCodecInfo.mediaType & mediaType)
            idList.push_back(codecIt->systemCodecInfo.id);
    }
    return idList;
}

void
Account::desactivateAllMedia(MediaType mediaType)
{
    for (auto& codecIt: accountCodecInfoList_) {
        if (codecIt->systemCodecInfo.mediaType & mediaType)
            codecIt->isActive = false;
    }
}

std::vector<std::shared_ptr<AccountCodecInfo>>
Account::getActiveAccountCodecInfoList(MediaType mediaType)
{
    std::vector<std::shared_ptr<AccountCodecInfo>> accountCodecList;
    for (auto& codecIt: accountCodecInfoList_) {
        if ((codecIt->systemCodecInfo.mediaType & mediaType) &&
            (codecIt->isActive))
            accountCodecList.push_back(codecIt);
    }
    return accountCodecList;
}

} // namespace ring
