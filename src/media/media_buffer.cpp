/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
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
#include "dring/videomanager_interface.h"

#include <new> // std::bad_alloc
#include <cstdlib>
#include <cstring> // std::memset
#include <ciso646> // fix windows compiler bug

namespace jami {

#ifdef ENABLE_VIDEO

//=== HELPERS ==================================================================

int
videoFrameSize(int format, int width, int height)
{
    return av_image_get_buffer_size((AVPixelFormat)format, width, height, 1);
}

#endif // ENABLE_VIDEO

} // namespace jami
