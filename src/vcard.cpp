/****************************************************************************
 *    Copyright (C) 2017-2023 Savoir-faire Linux Inc.                       *
 *   Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>           *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com>      *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "vcard.h"
#include "string_utils.h"

namespace vCard {

namespace utils {

std::map<std::string, std::string>
toMap(std::string_view content)
{
    std::map<std::string, std::string> vCard;

    std::string_view line;
    while (jami::getline(content, line)) {
        if (line.size()) {
            const auto dblptPos = line.find(':');
            if (dblptPos == std::string::npos)
                continue;
            vCard.emplace(line.substr(0, dblptPos), line.substr(dblptPos + 1));
        }
    }
    return vCard;
}
} // namespace utils

} // namespace vCard
