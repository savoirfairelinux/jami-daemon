/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "videomanager_interface.h"
#include "observer.h"

#include <memory>
#include <functional>

namespace jami {

using MediaFrame = libjami::MediaFrame;
using AudioFrame = libjami::AudioFrame;
using MediaObserver = std::function<void(std::shared_ptr<MediaFrame>&&)>;

#ifdef ENABLE_VIDEO

using VideoFrame = libjami::VideoFrame;

// Some helpers
int videoFrameSize(int format, int width, int height);

#endif // ENABLE_VIDEO

} // namespace jami
