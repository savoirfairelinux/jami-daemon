/*
 *  Copyright (C) 2019 Savoir-faire Linux Inc.
 *
 *  Author: Denys VIDAL <denys.vidal@savoirfairelinux.com>
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

#include "filter_transpose.h"
#include "logger.h"

namespace jami {
namespace video {

std::unique_ptr<MediaFilter>
getTransposeFilter(int rotation, std::string inputName, int width, int height, int format, bool rescale)
{
    JAMI_WARN("Rotation set to %d", rotation);
#ifdef _MSC_VER
    if (_isnan(rotation) || !rotation) {
#else
    if (std::isnan(rotation) || !rotation) {
#endif
        return {};
    }

    std::stringstream ss;
    ss << "[" << inputName << "] ";

    switch (rotation) {
        case 90:
        case -270:
            ss << "transpose=2";
           if (rescale) {
                if (width > height)
                    ss << ", scale=w=-1:h=" << height
                        << ", pad=" << width << ":" << height << ":(ow-iw)/2";
                else
                    ss << ", scale=w=" << width << ":h=-1"
                        << ", pad=" << width << ":" << height << ":0:(oh-ih)/2";
            }
            break;
        case 180 :
        case -180 :
            ss << "transpose=1, transpose=1";
            break;
        case 270 :
        case -90 :
            ss << "transpose=1";
            if (rescale) {
                if (width > height)
                    ss << ", scale=w=-1:h=" << height
                        << ", pad=" << width << ":" << height << ":(ow-iw)/2";
                else
                    ss << ", scale=w=" << width << ":h=-1"
                        << ", pad=" << width << ":" << height << ":0:(oh-ih)/2";
            }
            break;
        default :
            return {};
    }

    const auto one = rational<int>(1);
    std::vector<MediaStream> msv;
    msv.emplace_back(inputName, format, one, width, height, 0, one);

    std::unique_ptr<MediaFilter> filter(new MediaFilter);
    auto ret = filter->initialize(ss.str(), msv);
    if (ret < 0) {
        JAMI_ERR() << "filter init fail";
        return {};
    }
    return filter;
}

}
}
