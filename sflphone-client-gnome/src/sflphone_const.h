/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef __SFLPHONE_CONST_H
#define __SFLPHONE_CONST_H

#include <libintl.h>
#include "dbus.h"
#include "log4c.h"

/* @file sflphone_const.h
 * @brief Contains the global variables for the client code
 */

#define LOGO                ICONS_DIR "/sflphone.svg"
#define LOGO_NOTIF          ICONS_DIR "/sflphone_notif.svg"
#define LOGO_OFFLINE        ICONS_DIR "/sflphone_offline.svg"
#define LOGO_SMALL          ICONS_DIR "/sflphone_small.svg"

#define CURRENT_CALLS       "current_calls"
#define HISTORY             "history"
#define CONTACTS            "contacts"

/** Locale */
#define _(STRING)             gettext( STRING )
#define N_(STRING)			  (STRING)
#define c_(COMMENT,STRING)    gettext(STRING) 
#define n_(SING,PLUR,COUNT)   ngettext(SING,PLUR,COUNT)

#define IP2IP	"IP2IP"

/** Warnings unused variables **/
#define UNUSED_VAR(var)      (void*)var

#define UNUSED  __attribute__((__unused__))

#define IP2IP_PROFILE                      "IP2IP"

#define ACCOUNT_ID                         "Account.id"
#define ACCOUNT_TYPE                       "Account.type"
#define ACCOUNT_ALIAS		           "Account.alias"
#define ACCOUNT_ENABLED		           "Account.enable"
#define ACCOUNT_MAILBOX		           "Account.mailbox"
#define ACCOUNT_USERAGENT	           "useragent"
#define ACCOUNT_RESOLVE_ONCE               "Account.resolveOnce"
#define ACCOUNT_REGISTRATION_EXPIRE        "Account.expire"
#define ACCOUNT_SIP_STUN_SERVER	           "STUN.server"
#define ACCOUNT_SIP_STUN_ENABLED           "STUN.enable"
#define ACCOUNT_DTMF_TYPE                  "Account.dtmfType"
#define ACCOUNT_HOSTNAME                   "hostname"
#define ACCOUNT_USERNAME                   "username"
#define ACCOUNT_ROUTE                      "routeset"
#define ACCOUNT_PASSWORD                   "password"
#define ACCOUNT_AUTHENTICATION_USERNAME    "authenticationUsername"
#define ACCOUNT_REALM                      "realm"
#define ACCOUNT_KEY_EXCHANGE               "SRTP.keyExchange"
#define ACCOUNT_SRTP_ENABLED               "SRTP.enable"
#define ACCOUNT_SRTP_RTP_FALLBACK          "SRTP.rtpFallback"
#define ACCOUNT_ZRTP_DISPLAY_SAS           "ZRTP.displaySAS"
#define ACCOUNT_ZRTP_NOT_SUPP_WARNING      "ZRTP.notSuppWarning"
#define ACCOUNT_ZRTP_HELLO_HASH            "ZRTP.helloHashEnable"
#define ACCOUNT_DISPLAY_SAS_ONCE           "ZRTP.displaySasOnce"
#define KEY_EXCHANGE_NONE                  "0"
#define ZRTP                               "1"
#define SDES                               "2"

#define TLS_LISTENER_PORT                   "TLS.listenerPort"
#define TLS_ENABLE                          "TLS.enable"
#define TLS_PORT                            "TLS.port"
#define TLS_CA_LIST_FILE                    "TLS.certificateListFile"
#define TLS_CERTIFICATE_FILE                "TLS.certificateFile"
#define TLS_PRIVATE_KEY_FILE                "TLS.privateKeyFile"
#define TLS_PASSWORD                        "TLS.password"
#define TLS_METHOD                          "TLS.method"
#define TLS_CIPHERS                         "TLS.ciphers"
#define TLS_SERVER_NAME                     "TLS.serverName"
#define TLS_VERIFY_SERVER                   "TLS.verifyServer"
#define TLS_VERIFY_CLIENT                   "TLS.verifyClient"
#define TLS_REQUIRE_CLIENT_CERTIFICATE      "TLS.requireClientCertificate"  
#define TLS_NEGOTIATION_TIMEOUT_SEC         "TLS.negotiationTimeoutSec"
#define TLS_NEGOTIATION_TIMEOUT_MSEC        "TLS.negotiationTimemoutMsec"

#define LOCAL_INTERFACE                     "Account.localInterface"
#define PUBLISHED_SAMEAS_LOCAL              "Account.publishedSameAsLocal"
#define LOCAL_PORT                          "Account.localPort"
#define PUBLISHED_PORT                      "Account.publishedPort"
#define PUBLISHED_ADDRESS                   "Account.publishedAddress"

#define REGISTRATION_STATUS                 "Status"
#define REGISTRATION_STATE_CODE             "Registration.code" 
#define REGISTRATION_STATE_DESCRIPTION      "Registration.description"

/**
 * Global logger
 */
log4c_category_t* log4c_sfl_gtk_category;

/** Error while opening capture device */
#define ALSA_CAPTURE_DEVICE	      0x0001
/** Error while opening playback device */
#define ALSA_PLAYBACK_DEVICE	      0x0010
/** Error pulseaudio */
#define PULSEAUDIO_NOT_RUNNING        0x0100

/** Tone to play when no voice mails */
#define TONE_WITHOUT_MESSAGE  0
/** Tone to play when voice mails */
#define TONE_WITH_MESSAGE     1
/** Tells if the main window is reduced to the system tray or not */
#define MINIMIZED	      TRUE
/** Behaviour of the main window on incoming calls */
#define __POPUP_WINDOW  ( dbus_popup_mode() )
/** Show/Hide the dialpad */
#define SHOW_DIALPAD	( dbus_get_dialpad() )
/** Show/Hide the alsa configuration panel */
#define SHOW_ALSA_CONF  ( dbus_get_audio_manager() == ALSA )
/** Show/Hide the volume controls */
#define SHOW_VOLUME	(dbus_get_volume_controls() && SHOW_ALSA_CONF)

/** Audio Managers */
#define ALSA	      0
#define PULSEAUDIO    1

 /** DTMF type */
#define OVERRTP "overrtp"
#define SIPINFO "sipinfo"

/** Notification levels */
#define __NOTIF_LEVEL_MIN     0
#define __NOTIF_LEVEL_MED     1
#define __NOTIF_LEVEL_HIGH    2

/** Messages ID for the status bar - Incoming calls */
#define __MSG_INCOMING_CALL  0
/** Messages ID for the status bar - Calling */
#define __MSG_CALLING	     1
/** Messages ID for the status bar - Voice mails  notification */
#define __MSG_VOICE_MAILS    2
/** Messages ID for the status bar - Current account */
#define __MSG_ACCOUNT_DEFAULT  3

/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_MODE      "default"
/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_TIME      18000       // 30 secondes

/**
 * Macros for logging
 */
#define DEBUG(...) log4c_category_log(log4c_sfl_gtk_category, LOG4C_PRIORITY_DEBUG, __VA_ARGS__);
#define WARN(...) log4c_category_log(log4c_sfl_gtk_category, LOG4C_PRIORITY_WARN, __VA_ARGS__);
#define ERROR(...) log4c_category_log(log4c_sfl_gtk_category, LOG4C_PRIORITY_ERROR, __VA_ARGS__);
#define FATAL(...) log4c_category_log(log4c_sfl_gtk_category, LOG4C_PRIORITY_FATAL, __VA_ARGS__);

#endif
