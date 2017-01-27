/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *
 *  Author: XXX
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

#ifndef MIGRATION_STATES_H_
#define MIGRATION_STATES_H_

/* XXX il est possible que des etats doivent etre undef
#ifdef REGISTERED
#undef REGISTERED
#endif
*/
namespace ring {

/** Contains all the Migration states */
enum class MigrationState {
    TRYING,
    SUCCESS,
    INVALID
};

} // namespace ring

#endif // MIGRATION_STATES_H_
