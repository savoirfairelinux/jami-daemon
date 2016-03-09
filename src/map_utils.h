/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef MAP_UTILS_H_
#define MAP_UTILS_H_

#include <vector>
#include <map>

namespace ring { namespace map_utils {

template <typename M, typename V>
void vectorFromMapKeys(const M &m, V &v)
{
    for (typename M::const_iterator it = m.begin(); it != m.end(); ++it)
        v.push_back(it->first);
}

template <typename M, typename V>
void vectorFromMapValues(const M &m, V &v)
{
    for (typename M::const_iterator it = m.begin(); it != m.end(); ++it)
        v.push_back(it->second);
}

template <typename M, typename V>
typename M::const_iterator
findByValue(const M &m, V &v) {
    for (typename M::const_iterator it = m.begin(); it != m.end(); ++it)
        if (it->second == v)
            return it;
    return m.cend();
}

}} // namespace ring::map_utils

#endif  // MAP_UTILS_H_
