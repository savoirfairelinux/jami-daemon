/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_buffer.h"

#include <cstdlib>
#include <cstring> // std::memset

namespace jami {

#ifdef ENABLE_VIDEO

//=== HELPERS ==================================================================

int
videoFrameSize(int format, int width, int height)
{
    return av_image_get_buffer_size((AVPixelFormat) format, width, height, 1);
}

#endif // ENABLE_VIDEO

} // namespace jami
