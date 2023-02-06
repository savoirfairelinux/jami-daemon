/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
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
#ifndef LIBJAMI_PRESENCE_CONST_H
#define LIBJAMI_PRESENCE_CONST_H

#include "def.h"

namespace libjami {

namespace Presence {

constexpr static const char* BUDDY_KEY = "Buddy";
constexpr static const char* STATUS_KEY = "Status";
constexpr static const char* LINESTATUS_KEY = "LineStatus";
constexpr static const char* ONLINE_KEY = "Online";
constexpr static const char* OFFLINE_KEY = "Offline";

} // namespace Presence

} // namespace libjami

#endif
