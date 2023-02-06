/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#pragma once

#ifdef REGISTERED
#undef REGISTERED
#endif

namespace jami {

/** Contains all the Registration states for an account can be in */
enum class RegistrationState {
    UNLOADED,
    UNREGISTERED,
    TRYING,
    REGISTERED,
    ERROR_GENERIC,
    ERROR_AUTH,
    ERROR_NETWORK,
    ERROR_HOST,
    ERROR_SERVICE_UNAVAILABLE,
    ERROR_NEED_MIGRATION,
    INITIALIZING
};

} // namespace jami
