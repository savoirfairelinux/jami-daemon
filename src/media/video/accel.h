/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include "libav_deps.h"

#include <string>
#include <vector>

namespace ring { namespace video {

struct HardwareAccel {
        std::string name;
        bool requiresInitialization;
        AVPixelFormat format;
        AVPixelFormat swFormat;
        std::vector<AVCodecID> supportedCodecs;
};
int transferFrameDecode(HardwareAccel accel, AVCodecContext* codecCtx, VideoFrame& frame);
int transferFrameEncode(HardwareAccel accel, AVCodecContext* enc, VideoFrame& frame);
const HardwareAccel setupHardwareDecoding(AVCodecContext* codecCtx);
const HardwareAccel setupHardwareEncoding(AVCodecContext** codecCtx, AVCodec** codec);
}} // namespace ring::video
