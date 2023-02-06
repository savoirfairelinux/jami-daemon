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
#include "sipaccount_config.h"
#include "account_const.h"
#include "account_schema.h"
#include "config/yamlparser.h"

extern "C" {
#include <pjlib-util/md5.h>
}

namespace jami {

namespace Conf {
constexpr const char* ID_KEY = "id";
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

constexpr const char* const TLS_KEY = "tls";
constexpr const char* CERTIFICATE_KEY = "certificate";
constexpr const char* CALIST_KEY = "calist";
constexpr const char* TLS_PORT_KEY = "tlsPort";
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
constexpr const char* DISABLE_SECURE_DLG_CHECK = "keepAlive";

constexpr const char* STUN_ENABLED_KEY = "stunEnabled";
constexpr const char* STUN_SERVER_KEY = "stunServer";
constexpr const char* CRED_KEY = "credential";
constexpr const char* SRTP_KEY = "srtp";
constexpr const char* SRTP_ENABLE_KEY = "enable";
constexpr const char* KEY_EXCHANGE_KEY = "keyExchange";
constexpr const char* RTP_FALLBACK_KEY = "rtpFallback";
} // namespace Conf

static const SipAccountConfig DEFAULT_CONFIG {};
static constexpr unsigned MIN_REGISTRATION_TIME = 60;                  // seconds

using yaml_utils::parseValueOptional;
using yaml_utils::parseVectorMap;

void
SipAccountConfig::serialize(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << Conf::ID_KEY << YAML::Value << id;
    SipAccountBaseConfig::serializeDiff(out, DEFAULT_CONFIG);

    out << YAML::Key << Conf::BIND_ADDRESS_KEY << YAML::Value << bindAddress;
    out << YAML::Key << Conf::PORT_KEY << YAML::Value << localPort;
    out << YAML::Key << Conf::PUBLISH_PORT_KEY << YAML::Value << publishedPort;

    out << YAML::Key << Conf::USERNAME_KEY << YAML::Value << username;

    // each credential is a map, and we can have multiple credentials
    out << YAML::Key << Conf::CRED_KEY << YAML::Value << getCredentials();

    out << YAML::Key << Conf::KEEP_ALIVE_ENABLED << YAML::Value << registrationRefreshEnabled;

    //out << YAML::Key << PRESENCE_MODULE_ENABLED_KEY << YAML::Value
    //    << (presence_ and presence_->isEnabled());

    out << YAML::Key << Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE << YAML::Value
        << registrationExpire;
    out << YAML::Key << Conf::SERVICE_ROUTE_KEY << YAML::Value << serviceRoute;
    out << YAML::Key << Conf::ALLOW_IP_AUTO_REWRITE << YAML::Value << allowIPAutoRewrite;
    out << YAML::Key << Conf::STUN_ENABLED_KEY << YAML::Value << stunEnabled;
    out << YAML::Key << Conf::STUN_SERVER_KEY << YAML::Value << stunServer;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << Conf::CALIST_KEY << YAML::Value << tlsCaListFile;
    out << YAML::Key << Conf::CERTIFICATE_KEY << YAML::Value << tlsCertificateFile;
    out << YAML::Key << Conf::TLS_PASSWORD_KEY << YAML::Value << tlsPassword;
    out << YAML::Key << Conf::PRIVATE_KEY_KEY << YAML::Value << tlsPrivateKeyFile;
    out << YAML::Key << Conf::TLS_ENABLE_KEY << YAML::Value << tlsEnable;
    out << YAML::Key << Conf::TLS_PORT_KEY << YAML::Value << tlsListenerPort;
    out << YAML::Key << Conf::VERIFY_CLIENT_KEY << YAML::Value << tlsVerifyClient;
    out << YAML::Key << Conf::VERIFY_SERVER_KEY << YAML::Value << tlsVerifyServer;
    out << YAML::Key << Conf::REQUIRE_CERTIF_KEY << YAML::Value << tlsRequireClientCertificate;
    out << YAML::Key << Conf::DISABLE_SECURE_DLG_CHECK << YAML::Value << tlsDisableSecureDlgCheck;
    out << YAML::Key << Conf::TIMEOUT_KEY << YAML::Value << tlsNegotiationTimeout;
    out << YAML::Key << Conf::CIPHERS_KEY << YAML::Value << tlsCiphers;
    out << YAML::Key << Conf::METHOD_KEY << YAML::Value << tlsMethod;
    out << YAML::Key << Conf::SERVER_KEY << YAML::Value << tlsServerName;
    out << YAML::EndMap;

    // srtp submap
    out << YAML::Key << Conf::SRTP_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << Conf::KEY_EXCHANGE_KEY << YAML::Value
        << sip_utils::getKeyExchangeName(srtpKeyExchange);
    out << YAML::Key << Conf::RTP_FALLBACK_KEY << YAML::Value << srtpFallback;
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void
SipAccountConfig::unserialize(const YAML::Node& node)
{
    SipAccountBaseConfig::unserialize(node);
    parseValueOptional(node, Conf::USERNAME_KEY, username);
    parseValueOptional(node, Conf::BIND_ADDRESS_KEY, bindAddress);
    parseValueOptional(node, Conf::PORT_KEY, localPort);
    parseValueOptional(node, Conf::PUBLISH_PORT_KEY, publishedPort);
    parseValueOptional(node, Conf::CONFIG_ACCOUNT_REGISTRATION_EXPIRE, registrationExpire);
    registrationExpire = std::max(MIN_REGISTRATION_TIME, registrationExpire);
    parseValueOptional(node, Conf::KEEP_ALIVE_ENABLED, registrationRefreshEnabled);
    parseValueOptional(node, Conf::SERVICE_ROUTE_KEY, serviceRoute);
    parseValueOptional(node, Conf::ALLOW_IP_AUTO_REWRITE, allowIPAutoRewrite);

    parseValueOptional(node, Conf::PRESENCE_MODULE_ENABLED_KEY, presenceEnabled);
    parseValueOptional(node, Conf::PRESENCE_PUBLISH_SUPPORTED_KEY, publishSupported);
    parseValueOptional(node, Conf::PRESENCE_SUBSCRIBE_SUPPORTED_KEY, subscribeSupported);

    // ICE - STUN/TURN
    parseValueOptional(node, Conf::STUN_ENABLED_KEY, stunEnabled);
    parseValueOptional(node, Conf::STUN_SERVER_KEY, stunServer);

    const auto& credsNode = node[Conf::CRED_KEY];
    setCredentials(parseVectorMap(credsNode,
                                  {Conf::CONFIG_ACCOUNT_REALM,
                                   Conf::CONFIG_ACCOUNT_USERNAME,
                                   Conf::CONFIG_ACCOUNT_PASSWORD}));

    // get tls submap
    try {
        const auto& tlsMap = node[Conf::TLS_KEY];
        parseValueOptional(tlsMap, Conf::CERTIFICATE_KEY, tlsCertificateFile);
        parseValueOptional(tlsMap, Conf::CALIST_KEY, tlsCaListFile);
        parseValueOptional(tlsMap, Conf::TLS_PASSWORD_KEY, tlsPassword);
        parseValueOptional(tlsMap, Conf::PRIVATE_KEY_KEY, tlsPrivateKeyFile);
        parseValueOptional(tlsMap, Conf::TLS_ENABLE_KEY, tlsEnable);
        parseValueOptional(tlsMap, Conf::TLS_PORT_KEY, tlsListenerPort);
        parseValueOptional(tlsMap, Conf::CIPHERS_KEY, tlsCiphers);
        parseValueOptional(tlsMap, Conf::METHOD_KEY, tlsMethod);
        parseValueOptional(tlsMap, Conf::SERVER_KEY, tlsServerName);
        parseValueOptional(tlsMap, Conf::REQUIRE_CERTIF_KEY, tlsRequireClientCertificate);
        parseValueOptional(tlsMap, Conf::VERIFY_CLIENT_KEY, tlsVerifyClient);
        parseValueOptional(tlsMap, Conf::VERIFY_SERVER_KEY, tlsVerifyServer);
        parseValueOptional(tlsMap, Conf::DISABLE_SECURE_DLG_CHECK, tlsDisableSecureDlgCheck);
        parseValueOptional(tlsMap, Conf::TIMEOUT_KEY, tlsNegotiationTimeout);
    } catch (...) {}

    // get srtp submap
    const auto& srtpMap = node[Conf::SRTP_KEY];
    std::string tmpKey;
    parseValueOptional(srtpMap, Conf::KEY_EXCHANGE_KEY, tmpKey);
    srtpKeyExchange = sip_utils::getKeyExchangeProtocol(tmpKey);
    parseValueOptional(srtpMap, Conf::RTP_FALLBACK_KEY, srtpFallback);
}

std::map<std::string, std::string>
SipAccountConfig::toMap() const
{
    auto a = SipAccountBaseConfig::toMap();
    a.emplace(Conf::CONFIG_ACCOUNT_USERNAME, username);
    a.emplace(Conf::CONFIG_LOCAL_PORT, std::to_string(localPort));
    a.emplace(Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType);
    a.emplace(Conf::CONFIG_LOCAL_INTERFACE, interface);
    a.emplace(Conf::CONFIG_PUBLISHED_PORT, std::to_string(publishedPort));
    a.emplace(Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_PUBLISHED_ADDRESS, publishedIp);
    a.emplace(Conf::CONFIG_STUN_ENABLE, stunEnabled ? TRUE_STR : FALSE_STR);
    a.emplace(Conf::CONFIG_STUN_SERVER, stunServer);

    std::string password {};
    if (not credentials.empty()) {
        for (const auto& cred : credentials)
            if (cred.username == username) {
                password = cred.password;
                break;
            }
    }
    a.emplace(Conf::CONFIG_ACCOUNT_PASSWORD, std::move(password));

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
    a.emplace(Conf::CONFIG_TLS_DISABLE_SECURE_DLG_CHECK, tlsDisableSecureDlgCheck ? TRUE_STR : FALSE_STR);
    return a;
}

void
SipAccountConfig::fromMap(const std::map<std::string, std::string>& details)
{
    SipAccountBaseConfig::fromMap(details);

    // general sip settings
    parseString(details, Conf::CONFIG_ACCOUNT_USERNAME, username);
    parseInt(details, Conf::CONFIG_LOCAL_PORT, localPort);
    parseString(details, Conf::CONFIG_BIND_ADDRESS, bindAddress);
    parseString(details, Conf::CONFIG_ACCOUNT_ROUTESET, serviceRoute);
    parseBool(details, Conf::CONFIG_ACCOUNT_IP_AUTO_REWRITE, allowIPAutoRewrite);
    parseString(details, Conf::CONFIG_LOCAL_INTERFACE, interface);
    parseBool(details, Conf::CONFIG_PUBLISHED_SAMEAS_LOCAL, publishedSameasLocal);
    parseString(details, Conf::CONFIG_PUBLISHED_ADDRESS, publishedIp);
    parseInt(details, Conf::CONFIG_PUBLISHED_PORT, publishedPort);
    parseBool(details, Conf::CONFIG_PRESENCE_ENABLED, presenceEnabled);
    parseString(details, Conf::CONFIG_ACCOUNT_DTMF_TYPE, dtmfType);

    // srtp settings
    parseBool(details, Conf::CONFIG_SRTP_RTP_FALLBACK, srtpFallback);
    auto iter = details.find(Conf::CONFIG_SRTP_KEY_EXCHANGE);
    if (iter != details.end())
        srtpKeyExchange = sip_utils::getKeyExchangeProtocol(iter->second);

    if (credentials.empty()) { // credentials not set, construct 1 entry
        JAMI_WARN("No credentials set, inferring them...");
        std::map<std::string, std::string> map;
        map[Conf::CONFIG_ACCOUNT_USERNAME] = username;
        parseString(details, Conf::CONFIG_ACCOUNT_PASSWORD, map[Conf::CONFIG_ACCOUNT_PASSWORD]);
        map[Conf::CONFIG_ACCOUNT_REALM] = "*";
        setCredentials({map});
    }

    // ICE - STUN
    parseBool(details, Conf::CONFIG_STUN_ENABLE, stunEnabled);
    parseString(details, Conf::CONFIG_STUN_SERVER, stunServer);

    // TLS
    parseBool(details, Conf::CONFIG_TLS_ENABLE, tlsEnable);
    parseInt(details, Conf::CONFIG_TLS_LISTENER_PORT, tlsListenerPort);
    parsePath(details, Conf::CONFIG_TLS_CA_LIST_FILE, tlsCaListFile, path);
    parsePath(details, Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile, path);
    parsePath(details, Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile, path);
    parseString(details, Conf::CONFIG_TLS_PASSWORD, tlsPassword);
    parseString(details, Conf::CONFIG_TLS_METHOD, tlsMethod);
    parseString(details, Conf::CONFIG_TLS_CIPHERS, tlsCiphers);
    parseString(details, Conf::CONFIG_TLS_SERVER_NAME, tlsServerName);
    parseBool(details, Conf::CONFIG_TLS_VERIFY_SERVER, tlsVerifyServer);
    parseBool(details, Conf::CONFIG_TLS_VERIFY_CLIENT, tlsVerifyClient);
    parseBool(details, Conf::CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, tlsRequireClientCertificate);
    parseBool(details, Conf::CONFIG_TLS_DISABLE_SECURE_DLG_CHECK, tlsDisableSecureDlgCheck);
    parseInt(details, Conf::CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, tlsNegotiationTimeout);
}

SipAccountConfig::Credentials::Credentials(const std::map<std::string, std::string>& cred)
{
    auto itrealm = cred.find(Conf::CONFIG_ACCOUNT_REALM);
    auto user = cred.find(Conf::CONFIG_ACCOUNT_USERNAME);
    auto passw = cred.find(Conf::CONFIG_ACCOUNT_PASSWORD);
    realm = itrealm != cred.end() ? itrealm->second : "";
    username = user != cred.end() ? user->second : "";
    password = passw != cred.end() ? passw->second : "";
    computePasswordHash();
}

std::map<std::string, std::string>
SipAccountConfig::Credentials::toMap() const
{
    return {{Conf::CONFIG_ACCOUNT_REALM, realm},
            {Conf::CONFIG_ACCOUNT_USERNAME, username},
            {Conf::CONFIG_ACCOUNT_PASSWORD, password}};
}

void
SipAccountConfig::Credentials::computePasswordHash() {
    pj_md5_context pms;

    /* Compute md5 hash = MD5(username ":" realm ":" password) */
    pj_md5_init(&pms);
    pj_md5_update(&pms, (const uint8_t*) username.data(), username.length());
    pj_md5_update(&pms, (const uint8_t*) ":", 1);
    pj_md5_update(&pms, (const uint8_t*) realm.data(), realm.length());
    pj_md5_update(&pms, (const uint8_t*) ":", 1);
    pj_md5_update(&pms, (const uint8_t*) password.data(), password.length());

    unsigned char digest[16];
    pj_md5_final(&pms, digest);

    char hash[32];

    for (int i = 0; i < 16; ++i)
        pj_val_to_hex_digit(digest[i], &hash[2 * i]);

    password_h = {hash, 32};
}

std::vector<std::map<std::string, std::string>>
SipAccountConfig::getCredentials() const
{
    std::vector<std::map<std::string, std::string>> ret;
    ret.reserve(credentials.size());
    for (const auto& c : credentials) {
        ret.emplace_back(c.toMap());
    }
    return ret;
}

void
SipAccountConfig::setCredentials(const std::vector<std::map<std::string, std::string>>& creds)
{
    credentials.clear();
    credentials.reserve(creds.size());
    for (const auto& cred : creds)
        credentials.emplace_back(cred);
}

}
