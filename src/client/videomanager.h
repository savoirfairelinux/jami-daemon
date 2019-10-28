/*
 *  Copyright (C) 2012-2019 Savoir-faire Linux Inc.
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

#ifndef VIDEOMANAGER_H_
#define VIDEOMANAGER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory> // for weak/shared_ptr
#include <vector>
#include <map>
#include <mutex>
#include <string>

#include "audio/audio_input.h"
#include "video/video_device_monitor.h"
#include "video/video_base.h"
#include "video/video_input.h"

namespace jami {

struct VideoManager
{
public:

    void setDeviceOrientation(const std::string& deviceId, int angle);

    /**
     * VideoManager acts as a cache of the active VideoInput.
     * When this input is needed, you must use getVideoCamera
     * to create the instance if not done yet and obtain a shared pointer
     * for your own usage.
     * VideoManager instance doesn't increment the reference count of
     * this video input instance: this instance is destroyed when the last
     * external user has released its shared pointer.
     */
    std::weak_ptr<video::VideoInput> videoInput;
    std::shared_ptr<video::VideoFrameActiveWriter> videoPreview;
    video::VideoDeviceMonitor videoDeviceMonitor;
    std::atomic_bool started;
    /**
     * VideoManager also acts as a cache of the active AudioInput(s).
     * When one of these is needed, you must use getAudioInput, which will
     * create an instance if need be and return a shared_ptr.
     */
    std::map<std::string, std::weak_ptr<AudioInput>> audioInputs;
    std::mutex audioMutex;
    std::shared_ptr<AudioInput> audioPreview;
};

std::shared_ptr<video::VideoFrameActiveWriter> getVideoCamera();
video::VideoDeviceMonitor& getVideoDeviceMonitor();
std::shared_ptr<AudioInput> getAudioInput(const std::string& id);

} // namespace jami

#endif // VIDEOMANAGER_H_
