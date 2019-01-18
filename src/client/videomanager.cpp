/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "audio/ringbufferpool.h"
#include "dring/media_const.h"
#include "libav_utils.h"

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
    : frame_ {av_frame_alloc(), [](AVFrame* frame){ av_frame_free(&frame); }},
      packet_(nullptr, [](AVPacket* p) {
        if (p) {
            av_packet_unref(p);
            delete p;
        }
    })
{
    if (not frame_)
        throw std::bad_alloc();
}

void
MediaFrame::copyFrom(const MediaFrame& o)
{
    reset();
    av_frame_ref(frame_.get(), o.frame_.get());
}

void
MediaFrame::reset() noexcept
{
    av_frame_unref(frame_.get());
    packet_.reset();
}

void
MediaFrame::setPacket(std::unique_ptr<AVPacket, void(*)(AVPacket*)>&& pkt)
{
    packet_ = std::move(pkt);
}

AudioFrame::AudioFrame(const ring::AudioFormat& format, size_t nb_samples)
 : MediaFrame()
{
    setFormat(format);
    if (nb_samples)
        reserve(nb_samples);
}

void
AudioFrame::setFormat(const ring::AudioFormat& format)
{
    auto d = pointer();
    d->channels = format.nb_channels;
    d->channel_layout = av_get_default_channel_layout(format.nb_channels);
    d->sample_rate = format.sample_rate;
    d->format = format.sampleFormat;
}

void
AudioFrame::reserve(size_t nb_samples)
{
    if (nb_samples != 0) {
        auto d = pointer();
        d->nb_samples = nb_samples;
        int err;
        if ((err = av_frame_get_buffer(d, 0)) < 0) {
            throw std::bad_alloc();
        }
    }
}

void
AudioFrame::mix(const AudioFrame& frame)
{
    auto& f = *pointer();
    auto& fIn = *frame.pointer();
    if (f.channels != fIn.channels || f.format != fIn.format || f.sample_rate != fIn.sample_rate) {
        throw std::invalid_argument("Can't mix frames with different formats");
    }
    if (f.nb_samples == 0) {
        reserve(fIn.nb_samples);
        ring::libav_utils::fillWithSilence(&f);
    } else if (f.nb_samples != fIn.nb_samples) {
        throw std::invalid_argument("Can't mix frames with different length");
    }
    AVSampleFormat fmt = (AVSampleFormat)f.format;
    bool isPlanar = av_sample_fmt_is_planar(fmt);
    unsigned samplesPerChannel = isPlanar ? f.nb_samples : f.nb_samples * f.channels;
    unsigned channels = isPlanar ? f.channels : 1;
    if (fmt == AV_SAMPLE_FMT_S16 || fmt == AV_SAMPLE_FMT_S16P) {
        for (unsigned i=0; i < channels; i++) {
            auto c = (int16_t*)f.extended_data[i];
            auto cIn = (int16_t*)fIn.extended_data[i];
            for (unsigned s=0; s < samplesPerChannel; s++) {
                int32_t n = (int32_t)c[s] + (int32_t)cIn[s];
                n = std::min<int32_t>(n, std::numeric_limits<int16_t>::max());
                n = std::max<int32_t>(n, std::numeric_limits<int16_t>::min());
                c[s] = n;
            }
        }
    } else if (fmt == AV_SAMPLE_FMT_FLT || fmt == AV_SAMPLE_FMT_FLTP) {
        for (unsigned i=0; i < channels; i++) {
            auto c = (float*)f.extended_data[i];
            auto cIn = (float*)fIn.extended_data[i];
            for (unsigned s=0; s < samplesPerChannel; s++) {
                c[s] += cIn[s];
            }
        }
    } else {
        throw std::invalid_argument(std::string("Unsupported format for mixing: ") + av_get_sample_fmt_name(fmt));
    }
}

float
AudioFrame::calcRMS() const
{
    double rms = 0.0;
    auto fmt = static_cast<AVSampleFormat>(frame_->format);
    bool planar = av_sample_fmt_is_planar(fmt);
    int perChannel = planar ? frame_->nb_samples : frame_->nb_samples * frame_->channels;
    int channels = planar ? frame_->channels : 1;
    if (fmt == AV_SAMPLE_FMT_S16 || fmt == AV_SAMPLE_FMT_S16P) {
        for (int c = 0; c < channels; ++c) {
            auto buf = reinterpret_cast<int16_t*>(frame_->extended_data[c]);
            for (int i = 0; i < perChannel; ++i) {
                auto sample = buf[i] * 0.000030517578125f;
                rms += sample * sample;
            }
        }
    } else if (fmt == AV_SAMPLE_FMT_FLT || fmt == AV_SAMPLE_FMT_FLTP) {
        for (int c = 0; c < channels; ++c) {
            auto buf = reinterpret_cast<float*>(frame_->extended_data[c]);
            for (int i = 0; i < perChannel; ++i) {
                rms += buf[i] * buf[i];
            }
        }
    } else {
        // Should not happen
        RING_ERR() << "Unsupported format for getting volume level: " << av_get_sample_fmt_name(fmt);
        return 0.0;
    }
    // divide by the number of multi-byte samples
    return sqrt(rms / (frame_->nb_samples * frame_->channels));
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

void
VideoFrame::copyFrom(const VideoFrame& o)
{
    MediaFrame::copyFrom(o);
    ptr_ = o.ptr_;
    allocated_ = o.allocated_;
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

void
startAudioDevice()
{
    ring::Manager::instance().initAudioDriver();
    ring::Manager::instance().startAudioDriverStream();
    ring::Manager::instance().getVideoManager().audioPreview = ring::getAudioInput(ring::RingBufferPool::DEFAULT_ID);
}

void
stopAudioDevice()
{
    ring::Manager::instance().getVideoManager().audioPreview.reset();
    ring::Manager::instance().checkAudio(); // stops audio layer if no calls
}

std::string
startLocalRecorder(const bool& audioOnly, const std::string& filepath)
{
    if (!audioOnly && !ring::Manager::instance().getVideoManager().started) {
        RING_ERR("Couldn't start local video recorder (camera is not started)");
        return "";
    }

    auto rec = std::make_unique<ring::LocalRecorder>(audioOnly);
    rec->setPath(filepath);

    // retrieve final path (containing file extension)
    auto path = rec->getPath();

    try {
        ring::LocalRecorderManager::instance().insertRecorder(path, std::move(rec));
    } catch (const std::invalid_argument&) {
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
        bool ret = true;
        if (auto input = ring::Manager::instance().getVideoManager().videoInput.lock())
            ret = input->switchInput(resource).valid();
        else
            RING_WARN("Video input not initialized");

        if (auto input = ring::getAudioInput(ring::RingBufferPool::DEFAULT_ID))
            ret &= input->switchInput(resource).valid();
        return ret;
    }
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

std::map<std::string, std::string>
getRenderer(const std::string& callId)
{
   if (auto sink = ring::Manager::instance().getSinkClient(callId))
       return {
           {DRing::Media::Details::CALL_ID,  callId},
           {DRing::Media::Details::SHM_PATH, sink->openedName()},
           {DRing::Media::Details::WIDTH,    ring::to_string(sink->getWidth())},
           {DRing::Media::Details::HEIGHT,   ring::to_string(sink->getHeight())},
       };
   else
       return {
           {DRing::Media::Details::CALL_ID,  callId},
           {DRing::Media::Details::SHM_PATH, ""},
           {DRing::Media::Details::WIDTH,    "0"},
           {DRing::Media::Details::HEIGHT,   "0"},
       };
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

std::shared_ptr<AudioInput>
getAudioInput(const std::string& id)
{
    auto& vmgr = Manager::instance().getVideoManager();
    std::lock_guard<std::mutex> lk(vmgr.audioMutex);

    // erase expired audio inputs
    for (auto it = vmgr.audioInputs.cbegin(); it != vmgr.audioInputs.cend();) {
        if (it->second.expired())
            it = vmgr.audioInputs.erase(it);
        else
            ++it;
    }

    auto it = vmgr.audioInputs.find(id);
    if (it != vmgr.audioInputs.end()) {
        if (auto input = it->second.lock()) {
            return input;
        }
    }

    auto input = std::make_shared<AudioInput>(id);
    vmgr.audioInputs[id] = input;
    return input;
}

} // namespace ring
