/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#include "noncopyable.h"
#include "mapping.h"

namespace jami {
namespace upnp {

// Abstract class (interface) to receive mapping events.
class MappingRequestObserver
{
public:
    MappingRequestObserver(){};
    virtual ~MappingRequestObserver(){};

    // Invoked when a mapping request is successful.
    virtual void onMapAdded(const Mapping& map, bool success) const = 0;
    // Invoked when a mapping is removed.
    virtual void onMapRemoved(const Mapping& map, bool success) const = 0;
    // Invoked when a connection change is detected.
    virtual void onConnectionChanged() const = 0;

private:
    NON_COPYABLE(MappingRequestObserver);
};

} // namespace upnp
} // namespace jami
