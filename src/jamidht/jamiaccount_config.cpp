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
#include "jamiaccount_config.h"
#include "account_const.h"
#include "account_schema.h"
#include "configkeys.h"
#include "config/yamlparser.h"
#include "fileutils.h"

namespace jami {

namespace Conf {
constexpr const char* const TLS_KEY = "tls";
constexpr const char* CERTIFICATE_KEY = "certificate";
constexpr const char* CALIST_KEY = "calist";
const char* const TLS_PASSWORD_KEY = "password";
const char* const PRIVATE_KEY_KEY = "privateKey";
}

void
JamiAccountConfig::serialize(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    SipAccountBaseConfig::serialize(out);
    out << YAML::Key << Conf::DHT_PORT_KEY << YAML::Value << dhtPort;
    out << YAML::Key << Conf::DHT_PUBLIC_IN_CALLS << YAML::Value << allowPublicIncoming;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_HISTORY << YAML::Value << allowPeersFromHistory;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_CONTACT << YAML::Value << allowPeersFromContact;
    out << YAML::Key << Conf::DHT_ALLOW_PEERS_FROM_TRUSTED << YAML::Value << allowPeersFromTrusted;
    out << YAML::Key << libjami::Account::ConfProperties::DHT_PEER_DISCOVERY << YAML::Value
        << dhtPeerDiscovery;
    out << YAML::Key << libjami::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY << YAML::Value
        << accountPeerDiscovery;
    out << YAML::Key << libjami::Account::ConfProperties::ACCOUNT_PUBLISH << YAML::Value
        << accountPublish;

    out << YAML::Key << Conf::PROXY_ENABLED_KEY << YAML::Value << proxyEnabled;
    out << YAML::Key << Conf::PROXY_SERVER_KEY << YAML::Value << proxyServer;
    out << YAML::Key << libjami::Account::ConfProperties::DHT_PROXY_LIST_URL << YAML::Value
        << proxyListUrl;

#if HAVE_RINGNS
    out << YAML::Key << libjami::Account::ConfProperties::RingNS::URI << YAML::Value << nameServer;
    if (not registeredName.empty())
        out << YAML::Key << libjami::Account::VolatileProperties::REGISTERED_NAME << YAML::Value
            << registeredName;
#endif

    out << YAML::Key << libjami::Account::ConfProperties::ARCHIVE_PATH << YAML::Value << fileutils::getCleanPath(path, archivePath);
    out << YAML::Key << libjami::Account::ConfProperties::ARCHIVE_HAS_PASSWORD << YAML::Value
        << archiveHasPassword;
    out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT << YAML::Value << receipt;
    if (receiptSignature.size() > 0)
        out << YAML::Key << Conf::RING_ACCOUNT_RECEIPT_SIG << YAML::Value
            << YAML::Binary(receiptSignature.data(), receiptSignature.size());
    out << YAML::Key << libjami::Account::ConfProperties::DEVICE_NAME << YAML::Value << deviceName;
    out << YAML::Key << libjami::Account::ConfProperties::MANAGER_URI << YAML::Value << managerUri;
    out << YAML::Key << libjami::Account::ConfProperties::MANAGER_USERNAME << YAML::Value
        << managerUsername;

    // tls submap
    out << YAML::Key << Conf::TLS_KEY << YAML::Value << YAML::BeginMap;
    out << YAML::Key << Conf::CALIST_KEY << YAML::Value << fileutils::getCleanPath(path, tlsCaListFile);
    out << YAML::Key << Conf::CERTIFICATE_KEY << YAML::Value << fileutils::getCleanPath(path, tlsCertificateFile);;
    out << YAML::Key << Conf::TLS_PASSWORD_KEY << YAML::Value << tlsPassword;
    out << YAML::Key << Conf::PRIVATE_KEY_KEY << YAML::Value << fileutils::getCleanPath(path, tlsPrivateKeyFile);;
    out << YAML::EndMap;

    out << YAML::EndMap;
}

void
JamiAccountConfig::unserialize(const YAML::Node& node)
{
    using yaml_utils::parseValueOptional;
    using yaml_utils::parsePath;
    using yaml_utils::parsePathOptional;
    SipAccountBaseConfig::unserialize(node);

    // get tls submap
    try {
        const auto& tlsMap = node[Conf::TLS_KEY];
        parsePathOptional(tlsMap, Conf::CERTIFICATE_KEY, tlsCertificateFile, path);
        parsePathOptional(tlsMap, Conf::CALIST_KEY, tlsCaListFile, path);
        parseValueOptional(tlsMap, Conf::TLS_PASSWORD_KEY, tlsPassword);
        parsePathOptional(tlsMap, Conf::PRIVATE_KEY_KEY, tlsPrivateKeyFile, path);
    } catch (...) {

    }
    parseValueOptional(node, Conf::DHT_ALLOW_PEERS_FROM_HISTORY, allowPeersFromHistory);
    parseValueOptional(node, Conf::DHT_ALLOW_PEERS_FROM_CONTACT, allowPeersFromContact);
    parseValueOptional(node, Conf::DHT_ALLOW_PEERS_FROM_TRUSTED, allowPeersFromTrusted);

    parseValueOptional(node, Conf::PROXY_ENABLED_KEY, proxyEnabled);
    parseValueOptional(node, Conf::PROXY_SERVER_KEY, proxyServer);
    parseValueOptional(node, libjami::Account::ConfProperties::DHT_PROXY_LIST_URL, proxyListUrl);

    parseValueOptional(node, libjami::Account::ConfProperties::DEVICE_NAME, deviceName);
    parseValueOptional(node, libjami::Account::ConfProperties::MANAGER_URI, managerUri);
    parseValueOptional(node, libjami::Account::ConfProperties::MANAGER_USERNAME, managerUsername);

    parseValueOptional(node, libjami::Account::ConfProperties::ARCHIVE_PATH, archivePath);
    parseValueOptional(node, libjami::Account::ConfProperties::ARCHIVE_HAS_PASSWORD, archiveHasPassword);

    try {
        parseValueOptional(node, Conf::RING_ACCOUNT_RECEIPT, receipt);
        auto receipt_sig = node[Conf::RING_ACCOUNT_RECEIPT_SIG].as<YAML::Binary>();
        receiptSignature = {receipt_sig.data(), receipt_sig.data() + receipt_sig.size()};
    } catch (const std::exception& e) {
        JAMI_WARN("can't read receipt: %s", e.what());
    }

    parseValueOptional(node, libjami::Account::ConfProperties::DHT_PEER_DISCOVERY, dhtPeerDiscovery);
    parseValueOptional(node,
                       libjami::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY,
                       accountPeerDiscovery);
    parseValueOptional(node, libjami::Account::ConfProperties::ACCOUNT_PUBLISH, accountPublish);
    parseValueOptional(node, libjami::Account::ConfProperties::RingNS::URI, nameServer);
    parseValueOptional(node, libjami::Account::VolatileProperties::REGISTERED_NAME, registeredName);
    parseValueOptional(node, Conf::DHT_PUBLIC_IN_CALLS, allowPublicIncoming);
}

std::map<std::string, std::string>
JamiAccountConfig::toMap() const
{
    std::map<std::string, std::string> a = SipAccountBaseConfig::toMap();
    a.emplace(Conf::CONFIG_DHT_PORT, std::to_string(dhtPort));
    a.emplace(Conf::CONFIG_DHT_PUBLIC_IN_CALLS, allowPublicIncoming ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::DHT_PEER_DISCOVERY,
              dhtPeerDiscovery ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY,
              accountPeerDiscovery ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::ACCOUNT_PUBLISH,
              accountPublish ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::DEVICE_NAME, deviceName);
    a.emplace(libjami::Account::ConfProperties::Presence::SUPPORT_SUBSCRIBE, TRUE_STR);
    if (not archivePath.empty() or not managerUri.empty())
        a.emplace(libjami::Account::ConfProperties::ARCHIVE_HAS_PASSWORD,
                  archiveHasPassword ? TRUE_STR : FALSE_STR);

    a.emplace(Conf::CONFIG_TLS_CA_LIST_FILE, fileutils::getFullPath(path, tlsCaListFile));
    a.emplace(Conf::CONFIG_TLS_CERTIFICATE_FILE, fileutils::getFullPath(path, tlsCertificateFile));
    a.emplace(Conf::CONFIG_TLS_PRIVATE_KEY_FILE, fileutils::getFullPath(path, tlsPrivateKeyFile));
    a.emplace(Conf::CONFIG_TLS_PASSWORD, tlsPassword);
    a.emplace(libjami::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY,
              allowPeersFromHistory ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT,
              allowPeersFromContact ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED,
              allowPeersFromTrusted ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::PROXY_ENABLED, proxyEnabled ? TRUE_STR : FALSE_STR);
    a.emplace(libjami::Account::ConfProperties::PROXY_SERVER, proxyServer);
    a.emplace(libjami::Account::ConfProperties::DHT_PROXY_LIST_URL, proxyListUrl);
    a.emplace(libjami::Account::ConfProperties::MANAGER_URI, managerUri);
    a.emplace(libjami::Account::ConfProperties::MANAGER_USERNAME, managerUsername);
#if HAVE_RINGNS
    a.emplace(libjami::Account::ConfProperties::RingNS::URI, nameServer);
#endif
    return a;
}

void
JamiAccountConfig::fromMap(const std::map<std::string, std::string>& details)
{
    SipAccountBaseConfig::fromMap(details);
    // TLS
    parsePath(details, Conf::CONFIG_TLS_CA_LIST_FILE, tlsCaListFile, path);
    parsePath(details, Conf::CONFIG_TLS_CERTIFICATE_FILE, tlsCertificateFile, path);
    parsePath(details, Conf::CONFIG_TLS_PRIVATE_KEY_FILE, tlsPrivateKeyFile, path);
    parseString(details, Conf::CONFIG_TLS_PASSWORD, tlsPassword);

    if (hostname.empty())
        hostname = DHT_DEFAULT_BOOTSTRAP;
    parseString(details, libjami::Account::ConfProperties::BOOTSTRAP_LIST_URL, bootstrapListUrl);
    parseInt(details, Conf::CONFIG_DHT_PORT, dhtPort);
    parseBool(details, Conf::CONFIG_DHT_PUBLIC_IN_CALLS, allowPublicIncoming);
    parseBool(details, libjami::Account::ConfProperties::DHT_PEER_DISCOVERY, dhtPeerDiscovery);
    parseBool(details,
              libjami::Account::ConfProperties::ACCOUNT_PEER_DISCOVERY,
              accountPeerDiscovery);
    parseBool(details, libjami::Account::ConfProperties::ACCOUNT_PUBLISH, accountPublish);
    parseBool(details,
              libjami::Account::ConfProperties::ALLOW_CERT_FROM_HISTORY,
              allowPeersFromHistory);
    parseBool(details,
              libjami::Account::ConfProperties::ALLOW_CERT_FROM_CONTACT,
              allowPeersFromContact);
    parseBool(details,
              libjami::Account::ConfProperties::ALLOW_CERT_FROM_TRUSTED,
              allowPeersFromTrusted);

    parseString(details, libjami::Account::ConfProperties::MANAGER_URI, managerUri);
    parseString(details, libjami::Account::ConfProperties::MANAGER_USERNAME, managerUsername);
    //parseString(details, libjami::Account::ConfProperties::USERNAME, username);

    parseString(details, libjami::Account::ConfProperties::ARCHIVE_PASSWORD, archive_password);
    parseString(details, libjami::Account::ConfProperties::ARCHIVE_PIN, archive_pin);
    std::transform(archive_pin.begin(), archive_pin.end(), archive_pin.begin(), ::toupper);
    parseString(details, libjami::Account::ConfProperties::ARCHIVE_PATH, archive_path);
    parseString(details, libjami::Account::ConfProperties::DEVICE_NAME, deviceName);

    auto oldProxyServer = proxyServer, oldProxyServerList = proxyListUrl;
    parseString(details, libjami::Account::ConfProperties::DHT_PROXY_LIST_URL, proxyListUrl);
    parseBool(details, libjami::Account::ConfProperties::PROXY_ENABLED, proxyEnabled);
    parseString(details, libjami::Account::ConfProperties::PROXY_SERVER, proxyServer);
    if (not managerUri.empty() and managerUri.rfind("http", 0) != 0) {
        managerUri = "https://" + managerUri;
    }

#if HAVE_RINGNS
    parseString(details, libjami::Account::ConfProperties::RingNS::URI, nameServer);
#endif

}


}
