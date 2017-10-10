/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
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

#ifndef ACCOUNT_SCHEMA_H_
#define ACCOUNT_SCHEMA_H_

/**
 * @file account_schema.h
 * @brief Account specfic keys/constants that must be shared in daemon and clients.
 */

namespace ring { namespace Conf {

// Common account parameters
static constexpr const char* CONFIG_ACCOUNT_TYPE                    = "Account.type";
static constexpr const char* CONFIG_ACCOUNT_ALIAS                   = "Account.alias";
static constexpr const char* CONFIG_ACCOUNT_DISPLAYNAME             = "Account.displayName";
static constexpr const char* CONFIG_ACCOUNT_MAILBOX                 = "Account.mailbox";
static constexpr const char* CONFIG_ACCOUNT_ENABLE                  = "Account.enable";
static constexpr const char* CONFIG_ACCOUNT_AUTOANSWER              = "Account.autoAnswer";
static constexpr const char* CONFIG_ACCOUNT_REGISTRATION_EXPIRE     = "Account.registrationExpire";
static constexpr const char* CONFIG_ACCOUNT_DTMF_TYPE               = "Account.dtmfType";
static constexpr const char* CONFIG_RINGTONE_PATH                   = "Account.ringtonePath";
static constexpr const char* CONFIG_RINGTONE_ENABLED                = "Account.ringtoneEnabled";
static constexpr const char* CONFIG_VIDEO_ENABLED                   = "Account.videoEnabled";
static constexpr const char* CONFIG_KEEP_ALIVE_ENABLED              = "Account.keepAliveEnabled";
static constexpr const char* CONFIG_PRESENCE_ENABLED                = "Account.presenceEnabled";
static constexpr const char* CONFIG_PRESENCE_PUBLISH_SUPPORTED      = "Account.presencePublishSupported";
static constexpr const char* CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED    = "Account.presenceSubscribeSupported";
static constexpr const char* CONFIG_PRESENCE_STATUS                 = "Account.presenceStatus";
static constexpr const char* CONFIG_PRESENCE_NOTE                   = "Account.presenceNote";

static constexpr const char* CONFIG_ACCOUNT_HOSTNAME                = "Account.hostname";
static constexpr const char* CONFIG_ACCOUNT_USERNAME                = "Account.username";
static constexpr const char* CONFIG_ACCOUNT_ROUTESET                = "Account.routeset";
static constexpr const char* CONFIG_ACCOUNT_PASSWORD                = "Account.password";
static constexpr const char* CONFIG_ACCOUNT_REALM                   = "Account.realm";
static constexpr const char* CONFIG_ACCOUNT_USERAGENT               = "Account.useragent";
static constexpr const char* CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT    = "Account.hasCustomUserAgent";
static constexpr const char* CONFIG_ACCOUNT_AUDIO_PORT_MIN          = "Account.audioPortMin";
static constexpr const char* CONFIG_ACCOUNT_AUDIO_PORT_MAX          = "Account.audioPortMax";
static constexpr const char* CONFIG_ACCOUNT_VIDEO_PORT_MIN          = "Account.videoPortMin";
static constexpr const char* CONFIG_ACCOUNT_VIDEO_PORT_MAX          = "Account.videoPortMax";

static constexpr const char* CONFIG_LOCAL_INTERFACE                 = "Account.localInterface";
static constexpr const char* CONFIG_PUBLISHED_SAMEAS_LOCAL          = "Account.publishedSameAsLocal";
static constexpr const char* CONFIG_LOCAL_PORT                      = "Account.localPort";
static constexpr const char* CONFIG_PUBLISHED_PORT                  = "Account.publishedPort";
static constexpr const char* CONFIG_PUBLISHED_ADDRESS               = "Account.publishedAddress";
static constexpr const char* CONFIG_UPNP_ENABLED                    = "Account.upnpEnabled";

// SIP specific parameters
static constexpr const char* CONFIG_STUN_SERVER                     = "STUN.server";
static constexpr const char* CONFIG_STUN_ENABLE                     = "STUN.enable";
static constexpr const char* CONFIG_TURN_SERVER                     = "TURN.server";
static constexpr const char* CONFIG_TURN_ENABLE                     = "TURN.enable";
static constexpr const char* CONFIG_TURN_SERVER_UNAME               = "TURN.username";
static constexpr const char* CONFIG_TURN_SERVER_PWD                 = "TURN.password";
static constexpr const char* CONFIG_TURN_SERVER_REALM               = "TURN.realm";

// SRTP specific parameters
static constexpr const char* CONFIG_SRTP_ENABLE                     = "SRTP.enable";
static constexpr const char* CONFIG_SRTP_KEY_EXCHANGE               = "SRTP.keyExchange";
static constexpr const char* CONFIG_SRTP_RTP_FALLBACK               = "SRTP.rtpFallback";

static constexpr const char* CONFIG_TLS_LISTENER_PORT               = "TLS.listenerPort";
static constexpr const char* CONFIG_TLS_ENABLE                      = "TLS.enable";
static constexpr const char* CONFIG_TLS_CA_LIST_FILE                = "TLS.certificateListFile";
static constexpr const char* CONFIG_TLS_CERTIFICATE_FILE            = "TLS.certificateFile";
static constexpr const char* CONFIG_TLS_PRIVATE_KEY_FILE            = "TLS.privateKeyFile";
static constexpr const char* CONFIG_TLS_PASSWORD                    = "TLS.password";
static constexpr const char* CONFIG_TLS_METHOD                      = "TLS.method";
static constexpr const char* CONFIG_TLS_CIPHERS                     = "TLS.ciphers";
static constexpr const char* CONFIG_TLS_SERVER_NAME                 = "TLS.serverName";
static constexpr const char* CONFIG_TLS_VERIFY_SERVER               = "TLS.verifyServer";
static constexpr const char* CONFIG_TLS_VERIFY_CLIENT               = "TLS.verifyClient";
static constexpr const char* CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE  = "TLS.requireClientCertificate";
static constexpr const char* CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC     = "TLS.negotiationTimeoutSec";

// DHT specific parameters
static constexpr const char* CONFIG_DHT_PORT                        = "DHT.port";
static constexpr const char* CONFIG_DHT_PUBLIC_IN_CALLS             = "DHT.PublicInCalls";

// Volatile parameters
static constexpr const char* CONFIG_ACCOUNT_REGISTRATION_STATUS     = "Account.registrationStatus";
static constexpr const char* CONFIG_ACCOUNT_REGISTRATION_STATE_CODE = "Account.registrationCode";
static constexpr const char* CONFIG_ACCOUNT_REGISTRATION_STATE_DESC = "Account.registrationDescription";
static constexpr const char* CONFIG_TRANSPORT_STATE_CODE            = "Transport.statusCode";
static constexpr const char* CONFIG_TRANSPORT_STATE_DESC            = "Transport.statusDescription";

}} // namespace ring::Conf

#endif // ACCOUNT_SCHEMA_H_
