/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#ifndef SFLPHONE_CONST_H
#define SFLPHONE_CONST_H

#include <QtCore/QString>

/* @file sflphone_const.h
 * @brief Contains the global variables for the client code
 */

#define APP_NAME                          "SFLphone KDE Client"

#define SIP                               0
#define IAX                               1

#define TOOLBAR_SIZE                      22

#define CONTACT_ITEM_HEIGHT               40

#define CONFIG_FILE_PATH                  "/.sflphone/sflphonedrc"

#define ACTION_LABEL_CALL                 i18n("New call")
#define ACTION_LABEL_HANG_UP              i18n("Hang up")
#define ACTION_LABEL_HOLD                 i18n("Hold on")
#define ACTION_LABEL_TRANSFER             i18n("Transfer")
#define ACTION_LABEL_RECORD               i18n("Record")
#define ACTION_LABEL_ACCEPT               i18n("Pick up")
#define ACTION_LABEL_REFUSE               i18n("Hang up")
#define ACTION_LABEL_UNHOLD               i18n("Hold off")
#define ACTION_LABEL_GIVE_UP_TRANSF       i18n("Give up transfer")
#define ACTION_LABEL_CALL_BACK            i18n("Call back")
#define ACTION_LABEL_MAILBOX              i18n("Voicemail")

#define SCREEN_MAIN                       0
#define SCREEN_HISTORY                    1
#define SCREEN_ADDRESS                    2

#define ICON_INCOMING                     ":/images/icons/ring.svg"
#define ICON_RINGING                      ":/images/icons/ring.svg"
#define ICON_CURRENT                      ":/images/icons/current.svg"
#define ICON_CURRENT_REC                  ":/images/icons/rec_call.svg"
#define ICON_DIALING                      ":/images/icons/dial.svg"
#define ICON_HOLD                         ":/images/icons/hold.svg"
#define ICON_FAILURE                      ":/images/icons/fail.svg"
#define ICON_BUSY                         ":/images/icons/busy.svg"
#define ICON_TRANSFER                     ":/images/icons/transfert.svg"
#define ICON_TRANSF_HOLD                  ":/images/icons/transfert.svg"
#define ICON_CONFERENCE                   ":/images/icons/user-group-properties.svg"
#define ICON_CALL                         ":/images/icons/call.svg"
#define ICON_HANGUP                       ":/images/icons/hang_up.svg"
#define ICON_UNHOLD                       ":/images/icons/unhold.svg"
#define ICON_ACCEPT                       ":/images/icons/accept.svg"
#define ICON_REFUSE                       ":/images/icons/refuse.svg"
#define ICON_EXEC_TRANSF                  ":/images/icons/call.svg"
#define ICON_REC_DEL_OFF                  ":/images/icons/record_disabled.svg"
#define ICON_REC_DEL_ON                   ":/images/icons/record.svg"
#define ICON_MAILBOX                      ":/images/icons/mailbox.svg"
#define ICON_REC_VOL_0                    ":/images/icons/mic.svg"
#define ICON_REC_VOL_1                    ":/images/icons/mic_25.svg"
#define ICON_REC_VOL_2                    ":/images/icons/mic_50.svg"
#define ICON_REC_VOL_3                    ":/images/icons/mic_75.svg"
#define ICON_SND_VOL_0                    ":/images/icons/speaker.svg"
#define ICON_SND_VOL_1                    ":/images/icons/speaker_25.svg"
#define ICON_SND_VOL_2                    ":/images/icons/speaker_50.svg"
#define ICON_SND_VOL_3                    ":/images/icons/speaker_75.svg"
#define ICON_SCREEN_MAIN                  ":/images/icons/sflphone.svg"
#define ICON_SCREEN_HISTORY               ":/images/icons/history2.svg"
#define ICON_SCREEN_ADDRESS               ":/images/icons/x-office-address-book.png"
#define ICON_DISPLAY_VOLUME_CONSTROLS     ":/images/icons/icon_volume_off.svg"
#define ICON_DISPLAY_DIALPAD              ":/images/icons/icon_dialpad.svg"
#define ICON_HISTORY_INCOMING             ":/images/icons/incoming.svg"
#define ICON_HISTORY_OUTGOING             ":/images/icons/outgoing.svg"
#define ICON_HISTORY_MISSED               ":/images/icons/missed.svg"
#define ICON_ACCOUNT_LED_RED              ":/images/icons/led-red.svg"
#define ICON_ACCOUNT_LED_GREEN            ":/images/icons/led-green.svg"
#define ICON_ACCOUNT_LED_GRAY             ":/images/icons/led-gray.svg"
#define ICON_QUIT                         ":/images/icons/application-exit.png"
#define ICON_SFLPHONE                     ":/images/icons/sflphone.svg"
#define ICON_TRAY_NOTIF                   ":/images/icons/sflphone_notif.svg"

#define RECORD_DEVICE                     "mic"
#define SOUND_DEVICE                      "speaker"


/** Account details */

/** TLS */

#define IP2IP_PROFILE                      "IP2IP"

#define ACCOUNT_ID                         "Account.id"
#define ACCOUNT_TYPE                       "Account.type"
#define ACCOUNT_ALIAS                      "Account.alias"
#define ACCOUNT_ENABLED                    "Account.enable"
#define ACCOUNT_MAILBOX                    "Account.mailbox"
#define ACCOUNT_USERAGENT                  "Account.useragent"
#define ACCOUNT_REGISTRATION_EXPIRE        "Account.registrationExpire"
#define ACCOUNT_REGISTRATION_STATUS        "Account.registrationStatus"
#define ACCOUNT_REGISTRATION_STATE_CODE    "Account.registrationCode"
#define ACCOUNT_REGISTRATION_STATE_DESC    "Account.registrationDescription"

#define ACCOUNT_SIP_STUN_SERVER            "STUN.server"
#define ACCOUNT_SIP_STUN_ENABLED           "STUN.enable"
#define ACCOUNT_DTMF_TYPE                  "Account.dtmfType"
#define ACCOUNT_HOSTNAME                   "Account.hostname"
#define ACCOUNT_USERNAME                   "Account.username"
#define ACCOUNT_ROUTE                      "Account.routeset"
#define ACCOUNT_PASSWORD                   "Account.password"
#define ACCOUNT_REALM                      "Account.realm"
#define ACCOUNT_KEY_EXCHANGE               "SRTP.keyExchange"
#define ACCOUNT_SRTP_ENABLED               "SRTP.enable"
#define ACCOUNT_SRTP_RTP_FALLBACK          "SRTP.rtpFallback"
#define ACCOUNT_ZRTP_DISPLAY_SAS           "ZRTP.displaySAS"
#define ACCOUNT_ZRTP_NOT_SUPP_WARNING      "ZRTP.notSuppWarning"
#define ACCOUNT_ZRTP_HELLO_HASH            "ZRTP.helloHashEnable"
#define ACCOUNT_DISPLAY_SAS_ONCE           "ZRTP.displaySasOnce"
#define KEY_EXCHANGE_NONE                  "none"
#define ZRTP                               "zrtp"
#define SDES                               "sdes"

#define CONFIG_RINGTONE_PATH               "Account.ringtonePath"
#define CONFIG_RINGTONE_ENABLED            "Account.ringtoneEnabled"


/**Security */
#define TLS_LISTENER_PORT                  "TLS.listenerPort"
#define TLS_ENABLE                         "TLS.enable"
#define TLS_PORT                           "TLS.port"
#define TLS_CA_LIST_FILE                   "TLS.certificateListFile"
#define TLS_CERTIFICATE_FILE               "TLS.certificateFile"
#define TLS_PRIVATE_KEY_FILE               "TLS.privateKeyFile"
#define TLS_PASSWORD                       "TLS.password"
#define TLS_METHOD                         "TLS.method"
#define TLS_CIPHERS                        "TLS.ciphers"
#define TLS_SERVER_NAME                    "TLS.serverName"
#define TLS_VERIFY_SERVER                  "TLS.verifyServer"
#define TLS_VERIFY_CLIENT                  "TLS.verifyClient"
#define TLS_REQUIRE_CLIENT_CERTIFICATE     "TLS.requireClientCertificate"
#define TLS_NEGOTIATION_TIMEOUT_SEC        "TLS.negotiationTimeoutSec"
#define TLS_NEGOTIATION_TIMEOUT_MSEC       "TLS.negotiationTimemoutMsec"

/**Shortcut*/
#define SHORTCUT_PICKUP                    "pickUp"
#define SHORTCUT_HANGUP                    "hangUp"
#define SHORTCUT_POPUP                     "popupWindow"
#define SHORTCUT_TOGGLEPICKUPHANGUP        "togglePickupHangup"
#define SHORTCUT_TOGGLEHOLD                "toggleHold"


#define CONFIG_ACCOUNT_HOSTNAME            "Account.hostname"
#define CONFIG_ACCOUNT_USERNAME            "Account.username"
#define CONFIG_ACCOUNT_ROUTESET            "Account.routeset"
#define CONFIG_ACCOUNT_PASSWORD            "Account.password"
#define CONFIG_ACCOUNT_REALM               "Account.realm"
#define CONFIG_ACCOUNT_DEFAULT_REALM       "*"
#define CONFIG_ACCOUNT_USERAGENT           "Account.useragent"

#define LOCAL_INTERFACE                    "Account.localInterface"
#define PUBLISHED_SAMEAS_LOCAL             "Account.publishedSameAsLocal"
#define LOCAL_PORT                         "Account.localPort"
#define PUBLISHED_PORT                     "Account.publishedPort"
#define PUBLISHED_ADDRESS                  "Account.publishedAddress"


/** Maybe to remove **/
#define REGISTRATION_EXPIRE_DEFAULT       600
#define REGISTRATION_ENABLED_TRUE         "true"
#define REGISTRATION_ENABLED_FALSE        "false"
#define ACCOUNT_TYPE_SIP                  "SIP"
#define ACCOUNT_TYPE_IAX                  "IAX"
#define ACCOUNT_TYPES_TAB                 {QString(ACCOUNT_TYPE_SIP), QString(ACCOUNT_TYPE_IAX)}
/*********************/

/** Constant variables */
#define ACCOUNT_MAILBOX_DEFAULT_VALUE     "888"

/** Account States */
#define ACCOUNT_STATE_REGISTERED          "REGISTERED"
#define ACCOUNT_STATE_READY               "READY"
#define ACCOUNT_STATE_UNREGISTERED        "UNREGISTERED"
#define ACCOUNT_STATE_TRYING              "TRYING"
#define ACCOUNT_STATE_ERROR               "ERROR"
#define ACCOUNT_STATE_ERROR_AUTH          "ERROR_AUTH"
#define ACCOUNT_STATE_ERROR_NETWORK       "ERROR_NETWORK"
#define ACCOUNT_STATE_ERROR_HOST          "ERROR_HOST"
#define ACCOUNT_STATE_ERROR_CONF_STUN     "ERROR_CONF_STUN"
#define ACCOUNT_STATE_ERROR_EXIST_STUN    "ERROR_EXIST_STUN"

/** Calls details */
#define CALL_PEER_NAME                    "DISPLAY_NAME"
//#define CALL_PEER_NAME                    "PEER_NAME"
#define CALL_PEER_NUMBER                  "PEER_NUMBER"
#define CALL_ACCOUNTID                    "ACCOUNTID"
#define CALL_STATE                        "CALL_STATE"
#define CALL_TYPE                         "CALL_TYPE"
#define CALL_TIMESTAMP_START              "TIMESTAMP_START"

/** Call States */
#define CALL_STATE_CHANGE_HUNG_UP         "HUNGUP"
#define CALL_STATE_CHANGE_RINGING         "RINGING"
#define CALL_STATE_CHANGE_CURRENT         "CURRENT"
#define CALL_STATE_CHANGE_HOLD            "HOLD"
#define CALL_STATE_CHANGE_BUSY            "BUSY"
#define CALL_STATE_CHANGE_FAILURE         "FAILURE"
#define CALL_STATE_CHANGE_UNHOLD_CURRENT  "UNHOLD"
#define CALL_STATE_CHANGE_UNKNOWN         "UNKNOWN"

#define CONF_STATE_CHANGE_HOLD            "HOLD"
#define CONF_STATE_CHANGE_ACTIVE          "ACTIVE_ATTACHED"

#define DAEMON_CALL_STATE_INIT_CURRENT    "CURRENT"
#define DAEMON_CALL_STATE_INIT_HOLD       "HOLD"
#define DAEMON_CALL_STATE_INIT_BUSY       "BUSY"
#define DAEMON_CALL_STATE_INIT_INCOMING   "INCOMING"
#define DAEMON_CALL_STATE_INIT_RINGING    "RINGING"
#define DAEMON_CALL_STATE_INIT_INACTIVE   "INACTIVE"

#define DAEMON_CALL_TYPE_INCOMING         "0"
#define DAEMON_CALL_TYPE_OUTGOING         "1"

#define DAEMON_HISTORY_TYPE_MISSED        "0"
#define DAEMON_HISTORY_TYPE_OUTGOING      "1"
#define DAEMON_HISTORY_TYPE_INCOMING      "2"

/** Address Book Settings */
#define ADDRESSBOOK_MAX_RESULTS           "ADDRESSBOOK_MAX_RESULTS"
#define ADDRESSBOOK_DISPLAY_CONTACT_PHOTO "ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"
#define ADDRESSBOOK_DISPLAY_BUSINESS      "ADDRESSBOOK_DISPLAY_PHONE_BUSINESS"
#define ADDRESSBOOK_DISPLAY_HOME          "ADDRESSBOOK_DISPLAY_PHONE_HOME"
#define ADDRESSBOOK_DISPLAY_MOBILE        "ADDRESSBOOK_DISPLAY_PHONE_MOBILE"
#define ADDRESSBOOK_ENABLE                "ADDRESSBOOK_ENABLE"

/** Hooks settings */
#define HOOKS_ADD_PREFIX                  "PHONE_NUMBER_HOOK_ADD_PREFIX"
#define HOOKS_ENABLED                     "PHONE_NUMBER_HOOK_ENABLED"
#define HOOKS_COMMAND                     "URLHOOK_COMMAND"
#define HOOKS_IAX2_ENABLED                "URLHOOK_IAX2_ENABLED"
#define HOOKS_SIP_ENABLED                 "URLHOOK_SIP_ENABLED"
#define HOOKS_SIP_FIELD                   "URLHOOK_SIP_FIELD"

/** Constant variables */
#define MAX_HISTORY_CAPACITY              60

/** Codecs details */
#define CODEC_NAME                        0
#define CODEC_SAMPLE_RATE                 1
#define CODEC_BIT_RATE                    2
#define CODEC_BANDWIDTH                   3

/** Audio Managers */
#define CONST_ALSA                        0
#define CONST_PULSEAUDIO                  1

typedef enum
{
   
   CALL_STATE_INCOMING        = 0, /** Ringing incoming call */
   CALL_STATE_RINGING         = 1, /** Ringing outgoing call */
   CALL_STATE_CURRENT         = 2, /** Call to which the user can speak and hear */
   CALL_STATE_DIALING         = 3, /** Call which numbers are being added by the user */
   CALL_STATE_HOLD            = 4, /** Call is on hold */
   CALL_STATE_FAILURE         = 5, /** Call has failed */
   CALL_STATE_BUSY            = 6, /** Call is busy */
   CALL_STATE_TRANSFERRED     = 7, /** Call is being transferred.  During this state, the user can enter the new number. */
   CALL_STATE_TRANSF_HOLD     = 8, /** Call is on hold for transfer */
   CALL_STATE_OVER            = 9, /** Call is over and should not be used */
   CALL_STATE_ERROR           = 10,/** This state should never be reached */
   CALL_STATE_CONFERENCE      = 11,/** This call is the current conference*/
   CALL_STATE_CONFERENCE_HOLD = 12,/** This call is a conference on hold*/
} call_state;

static const QString empty("");
#define EMPTY_STRING empty

/** MIME API */
#define MIME_CALLID           "text/sflphone.call.id"
#define MIME_CONTACT          "text/sflphone.contact"
#define MIME_HISTORYID        "text/sflphone.history.id"
#define MIME_PHONENUMBER      "text/sflphone.phone.number"
#define MIME_CONTACT_PHONE    "text/sflphone.contact.phone"
#define MIME_PLAIN_TEXT       "text/plain"
#endif

/** HISTORY SERIALIZATION */
#define ACCOUNT_ID_KEY        "accountid"
#define CALLID_KEY            "callid"
#define CONFID_KEY            "confid"
#define DISPLAY_NAME_KEY      "display_name"
#define PEER_NUMBER_KEY       "peer_number"
#define RECORDING_PATH_KEY    "recordfile"
#define STATE_KEY             "state"
#define TIMESTAMP_START_KEY   "timestamp_start"
#define TIMESTAMP_STOP_KEY    "timestamp_stop"
#define MISSED_STRING         "missed"
#define INCOMING_STRING       "incoming"
#define OUTGOING_STRING       "outgoing"
