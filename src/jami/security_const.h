/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
#ifndef DRING_SECURITY_H
#define DRING_SECURITY_H

#include "def.h"

namespace DRing {

namespace Certificate {

namespace Status {
constexpr static char UNDEFINED[] = "UNDEFINED";
constexpr static char ALLOWED[] = "ALLOWED";
constexpr static char BANNED[] = "BANNED";
} // namespace Status

namespace TrustStatus {
constexpr static char UNTRUSTED[] = "UNTRUSTED";
constexpr static char TRUSTED[] = "TRUSTED";
} // namespace TrustStatus

} // namespace Certificate

namespace TlsTransport {
constexpr static char TLS_PEER_CERT[] = "TLS_PEER_CERT";
constexpr static char TLS_PEER_CA_NUM[] = "TLS_PEER_CA_NUM";
constexpr static char TLS_PEER_CA_[] = "TLS_PEER_CA_";
constexpr static char TLS_CIPHER[] = "TLS_CIPHER";
} // namespace TlsTransport

} // namespace DRing

#endif
