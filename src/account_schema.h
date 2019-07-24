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

#ifndef ACCOUNT_SCHEMA_H_
#define ACCOUNT_SCHEMA_H_

/**
 * @file account_schema.h
 * @brief Account specific keys/constants that must be shared in daemon and clients.
 */

namespace jami { namespace Conf {

// Common account parameters
static const char *const CONFIG_ACCOUNT_TYPE                    = "Account.type";
static const char *const CONFIG_ACCOUNT_ALIAS                   = "Account.alias";
static const char *const CONFIG_ACCOUNT_DISPLAYNAME             = "Account.displayName";
static const char *const CONFIG_ACCOUNT_MAILBOX                 = "Account.mailbox";
static const char *const CONFIG_ACCOUNT_ENABLE                  = "Account.enable";
static const char *const CONFIG_ACCOUNT_AUTOANSWER              = "Account.autoAnswer";
static const char *const CONFIG_ACCOUNT_REGISTRATION_EXPIRE     = "Account.registrationExpire";
static const char *const CONFIG_ACCOUNT_DTMF_TYPE               = "Account.dtmfType";
static const char *const CONFIG_RINGTONE_PATH                   = "Account.ringtonePath";
static const char *const CONFIG_RINGTONE_ENABLED                = "Account.ringtoneEnabled";
static const char *const CONFIG_VIDEO_ENABLED                   = "Account.videoEnabled";
static const char *const CONFIG_KEEP_ALIVE_ENABLED              = "Account.keepAliveEnabled";
static const char *const CONFIG_PRESENCE_ENABLED                = "Account.presenceEnabled";
static const char *const CONFIG_PRESENCE_PUBLISH_SUPPORTED      = "Account.presencePublishSupported";
static const char *const CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED    = "Account.presenceSubscribeSupported";
static const char *const CONFIG_PRESENCE_STATUS                 = "Account.presenceStatus";
static const char *const CONFIG_PRESENCE_NOTE                   = "Account.presenceNote";

static const char *const CONFIG_ACCOUNT_HOSTNAME                = "Account.hostname";
static const char *const CONFIG_ACCOUNT_USERNAME                = "Account.username";
static const char *const CONFIG_ACCOUNT_ROUTESET                = "Account.routeset";
static const char *const CONFIG_ACCOUNT_PASSWORD                = "Account.password";
static const char *const CONFIG_ACCOUNT_REALM                   = "Account.realm";
static const char *const CONFIG_ACCOUNT_USERAGENT               = "Account.useragent";
static const char *const CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT    = "Account.hasCustomUserAgent";
static const char *const CONFIG_ACCOUNT_AUDIO_PORT_MIN          = "Account.audioPortMin";
static const char *const CONFIG_ACCOUNT_AUDIO_PORT_MAX          = "Account.audioPortMax";
static const char *const CONFIG_ACCOUNT_VIDEO_PORT_MIN          = "Account.videoPortMin";
static const char *const CONFIG_ACCOUNT_VIDEO_PORT_MAX          = "Account.videoPortMax";

static const char *const CONFIG_BIND_ADDRESS                    = "Account.bindAddress";
static const char *const CONFIG_LOCAL_INTERFACE                 = "Account.localInterface";
static const char *const CONFIG_PUBLISHED_SAMEAS_LOCAL          = "Account.publishedSameAsLocal";
static const char *const CONFIG_LOCAL_PORT                      = "Account.localPort";
static const char *const CONFIG_PUBLISHED_PORT                  = "Account.publishedPort";
static const char *const CONFIG_PUBLISHED_ADDRESS               = "Account.publishedAddress";
static const char *const CONFIG_UPNP_ENABLED                    = "Account.upnpEnabled";

// SIP specific parameters
static const char *const CONFIG_STUN_SERVER                     = "STUN.server";
static const char *const CONFIG_STUN_ENABLE                     = "STUN.enable";
static const char *const CONFIG_TURN_SERVER                     = "TURN.server";
static const char *const CONFIG_TURN_ENABLE                     = "TURN.enable";
static const char *const CONFIG_TURN_SERVER_UNAME               = "TURN.username";
static const char *const CONFIG_TURN_SERVER_PWD                 = "TURN.password";
static const char *const CONFIG_TURN_SERVER_REALM               = "TURN.realm";

// SRTP specific parameters
static const char *const CONFIG_SRTP_ENABLE                     = "SRTP.enable";
static const char *const CONFIG_SRTP_KEY_EXCHANGE               = "SRTP.keyExchange";
static const char *const CONFIG_SRTP_RTP_FALLBACK               = "SRTP.rtpFallback";

static const char *const CONFIG_TLS_LISTENER_PORT               = "TLS.listenerPort";
static const char *const CONFIG_TLS_ENABLE                      = "TLS.enable";
static const char *const CONFIG_TLS_CA_LIST_FILE                = "TLS.certificateListFile";
static const char *const CONFIG_TLS_CERTIFICATE_FILE            = "TLS.certificateFile";
static const char *const CONFIG_TLS_PRIVATE_KEY_FILE            = "TLS.privateKeyFile";
static const char *const CONFIG_TLS_PASSWORD                    = "TLS.password";
static const char *const CONFIG_TLS_METHOD                      = "TLS.method";
static const char *const CONFIG_TLS_CIPHERS                     = "TLS.ciphers";
static const char *const CONFIG_TLS_SERVER_NAME                 = "TLS.serverName";
static const char *const CONFIG_TLS_VERIFY_SERVER               = "TLS.verifyServer";
static const char *const CONFIG_TLS_VERIFY_CLIENT               = "TLS.verifyClient";
static const char *const CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE  = "TLS.requireClientCertificate";
static const char *const CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC     = "TLS.negotiationTimeoutSec";

// DHT specific parameters
static const char *const CONFIG_DHT_PORT                        = "DHT.port";
static const char *const CONFIG_DHT_PUBLIC_IN_CALLS             = "DHT.PublicInCalls";

// Volatile parameters
static const char *const CONFIG_ACCOUNT_REGISTRATION_STATUS     = "Account.registrationStatus";
static const char *const CONFIG_ACCOUNT_REGISTRATION_STATE_CODE = "Account.registrationCode";
static const char *const CONFIG_ACCOUNT_REGISTRATION_STATE_DESC = "Account.registrationDescription";
static const char *const CONFIG_TRANSPORT_STATE_CODE            = "Transport.statusCode";
static const char *const CONFIG_TRANSPORT_STATE_DESC            = "Transport.statusDescription";

}} // namespace jami::Conf

#endif // ACCOUNT_SCHEMA_H_
