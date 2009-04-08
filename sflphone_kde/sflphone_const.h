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
 
#define APP_NAME                          "KDE Client"

/** Locale */
#define _(STRING)                         gettext( STRING )   

/** Warnings unused variables **/
#define UNUSED_VAR(var)                   (void*)var

#define UNUSED                            __attribute__((__unused__))

#define SIP                               0
#define IAX                               1

#define PAGE_GENERAL                      0
#define PAGE_DISPLAY                      1
#define PAGE_ACCOUNTS                     2
#define PAGE_AUDIO                        3


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
#define ICON_REC_DEL_OFF                  ":/images/icons/del_off.png"
#define ICON_REC_DEL_ON                   ":/images/icons/del_on.png"

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


#define RECORD_DEVICE                     "mic"
#define SOUND_DEVICE                      "speaker"


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

#define ACCOUNT_MAILBOX_DEFAULT_VALUE     "888"

#define ACCOUNT_STATE_REGISTERED          "REGISTERED"
#define ACCOUNT_STATE_UNREGISTERED        "UNREGISTERED"
#define ACCOUNT_STATE_TRYING              "TRYING"
#define ACCOUNT_STATE_ERROR               "ERROR"
#define ACCOUNT_STATE_ERROR_AUTH          "ERROR_AUTH"
#define ACCOUNT_STATE_ERROR_NETWORK       "ERROR_NETWORK"
#define ACCOUNT_STATE_ERROR_HOST          "ERROR_HOST"
#define ACCOUNT_STATE_ERROR_CONF_STUN     "ERROR_CONF_STUN"
#define ACCOUNT_STATE_ERROR_EXIST_STUN    "ERROR_EXIST_STUN"

#define ACCOUNT_ITEM_CHECKBOX             "checkbox"
#define ACCOUNT_ITEM_LED                  "led"


#define CALL_STATE_CHANGE_HUNG_UP         "HUNGUP"
#define CALL_STATE_CHANGE_RINGING         "RINGING"
#define CALL_STATE_CHANGE_CURRENT         "CURRENT"
#define CALL_STATE_CHANGE_HOLD            "HOLD"
#define CALL_STATE_CHANGE_BUSY            "BUSY"
#define CALL_STATE_CHANGE_FAILURE         "FAILURE"
#define CALL_STATE_CHANGE_UNHOLD_CURRENT  "UNHOLD_CURRENT"
#define CALL_STATE_CHANGE_UNHOLD_RECORD   "UNHOLD_RECORD"


#define MAX_HISTORY_CAPACITY              60


#define CODEC_NAME                        0
#define CODEC_SAMPLE_RATE                 1
#define CODEC_BIT_RATE                    2
#define CODEC_BANDWIDTH                   3

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
/** Tells if the main window is reduced to the system tray or not */
#define MINIMIZED	                        TRUE
/** Behaviour of the main window on incoming calls */
#define __POPUP_WINDOW                    ( dbus_popup_mode() )
/** Show/Hide the dialpad */
#define SHOW_DIALPAD	                     ( dbus_get_dialpad() ) 
/** Show/Hide the volume controls */
#define SHOW_VOLUME	                     ( dbus_get_volume_controls() ) 
/** Show/Hide the dialpad */
#define SHOW_SEARCHBAR	                  ( dbus_get_searchbar() ) 
/** Show/Hide the alsa configuration panel */
#define SHOW_ALSA_CONF                    ( dbus_get_audio_manager() == ALSA )

/** Audio Managers */
#define ALSA	                           0
#define PULSEAUDIO                        1

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
