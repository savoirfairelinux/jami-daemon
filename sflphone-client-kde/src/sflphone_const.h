/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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
 */

#ifndef __SFLPHONE_CONST_H
#define __SFLPHONE_CONST_H

#include <libintl.h>
#include <QtCore/QString>

/* @file sflphone_const.h
 * @brief Contains the global variables for the client code
 */
 
#define APP_NAME                          "SFLPhone KDE Client"

/** Locale */
// #define _(STRING)                         gettext( STRING )   

/** Warnings unused variables **/
// #define UNUSED_VAR(var)                   (void*)var

// #define UNUSED                            __attribute__((__unused__))



#define SIP                               0
#define IAX                               1

#define PAGE_GENERAL                      0
#define PAGE_DISPLAY                      1
#define PAGE_ACCOUNTS                     2
#define PAGE_AUDIO                        3

#define TOOLBAR_SIZE                      22

#define CONTACT_ITEM_HEIGHT               40

#define CONFIG_FILE_PATH                  "/.sflphone/sflphonedrc"

#define ACTION_LABEL_CALL                 tr2i18n("Call")
#define ACTION_LABEL_HANG_UP              tr2i18n("Hang up")
#define ACTION_LABEL_HOLD                 tr2i18n("Hold")
#define ACTION_LABEL_TRANSFER             tr2i18n("Transfer")
#define ACTION_LABEL_RECORD               tr2i18n("Record")
#define ACTION_LABEL_ACCEPT               tr2i18n("Accept")
#define ACTION_LABEL_REFUSE               tr2i18n("Refuse")
#define ACTION_LABEL_UNHOLD               tr2i18n("Unhold")
#define ACTION_LABEL_GIVE_UP_TRANSF       tr2i18n("Give up transfer")
#define ACTION_LABEL_CALL_BACK            tr2i18n("Call back")
#define ACTION_LABEL_GIVE_UP_SEARCH       tr2i18n("Give up search")


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

#define ICON_CALL                         ":/images/icons/call.svg"
#define ICON_HANGUP                       ":/images/icons/hang_up.svg"
#define ICON_UNHOLD                       ":/images/icons/unhold.svg"
#define ICON_ACCEPT                       ":/images/icons/accept.svg"
#define ICON_REFUSE                       ":/images/icons/refuse.svg"
#define ICON_EXEC_TRANSF                  ":/images/icons/call.svg"
#define ICON_REC_DEL_OFF                  ":/images/icons/record_disabled.svg"
#define ICON_REC_DEL_ON                   ":/images/icons/record.svg"

#define ICON_REC_VOL_0                    ":/images/icons/mic.svg"
#define ICON_REC_VOL_1                    ":/images/icons/mic_25.svg"
#define ICON_REC_VOL_2                    ":/images/icons/mic_50.svg"
#define ICON_REC_VOL_3                    ":/images/icons/mic_75.svg"

#define ICON_SND_VOL_0                    ":/images/icons/speaker.svg"
#define ICON_SND_VOL_1                    ":/images/icons/speaker_25.svg"
#define ICON_SND_VOL_2                    ":/images/icons/speaker_50.svg"
#define ICON_SND_VOL_3                    ":/images/icons/speaker_75.svg"

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
#define ACCOUNT_TYPE                      "Account.type"
#define ACCOUNT_ALIAS		               "Account.alias"
#define ACCOUNT_ENABLED		               "Account.enable"
#define ACCOUNT_MAILBOX		               "Account.mailbox"
#define ACCOUNT_HOSTNAME                  "hostname"
#define ACCOUNT_USERNAME                  "username"
#define ACCOUNT_PASSWORD                  "password"
#define ACCOUNT_STATUS                    "Status"
#define ACCOUNT_SIP_STUN_SERVER	         "STUN.server"
#define ACCOUNT_SIP_STUN_ENABLED          "STUN.enable"

#define ACCOUNT_ENABLED_TRUE              "TRUE"
#define ACCOUNT_ENABLED_FALSE             "FALSE"

#define ACCOUNT_TYPE_SIP                  "SIP"
#define ACCOUNT_TYPE_IAX                  "IAX"
#define ACCOUNT_TYPES_TAB                 {QString(ACCOUNT_TYPE_SIP), QString(ACCOUNT_TYPE_IAX)}

/** Constant variables */
#define ACCOUNT_MAILBOX_DEFAULT_VALUE     "888"

/** Account States */
#define ACCOUNT_STATE_REGISTERED          "REGISTERED"
#define ACCOUNT_STATE_UNREGISTERED        "UNREGISTERED"
#define ACCOUNT_STATE_TRYING              "TRYING"
#define ACCOUNT_STATE_ERROR               "ERROR"
#define ACCOUNT_STATE_ERROR_AUTH          "ERROR_AUTH"
#define ACCOUNT_STATE_ERROR_NETWORK       "ERROR_NETWORK"
#define ACCOUNT_STATE_ERROR_HOST          "ERROR_HOST"
#define ACCOUNT_STATE_ERROR_CONF_STUN     "ERROR_CONF_STUN"
#define ACCOUNT_STATE_ERROR_EXIST_STUN    "ERROR_EXIST_STUN"

/** Calls details */
#define CALL_PEER_NAME                    "PEER_NAME"
#define CALL_PEER_NUMBER                  "PEER_NUMBER"
#define CALL_ACCOUNTID                    "ACCOUNTID"

/** Call States */
#define CALL_STATE_CHANGE_HUNG_UP         "HUNGUP"
#define CALL_STATE_CHANGE_RINGING         "RINGING"
#define CALL_STATE_CHANGE_CURRENT         "CURRENT"
#define CALL_STATE_CHANGE_HOLD            "HOLD"
#define CALL_STATE_CHANGE_BUSY            "BUSY"
#define CALL_STATE_CHANGE_FAILURE         "FAILURE"
#define CALL_STATE_CHANGE_UNHOLD_CURRENT  "UNHOLD_CURRENT"
#define CALL_STATE_CHANGE_UNHOLD_RECORD   "UNHOLD_RECORD"

/** Address Book Settings */
#define ADDRESSBOOK_MAX_RESULTS           "ADDRESSBOOK_MAX_RESULTS"
#define ADDRESSBOOK_DISPLAY_CONTACT_PHOTO "ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"
#define ADDRESSBOOK_DISPLAY_BUSINESS      "ADDRESSBOOK_DISPLAY_PHONE_BUSINESS"
#define ADDRESSBOOK_DISPLAY_HOME          "ADDRESSBOOK_DISPLAY_PHONE_HOME"
#define ADDRESSBOOK_DISPLAY_MOBILE        "ADDRESSBOOK_DISPLAY_PHONE_MOBILE"

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
#define ALSA	                           0
#define PULSEAUDIO                        1



/** Error while opening capture device */
#define ALSA_CAPTURE_DEVICE	            0x0001
/** Error while opening playback device */
#define ALSA_PLAYBACK_DEVICE	            0x0010
/** Error pulseaudio */
#define PULSEAUDIO_NOT_RUNNING            0x0100

/** Tone to play when no voice mails */
#define TONE_WITHOUT_MESSAGE              0
/** Tone to play when voice mails */
#define TONE_WITH_MESSAGE                 1


/** Notification levels */
#define __NOTIF_LEVEL_MIN                 0
#define __NOTIF_LEVEL_MED                 1
#define __NOTIF_LEVEL_HIGH                2

/** Messages ID for the status bar - Incoming calls */
#define __MSG_INCOMING_CALL               0
/** Messages ID for the status bar - Calling */
#define __MSG_CALLING	                  1
/** Messages ID for the status bar - Voice mails  notification */
#define __MSG_VOICE_MAILS                 2
/** Messages ID for the status bar - Current account */
#define __MSG_ACCOUNT_DEFAULT             3

/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_MODE                    "default"
/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_TIME                    18000       // 30 secondes



#endif
