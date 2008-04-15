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
#include "dbus.h"

// Locale
#define _(STRING)   gettext( STRING )   

// Generic parameters for accounts registration
#define ACCOUNT_TYPE               "Account.type"
#define ACCOUNT_ALIAS              "Account.alias"
#define ACCOUNT_ENABLED		   "Account.enable"
// SIP specific parameters
#define ACCOUNT_SIP_FULL_NAME      "SIP.fullName"
#define ACCOUNT_SIP_HOST_PART      "SIP.hostPart"
#define ACCOUNT_SIP_USER_PART      "SIP.userPart"
#define ACCOUNT_SIP_AUTH_NAME      "SIP.username"
#define ACCOUNT_SIP_PASSWORD       "SIP.password"
#define ACCOUNT_SIP_PROXY          "SIP.proxy"
#define ACCOUNT_SIP_STUN_SERVER	   "STUN.server"
#define ACCOUNT_SIP_STUN_ENABLED   "STUN.enable"
// IAX2 specific parameters
#define ACCOUNT_IAX_FULL_NAME      "IAX.fullName"
#define ACCOUNT_IAX_HOST           "IAX.host"
#define ACCOUNT_IAX_USER           "IAX.user"
#define ACCOUNT_IAX_PASS           "IAX.pass"

// Error codes for error handling
#define ALSA_CAPTURE_DEVICE	      0x0001
#define ALSA_PLAYBACK_DEVICE	      0x0010
#define NETWORK_UNREACHABLE	      0x0011

// Customizing-related parameters
#define TONE_WITHOUT_MESSAGE  0 
#define TONE_WITH_MESSAGE     1
#define MINIMIZED	      TRUE
#define __POPUP_WINDOW  ( dbus_popup_mode() )

// Messages ID for status bar 
#define __MSG_INCOMING_CALL  0 
#define __MSG_CALLING	     1
#define __MSG_VOICE_MAILS    2
#define __MSG_ACCOUNT_DEFAULT  3

// Desktop notifications
#define __TIMEOUT_MODE      "default"
#define __TIMEOUT_TIME      30000       // 30 secondes

#endif
