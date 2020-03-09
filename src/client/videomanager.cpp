/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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
#include "call_const.h"
#include "system_codec_container.h"

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
    if (o.frame_)
        av_frame_ref(frame_.get(), o.frame_.get());
    if (o.packet_) {
        packet_.reset(av_packet_alloc());
        av_packet_ref(packet_.get(), o.packet_.get());
    }
}

void
MediaFrame::reset() noexcept
{
    if (frame_)
        av_frame_unref(frame_.get());
    packet_.reset();
}

void
MediaFrame::setPacket(std::unique_ptr<AVPacket, void(*)(AVPacket*)>&& pkt)
{
    packet_ = std::move(pkt);
}

AudioFrame::AudioFrame(const jami::AudioFormat& format, size_t nb_samples)
 : MediaFrame()
{
    setFormat(format);
    if (nb_samples)
        reserve(nb_samples);
}

void
AudioFrame::setFormat(const jami::AudioFormat& format)
{
    auto d = pointer();
    d->channels = format.nb_channels;
    d->channel_layout = av_get_default_channel_layout(format.nb_channels);
    d->sample_rate = format.sample_rate;
    d->format = format.sampleFormat;
}

jami::AudioFormat
AudioFrame::getFormat() const
{
    return {
        (unsigned)frame_->sample_rate,
        (unsigned)frame_->channels,
        (AVSampleFormat)frame_->format
    };
}

size_t
AudioFrame::getFrameSize() const
{
    return frame_->nb_samples;
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
        jami::libav_utils::fillWithSilence(&f);
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
        JAMI_ERR() << "Unsupported format for getting volume level: " << av_get_sample_fmt_name(fmt);
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
                          const std::function<void(uint8_t*)>& cb) noexcept
{
    setFromMemory(ptr, format, width, height);
    if (cb) {
        releaseBufferCb_ = cb;
        ptr_ = ptr;
    }
}

void
VideoFrame::setReleaseCb(const std::function<void(uint8_t*)>& cb) noexcept
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
    if (auto input = jami::Manager::instance().getVideoManager().videoInput.lock())
        return &input->getNewFrame();
    return nullptr;
}

void publishFrame()
{
    if (auto input = jami::Manager::instance().getVideoManager().videoInput.lock())
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
    return jami::Manager::instance().getVideoManager().videoDeviceMonitor.getDeviceList();
}

VideoCapabilities
getCapabilities(const std::string& deviceId)
{
    return jami::Manager::instance().getVideoManager().videoDeviceMonitor.getCapabilities(deviceId);
}

std::string
getDefaultDevice()
{
    return jami::Manager::instance().getVideoManager().videoDeviceMonitor.getDefaultDevice();
}

void
setDefaultDevice(const std::string& deviceId)
{
    JAMI_DBG("Setting default device to %s", deviceId.c_str());
    jami::Manager::instance().getVideoManager().videoDeviceMonitor.setDefaultDevice(deviceId);
    jami::Manager::instance().saveConfig();
}

void
setDeviceOrientation(const std::string& deviceId, int angle)
{
    jami::Manager::instance().getVideoManager().setDeviceOrientation(deviceId, angle);
}

std::map<std::string, std::string>
getDeviceParams(const std::string& deviceId)
{
    auto params = jami::Manager::instance().getVideoManager().videoDeviceMonitor.getDeviceParams(deviceId);
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
getSettings(const std::string& deviceId)
{
    return jami::Manager::instance().getVideoManager().videoDeviceMonitor.getSettings(deviceId).to_map();
}

void
applySettings(const std::string& deviceId,
              const std::map<std::string, std::string>& settings)
{
    jami::Manager::instance().getVideoManager().videoDeviceMonitor.applySettings(deviceId, settings);
    jami::Manager::instance().saveConfig();
}

void
startCamera()
{
    jami::Manager::instance().getVideoManager().videoPreview = jami::getVideoCamera();
    jami::Manager::instance().getVideoManager().started = switchToCamera();
}

void
stopCamera()
{
    if (switchInput(""))
        jami::Manager::instance().getVideoManager().started = false;
    jami::Manager::instance().getVideoManager().videoPreview.reset();
}

void
startAudioDevice()
{
    // Don't start audio layer if already done
    auto audioLayer = jami::Manager::instance().getAudioDriver();
    if (!audioLayer)
        jami::Manager::instance().initAudioDriver();
    if (!audioLayer->isStarted())
        jami::Manager::instance().startAudioDriverStream();
    jami::Manager::instance().getVideoManager().audioPreview = jami::getAudioInput(jami::RingBufferPool::DEFAULT_ID);
}

void
stopAudioDevice()
{
    jami::Manager::instance().getVideoManager().audioPreview.reset();
    jami::Manager::instance().checkAudio(); // stops audio layer if no calls
}

std::string
startLocalRecorder(const bool& audioOnly, const std::string& filepath)
{
    if (!audioOnly && !jami::Manager::instance().getVideoManager().started) {
        JAMI_ERR("Couldn't start local video recorder (camera is not started)");
        return "";
    }

    auto rec = std::make_unique<jami::LocalRecorder>(audioOnly);
    rec->setPath(filepath);

    // retrieve final path (containing file extension)
    auto path = rec->getPath();

    try {
        jami::LocalRecorderManager::instance().insertRecorder(path, std::move(rec));
    } catch (const std::invalid_argument&) {
        return "";
    }

    auto ret = jami::LocalRecorderManager::instance().getRecorderByPath(path)->startRecording();
    if (!ret) {
        jami::LocalRecorderManager::instance().removeRecorderByPath(filepath);
        return "";
    }

    return path;
}

void
stopLocalRecorder(const std::string& filepath)
{
    jami::LocalRecorder *rec = jami::LocalRecorderManager::instance().getRecorderByPath(filepath);
    if (!rec) {
        JAMI_WARN("Can't stop non existing local recorder.");
        return;
    }

    rec->stopRecording();
    jami::LocalRecorderManager::instance().removeRecorderByPath(filepath);
}

bool
switchInput(const std::string& resource)
{
    if (auto call = jami::Manager::instance().getCurrentCall()) {
        if (call->hasVideo()) {
            // TODO remove this part when clients are updated to use Calljami::Manager::switchInput
            call->switchInput(resource);
            return true;
        }
    }
    bool ret = true;
    if (auto input = jami::Manager::instance().getVideoManager().videoInput.lock())
        ret = input->switchInput(resource).valid();
    else
        JAMI_WARN("Video input not initialized");

    if (auto input = jami::getAudioInput(jami::RingBufferPool::DEFAULT_ID))
        ret &= input->switchInput(resource).valid();
    return ret;
}

bool
switchToCamera()
{
    return switchInput(jami::Manager::instance().getVideoManager().videoDeviceMonitor.getMRLForDefaultDevice());
}

bool
hasCameraStarted()
{
    return jami::Manager::instance().getVideoManager().started;
}

void
registerSinkTarget(const std::string& sinkId, const SinkTarget& target)
{
   if (auto sink = jami::Manager::instance().getSinkClient(sinkId))
       sink->registerTarget(target);
   else
       JAMI_WARN("No sink found for id '%s'", sinkId.c_str());
}

void
registerAVSinkTarget(const std::string& sinkId, const AVSinkTarget& target)
{
   if (auto sink = jami::Manager::instance().getSinkClient(sinkId))
       sink->registerAVTarget(target);
   else
       JAMI_WARN("No sink found for id '%s'", sinkId.c_str());
}

std::map<std::string, std::string>
getRenderer(const std::string& callId)
{
   if (auto sink = jami::Manager::instance().getSinkClient(callId))
       return {
           {DRing::Media::Details::CALL_ID,  callId},
           {DRing::Media::Details::SHM_PATH, sink->openedName()},
           {DRing::Media::Details::WIDTH,    std::to_string(sink->getWidth())},
           {DRing::Media::Details::HEIGHT,   std::to_string(sink->getHeight())},
       };
   else
       return {
           {DRing::Media::Details::CALL_ID,  callId},
           {DRing::Media::Details::SHM_PATH, ""},
           {DRing::Media::Details::WIDTH,    "0"},
           {DRing::Media::Details::HEIGHT,   "0"},
       };
}

std::string
createMediaPlayer(const std::string& path)
{
    return jami::createMediaPlayer(path);
}

bool
pausePlayer(const std::string& id, bool pause)
{
    return jami::pausePlayer(id, pause);
}

bool
closePlayer(const std::string& id)
{
    return jami::closePlayer(id);
}

bool
mutePlayerAudio(const std::string& id, bool mute)
{
    return jami::mutePlayerAudio(id, mute);
}

bool
playerSeekToTime(const std::string& id, int time)
{
    return jami::playerSeekToTime(id, time);
}

int64_t
getPlayerPosition(const std::string& id)
{
    return jami::getPlayerPosition(id);
}

bool
getDecodingAccelerated()
{
#ifdef RING_ACCEL
    return jami::Manager::instance().videoPreferences.getDecodingAccelerated();
#else
    return false;
#endif
}

void
setDecodingAccelerated(bool state)
{
#ifdef RING_ACCEL
    JAMI_DBG("%s hardware acceleration", (state ? "Enabling" : "Disabling"));
    jami::Manager::instance().videoPreferences.setDecodingAccelerated(state);
    jami::Manager::instance().saveConfig();
#endif
}

bool
getEncodingAccelerated()
{
#ifdef RING_ACCEL
    return jami::Manager::instance().videoPreferences.getEncodingAccelerated();
#else
    return false;
#endif
}

void
setEncodingAccelerated(bool state)
{
#ifdef RING_ACCEL
    JAMI_DBG("%s hardware acceleration", (state ? "Enabling" : "Disabling"));
    jami::Manager::instance().videoPreferences.setEncodingAccelerated(state);
    jami::Manager::instance().saveConfig();
#endif
    for (const auto& acc : jami::Manager::instance().getAllAccounts()) {
        if (state)
            acc->setCodecActive(AV_CODEC_ID_HEVC);
        else
            acc->setCodecInactive(AV_CODEC_ID_HEVC);
        // Update and sort codecs
        acc->setActiveCodecs(acc->getActiveCodecs());
        jami::Manager::instance().saveConfig(acc);
    }
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
void
addVideoDevice(const std::string &node, std::vector<std::map<std::string, std::string>> const * devInfo)
{
    jami::Manager::instance().getVideoManager().videoDeviceMonitor.addDevice(node, devInfo);
}

void
removeVideoDevice(const std::string &node)
{
    jami::Manager::instance().getVideoManager().videoDeviceMonitor.removeDevice(node);
}
#endif

} // namespace DRing

namespace jami {

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

std::shared_ptr<video::VideoInput>
getVideoInput(const std::string& id, video::VideoInputMode inputMode)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto it = vmgr.videoInputs.find(id);
    if (it != vmgr.videoInputs.end()) {
        if (auto input = it->second.lock()) {
            return input;
        }
    }

    auto input = std::make_shared<video::VideoInput>(inputMode);
    vmgr.videoInputs[id] = input;
    return input;
}

void
VideoManager::setDeviceOrientation(const std::string& deviceId, int angle)
{
    videoDeviceMonitor.setDeviceOrientation(deviceId, angle);
}

bool
VideoManager::hasRunningPlayers()
{
    auto& vmgr = Manager::instance().getVideoManager();
    return !vmgr.mediaPlayers.empty();
}

std::shared_ptr<MediaPlayer>
getMediaPlayer(const std::string& id)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto it = vmgr.mediaPlayers.find(id);
      if (it != vmgr.mediaPlayers.end()) {
          return it->second;
      }
    return {};
}

std::string
createMediaPlayer(const std::string& path)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto player = std::make_shared<MediaPlayer>(path);
    if (!player->isInputValid()) {
        return "";
    }
    auto playerId = player.get()->getId();
    vmgr.mediaPlayers[playerId] = player;
    return playerId;
}

bool
pausePlayer(const std::string& id, bool pause)
{
    auto player = getMediaPlayer(id);
    if (player) {
        player->pause(pause);
        return true;
    }
    return false;
}

bool
closePlayer(const std::string& id)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto player = getMediaPlayer(id);
    if (player) {
        vmgr.mediaPlayers.erase(id);
        if (vmgr.mediaPlayers.empty()) {
            jami::Manager::instance().checkAudio();
        }
        return true;
    }
    return false;
}

bool
mutePlayerAudio(const std::string& id, bool mute)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto player = getMediaPlayer(id);
    if (player) {
        player->muteAudio(mute);
        return true;
    }
    return false;
}

bool
playerSeekToTime(const std::string& id, int time)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto player = getMediaPlayer(id);
    if (player) {
        return player->seekToTime(time);
    }
    return false;
}

int64_t
getPlayerPosition(const std::string& id)
{
    auto& vmgr = Manager::instance().getVideoManager();
    auto player = getMediaPlayer(id);
    if (player) {
        return player->getPlayerPosition();
    }
    return -1;
}

} // namespace jami
