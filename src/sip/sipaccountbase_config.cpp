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
#include "sipaccountbase_config.h"
#include "account_const.h"
#include "account_schema.h"
#include "yamlparser.h"

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

// TODO: write an object to store tls params which implement serializable
const char* const TLS_KEY = "tls";
const char* const TLS_PORT_KEY = "tlsPort";
const char* const CERTIFICATE_KEY = "certificate";
const char* const CALIST_KEY = "calist";
const char* const CIPHERS_KEY = "ciphers";
const char* const TLS_ENABLE_KEY = "enable";
const char* const METHOD_KEY = "method";
const char* const TIMEOUT_KEY = "timeout";
const char* const TLS_PASSWORD_KEY = "password";
const char* const PRIVATE_KEY_KEY = "privateKey";
const char* const REQUIRE_CERTIF_KEY = "requireCertif";
const char* const SERVER_KEY = "server";
const char* const VERIFY_CLIENT_KEY = "verifyClient";
const char* const VERIFY_SERVER_KEY = "verifyServer";

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
SipAccountBaseConfig::serialize(YAML::Emitter& out) const
{
    AccountConfig::serialize(out);
    out << YAML::Key << Conf::AUDIO_PORT_MAX_KEY << YAML::Value << audioPortRange.second;
    out << YAML::Key << Conf::AUDIO_PORT_MIN_KEY << YAML::Value << audioPortRange.first;
    out << YAML::Key << Conf::DTMF_TYPE_KEY << YAML::Value << dtmfType;
    out << YAML::Key << Conf::INTERFACE_KEY << YAML::Value << interface;
    out << YAML::Key << Conf::PUBLISH_ADDR_KEY << YAML::Value << publishedIp;
    //out << YAML::Key << Conf::PUBLISH_PORT_KEY << YAML::Value << publishedPort;
    out << YAML::Key << Conf::SAME_AS_LOCAL_KEY << YAML::Value << publishedSameasLocal;

    out << YAML::Key << Conf::VIDEO_PORT_MAX_KEY << YAML::Value << videoPortRange.second;
    out << YAML::Key << Conf::VIDEO_PORT_MIN_KEY << YAML::Value << videoPortRange.first;

    //out << YAML::Key << Conf::STUN_ENABLED_KEY << YAML::Value << stunEnabled;
    //out << YAML::Key << Conf::STUN_SERVER_KEY << YAML::Value << stunServer;
    out << YAML::Key << Conf::TURN_ENABLED_KEY << YAML::Value << turnEnabled;
    out << YAML::Key << Conf::TURN_SERVER_KEY << YAML::Value << turnServer;
    out << YAML::Key << Conf::TURN_SERVER_UNAME_KEY << YAML::Value << turnServerUserName;
    out << YAML::Key << Conf::TURN_SERVER_PWD_KEY << YAML::Value << turnServerPwd;
    out << YAML::Key << Conf::TURN_SERVER_REALM_KEY << YAML::Value << turnServerRealm;

    out << YAML::Key << Conf::CALIST_KEY << YAML::Value << tlsCaListFile;
    out << YAML::Key << Conf::CERTIFICATE_KEY << YAML::Value << tlsCertificateFile;
    out << YAML::Key << Conf::TLS_PASSWORD_KEY << YAML::Value << tlsPassword;
    out << YAML::Key << Conf::PRIVATE_KEY_KEY << YAML::Value << tlsPrivateKeyFile;
}


void
SipAccountBaseConfig::unserialize(const YAML::Node& node)
{
    AccountConfig::unserialize(node);
    parseValueOptional(node, Conf::INTERFACE_KEY, interface);
    parseValueOptional(node, Conf::SAME_AS_LOCAL_KEY, publishedSameasLocal);
    parseValueOptional(node, Conf::PUBLISH_ADDR_KEY, publishedIp);

    int port = sip_utils::DEFAULT_SIP_PORT;
    parseValueOptional(node, Conf::PUBLISH_PORT_KEY, port);
    //publishedPort_ = port;

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
    //a.emplace(Conf::CONFIG_PUBLISHED_PORT, std::to_string(publishedPort));
    a.emplace(Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_PUBLISHED_ADDRESS, publishedIp);
    //a.emplace(Conf::CONFIG_STUN_ENABLE, stunEnabled ? TRUE_STR : FALSE_STR);
    //a.emplace(Conf::CONFIG_STUN_SERVER, stunServer);
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
    //parseInt(details, Conf::CONFIG_PUBLISHED_PORT, publishedPort);

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
