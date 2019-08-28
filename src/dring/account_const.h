/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#ifndef DRING_ACCOUNT_H
#define DRING_ACCOUNT_H

#include "def.h"

//Defined in windows.h
#ifdef ERROR
#undef ERROR
#endif

namespace DRing {

namespace Account {

namespace ProtocolNames {

constexpr static const char SIP   [] = "SIP";
constexpr static const char IP2IP [] = "IP2IP";
constexpr static const char RING  [] = "RING";

} //namespace DRing::ProtocolNames

namespace States {

constexpr static const char REGISTERED                [] = "REGISTERED";
constexpr static const char READY                     [] = "READY";
constexpr static const char UNREGISTERED              [] = "UNREGISTERED";
constexpr static const char TRYING                    [] = "TRYING";
constexpr static const char ERROR                     [] = "ERROR";
constexpr static const char ERROR_GENERIC             [] = "ERROR_GENERIC";
constexpr static const char ERROR_AUTH                [] = "ERROR_AUTH";
constexpr static const char ERROR_NETWORK             [] = "ERROR_NETWORK";
constexpr static const char ERROR_HOST                [] = "ERROR_HOST";
constexpr static const char ERROR_CONF_STUN           [] = "ERROR_CONF_STUN";
constexpr static const char ERROR_EXIST_STUN          [] = "ERROR_EXIST_STUN";
constexpr static const char ERROR_SERVICE_UNAVAILABLE [] = "ERROR_SERVICE_UNAVAILABLE";
constexpr static const char ERROR_NOT_ACCEPTABLE      [] = "ERROR_NOT_ACCEPTABLE";
constexpr static const char ERROR_MISSING_ID          [] = "ERROR_MISSING_ID";
constexpr static const char ERROR_NEED_MIGRATION      [] = "ERROR_NEED_MIGRATION";
constexpr static const char REQUEST_TIMEOUT           [] = "Request Timeout";
constexpr static const char INITIALIZING              [] = "INITIALIZING";

} //namespace DRing::Account

enum class MessageStates : int {
    UNKNOWN = 0,
    SENDING,
    SENT,
    READ,
    FAILURE,
    CANCELLED
}; //DRing::Account::MessageStates

enum class testAccountICEInitializationStatus : int {
    SUCCESS = 0,
    FAILURE = 1
}; //DRING:Account::testAccountICEInitializationStatus

namespace VolatileProperties {

constexpr static const char ACTIVE                    [] = "Account.active";
constexpr static const char REGISTERED_NAME           [] = "Account.registredName";

// Volatile parameters
namespace Registration {

constexpr static const char STATUS                    [] = "Account.registrationStatus";
constexpr static const char STATE_CODE                [] = "Account.registrationCode";
constexpr static const char STATE_DESC                [] = "Account.registrationDescription";

} //namespace DRing::VolatileProperties::Registration

namespace Transport {

constexpr static const char STATE_CODE                [] = "Transport.statusCode";
constexpr static const char STATE_DESC                [] = "Transport.statusDescription";

} //namespace DRing::VolatileProperties::Transport

namespace InstantMessaging {

constexpr static const char OFF_CALL                 [] = "IM.offCall";

}

} //namespace DRing::Account::VolatileProperties

namespace ConfProperties {

constexpr static const char ID                      [] = "Account.id";
constexpr static const char TYPE                    [] = "Account.type";
constexpr static const char ALIAS                   [] = "Account.alias";
constexpr static const char DISPLAYNAME             [] = "Account.displayName";
constexpr static const char ENABLED                 [] = "Account.enable";
constexpr static const char MAILBOX                 [] = "Account.mailbox";
constexpr static const char DTMF_TYPE               [] = "Account.dtmfType";
constexpr static const char AUTOANSWER              [] = "Account.autoAnswer";
constexpr static const char ACTIVE_CALL_LIMIT       [] = "Account.activeCallLimit";
constexpr static const char HOSTNAME                [] = "Account.hostname";
constexpr static const char USERNAME                [] = "Account.username";
constexpr static const char BIND_ADDRESS            [] = "Account.bindAddress";
constexpr static const char ROUTE                   [] = "Account.routeset";
constexpr static const char PASSWORD                [] = "Account.password";
constexpr static const char REALM                   [] = "Account.realm";
constexpr static const char LOCAL_INTERFACE         [] = "Account.localInterface";
constexpr static const char PUBLISHED_SAMEAS_LOCAL  [] = "Account.publishedSameAsLocal";
constexpr static const char LOCAL_PORT              [] = "Account.localPort";
constexpr static const char PUBLISHED_PORT          [] = "Account.publishedPort";
constexpr static const char PUBLISHED_ADDRESS       [] = "Account.publishedAddress";
constexpr static const char USER_AGENT              [] = "Account.useragent";
constexpr static const char UPNP_ENABLED            [] = "Account.upnpEnabled";
constexpr static const char HAS_CUSTOM_USER_AGENT   [] = "Account.hasCustomUserAgent";
constexpr static const char ALLOW_CERT_FROM_HISTORY [] = "Account.allowCertFromHistory";
constexpr static const char ALLOW_CERT_FROM_CONTACT [] = "Account.allowCertFromContact";
constexpr static const char ALLOW_CERT_FROM_TRUSTED [] = "Account.allowCertFromTrusted";
constexpr static const char ARCHIVE_PASSWORD        [] = "Account.archivePassword";
constexpr static const char ARCHIVE_HAS_PASSWORD    [] = "Account.archiveHasPassword";
constexpr static const char ARCHIVE_PATH            [] = "Account.archivePath";
constexpr static const char ARCHIVE_PIN             [] = "Account.archivePIN";
constexpr static const char RING_DEVICE_ID          [] = "Account.deviceID";
constexpr static const char RING_DEVICE_NAME        [] = "Account.deviceName";
constexpr static const char PROXY_ENABLED           [] = "Account.proxyEnabled";
constexpr static const char PROXY_SERVER            [] = "Account.proxyServer";
constexpr static const char PROXY_PUSH_TOKEN        [] = "Account.proxyPushToken";
constexpr static const char DHT_PEER_DISCOVERY      [] = "Account.peerDiscovery";
constexpr static const char ACCOUNT_PEER_DISCOVERY  [] = "Account.accountDiscovery";
constexpr static const char ACCOUNT_PUBLISH         [] = "Account.accountPublish";
constexpr static const char MANAGER_HOSTNAME        [] = "Account.managerHostname";

namespace Audio {

constexpr static const char PORT_MAX           [] = "Account.audioPortMax";
constexpr static const char PORT_MIN           [] = "Account.audioPortMin";

} //namespace DRing::Account::ConfProperties::Audio

namespace Video {

constexpr static const char ENABLED            [] = "Account.videoEnabled";
constexpr static const char PORT_MAX           [] = "Account.videoPortMax";
constexpr static const char PORT_MIN           [] = "Account.videoPortMin";

} //namespace DRing::Account::ConfProperties::Video

namespace STUN {

constexpr static const char SERVER             [] = "STUN.server";
constexpr static const char ENABLED            [] = "STUN.enable";

} //namespace DRing::Account::ConfProperties::STUN

namespace TURN {

constexpr static const char SERVER             [] = "TURN.server";
constexpr static const char ENABLED            [] = "TURN.enable";
constexpr static const char SERVER_UNAME       [] = "TURN.username";
constexpr static const char SERVER_PWD         [] = "TURN.password";
constexpr static const char SERVER_REALM       [] = "TURN.realm";

} //namespace DRing::Account::ConfProperties::TURN

namespace Presence {

constexpr static const char SUPPORT_PUBLISH    [] = "Account.presencePublishSupported";
constexpr static const char SUPPORT_SUBSCRIBE  [] = "Account.presenceSubscribeSupported";
constexpr static const char ENABLED            [] = "Account.presenceEnabled";

} //namespace DRing::Account::ConfProperties::Presence

namespace Registration {

constexpr static const char EXPIRE             [] = "Account.registrationExpire";
constexpr static const char STATUS             [] = "Account.registrationStatus";

} //namespace DRing::Account::ConfProperties::Registration

namespace Ringtone {

constexpr static const char PATH               [] = "Account.ringtonePath";
constexpr static const char ENABLED            [] = "Account.ringtoneEnabled";

} //namespace DRing::Account::ConfProperties::Ringtone

namespace SRTP {

constexpr static const char KEY_EXCHANGE       [] = "SRTP.keyExchange";
constexpr static const char ENABLED            [] = "SRTP.enable";
constexpr static const char RTP_FALLBACK       [] = "SRTP.rtpFallback";

} //namespace DRing::Account::ConfProperties::SRTP

namespace TLS {

constexpr static const char LISTENER_PORT      [] = "TLS.listenerPort";
constexpr static const char ENABLED            [] = "TLS.enable";
constexpr static const char PORT               [] = "TLS.port";
constexpr static const char CA_LIST_FILE       [] = "TLS.certificateListFile";
constexpr static const char CERTIFICATE_FILE   [] = "TLS.certificateFile";
constexpr static const char PRIVATE_KEY_FILE   [] = "TLS.privateKeyFile";
constexpr static const char PASSWORD           [] = "TLS.password";
constexpr static const char METHOD             [] = "TLS.method";
constexpr static const char CIPHERS            [] = "TLS.ciphers";
constexpr static const char SERVER_NAME        [] = "TLS.serverName";
constexpr static const char VERIFY_SERVER      [] = "TLS.verifyServer";
constexpr static const char VERIFY_CLIENT      [] = "TLS.verifyClient";
constexpr static const char REQUIRE_CLIENT_CERTIFICATE [] = "TLS.requireClientCertificate";
constexpr static const char NEGOTIATION_TIMEOUT_SEC    [] = "TLS.negotiationTimeoutSec";

} //namespace DRing::Account::ConfProperties::TLS

namespace DHT {

constexpr static const char PORT               [] = "DHT.port";
constexpr static const char PUBLIC_IN_CALLS    [] = "DHT.PublicInCalls";
constexpr static const char ALLOW_FROM_TRUSTED [] = "DHT.AllowFromTrusted";

} //namespace DRing::Account::DHT

namespace RingNS {

constexpr static const char URI         [] = "RingNS.uri";
constexpr static const char ACCOUNT     [] = "RingNS.account";

} //namespace DRing::Account::RingNS

namespace CodecInfo {

constexpr static const char NAME               [] = "CodecInfo.name";
constexpr static const char TYPE               [] = "CodecInfo.type";
constexpr static const char SAMPLE_RATE        [] = "CodecInfo.sampleRate";
constexpr static const char FRAME_RATE         [] = "CodecInfo.frameRate";
constexpr static const char BITRATE            [] = "CodecInfo.bitrate";
constexpr static const char MIN_BITRATE        [] = "CodecInfo.min_bitrate";
constexpr static const char MAX_BITRATE        [] = "CodecInfo.max_bitrate";
constexpr static const char QUALITY            [] = "CodecInfo.quality";
constexpr static const char MIN_QUALITY        [] = "CodecInfo.min_quality";
constexpr static const char MAX_QUALITY        [] = "CodecInfo.max_quality";
constexpr static const char CHANNEL_NUMBER     [] = "CodecInfo.channelNumber";
constexpr static const char AUTO_QUALITY_ENABLED [] = "CodecInfo.autoQualityEnabled";

} //namespace DRing::Account::ConfProperties::CodecInfo

} //namespace DRing::Account::ConfProperties

namespace TrustRequest {

constexpr static const char FROM     [] = "from";
constexpr static const char RECEIVED [] = "received";
constexpr static const char PAYLOAD  [] = "payload";

} //namespace DRing::Account::TrustRequest

} //namespace DRing::Account

} //namespace DRing

#endif
