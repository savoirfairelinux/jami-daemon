/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "sipaccountbase_config.h"
#include "account_const.h"
#include "account_schema.h"
#include "config/account_config_utils.h"

namespace jami {

namespace Conf {
// SIP specific configuration keys
const char* const BIND_ADDRESS_KEY = "bindAddress";
const char* const INTERFACE_KEY = "interface";
const char* const PORT_KEY = "port";
const char* const PUBLISH_ADDR_KEY = "publishAddr";
const char* const PUBLISH_PORT_KEY = "publishPort";
const char* const SAME_AS_LOCAL_KEY = "sameasLocal";
const char* const DTMF_TYPE_KEY = "dtmfType";
const char* const SERVICE_ROUTE_KEY = "serviceRoute";
const char* const ALLOW_IP_AUTO_REWRITE = "allowIPAutoRewrite";
const char* const PRESENCE_ENABLED_KEY = "presenceEnabled";
const char* const PRESENCE_PUBLISH_SUPPORTED_KEY = "presencePublishSupported";
const char* const PRESENCE_SUBSCRIBE_SUPPORTED_KEY = "presenceSubscribeSupported";
const char* const PRESENCE_STATUS_KEY = "presenceStatus";
const char* const PRESENCE_NOTE_KEY = "presenceNote";
const char* const STUN_ENABLED_KEY = "stunEnabled";
const char* const STUN_SERVER_KEY = "stunServer";
const char* const TURN_ENABLED_KEY = "turnEnabled";
const char* const TURN_SERVER_KEY = "turnServer";
const char* const TURN_SERVER_UNAME_KEY = "turnServerUserName";
const char* const TURN_SERVER_PWD_KEY = "turnServerPassword";
const char* const TURN_SERVER_REALM_KEY = "turnServerRealm";
const char* const CRED_KEY = "credential";
const char* const AUDIO_PORT_MIN_KEY = "audioPortMin";
const char* const AUDIO_PORT_MAX_KEY = "audioPortMax";
const char* const VIDEO_PORT_MIN_KEY = "videoPortMin";
const char* const VIDEO_PORT_MAX_KEY = "videoPortMax";
} // namespace Conf

using yaml_utils::parseValueOptional;

static void
unserializeRange(const YAML::Node& node,
                 const char* minKey,
                 const char* maxKey,
                 std::pair<uint16_t, uint16_t>& range)
{
    int tmpMin = yaml_utils::parseValueOptional(node, minKey, tmpMin);
    int tmpMax = yaml_utils::parseValueOptional(node, maxKey, tmpMax);
    updateRange(tmpMin, tmpMax, range);
}

static void
addRangeToDetails(std::map<std::string, std::string>& a,
                  const char* minKey,
                  const char* maxKey,
                  const std::pair<uint16_t, uint16_t>& range)
{
    a.emplace(minKey, std::to_string(range.first));
    a.emplace(maxKey, std::to_string(range.second));
}

void
SipAccountBaseConfig::serializeDiff(YAML::Emitter& out, const SipAccountBaseConfig& DEFAULT_CONFIG) const
{
    AccountConfig::serializeDiff(out, DEFAULT_CONFIG);
    SERIALIZE_CONFIG(Conf::DTMF_TYPE_KEY, dtmfType);
    SERIALIZE_CONFIG(Conf::INTERFACE_KEY, interface);
    SERIALIZE_CONFIG(Conf::PUBLISH_ADDR_KEY, publishedIp);
    SERIALIZE_CONFIG(Conf::SAME_AS_LOCAL_KEY, publishedSameasLocal);
    SERIALIZE_CONFIG(Conf::AUDIO_PORT_MAX_KEY, audioPortRange.second);
    SERIALIZE_CONFIG(Conf::AUDIO_PORT_MAX_KEY, audioPortRange.first);
    SERIALIZE_CONFIG(Conf::VIDEO_PORT_MAX_KEY, videoPortRange.second);
    SERIALIZE_CONFIG(Conf::VIDEO_PORT_MIN_KEY, videoPortRange.first);
    SERIALIZE_CONFIG(Conf::TURN_ENABLED_KEY, turnEnabled);
    SERIALIZE_CONFIG(Conf::TURN_SERVER_KEY, turnServer);
    SERIALIZE_CONFIG(Conf::TURN_SERVER_UNAME_KEY, turnServerUserName);
    SERIALIZE_CONFIG(Conf::TURN_SERVER_PWD_KEY, turnServerPwd);
    SERIALIZE_CONFIG(Conf::TURN_SERVER_REALM_KEY, turnServerRealm);
}

void
SipAccountBaseConfig::unserialize(const YAML::Node& node)
{
    AccountConfig::unserialize(node);
    parseValueOptional(node, Conf::INTERFACE_KEY, interface);
    parseValueOptional(node, Conf::SAME_AS_LOCAL_KEY, publishedSameasLocal);
    parseValueOptional(node, Conf::PUBLISH_ADDR_KEY, publishedIp);
    parseValueOptional(node, Conf::DTMF_TYPE_KEY, dtmfType);
    unserializeRange(node, Conf::AUDIO_PORT_MIN_KEY, Conf::AUDIO_PORT_MAX_KEY, audioPortRange);
    unserializeRange(node, Conf::VIDEO_PORT_MIN_KEY, Conf::VIDEO_PORT_MAX_KEY, videoPortRange);

    // ICE - STUN/TURN
    //parseValueOptional(node, Conf::STUN_ENABLED_KEY, stunEnabled);
    //parseValueOptional(node, Conf::STUN_SERVER_KEY, stunServer);
    parseValueOptional(node, Conf::TURN_ENABLED_KEY, turnEnabled);
    parseValueOptional(node, Conf::TURN_SERVER_KEY, turnServer);
    parseValueOptional(node, Conf::TURN_SERVER_UNAME_KEY, turnServerUserName);
    parseValueOptional(node, Conf::TURN_SERVER_PWD_KEY, turnServerPwd);
    parseValueOptional(node, Conf::TURN_SERVER_REALM_KEY, turnServerRealm);
}

std::map<std::string, std::string>
SipAccountBaseConfig::toMap() const
{
    auto a = AccountConfig::toMap();

    addRangeToDetails(a,
                      Conf::CONFIG_ACCOUNT_AUDIO_PORT_MIN,
                      Conf::CONFIG_ACCOUNT_AUDIO_PORT_MAX,
                      audioPortRange);
    addRangeToDetails(a,
                      Conf::CONFIG_ACCOUNT_VIDEO_PORT_MIN,
                      Conf::CONFIG_ACCOUNT_VIDEO_PORT_MAX,
                      videoPortRange);

    a.emplace(Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType);
    a.emplace(Conf::CONFIG_LOCAL_INTERFACE, interface);
    a.emplace(Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_PUBLISHED_ADDRESS, publishedIp);

    a.emplace(Conf::CONFIG_TURN_ENABLE, turnEnabled ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_TURN_SERVER, turnServer);
    a.emplace(Conf::CONFIG_TURN_SERVER_UNAME, turnServerUserName);
    a.emplace(Conf::CONFIG_TURN_SERVER_PWD, turnServerPwd);
    a.emplace(Conf::CONFIG_TURN_SERVER_REALM, turnServerRealm);
    return a;
}

void
SipAccountBaseConfig::fromMap(const std::map<std::string, std::string>& details)
{
    AccountConfig::fromMap(details);

    // general sip settings
    parseString(details, Conf::CONFIG_LOCAL_INTERFACE, interface);
    parseBool(details, Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal);
    parseString(details, Conf::CONFIG_PUBLISHED_ADDRESS, publishedIp);
    parseString(details, Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType);

    int tmpMin = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MIN, tmpMin);
    int tmpMax = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_AUDIO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, audioPortRange);
    tmpMin = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MIN, tmpMin);
    tmpMax = -1;
    parseInt(details, Conf::CONFIG_ACCOUNT_VIDEO_PORT_MAX, tmpMax);
    updateRange(tmpMin, tmpMax, videoPortRange);

    // ICE - STUN
    //parseBool(details, Conf::CONFIG_STUN_ENABLE, stunEnabled);
    //parseString(details, Conf::CONFIG_STUN_SERVER, stunServer);

    // ICE - TURN
    parseBool(details, Conf::CONFIG_TURN_ENABLE, turnEnabled);
    parseString(details, Conf::CONFIG_TURN_SERVER, turnServer);
    parseString(details, Conf::CONFIG_TURN_SERVER_UNAME, turnServerUserName);
    parseString(details, Conf::CONFIG_TURN_SERVER_PWD, turnServerPwd);
    parseString(details, Conf::CONFIG_TURN_SERVER_REALM, turnServerRealm);
}

}
