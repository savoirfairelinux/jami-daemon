/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

PulseMainLoopLock::PulseMainLoopLock(pa_threaded_mainloop* loop)
    : loop_(loop)
{
    pa_threaded_mainloop_lock(loop_);
}

PulseMainLoopLock::~PulseMainLoopLock()
{
    pa_threaded_mainloop_unlock(loop_);
}

PulseLayer::PulseLayer(AudioPreference& pref)
    : AudioLayer(pref)
    , playback_()
    , record_()
    , ringtone_()
    , mainloop_(pa_threaded_mainloop_new(), pa_threaded_mainloop_free)
    , preference_(pref)
{
    JAMI_INFO("[audiolayer] created pulseaudio layer");
    if (!mainloop_)
        throw std::runtime_error("Unable to create pulseaudio mainloop");

    if (pa_threaded_mainloop_start(mainloop_.get()) < 0)
        throw std::runtime_error("Failed to start pulseaudio mainloop");

    setHasNativeNS(false);

    PulseMainLoopLock lock(mainloop_.get());

    std::unique_ptr<pa_proplist, decltype(pa_proplist_free)&> pl(pa_proplist_new(),
                                                                 pa_proplist_free);
    pa_proplist_sets(pl.get(), PA_PROP_MEDIA_ROLE, "phone");

    context_ = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mainloop_.get()),
                                            PACKAGE_NAME,
                                            pl.get());
    if (!context_)
        throw std::runtime_error("Unable to create pulseaudio context");

    pa_context_set_state_callback(context_, context_state_callback, this);

    if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
        throw std::runtime_error("Unable to connect pulseaudio context to the server");

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

    playbackChanged(false);
    recordChanged(false);
}

void
PulseLayer::context_state_callback(pa_context* c, void* user_data)
{
    PulseLayer* pulse = static_cast<PulseLayer*>(user_data);
    if (c and pulse)
        pulse->contextStateChanged(c);
}

void
PulseLayer::contextStateChanged(pa_context* c)
{
    const pa_subscription_mask_t mask = (pa_subscription_mask_t) (PA_SUBSCRIPTION_MASK_SINK
                                                                  | PA_SUBSCRIPTION_MASK_SOURCE);

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
        JAMI_ERR("%s", pa_strerror(pa_context_errno(c)));
        pa_threaded_mainloop_signal(mainloop_.get(), 0);
        break;
    }
}

void
PulseLayer::updateSinkList()
{
    std::unique_lock lk(readyMtx_);
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

void
PulseLayer::updateSourceList()
{
    std::unique_lock lk(readyMtx_);
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

void
PulseLayer::updateServerInfo()
{
    std::unique_lock lk(readyMtx_);
    if (not gettingServerInfo_) {
        JAMI_DBG("Updating PulseAudio server infos");
        gettingServerInfo_ = true;
        if (auto op = pa_context_get_server_info(context_, server_info_callback, this))
            pa_operation_unref(op);
        else
            gettingServerInfo_ = false;
    }
}

bool
PulseLayer::inSinkList(const std::string& deviceName)
{
    return std::find_if(sinkList_.begin(),
                        sinkList_.end(),
                        PaDeviceInfos::NameComparator(deviceName))
           != sinkList_.end();
}

bool
PulseLayer::inSourceList(const std::string& deviceName)
{
    return std::find_if(sourceList_.begin(),
                        sourceList_.end(),
                        PaDeviceInfos::NameComparator(deviceName))
           != sourceList_.end();
}

std::vector<std::string>
PulseLayer::getCaptureDeviceList() const
{
    std::vector<std::string> names;
    names.reserve(sourceList_.size());
    for (const auto& s : sourceList_)
        names.emplace_back(s.description);
    return names;
}

std::vector<std::string>
PulseLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> names;
    names.reserve(sinkList_.size());
    for (const auto& s : sinkList_)
        names.emplace_back(s.description);
    return names;
}

int
PulseLayer::getAudioDeviceIndex(const std::string& descr, AudioDeviceType type) const
{
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        return std::distance(sinkList_.begin(),
                             std::find_if(sinkList_.begin(),
                                          sinkList_.end(),
                                          PaDeviceInfos::DescriptionComparator(descr)));
    case AudioDeviceType::CAPTURE:
        return std::distance(sourceList_.begin(),
                             std::find_if(sourceList_.begin(),
                                          sourceList_.end(),
                                          PaDeviceInfos::DescriptionComparator(descr)));
    default:
        JAMI_ERR("Unexpected device type");
        return 0;
    }
}

int
PulseLayer::getAudioDeviceIndexByName(const std::string& name, AudioDeviceType type) const
{
    if (name.empty())
        return 0;
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        return std::distance(sinkList_.begin(),
                             std::find_if(sinkList_.begin(),
                                          sinkList_.end(),
                                          PaDeviceInfos::NameComparator(name)));
    case AudioDeviceType::CAPTURE:
        return std::distance(sourceList_.begin(),
                             std::find_if(sourceList_.begin(),
                                          sourceList_.end(),
                                          PaDeviceInfos::NameComparator(name)));
    default:
        JAMI_ERR("Unexpected device type");
        return 0;
    }
}

bool
endsWith(const std::string& str, const std::string& ending)
{
    if (ending.size() >= str.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}

/**
 * Find default device for PulseAudio to open, filter monitors and EC.
 */
const PaDeviceInfos*
findBest(const std::vector<PaDeviceInfos>& list)
{
    if (list.empty())
        return nullptr;
    for (const auto& info : list)
        if (info.monitor_of == PA_INVALID_INDEX)
            return &info;
    return &list[0];
}

const PaDeviceInfos*
PulseLayer::getDeviceInfos(const std::vector<PaDeviceInfos>& list, const std::string& name) const
{
    auto dev_info = std::find_if(list.begin(), list.end(), PaDeviceInfos::NameComparator(name));
    if (dev_info == list.end()) {
        JAMI_WARN("Preferred device %s not found in device list, selecting default %s instead.",
                  name.c_str(),
                  list.front().name.c_str());
        return &list.front();
    }
    return &(*dev_info);
}

std::string
PulseLayer::getAudioDeviceName(int index, AudioDeviceType type) const
{
    switch (type) {
    case AudioDeviceType::PLAYBACK:
    case AudioDeviceType::RINGTONE:
        if (index < 0 or static_cast<size_t>(index) >= sinkList_.size()) {
            JAMI_ERR("Index %d out of range", index);
            return "";
        }
        return sinkList_[index].name;

    case AudioDeviceType::CAPTURE:
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

void
PulseLayer::onStreamReady()
{
    if (--pendingStreams == 0) {
        JAMI_DBG("All streams ready, starting audio");
        // Flush outside the if statement: every time start stream is
        // called is to notify a new event
        flushUrgent();
        flushMain();
        if (playback_) {
            playback_->start();
            playbackChanged(true);
        }
        if (ringtone_) {
            ringtone_->start();
        }
        if (record_) {
            record_->start();
            recordChanged(true);
        }
    }
}

void
PulseLayer::createStream(std::unique_ptr<AudioStream>& stream,
                         AudioDeviceType type,
                         const PaDeviceInfos& dev_infos,
                         bool ec,
                         std::function<void(size_t)>&& onData)
{
    if (stream) {
        JAMI_WARN("Stream already exists");
        return;
    }
    pendingStreams++;
    const char* name = type == AudioDeviceType::PLAYBACK
                           ? "Playback"
                           : (type == AudioDeviceType::CAPTURE
                                  ? "Record"
                                  : (type == AudioDeviceType::RINGTONE ? "Ringtone" : "?"));
    stream.reset(new AudioStream(context_,
                                 mainloop_.get(),
                                 name,
                                 type,
                                 audioFormat_.sample_rate,
                                 pulseSampleFormatFromAv(audioFormat_.sampleFormat),
                                 dev_infos,
                                 ec,
                                 std::bind(&PulseLayer::onStreamReady, this),
                                 std::move(onData)));
}

void
PulseLayer::disconnectAudioStream()
{
    PulseMainLoopLock lock(mainloop_.get());
    playback_.reset();
    ringtone_.reset();
    record_.reset();
    playbackChanged(false);
    recordChanged(false);
    pendingStreams = 0;
    status_ = Status::Idle;
    startedCv_.notify_all();
}

void
PulseLayer::startStream(AudioDeviceType type)
{
    waitForDevices();
    PulseMainLoopLock lock(mainloop_.get());
    bool ec = preference_.getEchoCanceller() == "system"
              || preference_.getEchoCanceller() == "auto";

    // Create Streams
    if (type == AudioDeviceType::PLAYBACK) {
        if (auto dev_infos = getDeviceInfos(sinkList_, getPreferredPlaybackDevice())) {
            createStream(playback_,
                         type,
                         *dev_infos,
                         ec,
                         std::bind(&PulseLayer::writeToSpeaker, this));
        }
    } else if (type == AudioDeviceType::RINGTONE) {
        if (auto dev_infos = getDeviceInfos(sinkList_, getPreferredRingtoneDevice()))
            createStream(ringtone_,
                         type,
                         *dev_infos,
                         false,
                         std::bind(&PulseLayer::ringtoneToSpeaker, this));
    } else if (type == AudioDeviceType::CAPTURE) {
        if (auto dev_infos = getDeviceInfos(sourceList_, getPreferredCaptureDevice())) {
            createStream(record_, type, *dev_infos, ec, std::bind(&PulseLayer::readFromMic, this));

            // whenever the stream is moved, it will call this cb
            record_->setEchoCancelCb([this](bool echoCancel) { setHasNativeAEC(echoCancel); });
        }
    }
    pa_threaded_mainloop_signal(mainloop_.get(), 0);

    std::lock_guard lk(mutex_);
    status_ = Status::Started;
    startedCv_.notify_all();
}

void
PulseLayer::stopStream(AudioDeviceType type)
{
    waitForDevices();
    PulseMainLoopLock lock(mainloop_.get());
    auto& stream(getStream(type));
    if (not stream)
        return;

    if (not stream->isReady())
        pendingStreams--;
    stream->stop();
    stream.reset();

    if (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::ALL)
        playbackChanged(false);

    std::lock_guard lk(mutex_);
    if (not playback_ and not ringtone_ and not record_) {
        pendingStreams = 0;
        status_ = Status::Idle;
        startedCv_.notify_all();
    }
}

void
PulseLayer::writeToSpeaker()
{
    if (!playback_ or !playback_->isReady())
        return;

    // available bytes to be written in pulseaudio internal buffer
    void* data = nullptr;
    size_t writableBytes = (size_t) -1;
    int ret = pa_stream_begin_write(playback_->stream(), &data, &writableBytes);
    if (ret == 0 and data and writableBytes != 0) {
        writableBytes = std::min(pa_stream_writable_size(playback_->stream()), writableBytes);
        const auto& buff = getToPlay(playback_->format(), writableBytes / playback_->frameSize());
        if (not buff or isPlaybackMuted_)
            memset(data, 0, writableBytes);
        else
            std::memcpy(data,
                        buff->pointer()->data[0],
                        buff->pointer()->nb_samples * playback_->frameSize());
        pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
    }
}

void
PulseLayer::readFromMic()
{
    if (!record_ or !record_->isReady())
        return;

    const char* data = nullptr;
    size_t bytes;
    if (pa_stream_peek(record_->stream(), (const void**) &data, &bytes) < 0 or !data)
        return;

    if (bytes == 0)
        return;

    size_t sample_size = record_->frameSize();
    const size_t samples = bytes / sample_size;

    auto out = std::make_shared<AudioFrame>(record_->format(), samples);
    if (isCaptureMuted_)
        libav_utils::fillWithSilence(out->pointer());
    else
        std::memcpy(out->pointer()->data[0], data, bytes);

    if (pa_stream_drop(record_->stream()) < 0)
        JAMI_ERR("Capture stream drop failed: %s", pa_strerror(pa_context_errno(context_)));

    putRecorded(std::move(out));
}

void
PulseLayer::ringtoneToSpeaker()
{
    if (!ringtone_ or !ringtone_->isReady())
        return;

    void* data = nullptr;
    size_t writableBytes = (size_t) -1;
    int ret = pa_stream_begin_write(ringtone_->stream(), &data, &writableBytes);
    if (ret == 0 and data and writableBytes != 0) {
        writableBytes = std::min(pa_stream_writable_size(ringtone_->stream()), writableBytes);
        const auto& buff = getToRing(ringtone_->format(), writableBytes / ringtone_->frameSize());
        if (not buff or isRingtoneMuted_)
            memset(data, 0, writableBytes);
        else
            std::memcpy(data,
                        buff->pointer()->data[0],
                        buff->pointer()->nb_samples * ringtone_->frameSize());
        pa_stream_write(ringtone_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
    }
}

std::string
stripEchoSufix(const std::string& deviceName)
{
    return std::regex_replace(deviceName, PA_EC_SUFFIX, "");
}

void
PulseLayer::context_changed_callback(pa_context* c,
                                     pa_subscription_event_type_t type,
                                     uint32_t idx,
                                     void* userdata)
{
    static_cast<PulseLayer*>(userdata)->contextChanged(c, type, idx);
}

void
PulseLayer::contextChanged(pa_context* c UNUSED,
                           pa_subscription_event_type_t type,
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

void
PulseLayer::waitForDevices()
{
    std::unique_lock lk(readyMtx_);
    readyCv_.wait(lk, [this] {
        return !(enumeratingSinks_ or enumeratingSources_ or gettingServerInfo_);
    });
}

void
PulseLayer::waitForDeviceList()
{
    std::unique_lock lock(readyMtx_);
    if (waitingDeviceList_.exchange(true))
        return;
    if (streamStarter_.joinable())
        streamStarter_.join();
    streamStarter_ = std::thread([this]() mutable {
        bool playbackDeviceChanged, recordDeviceChanged;

        waitForDevices();
        waitingDeviceList_ = false;

        // If a current device changed, restart streams
        devicesChanged();
        auto playbackInfo = getDeviceInfos(sinkList_, getPreferredPlaybackDevice());
        playbackDeviceChanged = playback_
                                and (!playbackInfo->name.empty()
                                     and playbackInfo->name
                                             != stripEchoSufix(playback_->getDeviceName()));

        auto recordInfo = getDeviceInfos(sourceList_, getPreferredCaptureDevice());
        recordDeviceChanged = record_
                              and (!recordInfo->name.empty()
                                   and recordInfo->name != stripEchoSufix(record_->getDeviceName()));

        if (status_ != Status::Started)
            return;
        if (playbackDeviceChanged) {
            JAMI_WARN("Playback devices changed, restarting streams.");
            stopStream(AudioDeviceType::PLAYBACK);
            startStream(AudioDeviceType::PLAYBACK);
        }
        if (recordDeviceChanged) {
            JAMI_WARN("Record devices changed, restarting streams.");
            stopStream(AudioDeviceType::CAPTURE);
            startStream(AudioDeviceType::CAPTURE);
        }
    });
}

void
PulseLayer::server_info_callback(pa_context*, const pa_server_info* i, void* userdata)
{
    if (!i)
        return;
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

    PulseLayer* context = static_cast<PulseLayer*>(userdata);
    std::lock_guard lk(context->readyMtx_);
    context->defaultSink_ = {};
    context->defaultSource_ = {};
    context->defaultAudioFormat_ = {
        i->sample_spec.rate,
        i->sample_spec.channels,
        sampleFormatFromPulse(i->sample_spec.format)
    };
    {
        std::lock_guard lk(context->mutex_);
        context->hardwareFormatAvailable(context->defaultAudioFormat_);
    }
    /*if (not context->sinkList_.empty())
        context->sinkList_.front().channel_map.channels = std::min(i->sample_spec.channels,
                                                                   (uint8_t) 2);
    if (not context->sourceList_.empty())
        context->sourceList_.front().channel_map.channels = std::min(i->sample_spec.channels,
                                                                     (uint8_t) 2);*/
    context->gettingServerInfo_ = false;
    context->readyCv_.notify_all();
}

void
PulseLayer::source_input_info_callback(pa_context* c UNUSED,
                                       const pa_source_info* i,
                                       int eol,
                                       void* userdata)
{
    PulseLayer* context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        std::lock_guard lk(context->readyMtx_);
        context->enumeratingSources_ = false;
        context->readyCv_.notify_all();
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
        context->sourceList_.emplace_back(*i);
    }
}

void
PulseLayer::sink_input_info_callback(pa_context* c UNUSED,
                                     const pa_sink_info* i,
                                     int eol,
                                     void* userdata)
{
    PulseLayer* context = static_cast<PulseLayer*>(userdata);
    std::lock_guard lk(context->readyMtx_);

    if (eol) {
        context->enumeratingSinks_ = false;
        context->readyCv_.notify_all();
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
        context->sinkList_.emplace_back(*i);
    }
}

void
PulseLayer::updatePreference(AudioPreference& preference, int index, AudioDeviceType type)
{
    const std::string devName(getAudioDeviceName(index, type));

    switch (type) {
    case AudioDeviceType::PLAYBACK:
        JAMI_DBG("setting %s for playback", devName.c_str());
        preference.setPulseDevicePlayback(devName);
        break;

    case AudioDeviceType::CAPTURE:
        JAMI_DBG("setting %s for capture", devName.c_str());
        preference.setPulseDeviceRecord(devName);
        break;

    case AudioDeviceType::RINGTONE:
        JAMI_DBG("setting %s for ringer", devName.c_str());
        preference.setPulseDeviceRingtone(devName);
        break;

    default:
        break;
    }
}

int
PulseLayer::getIndexCapture() const
{
    return getAudioDeviceIndexByName(preference_.getPulseDeviceRecord(), AudioDeviceType::CAPTURE);
}

int
PulseLayer::getIndexPlayback() const
{
    return getAudioDeviceIndexByName(preference_.getPulseDevicePlayback(),
                                     AudioDeviceType::PLAYBACK);
}

int
PulseLayer::getIndexRingtone() const
{
    return getAudioDeviceIndexByName(preference_.getPulseDeviceRingtone(),
                                     AudioDeviceType::RINGTONE);
}

std::string
PulseLayer::getPreferredPlaybackDevice() const
{
    const std::string& device(preference_.getPulseDevicePlayback());
    return stripEchoSufix(device.empty() ? defaultSink_ : device);
}

std::string
PulseLayer::getPreferredRingtoneDevice() const
{
    const std::string& device(preference_.getPulseDeviceRingtone());
    return stripEchoSufix(device.empty() ? defaultSink_ : device);
}

std::string
PulseLayer::getPreferredCaptureDevice() const
{
    const std::string& device(preference_.getPulseDeviceRecord());
    return stripEchoSufix(device.empty() ? defaultSource_ : device);
}

} // namespace jami
