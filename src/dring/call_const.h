/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
#ifndef DRING_CALL_H
#define DRING_CALL_H

#include "def.h"

namespace DRing {

namespace Call {

namespace StateEvent {

constexpr static char INCOMING                 [] = "INCOMING"           ;
constexpr static char CONNECTING               [] = "CONNECTING"         ;
constexpr static char RINGING                  [] = "RINGING"            ;
constexpr static char CURRENT                  [] = "CURRENT"            ;
constexpr static char HUNGUP                   [] = "HUNGUP"             ;
constexpr static char BUSY                     [] = "BUSY"               ;
constexpr static char PEER_BUSY                [] = "PEER_BUSY"          ;
constexpr static char FAILURE                  [] = "FAILURE"            ;
constexpr static char HOLD                     [] = "HOLD"               ;
constexpr static char UNHOLD                   [] = "UNHOLD"             ;
constexpr static char INACTIVE                 [] = "INACTIVE"           ;
constexpr static char OVER                     [] = "OVER"               ;

}

namespace Details {

constexpr static char CALL_TYPE                [] = "CALL_TYPE"           ;
constexpr static char PEER_NUMBER              [] = "PEER_NUMBER"         ;
constexpr static char REGISTERED_NAME          [] = "REGISTERED_NAME"     ;
constexpr static char DISPLAY_NAME             [] = "DISPLAY_NAME"        ;
constexpr static char CALL_STATE               [] = "CALL_STATE"          ;
constexpr static char CONF_ID                  [] = "CONF_ID"             ;
constexpr static char TIMESTAMP_START          [] = "TIMESTAMP_START"     ;
constexpr static char ACCOUNTID                [] = "ACCOUNTID"           ;
constexpr static char PEER_HOLDING             [] = "PEER_HOLDING"        ;
constexpr static char AUDIO_MUTED              [] = "AUDIO_MUTED"         ;
constexpr static char VIDEO_MUTED              [] = "VIDEO_MUTED"         ;
constexpr static char VIDEO_SOURCE             [] = "VIDEO_SOURCE"        ;
constexpr static char AUDIO_ONLY               [] = "AUDIO_ONLY"          ;
constexpr static char AUDIO_CODEC              [] = "AUDIO_CODEC"         ;
constexpr static char VIDEO_CODEC              [] = "VIDEO_CODEC"         ;

}


} //namespace DRing::Call

} //namespace DRing

#endif
