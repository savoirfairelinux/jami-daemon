/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#include "video_recorder.h"

#include "media_encoder.h"
#include "media_codec.h"
#include "media_device.h"

#include "logger.h"

namespace ring { namespace video {

VideoRecorder::VideoRecorder(const std::string& filename)
    : encoder_(new MediaEncoder)
{
    DeviceParams dev;

    dev.input = filename.c_str();
    dev.format = "avi";
    dev.width = 640;
    dev.height = 480;
    dev.framerate = 25;
    dev.channel = 1;

    RING_ERR("start video recording");

    encoder_->setDeviceOptions(dev);
    encoder_->openOutput(filename.c_str(), {});
    encoder_->startIO();
}

VideoRecorder::~VideoRecorder()
{
    RING_ERR("stop video recording");
    encoder_->flush();
}

void
VideoRecorder::update(Observable<std::shared_ptr<VideoFrame>>* /*obs*/,
                      std::shared_ptr<VideoFrame> frame_p)
{
    bool is_keyframe = true; // for testing
    if (encoder_->encode(*frame_p, is_keyframe, ++frameNumber_) < 0)
        RING_ERR("encoding failed");
}

}} // namespace ring::video
