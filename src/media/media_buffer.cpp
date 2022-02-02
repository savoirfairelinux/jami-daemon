/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "libav_utils.h"
#include "media_buffer.h"
#include "jami/videomanager_interface.h"

#include <new> // std::bad_alloc
#include <cstdlib>
#include <cstring> // std::memset
#include <ciso646> // fix windows compiler bug

namespace jami {

#ifdef ENABLE_VIDEO

//=== HELPERS ==================================================================
// Memory alignment for video buffers.
/**
 * NOTE:
 * Ideally, the alignment should be configurable and reflects the
 * setup constraints (architecture, instruction set used, OS,
 * thirdparty libs, ...).
 */
int
videoFrameAlign(int format)
{
    auto pixelFormat = static_cast<AVPixelFormat>(format);
    switch (pixelFormat) {
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_BGRA:
        return 4;
    default:
        return 1;
    }
}

int
videoFrameSize(int format, int width, int height)
{
    return av_image_get_buffer_size((AVPixelFormat) format, width, height, videoFrameAlign(format));
}

#endif // ENABLE_VIDEO

} // namespace jami
