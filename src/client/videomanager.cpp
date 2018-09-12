/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include "videomanager_interface.h"
#include "videomanager.h"
#include "localrecorder.h"
#include "localrecordermanager.h"
#include "libav_utils.h"
#include "video/video_input.h"
#include "video/video_device_monitor.h"
#include "account.h"
#include "logger.h"
#include "manager.h"
#include "system_codec_container.h"
#include "video/sinkclient.h"
#include "client/ring_signal.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <new> // std::bad_alloc
#include <cstdlib>
#include <cstring> // std::memset
#include <ciso646> // fix windows compiler bug

namespace DRing {

MediaFrame::MediaFrame()
    : frame_ {av_frame_alloc(), [](AVFrame* frame){ av_frame_free(&frame); }}
{
    if (not frame_)
        throw std::bad_alloc();
}

void
MediaFrame::reset() noexcept
{
    av_frame_unref(frame_.get());
}

VideoFrame::~VideoFrame()
{
    if (releaseBufferCb_)
        releaseBufferCb_(ptr_);
}

void
VideoFrame::reset() noexcept
{
    MediaFrame::reset();
    allocated_ = false;
    releaseBufferCb_ = {};
}

size_t
VideoFrame::size() const noexcept
{
    return av_image_get_buffer_size((AVPixelFormat)frame_->format, frame_->width, frame_->height, 1);
}

int
VideoFrame::format() const noexcept
{
    return frame_->format;
}

int
VideoFrame::width() const noexcept
{
    return frame_->width;
}

int
VideoFrame::height() const noexcept
{
    return frame_->height;
}

void
VideoFrame::setGeometry(int format, int width, int height) noexcept
{
    frame_->format = format;
    frame_->width = width;
    frame_->height = height;
}

void
VideoFrame::reserve(int format, int width, int height)
{
    auto libav_frame = frame_.get();

    if (allocated_) {
        // nothing to do if same properties
        if (width == libav_frame->width
            and height == libav_frame->height
            and format == libav_frame->format)
        av_frame_unref(libav_frame);
    }

    setGeometry(format, width, height);
    if (av_frame_get_buffer(libav_frame, 32))
        throw std::bad_alloc();
    allocated_ = true;
    releaseBufferCb_ = {};
}

void
VideoFrame::setFromMemory(uint8_t* ptr, int format, int width, int height) noexcept
{
    reset();
    setGeometry(format, width, height);
    if (not ptr)
        return;
    av_image_fill_arrays(frame_->data, frame_->linesize, (uint8_t*)ptr,
                         (AVPixelFormat)frame_->format, width, height, 1);
}

void
VideoFrame::setFromMemory(uint8_t* ptr, int format, int width, int height,
                          std::function<void(uint8_t*)> cb) noexcept
{
    setFromMemory(ptr, format, width, height);
    if (cb) {
        releaseBufferCb_ = cb;
        ptr_ = ptr;
    }
}

void
VideoFrame::setReleaseCb(std::function<void(uint8_t*)> cb) noexcept
{
    if (cb) {
        releaseBufferCb_ = cb;
    }
}


void
VideoFrame::noise()
{
    auto f = frame_.get();
    if (f->data[0] == nullptr)
        return;
    for (std::size_t i=0 ; i < size(); ++i) {
        f->data[0][i] = std::rand() & 255;
    }
}

VideoFrame&
VideoFrame::operator =(const VideoFrame& src)
{
    reserve(src.format(), src.width(), src.height());
    auto source = src.pointer();
    av_image_copy(frame_->data, frame_->linesize, (const uint8_t **)source->data,
                  source->linesize, (AVPixelFormat)frame_->format,
                  frame_->width, frame_->height);
    return *this;
}

VideoFrame* getNewFrame()
{
    if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
        return &input->getNewFrame();
    return nullptr;
}

void publishFrame()
{
    if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
        return input->publishFrame();
}

void
registerVideoHandlers(const std::map<std::string,
    std::shared_ptr<CallbackWrapperBase>>&handlers)
{
    registerSignalHandlers(handlers);
}

std::vector<std::string>
getDeviceList()
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getDeviceList();
}

VideoCapabilities
getCapabilities(const std::string& name)
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getCapabilities(name);
}

std::string
getDefaultDevice()
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getDefaultDevice();
}

void
setDefaultDevice(const std::string& name)
{
    RING_DBG("Setting default device to %s", name.c_str());
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.setDefaultDevice(name);
    ring::Manager::instance().saveConfig();
}

std::map<std::string, std::string>
getDeviceParams(const std::string& name)
{
    auto params = ring::Manager::instance().getVideoManager().videoDeviceMonitor.getDeviceParams(name);
    std::stringstream width, height, rate;
    width << params.width;
    height << params.height;
    rate << params.framerate;
    return {
        {"format", params.format},
        {"width", width.str()},
        {"height", height.str()},
        {"rate", rate.str()}
    };
}

std::map<std::string, std::string>
getSettings(const std::string& name)
{
    return ring::Manager::instance().getVideoManager().videoDeviceMonitor.getSettings(name).to_map();
}

void
applySettings(const std::string& name,
              const std::map<std::string, std::string>& settings)
{
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.applySettings(name, settings);
}

void
startCamera()
{
    ring::Manager::instance().getVideoManager().videoPreview = ring::getVideoCamera();
    ring::Manager::instance().getVideoManager().started = switchToCamera();
}

void
stopCamera()
{
    if (switchInput(""))
        ring::Manager::instance().getVideoManager().started = false;
    ring::Manager::instance().getVideoManager().videoPreview.reset();
}

std::string
startLocalRecorder(const bool& audioOnly, const std::string& filepath)
{
    if (!audioOnly && !ring::Manager::instance().getVideoManager().started) {
        RING_ERR("Couldn't start local video recorder (camera is not started)");
        return "";
    }

    std::unique_ptr<ring::LocalRecorder> rec;
    std::shared_ptr<ring::video::VideoInput> input = nullptr;
    if (!audioOnly) {
        input = std::static_pointer_cast<ring::video::VideoInput>(ring::getVideoCamera());
    }

    /* in case of audio-only recording, nullptr is passed and LocalRecorder will
       assume isAudioOnly_ = true, so no need to call Recordable::isAudioOnly(). */
    rec.reset(new ring::LocalRecorder(input));
    rec->setPath(filepath);

    // retrieve final path (containing file extension)
    auto path = rec->getPath();

    try {
        ring::LocalRecorderManager::instance().insertRecorder(path, std::move(rec));
    } catch (std::invalid_argument) {
        return "";
    }

    auto ret = ring::LocalRecorderManager::instance().getRecorderByPath(path)->startRecording();
    if (!ret) {
        ring::LocalRecorderManager::instance().removeRecorderByPath(filepath);
        return "";
    }

    return path;
}

void
stopLocalRecorder(const std::string& filepath)
{
    ring::LocalRecorder *rec = ring::LocalRecorderManager::instance().getRecorderByPath(filepath);
    if (!rec) {
        RING_WARN("Can't stop non existing local recorder.");
        return;
    }

    rec->stopRecording();
    ring::LocalRecorderManager::instance().removeRecorderByPath(filepath);
}

bool
switchInput(const std::string& resource)
{
    if (auto call = ring::Manager::instance().getCurrentCall()) {
        // TODO remove this part when clients are updated to use Callring::Manager::switchInput
        call->switchInput(resource);
        return true;
    } else {
        if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
            return input->switchInput(resource).valid();
        RING_WARN("Video input not initialized");
    }
    return false;
}

bool
switchToCamera()
{
    return switchInput(ring::Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice());
}

bool
hasCameraStarted()
{
    return ring::Manager::instance().getVideoManager().started;
}

void
registerSinkTarget(const std::string& sinkId, const SinkTarget& target)
{
   if (auto sink = ring::Manager::instance().getSinkClient(sinkId))
       sink->registerTarget(target);
   else
       RING_WARN("No sink found for id '%s'", sinkId.c_str());
}

bool
getDecodingAccelerated()
{
#ifdef RING_ACCEL
    return ring::Manager::instance().videoPreferences.getDecodingAccelerated();
#else
    return false;
#endif
}

void
setDecodingAccelerated(bool state)
{
#ifdef RING_ACCEL
    RING_DBG("%s hardware acceleration", (state ? "Enabling" : "Disabling"));
    ring::Manager::instance().videoPreferences.setDecodingAccelerated(state);
#endif
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
void
addVideoDevice(const std::string &node, std::vector<std::map<std::string, std::string>> const * devInfo)
{
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.addDevice(node, devInfo);
}

void
removeVideoDevice(const std::string &node)
{
    ring::Manager::instance().getVideoManager().videoDeviceMonitor.removeDevice(node);
}

void*
obtainFrame(int length)
{
    if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
        return (*input).obtainFrame(length);

    return nullptr;
}

void
releaseFrame(void* frame)
{
    if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
        (*input).releaseFrame(frame);
}
#endif

} // namespace DRing

namespace ring {

std::shared_ptr<video::VideoFrameActiveWriter>
getVideoCamera()
{
    auto& vmgr = Manager::instance().getVideoManager();
    if (auto input = vmgr.videoInput.lock())
        return input;

    vmgr.started = false;
    auto input = std::make_shared<video::VideoInput>();
    vmgr.videoInput = input;
    return input;
}

video::VideoDeviceMonitor&
getVideoDeviceMonitor()
{
    return Manager::instance().getVideoManager().videoDeviceMonitor;
}

} // namespace ring
