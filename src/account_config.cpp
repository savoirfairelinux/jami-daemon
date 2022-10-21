/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
#include "account_config.h"
#include "account_const.h"
#include "account_schema.h"
#include "yamlparser.h"
#include "string_utils.h"
#include "fileutils.h"

namespace jami {

constexpr const char* ALL_CODECS_KEY = "allCodecs";
constexpr const char* VIDEO_CODEC_ENABLED = "enabled";
constexpr const char* VIDEO_CODEC_NAME = "name";
constexpr const char* VIDEO_CODEC_PARAMETERS = "parameters";
constexpr const char* VIDEO_CODEC_BITRATE = "bitrate";
constexpr const char* RINGTONE_PATH_KEY = "ringtonePath";
constexpr const char* RINGTONE_ENABLED_KEY = "ringtoneEnabled";
constexpr const char* VIDEO_ENABLED_KEY = "videoEnabled";
constexpr const char* DISPLAY_NAME_KEY = "displayName";
constexpr const char* ALIAS_KEY = "alias";
constexpr const char* TYPE_KEY = "type";
constexpr const char* ID_KEY = "id";
constexpr const char* AUTHENTICATION_USERNAME_KEY = "authenticationUsername";
constexpr const char* PASSWORD_KEY = "password";
constexpr const char* HOSTNAME_KEY = "hostname";
constexpr const char* ACCOUNT_ENABLE_KEY = "enable";
constexpr const char* ACCOUNT_AUTOANSWER_KEY = "autoAnswer";
constexpr const char* ACCOUNT_READRECEIPT_KEY = "sendReadReceipt";
constexpr const char* ACCOUNT_ISRENDEZVOUS_KEY = "rendezVous";
constexpr const char* ACCOUNT_ACTIVE_CALL_LIMIT_KEY = "activeCallLimit";
constexpr const char* MAILBOX_KEY = "mailbox";
constexpr const char* USER_AGENT_KEY = "useragent";
constexpr const char* HAS_CUSTOM_USER_AGENT_KEY = "hasCustomUserAgent";
constexpr const char* UPNP_ENABLED_KEY = "upnpEnabled";
constexpr const char* ACTIVE_CODEC_KEY = "activeCodecs";
constexpr const char* DISABLED_CODECS_KEY = "disabledCodecs";
constexpr const char* DEFAULT_MODERATORS_KEY = "defaultModerators";
constexpr const char* LOCAL_MODERATORS_ENABLED_KEY = "localModeratorsEnabled";
constexpr const char* ALL_MODERATORS_ENABLED_KEY = "allModeratorsEnabled";
constexpr const char* PROXY_PUSH_TOKEN_KEY = "proxyPushToken";
constexpr const char* PROXY_PUSH_TOPIC_KEY = "proxyPushiOSTopic";

using yaml_utils::parseValueOptional;

void
AccountConfig::serialize(YAML::Emitter& out) const
{
    out << YAML::Key << ACCOUNT_ENABLE_KEY << YAML::Value << enabled;
    out << YAML::Key << TYPE_KEY << YAML::Value << type;
    out << YAML::Key << ALIAS_KEY << YAML::Value << alias;
    out << YAML::Key << HOSTNAME_KEY << YAML::Value << hostname;
    out << YAML::Key << DISABLED_CODECS_KEY << YAML::Value << disabledCodecs;
    out << YAML::Key << MAILBOX_KEY << YAML::Value << mailbox;
    out << YAML::Key << ACCOUNT_AUTOANSWER_KEY << YAML::Value << autoAnswerEnabled;
    out << YAML::Key << ACCOUNT_READRECEIPT_KEY << YAML::Value << sendReadReceipt;
    out << YAML::Key << ACCOUNT_ISRENDEZVOUS_KEY << YAML::Value << isRendezVous;
    out << YAML::Key << ACCOUNT_ACTIVE_CALL_LIMIT_KEY << YAML::Value << activeCallLimit;
    out << YAML::Key << RINGTONE_ENABLED_KEY << YAML::Value << ringtoneEnabled;
    out << YAML::Key << RINGTONE_PATH_KEY << YAML::Value << ringtonePath;
    out << YAML::Key << USER_AGENT_KEY << YAML::Value << customUserAgent;
    out << YAML::Key << DISPLAY_NAME_KEY << YAML::Value << displayName;
    out << YAML::Key << UPNP_ENABLED_KEY << YAML::Value << upnpEnabled;
    out << YAML::Key << DEFAULT_MODERATORS_KEY << YAML::Value << defaultModerators;
    out << YAML::Key << LOCAL_MODERATORS_ENABLED_KEY << YAML::Value << localModeratorsEnabled;
    out << YAML::Key << ALL_MODERATORS_ENABLED_KEY << YAML::Value << allModeratorsEnabled;
    out << YAML::Key << PROXY_PUSH_TOKEN_KEY << YAML::Value << deviceKey;
    out << YAML::Key << PROXY_PUSH_TOPIC_KEY << YAML::Value << notificationTopic;
    out << YAML::Key << VIDEO_ENABLED_KEY << YAML::Value << videoEnabled;
}

void
AccountConfig::unserialize(const YAML::Node& node)
{
    parseValueOptional(node, ALIAS_KEY, alias);
    parseValueOptional(node, TYPE_KEY, type);
    parseValueOptional(node, ACCOUNT_ENABLE_KEY, enabled);
    parseValueOptional(node, HOSTNAME_KEY, hostname);
    parseValueOptional(node, ACCOUNT_AUTOANSWER_KEY, autoAnswerEnabled);
    parseValueOptional(node, ACCOUNT_READRECEIPT_KEY, sendReadReceipt);
    parseValueOptional(node, ACCOUNT_ISRENDEZVOUS_KEY, isRendezVous);
    parseValueOptional(node, ACCOUNT_ACTIVE_CALL_LIMIT_KEY, activeCallLimit);
    parseValueOptional(node, MAILBOX_KEY, mailbox);

    std::string codecs;
    if (parseValueOptional(node, ACTIVE_CODEC_KEY, codecs))
        activeCodecs = split_string_to_unsigned(codecs, '/');

    parseValueOptional(node, DISPLAY_NAME_KEY, displayName);

    parseValueOptional(node, USER_AGENT_KEY, customUserAgent);
    parseValueOptional(node, RINGTONE_PATH_KEY, ringtonePath);
    parseValueOptional(node, RINGTONE_ENABLED_KEY, ringtoneEnabled);
    parseValueOptional(node, VIDEO_ENABLED_KEY, videoEnabled);

    parseValueOptional(node, UPNP_ENABLED_KEY, upnpEnabled);

    std::string defMod;
    parseValueOptional(node, DEFAULT_MODERATORS_KEY, defMod);
    defaultModerators = string_split_set(defMod);
    parseValueOptional(node, LOCAL_MODERATORS_ENABLED_KEY, localModeratorsEnabled);
    parseValueOptional(node, ALL_MODERATORS_ENABLED_KEY, allModeratorsEnabled);
    parseValueOptional(node, PROXY_PUSH_TOKEN_KEY, deviceKey);
    parseValueOptional(node, PROXY_PUSH_TOPIC_KEY, notificationTopic);
}

std::map<std::string, std::string>
AccountConfig::toMap() const
{
    return {{Conf::CONFIG_ACCOUNT_ALIAS, alias},
            {Conf::CONFIG_ACCOUNT_DISPLAYNAME, displayName},
            {Conf::CONFIG_ACCOUNT_ENABLE, enabled ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ACCOUNT_TYPE, type},
            {Conf::CONFIG_ACCOUNT_HOSTNAME, hostname},
            {Conf::CONFIG_ACCOUNT_MAILBOX, mailbox},
            {Conf::CONFIG_ACCOUNT_USERAGENT, customUserAgent},
            {Conf::CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ACCOUNT_SENDREADRECEIPT, sendReadReceipt ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ACCOUNT_ISRENDEZVOUS, isRendezVous ? TRUE_STR : FALSE_STR},
            {DRing::Account::ConfProperties::ACTIVE_CALL_LIMIT, std::to_string(activeCallLimit)},
            {Conf::CONFIG_RINGTONE_ENABLED, ringtoneEnabled ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_RINGTONE_PATH, ringtonePath},
            {Conf::CONFIG_VIDEO_ENABLED, videoEnabled ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_UPNP_ENABLED, upnpEnabled ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_DEFAULT_MODERATORS, string_join(defaultModerators)},
            {Conf::CONFIG_LOCAL_MODERATORS_ENABLED, localModeratorsEnabled ? TRUE_STR : FALSE_STR},
            {Conf::CONFIG_ALL_MODERATORS_ENABLED, allModeratorsEnabled ? TRUE_STR : FALSE_STR}};
}

void
AccountConfig::fromMap(const std::map<std::string, std::string>& details)
{
    parseString(details, Conf::CONFIG_ACCOUNT_ALIAS, alias);
    parseString(details, Conf::CONFIG_ACCOUNT_DISPLAYNAME, displayName);
    parseBool(details, Conf::CONFIG_ACCOUNT_ENABLE, enabled);
    parseBool(details, Conf::CONFIG_VIDEO_ENABLED, videoEnabled);
    parseString(details, Conf::CONFIG_ACCOUNT_HOSTNAME, hostname);
    parseString(details, Conf::CONFIG_ACCOUNT_MAILBOX, mailbox);
    parseBool(details, Conf::CONFIG_ACCOUNT_AUTOANSWER, autoAnswerEnabled);
    parseBool(details, Conf::CONFIG_ACCOUNT_SENDREADRECEIPT, sendReadReceipt);
    parseBool(details, Conf::CONFIG_ACCOUNT_ISRENDEZVOUS, isRendezVous);
    parseInt(details, DRing::Account::ConfProperties::ACTIVE_CALL_LIMIT, activeCallLimit);
    parseBool(details, Conf::CONFIG_RINGTONE_ENABLED, ringtoneEnabled);
    parseString(details, Conf::CONFIG_RINGTONE_PATH, ringtonePath);
    parseString(details, Conf::CONFIG_ACCOUNT_USERAGENT, customUserAgent);
    parseBool(details, Conf::CONFIG_UPNP_ENABLED, upnpEnabled);
    std::string defMod;
    parseString(details, Conf::CONFIG_DEFAULT_MODERATORS, defMod);
    defaultModerators = string_split_set(defMod);
    parseBool(details, Conf::CONFIG_LOCAL_MODERATORS_ENABLED, localModeratorsEnabled);
    parseBool(details, Conf::CONFIG_ALL_MODERATORS_ENABLED, allModeratorsEnabled);
}

void
parsePath(const std::map<std::string, std::string>& details,
                   const char* key,
                   std::string& s,
                   const std::string& base)
{
    auto it = details.find(key);
    if (it != details.end())
        s = fileutils::getCleanPath(base, it->second);
}

}
