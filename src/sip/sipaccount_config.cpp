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
#include "sipaccount_config.h"
#include "account_const.h"
#include "account_schema.h"
#include "yamlparser.h"

namespace jami {

namespace Conf {
constexpr const char* USERNAME_KEY = "username";
constexpr const char* BIND_ADDRESS_KEY = "bindAddress";
constexpr const char* INTERFACE_KEY = "interface";
constexpr const char* PORT_KEY = "port";
constexpr const char* PUBLISH_ADDR_KEY = "publishAddr";
constexpr const char* PUBLISH_PORT_KEY = "publishPort";
constexpr const char* SAME_AS_LOCAL_KEY = "sameasLocal";
constexpr const char* DTMF_TYPE_KEY = "dtmfType";
constexpr const char* SERVICE_ROUTE_KEY = "serviceRoute";
constexpr const char* ALLOW_IP_AUTO_REWRITE = "allowIPAutoRewrite";
constexpr const char* PRESENCE_ENABLED_KEY = "presenceEnabled";
constexpr const char* PRESENCE_PUBLISH_SUPPORTED_KEY = "presencePublishSupported";
constexpr const char* PRESENCE_SUBSCRIBE_SUPPORTED_KEY = "presenceSubscribeSupported";
constexpr const char* PRESENCE_STATUS_KEY = "presenceStatus";
constexpr const char* PRESENCE_NOTE_KEY = "presenceNote";
constexpr const char* PRESENCE_MODULE_ENABLED_KEY = "presenceModuleEnabled";
constexpr const char* KEEP_ALIVE_ENABLED = "keepAlive";

constexpr const char* TLS_KEY = "tls";
constexpr const char* TLS_PORT_KEY = "tlsPort";
constexpr const char* CERTIFICATE_KEY = "certificate";
constexpr const char* CALIST_KEY = "calist";
constexpr const char* CIPHERS_KEY = "ciphers";
constexpr const char* TLS_ENABLE_KEY = "enable";
constexpr const char* METHOD_KEY = "method";
constexpr const char* TIMEOUT_KEY = "timeout";
constexpr const char* TLS_PASSWORD_KEY = "password";
constexpr const char* PRIVATE_KEY_KEY = "privateKey";
constexpr const char* REQUIRE_CERTIF_KEY = "requireCertif";
constexpr const char* SERVER_KEY = "server";
constexpr const char* VERIFY_CLIENT_KEY = "verifyClient";
constexpr const char* VERIFY_SERVER_KEY = "verifyServer";

constexpr const char* STUN_ENABLED_KEY = "stunEnabled";
constexpr const char* STUN_SERVER_KEY = "stunServer";
constexpr const char* TURN_ENABLED_KEY = "turnEnabled";
constexpr const char* TURN_SERVER_KEY = "turnServer";
constexpr const char* TURN_SERVER_UNAME_KEY = "turnServerUserName";
constexpr const char* TURN_SERVER_PWD_KEY = "turnServerPassword";
constexpr const char* TURN_SERVER_REALM_KEY = "turnServerRealm";
constexpr const char* CRED_KEY = "credential";
constexpr const char* AUDIO_PORT_MIN_KEY = "audioPortMin";
constexpr const char* AUDIO_PORT_MAX_KEY = "audioPortMax";
constexpr const char* VIDEO_PORT_MIN_KEY = "videoPortMin";
constexpr const char* VIDEO_PORT_MAX_KEY = "videoPortMax";

constexpr const char* SRTP_KEY = "srtp";
constexpr const char* SRTP_ENABLE_KEY = "enable";
constexpr const char* KEY_EXCHANGE_KEY = "keyExchange";
constexpr const char* RTP_FALLBACK_KEY = "rtpFallback";
} // namespace Conf

static constexpr unsigned MIN_REGISTRATION_TIME = 60;                  // seconds

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
SipAccountConfig::serialize(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    SipAccountBaseConfig::serialize(out);

    out << YAML::Key << Conf::BIND_ADDRESS_KEY << YAML::Value << bindAddress;
    out << YAML::Key << Conf::PORT_KEY << YAML::Value << localPort;

    out << YAML::Key << Conf::USERNAME_KEY << YAML::Value << username;

    // each credential is a map, and we can have multiple credentials
    // out << YAML::Key << Conf::CRED_KEY << YAML::Value << getCredentials();

    out << YAML::Key << Conf::KEEP_ALIVE_ENABLED << YAML::Value << registrationRefreshEnabled;

    //out << YAML::Key << PRESENCE_MODULE_ENABLED_KEY << YAML::Value
    //    << (presence_ and presence_->isEnabled());

    out << YAML::Key << Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE << YAML::Value
        << registrationExpire;
    out << YAML::Key << Conf::SERVICE_ROUTE_KEY << YAML::Value << serviceRoute;
    out << YAML::Key << Conf::ALLOW_IP_AUTO_REWRITE << YAML::Value << allowIPAutoRewrite;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    // SIPAccountBase::serializeTls(out);
    out << YAML::Key << Conf::TLS_ENABLE_KEY << YAML::Value << tlsEnable;
    out << YAML::Key << Conf::TLS_PORT_KEY << YAML::Value << tlsListenerPort;
    out << YAML::Key << Conf::VERIFY_CLIENT_KEY << YAML::Value << tlsVerifyClient;
    out << YAML::Key << Conf::VERIFY_SERVER_KEY << YAML::Value << tlsVerifyServer;
    out << YAML::Key << Conf::REQUIRE_CERTIF_KEY << YAML::Value << tlsRequireClientCertificate;
    out << YAML::Key << Conf::TIMEOUT_KEY << YAML::Value << tlsNegotiationTimeout;
    out << YAML::Key << Conf::CIPHERS_KEY << YAML::Value << tlsCiphers;
    out << YAML::Key << Conf::METHOD_KEY << YAML::Value << tlsMethod;
    out << YAML::Key << Conf::SERVER_KEY << YAML::Value << tlsServerName;
    out << YAML::EndMap;

    // srtp submap
    out << YAML::Key << Conf::SRTP_KEY << YAML::Value << YAML::BeginMap;
    //out << YAML::Key << Conf::KEY_EXCHANGE_KEY << YAML::Value
    //    << sip_utils::getKeyExchangeName(srtpKeyExchange);
    out << YAML::Key << Conf::RTP_FALLBACK_KEY << YAML::Value << srtpFallback;
    out << YAML::EndMap;

    out << YAML::EndMap;}

void
SipAccountConfig::unserialize(const YAML::Node& node)
{
    SipAccountBaseConfig::unserialize(node);
    parseValueOptional(node, Conf::USERNAME_KEY, username);
    parseValueOptional(node, Conf::BIND_ADDRESS_KEY, bindAddress);

    int port = sip_utils::DEFAULT_SIP_PORT;
    parseValueOptional(node, Conf::PORT_KEY, port);
    localPort = port;

    parseValueOptional(node, Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire);
    registrationExpire = std::max(MIN_REGISTRATION_TIME, registrationExpire);
    parseValueOptional(node, Conf::KEEP_ALIVE_ENABLED, registrationRefreshEnabled);
    parseValueOptional(node, Conf::SERVICE_ROUTE_KEY, serviceRoute);
    parseValueOptional(node, Conf::ALLOW_IP_AUTO_REWRITE, allowIPAutoRewrite);

    const auto& credsNode = node[Conf::CRED_KEY];

    //parseValueOptional(node, PRESENCE_MODULE_ENABLED_KEY, presenceEnabled);
    parseValueOptional(node, Conf::PRESENCE_PUBLISH_SUPPORTED_KEY, publishSupported);
    parseValueOptional(node, Conf::PRESENCE_SUBSCRIBE_SUPPORTED_KEY, subscribeSupported);
    /*if (presence_) {
        presence_->support(PRESENCE_FUNCTION_PUBLISH, publishSupported);
        presence_->support(PRESENCE_FUNCTION_SUBSCRIBE, subscribeSupported);
    }*/

    // Init stun server name with default server name
    //stunServerName_ = pj_str((char*) stunServer_.data());

    /*const auto& credsNode = node[Conf::CRED_KEY];
    setCredentials(parseVectorMap(credsNode,
                                  {Conf::CONFIG_ACCOUNT_REALM,
                                   Conf::CONFIG_ACCOUNT_USERNAME,
                                   Conf::CONFIG_ACCOUNT_PASSWORD}));*/

    // get tls submap
    const auto& tlsMap = node[Conf::TLS_KEY];
    parseValueOptional(tlsMap, Conf::CERTIFICATE_KEY, tlsCertificateFile);
    parseValueOptional(tlsMap, Conf::CALIST_KEY, tlsCaListFile);
    parseValueOptional(tlsMap, Conf::TLS_PASSWORD_KEY, tlsPassword);
    parseValueOptional(tlsMap, Conf::PRIVATE_KEY_KEY, tlsPrivateKeyFile);
    parseValueOptional(tlsMap, Conf::TLS_ENABLE_KEY, tlsEnable);
    parseValueOptional(tlsMap, Conf::TLS_PORT_KEY, tlsListenerPort);
    parseValueOptional(tlsMap, Conf::CIPHERS_KEY, tlsCiphers);

    std::string tmpMethod(tlsMethod);
    parseValueOptional(tlsMap, Conf::METHOD_KEY, tmpMethod);
    //validate(tlsMethod_, tmpMethod, VALID_TLS_PROTOS);

    parseValueOptional(tlsMap, Conf::SERVER_KEY, tlsServerName);
    parseValueOptional(tlsMap, Conf::REQUIRE_CERTIF_KEY, tlsRequireClientCertificate);
    parseValueOptional(tlsMap, Conf::VERIFY_CLIENT_KEY, tlsVerifyClient);
    parseValueOptional(tlsMap, Conf::VERIFY_SERVER_KEY, tlsVerifyServer);
    // FIXME
    parseValueOptional(tlsMap, Conf::TIMEOUT_KEY, tlsNegotiationTimeout);

    // get srtp submap
    const auto& srtpMap = node[Conf::SRTP_KEY];
    std::string tmpKey;
    parseValueOptional(srtpMap, Conf::KEY_EXCHANGE_KEY, tmpKey);
    //srtpKeyExchange = sip_utils::getKeyExchangeProtocol(tmpKey.c_str());
    parseValueOptional(srtpMap, Conf::RTP_FALLBACK_KEY, srtpFallback);
}

std::map<std::string, std::string>
SipAccountConfig::toMap() const
{
    auto a = SipAccountBaseConfig::toMap();

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

    a.emplace(Conf::CONFIG_TLS_ENABLE, tlsEnable ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_TLS_LISTENER_PORT, std::to_string(tlsListenerPort));
    a.emplace(Conf::CONFIG_TLS_CA_LIST_FILE, tlsCaListFile);
    a.emplace(Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile);
    a.emplace(Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile);
    a.emplace(Conf::CONFIG_TLS_PASSWORD, tlsPassword);
    a.emplace(Conf::CONFIG_TLS_METHOD, tlsMethod);
    a.emplace(Conf::CONFIG_TLS_CIPHERS, tlsCiphers);
    a.emplace(Conf::CONFIG_TLS_SERVER_NAME, tlsServerName);
    a.emplace(Conf::CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, std::to_string(tlsNegotiationTimeout));

    return a;
}

void
SipAccountConfig::fromMap(const std::map<std::string, std::string>& details)
{
    SipAccountBaseConfig::fromMap(details);

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
