/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

namespace jami {

namespace Conf {
constexpr const char* const DHT_PORT_KEY = "dhtPort";
constexpr const char* const DHT_VALUES_PATH_KEY = "dhtValuesPath";
constexpr const char* const DHT_CONTACTS = "dhtContacts";
constexpr const char* const DHT_PUBLIC_PROFILE = "dhtPublicProfile";
constexpr const char* const DHT_PUBLIC_IN_CALLS = "dhtPublicInCalls";
constexpr const char* const DHT_ALLOW_PEERS_FROM_HISTORY = "allowPeersFromHistory";
constexpr const char* const DHT_ALLOW_PEERS_FROM_CONTACT = "allowPeersFromContact";
constexpr const char* const DHT_ALLOW_PEERS_FROM_TRUSTED = "allowPeersFromTrusted";
constexpr const char* const ETH_KEY = "ethKey";
constexpr const char* const ETH_PATH = "ethPath";
constexpr const char* const ETH_ACCOUNT = "ethAccount";
constexpr const char* const RING_CA_KEY = "ringCaKey";
constexpr const char* const RING_ACCOUNT_KEY = "ringAccountKey";
constexpr const char* const RING_ACCOUNT_CERT = "ringAccountCert";
constexpr const char* const RING_ACCOUNT_RECEIPT = "ringAccountReceipt";
constexpr const char* const RING_ACCOUNT_RECEIPT_SIG = "ringAccountReceiptSignature";
constexpr const char* const RING_ACCOUNT_CRL = "ringAccountCRL";
constexpr const char* const RING_ACCOUNT_CONTACTS = "ringAccountContacts";
constexpr const char* const PROXY_ENABLED_KEY = "proxyEnabled";
constexpr const char* const PROXY_SERVER_KEY = "proxyServer";
constexpr const char* const PROXY_PUSH_TOKEN_KEY = "proxyPushToken";

}

}
