/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <map>
#include <functional>
#include <chrono>

#include "noncopyable.h"

#include "mapping.h"

namespace jami { namespace upnp {

/**
 * GlobalMapping is like a mapping, but it tracks the number of global users,
 * ie: the number of upnp:Controller which are using this mapping
 * this is usually only relevant for accounts (not calls) as multiple SIP accounts
 * can use the same SIP port and we don't want to delete a mapping from the router
 * if other accounts are using it
 */
class GlobalMapping : public Mapping 
{
public:
    /* number of users of this mapping;
     * this is only relevant when multiple accounts are using the same SIP port */
    unsigned users;

    GlobalMapping(const Mapping& mapping, unsigned users = 1);
};

}} // namespace jami::upnp