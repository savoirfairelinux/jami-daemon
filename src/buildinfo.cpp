/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dring.h"
#include <string>

#include <ciso646> // fix windows compiler bug

#ifndef RING_REVISION
#define RING_REVISION ""
#endif

#ifndef RING_DIRTY_REPO
#define RING_DIRTY_REPO ""
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "unknown"
#endif

namespace DRing {

const char*
version() noexcept
{
    return RING_REVISION[0] and RING_DIRTY_REPO[0] ?
        PACKAGE_VERSION "-" RING_REVISION "-" RING_DIRTY_REPO :
        (RING_REVISION[0] ? PACKAGE_VERSION "-" RING_REVISION : PACKAGE_VERSION);
}

} // namespace DRing
