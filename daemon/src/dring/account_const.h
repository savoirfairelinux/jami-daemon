/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  This program is free software you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program if not, write to the Free Software
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
#ifndef DRING_ACCOUNT_H
#define DRING_ACCOUNT_H

namespace DRing {

namespace Account {

namespace States {

constexpr static const char REGISTERED                [] = "REGISTERED"             ;
constexpr static const char READY                     [] = "READY"                  ;
constexpr static const char UNREGISTERED              [] = "UNREGISTERED"           ;
constexpr static const char TRYING                    [] = "TRYING"                 ;
constexpr static const char ERROR                     [] = "ERROR"                  ;
constexpr static const char ERROR_AUTH                [] = "ERRORAUTH"              ;
constexpr static const char ERROR_NETWORK             [] = "ERRORNETWORK"           ;
constexpr static const char ERROR_HOST                [] = "ERRORHOST"              ;
constexpr static const char ERROR_CONF_STUN           [] = "ERROR_CONF_STUN"        ;
constexpr static const char ERROR_EXIST_STUN          [] = "ERROREXISTSTUN"         ;
constexpr static const char ERROR_SERVICE_UNAVAILABLE [] = "ERRORSERVICEUNAVAILABLE";
constexpr static const char ERROR_NOT_ACCEPTABLE      [] = "ERRORNOTACCEPTABLE"     ;
constexpr static const char REQUEST_TIMEOUT           [] = "Request Timeout"        ;

} //namespace DRing::Account

namespace VolatileProperties {

// Volatile parameters
namespace Registration {

constexpr static const char STATUS                    [] = "Account.registrationStatus"     ;
constexpr static const char STATE_CODE                [] = "Account.registrationCode"       ;
constexpr static const char STATE_DESC                [] = "Account.registrationDescription";

} //namespace DRing::VolatileProperties::Registration

namespace Transport {

constexpr static const char STATE_CODE                [] = "Transport.statusCode"           ;
constexpr static const char STATE_DESC                [] = "Transport.statusDescription"    ;

} //namespace DRing::VolatileProperties::Transport

} //namespace DRing::Account::VolatileProperties

namespace ConfProperties {

constexpr static const char ID                    [] = "Account.id"                         ;
constexpr static const char TYPE                  [] = "Account.type"                       ;
constexpr static const char ALIAS                 [] = "Account.alias"                      ;
constexpr static const char ENABLED               [] = "Account.enable"                     ;
constexpr static const char MAILBOX               [] = "Account.mailbox"                    ;
constexpr static const char DTMF_TYPE             [] = "Account.dtmfType"                   ;
constexpr static const char AUTOANSWER            [] = "Account.autoAnswer"                 ;
constexpr static const char HOSTNAME              [] = "Account.hostname"                   ;
constexpr static const char USERNAME              [] = "Account.username"                   ;
constexpr static const char ROUTE                 [] = "Account.routeset"                   ;
constexpr static const char PASSWORD              [] = "Account.password"                   ;
constexpr static const char REALM                 [] = "Account.realm"                      ;
constexpr static const char LOCAL_INTERFACE       [] = "Account.localInterface"             ;
constexpr static const char PUBLISHED_SAMEAS_LOCAL[] = "Account.publishedSameAsLocal"       ;
constexpr static const char LOCAL_PORT            [] = "Account.localPort"                  ;
constexpr static const char PUBLISHED_PORT        [] = "Account.publishedPort"              ;
constexpr static const char PUBLISHED_ADDRESS     [] = "Account.publishedAddress"           ;
constexpr static const char USER_AGENT            [] = "Account.useragent"                  ;


namespace Audio {

constexpr static const char PORT_MAX           [] = "Account.audioPortMax"               ;
constexpr static const char PORT_MIN           [] = "Account.audioPortMin"               ;

} //namespace DRing::Account::ConfProperties::Audio

namespace Video {

constexpr static const char ENABLED            [] = "Account.videoEnabled"               ;
constexpr static const char PORT_MAX           [] = "Account.videoPortMax"               ;
constexpr static const char PORT_MIN           [] = "Account.videoPortMin"               ;

} //namespace DRing::Account::ConfProperties::Video

namespace STUN {

constexpr static const char SERVER             [] = "STUN.server"                        ;
constexpr static const char ENABLED            [] = "STUN.enable"                        ;

} //namespace DRing::Account::ConfProperties::STUN

namespace Presence {

constexpr static const char SUPPORT_PUBLISH    [] = "Account.presencePublishSupported"   ;
constexpr static const char SUPPORT_SUBSCRIBE  [] = "Account.presenceSubscribeSupported" ;
constexpr static const char ENABLED            [] = "Account.presenceEnabled"            ;

} //namespace DRing::Account::ConfProperties::Presence

namespace Registration {

constexpr static const char EXPIRE             [] = "Account.registrationExpire"         ;
constexpr static const char STATUS             [] = "Account.registrationStatus"         ;

} //namespace DRing::Account::ConfProperties::Registration

namespace Ringtone {

constexpr static const char PATH               [] = "Account.ringtonePath"               ;
constexpr static const char ENABLED            [] = "Account.ringtoneEnabled"            ;

} //namespace DRing::Account::ConfProperties::Ringtone

namespace SRTP {

constexpr static const char KEY_EXCHANGE       [] = "SRTP.keyExchange"                   ;
constexpr static const char ENABLED            [] = "SRTP.enable"                        ;
constexpr static const char RTP_FALLBACK       [] = "SRTP.rtpFallback"                   ;

} //namespace DRing::Account::ConfProperties::SRTP


namespace ZRTP {

constexpr static const char DISPLAY_SAS        [] = "ZRTP.displaySAS"                    ;
constexpr static const char NOT_SUPP_WARNING   [] = "ZRTP.notSuppWarning"                ;
constexpr static const char HELLO_HASH         [] = "ZRTP.helloHashEnable"               ;
constexpr static const char DISPLAY_SAS_ONCE   [] = "ZRTP.displaySasOnce"                ;

} //namespace DRing::Account::ConfProperties::ZRTP

namespace TLS {

constexpr static const char LISTENER_PORT      [] = "TLS.listenerPort"                   ;
constexpr static const char ENABLED            [] = "TLS.enable"                         ;
constexpr static const char PORT               [] = "TLS.port"                           ;
constexpr static const char CA_LIST_FILE       [] = "TLS.certificateListFile"            ;
constexpr static const char CERTIFICATE_FILE   [] = "TLS.certificateFile"                ;
constexpr static const char PRIVATE_KEY_FILE   [] = "TLS.privateKeyFile"                 ;
constexpr static const char PASSWORD           [] = "TLS.password"                       ;
constexpr static const char METHOD             [] = "TLS.method"                         ;
constexpr static const char CIPHERS            [] = "TLS.ciphers"                        ;
constexpr static const char SERVER_NAME        [] = "TLS.serverName"                     ;
constexpr static const char VERIFY_SERVER      [] = "TLS.verifyServer"                   ;
constexpr static const char VERIFY_CLIENT      [] = "TLS.verifyClient"                   ;
constexpr static const char REQUIRE_CLIENT_CERTIFICATE [] = "TLS.requireClientCertificate";
constexpr static const char NEGOTIATION_TIMEOUT_SEC    [] = "TLS.negotiationTimeoutSec"   ;

} //namespace DRing::Account::ConfProperties::TLS

namespace DHT {

constexpr static const char PORT               [] = "DHT.port"                           ;
constexpr static const char PRIVATE_PATH       [] = "DHT.privkeyPath"                    ;
constexpr static const char CERT_PATH          [] = "DHT.certificatePath"                ;

} //namespace DRing::Account::DHT

} //namespace DRing::Account::ConfProperties

} //namespace DRing::Account

} //namespace DRing

#endif
