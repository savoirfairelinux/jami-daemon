/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include <vector>
#include <iterator>
#include <algorithm>
#include <tuple>

namespace jami {
namespace map_utils {

///< Return the N-th type of a tuple type used as the Container compliant value type
template<typename C, std::size_t N>
using type_element =
    typename std::remove_cv<typename std::tuple_element<N, typename C::value_type>::type>::type;

///< Extract in a std::vector object each N-th values of tuples contained in a Container compliant
///< object \a container.
template<std::size_t N, typename C>
inline std::vector<type_element<C, N>>
extractElements(const C& container)
{
    std::vector<type_element<C, N>> result;
    if (container.size() > 0) {
        result.resize(container.size());
        auto iter = std::begin(container);
        std::generate(std::begin(result), std::end(result), [&] { return std::get<N>(*iter++); });
    }
    return result;
}

template<typename M>
inline auto
extractKeys(const M& map) -> decltype(extractElements<0>(map))
{
    return extractElements<0>(map);
}

template<typename M>
inline auto
extractValues(const M& map) -> decltype(extractElements<1>(map))
{
    return extractElements<1>(map);
}

} // namespace map_utils
} // namespace jami
