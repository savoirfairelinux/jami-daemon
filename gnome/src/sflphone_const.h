/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <glib.h>

/* @file sflphone_const.h
 * @brief Contains the global variables for the client code
 */

#define LOGO                ICONS_DIR "/sflphone.svg"
#define LOGO_NOTIF          ICONS_DIR "/sflphone_notif.svg"
#define LOGO_OFFLINE        ICONS_DIR "/sflphone_offline.svg"
#define LOGO_SMALL          ICONS_DIR "/sflphone_small.svg"

/** Locale */
#define c_(COMMENT,STRING)    gettext(STRING)
#define n_(SING,PLUR,COUNT)   ngettext(SING,PLUR,COUNT)

#define IP2IP_PROFILE                      "IP2IP"

#define ZRTP                               "zrtp"
#define SDES                               "sdes"

#define SHORTCUT_PICKUP                    "pickUp"
#define SHORTCUT_HANGUP                    "hangUp"
#define SHORTCUT_POPUP                     "popupWindow"
#define SHORTCUT_TOGGLEPICKUPHANGUP        "togglePickupHangup"
#define SHORTCUT_TOGGLEHOLD                "toggleHold"

/** Error while opening capture device */
#define ALSA_CAPTURE_DEVICE         0x0001
/** Error while opening playback device */
#define ALSA_PLAYBACK_DEVICE        0x0010
/** Error pulseaudio */
#define PULSEAUDIO_NOT_RUNNING      0x0100
/** Error codecs not loaded */
#define CODECS_NOT_LOADED           0x1000

#define PULSEAUDIO_API_STR          "pulseaudio"
#define ALSA_API_STR                "alsa"

/** Tone to play when no voice mails */
#define TONE_WITHOUT_MESSAGE  0
/** Tone to play when voice mails */
#define TONE_WITH_MESSAGE     1
/** Tells if the main window is reduced to the system tray or not */
#define MINIMIZED          TRUE
/** Behaviour of the main window on incoming calls */
#define __POPUP_WINDOW  (eel_gconf_get_integer (POPUP_ON_CALL))
/** Show/Hide the volume controls */
#define SHOW_VOLUME    (eel_gconf_get_integer (SHOW_VOLUME_CONTROLS) && must_show_alsa_conf())

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
#define __MSG_CALLING         1
/** Messages ID for the status bar - Voice mails  notification */
#define __MSG_VOICE_MAILS    2
/** Messages ID for the status bar - Current account */
#define __MSG_ACCOUNT_DEFAULT  3

/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_MODE      "default"
/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_TIME      18000       // 30 secondes

#endif
