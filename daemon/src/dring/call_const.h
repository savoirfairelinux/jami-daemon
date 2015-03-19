/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
#ifndef DRING_CALL_H
#define DRING_CALL_H

namespace DRing {

namespace Call {

namespace Details {

constexpr static char CALL_TYPE                [] = "CALL_TYPE"           ;
constexpr static char PEER_NUMBER              [] = "PEER_NUMBER"         ;
constexpr static char DISPLAY_NAME             [] = "DISPLAY_NAME"        ;
constexpr static char CALL_STATE               [] = "CALL_STATE"          ;
constexpr static char CONF_ID                  [] = "CONF_ID"             ;
constexpr static char TIMESTAMP_START          [] = "TIMESTAMP_START"     ;
constexpr static char ACCOUNTID                [] = "ACCOUNTID"           ;
constexpr static char PEER_HOLDING             [] = "PEER_HOLDING"        ;
constexpr static char TLS_PEER_CERT            [] = "TLS_PEER_CERT"       ;
constexpr static char TLS_CIPHER               [] = "TLS_CIPHER"          ;

}


} //namespace DRing::Call

} //namespace DRing

#endif
