/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
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
