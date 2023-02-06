/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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

#ifndef __MEDIA_DEVICE_H__
#define __MEDIA_DEVICE_H__

#include "rational.h"

#include <string>
#include <chrono>

namespace jami {

constexpr static auto NEWPARAMS_TIMEOUT = std::chrono::milliseconds(1000);

/**
 * DeviceParams
 * Parameters used by MediaDecoder and MediaEncoder
 * to open a LibAV device/stream
 */
struct DeviceParams
{
    std::string name {};      // friendly name (e.g. Logitech BRIO)
    std::string input {};     // Device path (e.g. /dev/video0)
    std::string unique_id {}; // unique id (e.g. 046d082d8A8B667F)
    std::string format {};
    unsigned width {}, height {};
    rational<double> framerate {};
    std::string pixel_format {};
    std::string channel_name {};
    unsigned channel {}; // Channel number
    std::string loop {};
    std::string sdp_flags {};
    int offset_x {};
    int offset_y {};
    int orientation {};
    std::string window_id {};
    int is_area {};
};

} // namespace jami

#endif // __MEDIA_DEVICE_H__
