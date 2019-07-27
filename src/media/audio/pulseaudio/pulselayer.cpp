/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Андрей Лухнов <aol.nnov@gmail.com>
 *  Author: Adrien Beraud <adrien.beraud@gmail.com>
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

#include "compiler_intrinsics.h"
#include "audiostream.h"
#include "pulselayer.h"
#include "audio/dcblocker.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "libav_utils.h"
#include "logger.h"
#include "manager.h"

#include <algorithm> // for std::find
#include <stdexcept>

#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <cstring>

#include <regex>

// uncomment to log pulseaudio sink and sources
//#define PA_LOG_SINK_SOURCES

namespace jami {

static const std::regex PA_EC_SUFFIX {"\\.echo-cancel(?:\\..+)?$"};

PulseMainLoopLock::PulseMainLoopLock(pa_threaded_mainloop *loop) : loop_(loop)
{
    pa_threaded_mainloop_lock(loop_);
}

PulseMainLoopLock::~PulseMainLoopLock()
{
    pa_threaded_mainloop_unlock(loop_);
}

PulseLayer::PulseLayer(AudioPreference &pref)
    : AudioLayer(pref)
    , playback_()
    , record_()
    , ringtone_()
    , mainloop_(pa_threaded_mainloop_new(), pa_threaded_mainloop_free)
    , preference_(pref)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{
    if (!mainloop_)
        throw std::runtime_error("Couldn't create pulseaudio mainloop");

    if (pa_threaded_mainloop_start(mainloop_.get()) < 0)
        throw std::runtime_error("Failed to start pulseaudio mainloop");

    PulseMainLoopLock lock(mainloop_.get());

    std::unique_ptr<pa_proplist, decltype(pa_proplist_free)&> pl (pa_proplist_new(), pa_proplist_free);
    pa_proplist_sets(pl.get(), PA_PROP_MEDIA_ROLE, "phone");

    context_ = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mainloop_.get()), PACKAGE_NAME, pl.get());
    if (!context_)
        throw std::runtime_error("Couldn't create pulseaudio context");

    pa_context_set_state_callback(context_, context_state_callback, this);

    if (pa_context_connect(context_, nullptr , PA_CONTEXT_NOAUTOSPAWN , nullptr) < 0)
        throw std::runtime_error("Could not connect pulseaudio context to the server");

    // wait until context is ready
    for (;;) {
        pa_context_state_t context_state = pa_context_get_state(context_);
        if (not PA_CONTEXT_IS_GOOD(context_state))
            throw std::runtime_error("Pulse audio context is bad");
        if (context_state == PA_CONTEXT_READY)
            break;
        pa_threaded_mainloop_wait(mainloop_.get());
    }
}

PulseLayer::~PulseLayer()
{
    if (streamStarter_.joinable())
        streamStarter_.join();

    disconnectAudioStream();

    {
        PulseMainLoopLock lock(mainloop_.get());
        pa_context_set_state_callback(context_, NULL, NULL);
        pa_context_set_subscribe_callback(context_, NULL, NULL);
        pa_context_disconnect(context_);
        pa_context_unref(context_);
    }

    if (subscribeOp_)
        pa_operation_unref(subscribeOp_);
}

void PulseLayer::context_state_callback(pa_context* c, void *user_data)
{
    PulseLayer* pulse = static_cast<PulseLayer*>(user_data);
    if (c and pulse)
        pulse->contextStateChanged(c);
}

void PulseLayer::contextStateChanged(pa_context* c)
{
    const pa_subscription_mask_t mask = (pa_subscription_mask_t)
                                        (PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            JAMI_DBG("Waiting....");
            break;

        case PA_CONTEXT_READY:
            JAMI_DBG("Connection to PulseAudio server established");
            pa_threaded_mainloop_signal(mainloop_.get(), 0);
            subscribeOp_ = pa_context_subscribe(c, mask, nullptr, this);
            pa_context_set_subscribe_callback(c, context_changed_callback, this);
            updateSinkList();
            updateSourceList();
            updateServerInfo();
            waitForDeviceList();
            break;

        case PA_CONTEXT_TERMINATED:
            if (subscribeOp_) {
                pa_operation_unref(subscribeOp_);
                subscribeOp_ = nullptr;
            }
            break;

        case PA_CONTEXT_FAILED:
        default:
            JAMI_ERR("%s" , pa_strerror(pa_context_errno(c)));
            pa_threaded_mainloop_signal(mainloop_.get(), 0);
            break;
    }
}

void PulseLayer::updateSinkList()
{
    std::unique_lock<std::mutex> lk(mutex_);
    if (not enumeratingSinks_) {
        JAMI_DBG("Updating PulseAudio sink list");
        enumeratingSinks_ = true;
        sinkList_.clear();
        sinkList_.emplace_back();
        sinkList_.front().channel_map.channels = std::min(defaultAudioFormat_.nb_channels, 2u);
        if (auto op = pa_context_get_sink_info_list(context_, sink_input_info_callback, this))
            pa_operation_unref(op);
        else
            enumeratingSinks_ = false;
    }
}

void PulseLayer::updateSourceList()
{
    std::unique_lock<std::mutex> lk(mutex_);
    if (not enumeratingSources_) {
        JAMI_DBG("Updating PulseAudio source list");
        enumeratingSources_ = true;
        sourceList_.clear();
        sourceList_.emplace_back();
        sourceList_.front().channel_map.channels = std::min(defaultAudioFormat_.nb_channels, 2u);
        if (auto op = pa_context_get_source_info_list(context_, source_input_info_callback, this))
            pa_operation_unref(op);
        else
            enumeratingSources_ = false;
    }
}

void PulseLayer::updateServerInfo()
{
    std::unique_lock<std::mutex> lk(mutex_);
    if (not gettingServerInfo_) {
        JAMI_DBG("Updating PulseAudio server infos");
        gettingServerInfo_ = true;
        if (auto op = pa_context_get_server_info(context_, server_info_callback, this))
            pa_operation_unref(op);
        else
            gettingServerInfo_ = false;
    }
}

bool PulseLayer::inSinkList(const std::string &deviceName)
{
    return std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::NameComparator(deviceName)) != sinkList_.end();
}

bool PulseLayer::inSourceList(const std::string &deviceName)
{
    return std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::NameComparator(deviceName)) != sourceList_.end();
}

std::vector<std::string> PulseLayer::getCaptureDeviceList() const
{
    std::vector<std::string> names;
    names.reserve(sourceList_.size());
    for (const auto& s : sourceList_)
        names.emplace_back(s.description);
    return names;
}

std::vector<std::string> PulseLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> names;
    names.reserve(sinkList_.size());
    for (const auto& s : sinkList_)
        names.emplace_back(s.description);
    return names;
}

int PulseLayer::getAudioDeviceIndex(const std::string& descr, DeviceType type) const
{
    switch (type) {
    case DeviceType::PLAYBACK:
    case DeviceType::RINGTONE:
        return std::distance(sinkList_.begin(), std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::DescriptionComparator(descr)));
    case DeviceType::CAPTURE:
        return std::distance(sourceList_.begin(), std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::DescriptionComparator(descr)));
    default:
        JAMI_ERR("Unexpected device type");
        return 0;
    }
}

int PulseLayer::getAudioDeviceIndexByName(const std::string& name, DeviceType type) const
{
    if (name.empty())
        return 0;
    switch (type) {
    case DeviceType::PLAYBACK:
    case DeviceType::RINGTONE:
        return std::distance(sinkList_.begin(), std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::NameComparator(name)));
    case DeviceType::CAPTURE:
        return std::distance(sourceList_.begin(), std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::NameComparator(name)));
    default:
        JAMI_ERR("Unexpected device type");
        return 0;
    }
}

bool endsWith(const std::string& str, const std::string& ending)
{
    if (ending.size() >= str.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}

/**
 * Find default device for PulseAudio to open, filter monitors and EC.
 */
const PaDeviceInfos* findBest(const std::vector<PaDeviceInfos>& list) {
    if (list.empty()) return nullptr;
    for (const auto& info : list)
        if (info.monitor_of == PA_INVALID_INDEX)
            return &info;
    return &list[0];
}

const PaDeviceInfos* PulseLayer::getDeviceInfos(const std::vector<PaDeviceInfos>& list, const std::string& name) const
{
    auto dev_info = std::find_if(list.begin(), list.end(), PaDeviceInfos::NameComparator(name));
    if (dev_info == list.end()) {
        JAMI_WARN("Preferred device %s not found in device list, selecting default %s instead.",
                        name.c_str(), list.front().name.c_str());
        return &list.front();
    }
    return &(*dev_info);
}

std::string PulseLayer::getAudioDeviceName(int index, DeviceType type) const
{
    switch (type) {
        case DeviceType::PLAYBACK:
        case DeviceType::RINGTONE:
            if (index < 0 or static_cast<size_t>(index) >= sinkList_.size()) {
                JAMI_ERR("Index %d out of range", index);
                return "";
            }
            return sinkList_[index].name;

        case DeviceType::CAPTURE:
            if (index < 0 or static_cast<size_t>(index) >= sourceList_.size()) {
                JAMI_ERR("Index %d out of range", index);
                return "";
            }
            return sourceList_[index].name;

        default:
            // Should never happen
            JAMI_ERR("Unexpected type");
            return "";
    }
}

void PulseLayer::createStreams(pa_context* c)
{
    hardwareFormatAvailable(defaultAudioFormat_);

    auto onReady = [this] {
        std::unique_lock<std::mutex> lk(mutex_);
        bool playbackReady = not playback_ or playback_->isReady();
        bool ringtoneReady = not ringtone_ or ringtone_->isReady();
        bool recordReady = not record_ or record_->isReady();
        if (playbackReady and recordReady and ringtoneReady) {
            JAMI_DBG("All streams ready, starting !");
            if (playback_) playback_->start();
            if (ringtone_) ringtone_->start();
            if (record_) record_->start();
            status_ = Status::Started;
            startedCv_.notify_all();
        }
    };

    // Create playback stream
    if (auto dev_infos = getDeviceInfos(sinkList_, getPreferredPlaybackDevice())) {
        playback_.reset(new AudioStream(c, mainloop_.get(), "Playback", StreamType::Playback, audioFormat_.sample_rate, dev_infos, true, onReady));
        pa_stream_set_write_callback(playback_->stream(), [](pa_stream * /*s*/, size_t /*bytes*/, void* userdata) {
            static_cast<PulseLayer*>(userdata)->writeToSpeaker();
        }, this);
    }

    // Create ringtone stream
    // Echo canceling is not enabled for ringtone, because PA can only cancel a single output source with an input source
    if (auto dev_infos = getDeviceInfos(sinkList_, getPreferredRingtoneDevice())) {
        ringtone_.reset(new AudioStream(c, mainloop_.get(), "Ringtone", StreamType::Ringtone, audioFormat_.sample_rate, dev_infos, false, onReady));
        pa_stream_set_write_callback(ringtone_->stream(), [](pa_stream * /*s*/, size_t /*bytes*/, void* userdata) {
            static_cast<PulseLayer*>(userdata)->ringtoneToSpeaker();
        }, this);
    }

    // Create capture stream
    if (auto dev_infos = getDeviceInfos(sourceList_, getPreferredCaptureDevice())) {
        record_.reset(new AudioStream(c, mainloop_.get(), "Capture", StreamType::Capture, audioFormat_.sample_rate, dev_infos, true, onReady));
        pa_stream_set_read_callback(record_->stream() , [](pa_stream * /*s*/, size_t /*bytes*/, void* userdata) {
            static_cast<PulseLayer*>(userdata)->readFromMic();
        }, this);
    }

    pa_threaded_mainloop_signal(mainloop_.get(), 0);

    flush();
}

void PulseLayer::disconnectAudioStream()
{
    playback_.reset();
    ringtone_.reset();
    record_.reset();
}

void PulseLayer::startStream()
{
    std::unique_lock<std::mutex> lk(mutex_);
    startedCv_.wait(lk, [this] {
        return !(enumeratingSinks_ or enumeratingSources_ or gettingServerInfo_);
    });
    if (status_ != Status::Idle)
        return;
    status_ = Status::Starting;

    // Create Streams
    if (!playback_ or !record_)
        createStreams(context_);

    // Flush outside the if statement: every time start stream is
    // called is to notify a new event
    flush();

    startedCv_.notify_all();
}

void
PulseLayer::stopStream()
{
    std::unique_lock<std::mutex> lk(mutex_);
    startedCv_.wait(lk, [this] {
        return !(enumeratingSinks_ or enumeratingSources_ or gettingServerInfo_);
    });
    if (status_ != Status::Started)
        return;

    {
        PulseMainLoopLock lock(mainloop_.get());

        if (playback_)
            pa_stream_flush(playback_->stream(), nullptr, nullptr);

        if (record_)
            pa_stream_flush(record_->stream(), nullptr, nullptr);
    }

    disconnectAudioStream();
    status_ = Status::Idle;
    startedCv_.notify_all();
}

void PulseLayer::writeToSpeaker()
{
    if (!playback_ or !playback_->isReady())
        return;

    // available bytes to be written in pulseaudio internal buffer
    size_t writableBytes = pa_stream_writable_size(playback_->stream());
    if (writableBytes == 0)
        return;

    const auto& buff = getToPlay(playback_->format(), writableBytes / playback_->frameSize());

    AudioSample* data = nullptr;
    pa_stream_begin_write(playback_->stream(), (void**)&data, &writableBytes);

    if (not buff or isPlaybackMuted_)
        memset(data, 0, writableBytes);
    else
        std::memcpy(data, buff->pointer()->data[0], buff->pointer()->nb_samples * playback_->frameSize());

    pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
}

void PulseLayer::readFromMic()
{
    if (!record_ or !record_->isReady())
        return;

    const char *data = nullptr;
    size_t bytes;
    if (pa_stream_peek(record_->stream() , (const void**) &data , &bytes) < 0 or !data)
        return;

    if (bytes == 0)
        return;

    size_t sample_size = record_->frameSize();
    const size_t samples = bytes / sample_size;

    auto out = std::make_unique<AudioFrame>(record_->format(), samples);
    if (isCaptureMuted_)
        libav_utils::fillWithSilence(out->pointer());
    else
        std::memcpy(out->pointer()->data[0], data, bytes);

    if (pa_stream_drop(record_->stream()) < 0)
        JAMI_ERR("Capture stream drop failed: %s" , pa_strerror(pa_context_errno(context_)));

    //dcblocker_.process(*out);
    mainRingBuffer_->put(std::move(out));
}

void PulseLayer::ringtoneToSpeaker()
{
    if (!ringtone_ or !ringtone_->isReady())
        return;

    size_t bytes = pa_stream_writable_size(ringtone_->stream());
    if (bytes == 0)
        return;

    const auto& buff = getToRing(ringtone_->format(), bytes / ringtone_->frameSize());

    AudioSample* data;
    pa_stream_begin_write(ringtone_->stream(), (void**)&data, &bytes);

    if (not buff or isRingtoneMuted_)
        memset(data, 0, bytes);
    else
        std::memcpy(data, buff->pointer()->data[0], buff->pointer()->nb_samples * playback_->frameSize());

    pa_stream_write(ringtone_->stream(), data, bytes, nullptr, 0, PA_SEEK_RELATIVE);
}


std::string stripEchoSufix(const std::string& deviceName) {
    return std::regex_replace(deviceName, PA_EC_SUFFIX, "");
}

void
PulseLayer::context_changed_callback(pa_context* c,
                                     pa_subscription_event_type_t type,
                                     uint32_t idx, void *userdata)
{
    static_cast<PulseLayer*>(userdata)->contextChanged(c, type, idx);
}

void
PulseLayer::contextChanged(pa_context* c UNUSED, pa_subscription_event_type_t type,
                           uint32_t idx UNUSED)
{
    bool reset = false;

    switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
                case PA_SUBSCRIPTION_EVENT_NEW:
                case PA_SUBSCRIPTION_EVENT_REMOVE:
                    updateSinkList();
                    reset = true;
                default:
                    break;
            }

            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
                case PA_SUBSCRIPTION_EVENT_NEW:
                case PA_SUBSCRIPTION_EVENT_REMOVE:
                    updateSourceList();
                    reset = true;
                default:
                    break;
            }

            break;

        default:
            JAMI_DBG("Unhandled event type 0x%x", type);
            break;
    }

    if (reset) {
        updateServerInfo();
        waitForDeviceList();
    }
}

void PulseLayer::waitForDeviceList()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (waitingDeviceList_)
        return;
    waitingDeviceList_ = true;
    if (streamStarter_.joinable())
        streamStarter_.join();
    streamStarter_ = std::thread([this]() mutable {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            startedCv_.wait(lock, [&]() {
                return not enumeratingSources_ and not enumeratingSinks_ and not gettingServerInfo_;
            });
        }
        devicesChanged();
        waitingDeviceList_ = false;
        if (status_ != Status::Starting)
            return;

        // If a current device changed, restart streams
        auto playbackInfo = getDeviceInfos(sinkList_, getPreferredPlaybackDevice());
        bool playbackDeviceChanged = !playback_ or (!playbackInfo->name.empty() and
                                                     playbackInfo->name != stripEchoSufix(playback_->getDeviceName()));

        auto recordInfo = getDeviceInfos(sourceList_, getPreferredCaptureDevice());
        bool recordDeviceChanged = !record_ or (!recordInfo->name.empty() and
                                                 recordInfo->name != stripEchoSufix(record_->getDeviceName()));

        if (playbackDeviceChanged or recordDeviceChanged) {
            JAMI_WARN("Audio devices changed, restarting streams.");
            stopStream();
            startStream();
        } else {
            JAMI_WARN("Staying on \n %s \n %s", playback_->getDeviceName().c_str(), record_->getDeviceName().c_str());
            status_ = Status::Started;
            startedCv_.notify_all();
        }
    });
}

void PulseLayer::server_info_callback(pa_context*, const pa_server_info *i, void *userdata)
{
    if (!i) return;
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    JAMI_DBG("PulseAudio server info:\n"
          "    Server name: %s\n"
          "    Server version: %s\n"
          "    Default Sink %s\n"
          "    Default Source %s\n"
          "    Default Sample Specification: %s\n"
          "    Default Channel Map: %s\n",
          i->server_name,
          i->server_version,
          i->default_sink_name,
          i->default_source_name,
          pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
          pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map));

    PulseLayer *context = static_cast<PulseLayer*>(userdata);
    context->defaultSink_ = {};
    context->defaultSource_ = {};
    context->defaultAudioFormat_ = {i->sample_spec.rate, i->sample_spec.channels};
    if (not context->sinkList_.empty())
        context->sinkList_.front().channel_map.channels = std::min(i->sample_spec.channels, (uint8_t)2);
    if (not context->sourceList_.empty())
        context->sourceList_.front().channel_map.channels = std::min(i->sample_spec.channels, (uint8_t)2);
    {
        std::lock_guard<std::mutex> lk(context->mutex_);
        context->gettingServerInfo_ = false;
    }
    context->startedCv_.notify_all();
}

void PulseLayer::source_input_info_callback(pa_context *c UNUSED, const pa_source_info *i, int eol, void *userdata)
{
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        {
            std::lock_guard<std::mutex> lk(context->mutex_);
            context->enumeratingSources_ = false;
        }
        context->startedCv_.notify_all();
        return;
    }
#ifdef PA_LOG_SINK_SOURCES
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    JAMI_DBG("Source %u\n"
          "    Name: %s\n"
          "    Driver: %s\n"
          "    Description: %s\n"
          "    Sample Specification: %s\n"
          "    Channel Map: %s\n"
          "    Owner Module: %u\n"
          "    Volume: %s\n"
          "    Monitor if Sink: %u\n"
          "    Latency: %0.0f usec\n"
          "    Flags: %s%s%s\n",
          i->index,
          i->name,
          i->driver,
          i->description,
          pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
          pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map),
          i->owner_module,
          i->mute ? "muted" : pa_cvolume_snprint(cv, sizeof(cv), &i->volume),
          i->monitor_of_sink,
          (double) i->latency,
          i->flags & PA_SOURCE_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
          i->flags & PA_SOURCE_LATENCY ? "LATENCY " : "",
          i->flags & PA_SOURCE_HARDWARE ? "HARDWARE" : "");
#endif
    if (not context->inSourceList(i->name)) {
        context->sourceList_.push_back(*i);
    }
}

void PulseLayer::sink_input_info_callback(pa_context *c UNUSED, const pa_sink_info *i, int eol, void *userdata)
{
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        {
            std::lock_guard<std::mutex> lk(context->mutex_);
            context->enumeratingSinks_ = false;
        }
        context->startedCv_.notify_all();
        return;
    }
#ifdef PA_LOG_SINK_SOURCES
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    JAMI_DBG("Sink %u\n"
          "    Name: %s\n"
          "    Driver: %s\n"
          "    Description: %s\n"
          "    Sample Specification: %s\n"
          "    Channel Map: %s\n"
          "    Owner Module: %u\n"
          "    Volume: %s\n"
          "    Monitor Source: %u\n"
          "    Latency: %0.0f usec\n"
          "    Flags: %s%s%s\n",
          i->index,
          i->name,
          i->driver,
          i->description,
          pa_sample_spec_snprint(s, sizeof(s), &i->sample_spec),
          pa_channel_map_snprint(cm, sizeof(cm), &i->channel_map),
          i->owner_module,
          i->mute ? "muted" : pa_cvolume_snprint(cv, sizeof(cv), &i->volume),
          i->monitor_source,
          static_cast<double>(i->latency),
          i->flags & PA_SINK_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
          i->flags & PA_SINK_LATENCY ? "LATENCY " : "",
          i->flags & PA_SINK_HARDWARE ? "HARDWARE" : "");
#endif
    if (not context->inSinkList(i->name)) {
        context->sinkList_.push_back(*i);
    }
}

void PulseLayer::updatePreference(AudioPreference &preference, int index, DeviceType type)
{
    const std::string devName(getAudioDeviceName(index, type));

    switch (type) {
        case DeviceType::PLAYBACK:
            JAMI_DBG("setting %s for playback", devName.c_str());
            preference.setPulseDevicePlayback(devName);
            break;

        case DeviceType::CAPTURE:
            JAMI_DBG("setting %s for capture", devName.c_str());
            preference.setPulseDeviceRecord(devName);
            break;

        case DeviceType::RINGTONE:
            JAMI_DBG("setting %s for ringer", devName.c_str());
            preference.setPulseDeviceRingtone(devName);
            break;
    }
}

int PulseLayer::getIndexCapture() const
{
    return getAudioDeviceIndexByName(preference_.getPulseDeviceRecord(), DeviceType::CAPTURE);
}

int PulseLayer::getIndexPlayback() const
{
    return getAudioDeviceIndexByName(preference_.getPulseDevicePlayback(), DeviceType::PLAYBACK);
}

int PulseLayer::getIndexRingtone() const
{
    return getAudioDeviceIndexByName(preference_.getPulseDeviceRingtone(), DeviceType::RINGTONE);
}

std::string
PulseLayer::getPreferredPlaybackDevice() const {
    std::string playbackDevice(preference_.getPulseDevicePlayback());
    if (playbackDevice.empty())
        playbackDevice = defaultSink_;
    return stripEchoSufix(playbackDevice);
}

std::string
PulseLayer::getPreferredRingtoneDevice() const {
    std::string ringtoneDevice(preference_.getPulseDeviceRingtone());
    if (ringtoneDevice.empty())
        ringtoneDevice = defaultSink_;
    return stripEchoSufix(ringtoneDevice);
}

std::string
PulseLayer::getPreferredCaptureDevice() const {
    std::string captureDevice(preference_.getPulseDeviceRecord());
    if (captureDevice.empty())
        captureDevice = defaultSource_;
    return stripEchoSufix(captureDevice);
}


} // namespace jami
