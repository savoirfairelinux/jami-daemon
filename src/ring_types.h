/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef RING_TYPES_H_
#define RING_TYPES_H_

#include <type_traits>
#include <memory>
#include <mutex>
#include <cstddef> // for size_t

#include <ciso646> // fix windows compiler bug

namespace jami {


static constexpr size_t SIZEBUF = 16000; /** About 62.5ms of buffering at 48kHz */

/**
 * This meta-function is used to enable a template overload
 * only if given class T is a base of class U
 */
template<class T, class U>
using enable_if_base_of = typename std::enable_if<std::is_base_of<T, U>::value, T>::type;

/**
 * Return a shared pointer on an auto-generated global instance of class T.
 * This instance is created only at usage and destroyed when not,
 * as we keep only a weak reference on it.
 * But when created it's always the same object until all holders release
 * their sharing.
 * An optional MaxRespawn positive integer can be given to limit the number
 * of time the object can be created (i.e. different instance).
 * Any negative values (default) block this effect (unlimited respawn).
 * This function is thread-safe.
 */
template<class T, signed MaxRespawn = -1>
std::shared_ptr<T>
getGlobalInstance()
{
    static std::recursive_mutex mutex; // recursive as instance calls recursively
    static std::weak_ptr<T> wlink;

    std::unique_lock<std::recursive_mutex> lock(mutex);

    if (wlink.expired()) {
        static signed counter {MaxRespawn};
        if (not counter)
            return nullptr;
        auto link = std::make_shared<T>();
        wlink = link;
        if (counter > 0)
            --counter;
        return link;
    }

    return wlink.lock();
}

} // namespace jami

#endif // RING_TYPES_H_
