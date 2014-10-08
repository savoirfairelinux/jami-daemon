/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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

#ifdef SFL_VIDEO
#include "video/libav_utils.h"
#endif

#include "logger.h"
#include "manager.h"

#include "client/configurationmanager.h"
#include "account_schema.h"
#include "config/yamlparser.h"

#include <yaml-cpp/yaml.h>

const char * const Account::AUDIO_CODECS_KEY            = "audioCodecs";  // 0/9/110/111/112/
const char * const Account::VIDEO_CODECS_KEY            = "videoCodecs";
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
const char * const Account::DEFAULT_USER_AGENT          = "SFLphone/" PACKAGE_VERSION;
const char * const Account::USER_AGENT_KEY              = "useragent";
const char * const Account::HAS_CUSTOM_USER_AGENT_KEY   = "hasCustomUserAgent";
const char * const Account::PRESENCE_MODULE_ENABLED_KEY = "presenceModuleEnabled";

using std::map;
using std::string;
using std::vector;


Account::Account(const string &accountID)
    : accountID_(accountID)
    , username_()
    , hostname_()
    , alias_()
    , enabled_(true)
    , autoAnswerEnabled_(false)
    , registrationState_(RegistrationState::UNREGISTERED)
    , audioCodecList_()
    , videoCodecList_()
    , audioCodecStr_()
    , ringtonePath_("")
    , ringtoneEnabled_(true)
    , displayName_("")
    , userAgent_(DEFAULT_USER_AGENT)
    , hasCustomUserAgent_(false)
    , mailBox_()
{
    // Initialize the codec order, used when creating a new account
    loadDefaultCodecs();
    #ifdef __ANDROID__
        ringtonePath_ = "/data/data/org.sflphone/files/ringtones/konga.ul";
    #else
        ringtonePath_ = "/usr/share/sflphone/ringtones/konga.ul";
    #endif
}

Account::~Account()
{}

void
Account::attachCall(const string& id)
{
    callIDSet_.insert(id);
}

void
Account::detachCall(const string& id)
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

void Account::setRegistrationState(RegistrationState state)
{
    if (state != registrationState_) {
        registrationState_ = state;
        // Notify the client
        ConfigurationManager *c(Manager::instance().getClient()->getConfigurationManager());
        c->registrationStateChanged(accountID_, static_cast<int32_t>(registrationState_));
        c->volatileAccountDetailsChanged(accountID_, getVolatileAccountDetails());
    }
}

void Account::loadDefaultCodecs()
{
    // TODO
    // CodecMap codecMap = Manager::instance ().getCodecDescriptorMap ().getCodecsMap();

    // Initialize codec
    vector<string> result;
    result.push_back("0");
    result.push_back("3");
    result.push_back("8");
    result.push_back("9");
    result.push_back("104");
    result.push_back("110");
    result.push_back("111");
    result.push_back("112");

    setActiveAudioCodecs(result);
#ifdef SFL_VIDEO
    // we don't need to validate via setVideoCodecs, since these are defaults
    videoCodecList_ = libav_utils::getDefaultCodecs();
#endif
}


void Account::serialize(YAML::Emitter &out)
{
    using namespace Conf;

    out << YAML::Key << ID_KEY << YAML::Value << accountID_;
    out << YAML::Key << ALIAS_KEY << YAML::Value << alias_;
    out << YAML::Key << ACCOUNT_ENABLE_KEY << YAML::Value << enabled_;
    out << YAML::Key << TYPE_KEY << YAML::Value << getAccountType();
    out << YAML::Key << AUDIO_CODECS_KEY << YAML::Value << audioCodecStr_;
    out << YAML::Key << MAILBOX_KEY << YAML::Value << mailBox_;
    out << YAML::Key << ACCOUNT_AUTOANSWER_KEY << YAML::Value << autoAnswerEnabled_;
    out << YAML::Key << RINGTONE_ENABLED_KEY << YAML::Value << ringtoneEnabled_;
    out << YAML::Key << RINGTONE_PATH_KEY << YAML::Value << ringtonePath_;
    out << YAML::Key << HAS_CUSTOM_USER_AGENT_KEY << YAML::Value << hasCustomUserAgent_;
    out << YAML::Key << USER_AGENT_KEY << YAML::Value << userAgent_;
    out << YAML::Key << USERNAME_KEY << YAML::Value << username_;
    out << YAML::Key << DISPLAY_NAME_KEY << YAML::Value << displayName_;
    out << YAML::Key << HOSTNAME_KEY << YAML::Value << hostname_;
}

void Account::unserialize(const YAML::Node &node)
{
    using namespace yaml_utils;
    parseValue(node, ALIAS_KEY, alias_);
    parseValue(node, ACCOUNT_ENABLE_KEY, enabled_);
    parseValue(node, USERNAME_KEY, username_);
    parseValue(node, ACCOUNT_AUTOANSWER_KEY, autoAnswerEnabled_);
    //parseValue(node, PASSWORD_KEY, password_);

    parseValue(node, MAILBOX_KEY, mailBox_);
    parseValue(node, AUDIO_CODECS_KEY, audioCodecStr_);

    // Update codec list which one is used for SDP offer
    setActiveAudioCodecs(split_string(audioCodecStr_));
    parseValue(node, DISPLAY_NAME_KEY, displayName_);
    parseValue(node, HOSTNAME_KEY, hostname_);

    parseValue(node, HAS_CUSTOM_USER_AGENT_KEY, hasCustomUserAgent_);
    parseValue(node, USER_AGENT_KEY, userAgent_);
    parseValue(node, RINGTONE_PATH_KEY, ringtonePath_);
    parseValue(node, RINGTONE_ENABLED_KEY, ringtoneEnabled_);
}

void Account::setAccountDetails(const std::map<std::string, std::string> &details)
{
    // Account setting common to SIP and IAX
    parseString(details, CONFIG_ACCOUNT_ALIAS, alias_);
    parseBool(details, CONFIG_ACCOUNT_ENABLE, enabled_);
    parseString(details, CONFIG_ACCOUNT_USERNAME, username_);
    parseString(details, CONFIG_ACCOUNT_HOSTNAME, hostname_);
    parseString(details, CONFIG_ACCOUNT_MAILBOX, mailBox_);
    parseString(details, CONFIG_ACCOUNT_USERAGENT, userAgent_);
    parseBool(details, CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled_);
    parseBool(details, CONFIG_RINGTONE_ENABLED, ringtoneEnabled_);
    parseString(details, CONFIG_RINGTONE_PATH, ringtonePath_);
    parseBool(details, CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT, hasCustomUserAgent_);
    if (hasCustomUserAgent_)
        parseString(details, CONFIG_ACCOUNT_USERAGENT, userAgent_);
    else
        userAgent_ = DEFAULT_USER_AGENT;
}

std::map<std::string, std::string> Account::getAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[CONFIG_ACCOUNT_ALIAS] = alias_;
    a[CONFIG_ACCOUNT_ENABLE] = enabled_ ? "true" : "false";
    a[CONFIG_ACCOUNT_TYPE] = getAccountType();
    a[CONFIG_ACCOUNT_HOSTNAME] = hostname_;
    a[CONFIG_ACCOUNT_USERNAME] = username_;
    a[CONFIG_ACCOUNT_MAILBOX] = mailBox_;

    RegistrationState state(registrationState_);

    // This method should only stores user-settable fields
    // For legacy reasons, the STATUS will be kept for some time
    a[CONFIG_ACCOUNT_REGISTRATION_STATUS] = mapStateNumberToString(state);
    a[CONFIG_ACCOUNT_USERAGENT] = hasCustomUserAgent_ ? userAgent_ : DEFAULT_USER_AGENT;
    a[CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT] = hasCustomUserAgent_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_ACCOUNT_AUTOANSWER] = autoAnswerEnabled_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_RINGTONE_ENABLED] = ringtoneEnabled_ ? TRUE_STR : FALSE_STR;
    a[CONFIG_RINGTONE_PATH] = ringtonePath_;

    return a;
}

std::map<std::string, std::string> Account::getVolatileAccountDetails() const
{
    std::map<std::string, std::string> a;

    a[CONFIG_ACCOUNT_REGISTRATION_STATUS] = mapStateNumberToString(registrationState_);
    return a;
}

#ifdef SFL_VIDEO
static bool
isPositiveInteger(const string &s)
{
    string::const_iterator it = s.begin();
    while (it != s.end() and std::isdigit(*it))
        ++it;
    return not s.empty() and it == s.end();
}

static bool
isBoolean(const string &s)
{
    return s == "true" or s == "false";
}

template <typename Predicate>
static bool
isFieldValid(const map<string, string> &codec, const char *field, Predicate p)
{
    map<string, string>::const_iterator key(codec.find(field));
    return key != codec.end() and p(key->second);
}

static bool
isCodecValid(const map<string, string> &codec, const vector<map<string, string> > &defaults)
{
    const map<string, string>::const_iterator name(codec.find(Account::VIDEO_CODEC_NAME));
    if (name == codec.end()) {
        ERROR("Field \"name\" missing in codec specification");
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
    ERROR("Codec %s not supported", name->second.c_str());
    return false;
}

static bool
isCodecListValid(const vector<map<string, string> > &list)
{
    const auto defaults(libav_utils::getDefaultCodecs());
    if (list.size() != defaults.size()) {
        ERROR("New codec list has a different length than the list of supported codecs");
        return false;
    }

    // make sure that all codecs are present
    for (const auto &i : list) {
        if (not isCodecValid(i, defaults))
            return false;
    }
    return true;
}
#endif

void Account::setVideoCodecs(const vector<map<string, string> > &list)
{
#ifdef SFL_VIDEO
    if (isCodecListValid(list))
        videoCodecList_ = list;
#else
    (void) list;
#endif
}

// Convert a list of payloads in a special format, readable by the server.
// Required format: payloads separated by slashes.
// @return std::string The serializable string
static std::string
join_string(const std::vector<std::string> &v)
{
    std::ostringstream os;
    std::copy(v.begin(), v.end(), std::ostream_iterator<std::string>(os, "/"));
    return os.str();
}

std::vector<std::string>
Account::split_string(std::string s)
{
    std::vector<std::string> list;
    std::string temp;

    while (s.find("/", 0) != std::string::npos) {
        size_t pos = s.find("/", 0);
        temp = s.substr(0, pos);
        s.erase(0, pos + 1);
        list.push_back(temp);
    }

    return list;
}


void Account::setActiveAudioCodecs(const vector<string> &list)
{
    // first clear the previously stored codecs
    audioCodecList_.clear();

    // list contains the ordered payload of active codecs picked by the user for this account
    // we used the codec vector to save the order.
    for (const auto &item : list) {
        int payload = std::atoi(item.c_str());
        audioCodecList_.push_back(payload);
    }

    // update the codec string according to new codec selection
    audioCodecStr_ = join_string(list);
}

string Account::mapStateNumberToString(RegistrationState state)
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
            return "ERROR_GENERIC";
    }

#undef CASE_STATE
}

vector<map<string, string> >
Account::getAllVideoCodecs() const
{
    return videoCodecList_;
}

static bool
is_inactive(const map<string, string> &codec)
{
    map<string, string>::const_iterator iter = codec.find(Account::VIDEO_CODEC_ENABLED);
    return iter == codec.end() or iter->second != "true";
}

vector<int>
Account::getDefaultAudioCodecs()
{
    vector<int> result;
    result.push_back(0);
    result.push_back(3);
    result.push_back(8);
    result.push_back(9);
    result.push_back(104);
    result.push_back(110);
    result.push_back(111);
    result.push_back(112);

    return result;
}

vector<map<string, string> >
Account::getActiveVideoCodecs() const
{
    if (not videoEnabled_)
        return vector<map<string, string>>();

    // FIXME: validate video codec details first
    vector<map<string, string> > result(videoCodecList_);
    result.erase(std::remove_if(result.begin(), result.end(), is_inactive), result.end());
    return result;
}

#define find_iter()                             \
        const auto iter = details.find(key);    \
        if (iter == details.end()) {            \
            ERROR("Couldn't find key \"%s\"", key); \
            return;                             \
        }

void
Account::parseString(const std::map<std::string, std::string> &details, const char *key, std::string &s)
{
    find_iter();
    s = iter->second;
}

void
Account::parseBool(const std::map<std::string, std::string> &details, const char *key, bool &b)
{
    find_iter();
    b = iter->second == TRUE_STR;
}

#undef find_iter
