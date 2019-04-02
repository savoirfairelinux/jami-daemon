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

#pragma once

#include "config.h"
#include "videomanager_interface.h"

#include <memory>
#include <functional>

struct AVFrame;

namespace DRing {
struct FrameBuffer; //  from dring/videomanager_interface.h
}

namespace jami {

using MediaFrame = DRing::MediaFrame;
using AudioFrame = DRing::AudioFrame;

#ifdef ENABLE_VIDEO

using VideoFrame = DRing::VideoFrame;

// Some helpers
int videoFrameSize(int format, int width, int height);

#endif // ENABLE_VIDEO

} // namespace jami
