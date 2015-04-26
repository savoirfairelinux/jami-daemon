/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <algorithm> // for std::find
#include <stdexcept>

#include "intrin.h"
#include "audiostream.h"
#include "pulselayer.h"
#include "audio/resampler.h"
#include "audio/dcblocker.h"
#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "logger.h"
#include "manager.h"

#include <unistd.h>
#include <cstdlib>
#include <fstream>

// uncomment to log pulseaudio sink and sources
//#define PA_LOG_SINK_SOURCES

namespace ring {

PulseMainLoopLock::PulseMainLoopLock(pa_threaded_mainloop *loop) : loop_(loop), destroyLoop_(false)
{
    pa_threaded_mainloop_lock(loop_);
}

// set this flag if we want the loop to be destroyed once it's unlocked
void PulseMainLoopLock::destroyLoop()
{
    destroyLoop_ = true;
}

PulseMainLoopLock::~PulseMainLoopLock()
{
    pa_threaded_mainloop_unlock(loop_);
    if (destroyLoop_) {
        pa_threaded_mainloop_stop(loop_);
        pa_threaded_mainloop_free(loop_);
    }
}

PulseLayer::PulseLayer(AudioPreference &pref)
    : AudioLayer(pref)
    , playback_()
    , record_()
    , ringtone_()
    , mainloop_(pa_threaded_mainloop_new())
    , preference_(pref)
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{
    setCaptureGain(pref.getVolumemic());
    setPlaybackGain(pref.getVolumespkr());
    muteCapture(pref.getCaptureMuted());

    if (!mainloop_)
        throw std::runtime_error("Couldn't create pulseaudio mainloop");

    if (pa_threaded_mainloop_start(mainloop_) < 0) {
        pa_threaded_mainloop_free(mainloop_);
        throw std::runtime_error("Failed to start pulseaudio mainloop");
    }

    PulseMainLoopLock lock(mainloop_);

#if PA_CHECK_VERSION(1, 0, 0)
    pa_proplist *pl = pa_proplist_new();
    pa_proplist_sets(pl, PA_PROP_MEDIA_ROLE, "phone");
    pa_proplist_sets(pl, PA_PROP_FILTER_WANT, "echo-cancel");

    context_ = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mainloop_), PACKAGE_NAME, pl);

    if (pl)
        pa_proplist_free(pl);

#else
    setenv("PULSE_PROP_media.role", "phone", 1);
    setenv("PULSE_PROP_filter.want", "echo-cancel", 1);
    context_ = pa_context_new(pa_threaded_mainloop_get_api(mainloop_), PACKAGE_NAME);
#endif

    if (!context_) {
        lock.destroyLoop();
        throw std::runtime_error("Couldn't create pulseaudio context");
    }

    pa_context_set_state_callback(context_, context_state_callback, this);

    if (pa_context_connect(context_, nullptr , PA_CONTEXT_NOAUTOSPAWN , nullptr) < 0) {
        lock.destroyLoop();
        throw std::runtime_error("Could not connect pulseaudio context to the server");
    }

    // wait until context is ready
    for (;;) {
        pa_context_state_t context_state = pa_context_get_state(context_);
        if (not PA_CONTEXT_IS_GOOD(context_state)) {
            lock.destroyLoop();
            throw std::runtime_error("Pulse audio context is bad");
        }
        if (context_state == PA_CONTEXT_READY)
            break;
        pa_threaded_mainloop_wait(mainloop_);
    }

    isStarted_ = true;
}

PulseLayer::~PulseLayer()
{
    disconnectAudioStream();

    {
        PulseMainLoopLock lock(mainloop_);
        pa_context_set_state_callback(context_, NULL, NULL);
        pa_context_set_subscribe_callback(context_, NULL, NULL);
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        lock.destroyLoop();
    }
}

void PulseLayer::context_state_callback(pa_context* c, void *user_data)
{
    PulseLayer *pulse = static_cast<PulseLayer*>(user_data);
    assert(c and pulse and pulse->mainloop_);
    const pa_subscription_mask_t mask = (pa_subscription_mask_t)
                                        (PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            RING_DBG("Waiting....");
            break;

        case PA_CONTEXT_READY:
            RING_DBG("Connection to PulseAudio server established");
            pa_threaded_mainloop_signal(pulse->mainloop_, 0);
            pa_context_subscribe(c, mask, nullptr, pulse);
            pa_context_set_subscribe_callback(c, context_changed_callback, pulse);
            pulse->updateSinkList();
            pulse->updateSourceList();
            pulse->updateServerInfo();
            break;

        case PA_CONTEXT_TERMINATED:
            break;

        case PA_CONTEXT_FAILED:
        default:
            RING_ERR("%s" , pa_strerror(pa_context_errno(c)));
            pa_threaded_mainloop_signal(pulse->mainloop_, 0);
            break;
    }
}


void PulseLayer::updateSinkList()
{
    sinkList_.clear();
    enumeratingSinks_ = true;
    pa_operation *op = pa_context_get_sink_info_list(context_, sink_input_info_callback, this);

    if (op != nullptr)
        pa_operation_unref(op);
}

void PulseLayer::updateSourceList()
{
    sourceList_.clear();
    enumeratingSources_ = true;
    pa_operation *op = pa_context_get_source_info_list(context_, source_input_info_callback, this);

    if (op != nullptr)
        pa_operation_unref(op);
}

void PulseLayer::updateServerInfo()
{
    gettingServerInfo_ = true;
    pa_operation *op = pa_context_get_server_info(context_, server_info_callback, this);

    if (op != nullptr)
        pa_operation_unref(op);
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
    names.reserve(sourceList_.size() + 1);
    names.push_back("default");
    for (const auto& s : sourceList_)
        names.push_back(s.description);
    return names;
}

std::vector<std::string> PulseLayer::getPlaybackDeviceList() const
{
    std::vector<std::string> names;
    names.reserve(sinkList_.size() + 1);
    names.push_back("default");
    for (const auto& s : sinkList_)
        names.push_back(s.description);
    return names;
}

int PulseLayer::getAudioDeviceIndex(const std::string& descr, DeviceType type) const
{
    if (descr == "default")
        return 0;
    switch (type) {
    case DeviceType::PLAYBACK:
    case DeviceType::RINGTONE:
        return 1 + std::distance(sinkList_.begin(), std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::DescriptionComparator(descr)));
    case DeviceType::CAPTURE:
        return 1 + std::distance(sourceList_.begin(), std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::DescriptionComparator(descr)));
    default:
        RING_ERR("Unexpected device type");
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
        return 1 + std::distance(sinkList_.begin(), std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::NameComparator(name)));
    case DeviceType::CAPTURE:
        return 1 + std::distance(sourceList_.begin(), std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::NameComparator(name)));
    default:
        RING_ERR("Unexpected device type");
        return 0;
    }
}

const PaDeviceInfos* PulseLayer::getDeviceInfos(const std::vector<PaDeviceInfos>& list, const std::string& name) const
{
    std::vector<PaDeviceInfos>::const_iterator dev_info = std::find_if(list.begin(), list.end(), PaDeviceInfos::NameComparator(name));

    if (dev_info == list.end()) return nullptr;

    return &(*dev_info);
}

std::string PulseLayer::getAudioDeviceName(int index, DeviceType type) const
{
    if (index == 0)
        return "";
    index--;
    switch (type) {
        case DeviceType::PLAYBACK:
        case DeviceType::RINGTONE:
            if (index < 0 or static_cast<size_t>(index) >= sinkList_.size()) {
                RING_ERR("Index %d out of range", index);
                return "";
            }
            return sinkList_[index].name;

        case DeviceType::CAPTURE:
            if (index < 0 or static_cast<size_t>(index) >= sourceList_.size()) {
                RING_ERR("Index %d out of range", index);
                return "";
            }
            return sourceList_[index].name;

        default:
            // Should never happen
            RING_ERR("Unexpected type");
            return "";
    }
}

void PulseLayer::createStreams(pa_context* c)
{
    std::unique_lock<std::mutex> lk(readyMtx_);
    readyCv_.wait(lk, [this] {
        return !(enumeratingSinks_ or enumeratingSources_ or gettingServerInfo_);
    });

    hardwareFormatAvailable(defaultAudioFormat_);

    std::string playbackDevice(preference_.getPulseDevicePlayback());
    if (playbackDevice.empty())
        playbackDevice = defaultSink_;
    std::string captureDevice(preference_.getPulseDeviceRecord());
    if (captureDevice.empty())
        captureDevice = defaultSource_;
    std::string ringtoneDevice(preference_.getPulseDeviceRingtone());
    if (ringtoneDevice.empty())
        ringtoneDevice = defaultSink_;

    RING_DBG("playback: %s record: %s ringtone: %s", playbackDevice.c_str(),
          captureDevice.c_str(), ringtoneDevice.c_str());

    // Create playback stream
    const PaDeviceInfos* dev_infos = getDeviceInfos(sinkList_, playbackDevice);

    if (dev_infos == nullptr) {
        dev_infos = &sinkList_[0];
        RING_WARN("Prefered playback device %s not found in device list, selecting %s instead.",
             playbackDevice.c_str(), dev_infos->name.c_str());
    }

    playback_.reset(new AudioStream(c, mainloop_, "Playback", PLAYBACK_STREAM, audioFormat_.sample_rate, dev_infos));
    pa_stream_set_write_callback(playback_->stream(), [](pa_stream * /*s*/, size_t /*bytes*/, void* userdata) {
        static_cast<PulseLayer*>(userdata)->writeToSpeaker();
    }, this);

    // Create capture stream
    dev_infos = getDeviceInfos(sourceList_, captureDevice);

    if (dev_infos == nullptr) {
        dev_infos = &sourceList_[0];
        RING_WARN("Prefered capture device %s not found in device list, selecting %s instead.",
             captureDevice.c_str(), dev_infos->name.c_str());
    }

    record_.reset(new AudioStream(c, mainloop_, "Capture", CAPTURE_STREAM, audioFormat_.sample_rate, dev_infos));
    pa_stream_set_read_callback(record_->stream() , [](pa_stream * /*s*/, size_t /*bytes*/, void* userdata) {
        static_cast<PulseLayer*>(userdata)->readFromMic();
    }, this);

    // Create ringtone stream
    dev_infos = getDeviceInfos(sinkList_, ringtoneDevice);

    if (dev_infos == nullptr) {
        dev_infos = &sinkList_[0];
        RING_WARN("Prefered ringtone device %s not found in device list, selecting %s instead.",
             ringtoneDevice.c_str(), dev_infos->name.c_str());
    }

    ringtone_.reset(new AudioStream(c, mainloop_, "Ringtone", RINGTONE_STREAM, audioFormat_.sample_rate, dev_infos));
    pa_stream_set_write_callback(ringtone_->stream(), [](pa_stream * /*s*/, size_t /*bytes*/, void* userdata) {
        static_cast<PulseLayer*>(userdata)->ringtoneToSpeaker();
    }, this);

    pa_threaded_mainloop_signal(mainloop_, 0);

    flushMain();
    flushUrgent();
}

void PulseLayer::disconnectAudioStream()
{
    playback_.reset();
    ringtone_.reset();
    record_.reset();
}

void PulseLayer::startStream()
{
    // Create Streams
    if (!playback_ or !record_)
        createStreams(context_);

    // Flush outside the if statement: every time start stream is
    // called is to notify a new event
    flushUrgent();
    flushMain();
}

void
PulseLayer::stopStream()
{
    {
        PulseMainLoopLock lock(mainloop_);

        if (playback_)
            pa_stream_flush(playback_->stream(), nullptr, nullptr);

        if (record_)
            pa_stream_flush(record_->stream(), nullptr, nullptr);
    }

    disconnectAudioStream();
}

void PulseLayer::writeToSpeaker()
{
    if (!playback_ or !playback_->isReady())
        return;

    size_t sample_size = playback_->frameSize();
    const AudioFormat format = playback_->format();

    // available bytes to be written in pulseaudio internal buffer
    size_t writableBytes = pa_stream_writable_size(playback_->stream());
    if (writableBytes == 0)
        return;

    const size_t writableSamples = writableBytes / sample_size;

    notifyIncomingCall();

    size_t urgentSamples = urgentRingBuffer_.availableForGet(RingBufferPool::DEFAULT_ID);
    size_t urgentBytes = urgentSamples * sample_size;

    if (urgentSamples > writableSamples) {
        urgentSamples = writableSamples;
        urgentBytes = urgentSamples * sample_size;
    }

    AudioSample* data = nullptr;

    if (urgentBytes) {
        playbackBuffer_.setFormat(format);
        playbackBuffer_.resize(urgentSamples);
        pa_stream_begin_write(playback_->stream(), (void**)&data, &urgentBytes);
        urgentRingBuffer_.get(playbackBuffer_, RingBufferPool::DEFAULT_ID); // retrive only the first sample_spec->channels channels
        playbackBuffer_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        playbackBuffer_.interleave(data);
        pa_stream_write(playback_->stream(), data, urgentBytes, nullptr, 0, PA_SEEK_RELATIVE);
        // Consume the regular one as well (same amount of samples)
        Manager::instance().getRingBufferPool().discard(urgentSamples, RingBufferPool::DEFAULT_ID);
        return;
    }

    // FIXME: not thread safe! we only lock the mutex when we get the
    // pointer, we have no guarantee that it will stay safe to use
    AudioLoop *toneToPlay = Manager::instance().getTelephoneTone();
    if (toneToPlay) {
        if (playback_->isReady()) {
            pa_stream_begin_write(playback_->stream(), (void**)&data, &writableBytes);
            playbackBuffer_.setFormat(format);
            playbackBuffer_.resize(writableSamples);
            toneToPlay->getNext(playbackBuffer_, playbackGain_); // retrive only n_channels
            playbackBuffer_.interleave(data);
            pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
        }

        return;
    }

    flushUrgent(); // flush remaining samples in _urgentRingBuffer

    size_t availSamples = Manager::instance().getRingBufferPool().availableForGet(RingBufferPool::DEFAULT_ID);

    if (availSamples == 0) {
        pa_stream_begin_write(playback_->stream(), (void**)&data, &writableBytes);
        memset(data, 0, writableBytes);
        pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
        return;
    }

    // how many samples we want to read from the buffer
    size_t readableSamples = writableSamples;

    double resampleFactor = 1.;

    AudioFormat mainBufferAudioFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample = audioFormat_.sample_rate != mainBufferAudioFormat.sample_rate;

    if (resample) {
        resampleFactor = (double) audioFormat_.sample_rate / mainBufferAudioFormat.sample_rate;
        readableSamples = (double) readableSamples / resampleFactor;
    }

    readableSamples = std::min(readableSamples, availSamples);
    size_t nResampled = (double) readableSamples * resampleFactor;
    size_t resampledBytes =  nResampled * sample_size;

    pa_stream_begin_write(playback_->stream(), (void**)&data, &resampledBytes);

    playbackBuffer_.setFormat(mainBufferAudioFormat);
    playbackBuffer_.resize(readableSamples);
    Manager::instance().getRingBufferPool().getData(playbackBuffer_, RingBufferPool::DEFAULT_ID);
    playbackBuffer_.setChannelNum(format.nb_channels, true);

    if (resample) {
        AudioBuffer rsmpl_out(nResampled, format);
        resampler_->resample(playbackBuffer_, rsmpl_out);
        rsmpl_out.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        rsmpl_out.interleave(data);
        pa_stream_write(playback_->stream(), data, resampledBytes, nullptr, 0, PA_SEEK_RELATIVE);
    } else {
        playbackBuffer_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        playbackBuffer_.interleave(data);
        pa_stream_write(playback_->stream(), data, resampledBytes, nullptr, 0, PA_SEEK_RELATIVE);
    }
}

void PulseLayer::readFromMic()
{
    if (!record_ or !record_->isReady())
        return;

    const char *data = nullptr;
    size_t bytes;
    if (pa_stream_peek(record_->stream() , (const void**) &data , &bytes) < 0 or !data)
        return;

    size_t sample_size = playback_->frameSize();
    const AudioFormat format = playback_->format();
    assert(format.nb_channels);
    assert(sample_size);
    const size_t samples = bytes / sample_size;

    micBuffer_.setFormat(format);
    micBuffer_.resize(samples);
    micBuffer_.deinterleave((AudioSample*)data, samples, format.nb_channels);
    micBuffer_.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    auto mainBufferAudioFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample = format.sample_rate != mainBufferAudioFormat.sample_rate;

    AudioBuffer* out;
    if (resample) {
        micResampleBuffer_.setSampleRate(mainBufferAudioFormat.sample_rate);
        resampler_->resample(micBuffer_, micResampleBuffer_);
        out = &micResampleBuffer_;
    } else {
        out = &micBuffer_;
    }

    dcblocker_.process(*out);
    out->applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
    mainRingBuffer_->put(*out);

    if (pa_stream_drop(record_->stream()) < 0)
        RING_ERR("Capture stream drop failed: %s" , pa_strerror(pa_context_errno(context_)));
}


void PulseLayer::ringtoneToSpeaker()
{
    if (!ringtone_ or !ringtone_->isReady())
        return;

    size_t bytes = pa_stream_writable_size(ringtone_->stream());
    if (bytes == 0)
        return;

    void *data;
    pa_stream_begin_write(ringtone_->stream(), &data, &bytes);
    AudioLoop *fileToPlay = Manager::instance().getTelephoneFile();

    if (fileToPlay) {
        const unsigned samples = bytes / ringtone_->frameSize();
        auto fileformat = fileToPlay->getFormat();
        ringtoneBuffer_.setFormat(fileformat);
        ringtoneBuffer_.resize(samples);
        fileToPlay->getNext(ringtoneBuffer_, playbackGain_);
        bool resample = ringtone_->format().sample_rate != fileformat.sample_rate;
        AudioBuffer* out;
        if (resample) {
            ringtoneResampleBuffer_.setSampleRate(ringtone_->format().sample_rate);
            resampler_->resample(ringtoneBuffer_, ringtoneResampleBuffer_);
            out = &ringtoneResampleBuffer_;
        } else {
            out = &ringtoneBuffer_;
        }
        out->interleave((AudioSample*) data);
    } else {
        memset(data, 0, bytes);
    }

    pa_stream_write(ringtone_->stream(), data, bytes, nullptr, 0, PA_SEEK_RELATIVE);
}

void
PulseLayer::context_changed_callback(pa_context* c,
                                     pa_subscription_event_type_t type,
                                     uint32_t idx UNUSED, void *userdata)
{
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
            pa_operation *op;

        case PA_SUBSCRIPTION_EVENT_SINK:
            switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
                case PA_SUBSCRIPTION_EVENT_NEW:
                case PA_SUBSCRIPTION_EVENT_REMOVE:
                    RING_DBG("Updating sink list");
                    context->sinkList_.clear();
                    op = pa_context_get_sink_info_list(c, sink_input_info_callback, userdata);

                    if (op != nullptr)
                        pa_operation_unref(op);

                default:
                    break;
            }

            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
                case PA_SUBSCRIPTION_EVENT_NEW:
                case PA_SUBSCRIPTION_EVENT_REMOVE:
                    RING_DBG("Updating source list");
                    context->sourceList_.clear();
                    op = pa_context_get_source_info_list(c, source_input_info_callback, userdata);

                    if (op != nullptr)
                        pa_operation_unref(op);

                default:
                    break;
            }

            break;

        default:
            RING_DBG("Unhandled event type 0x%x", type);
            break;
    }
}

void PulseLayer::server_info_callback(pa_context*, const pa_server_info *i, void *userdata)
{
    if (!i) return;
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    RING_DBG("PulseAudio server info:\n"
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
    context->defaultSink_ = i->default_sink_name;
    context->defaultSource_ = i->default_source_name;
    context->defaultAudioFormat_ = {i->sample_spec.rate, i->sample_spec.channels};
    {
        std::lock_guard<std::mutex> lk(context->readyMtx_);
        context->gettingServerInfo_ = false;
    }
    context->readyCv_.notify_one();
}

void PulseLayer::source_input_info_callback(pa_context *c UNUSED, const pa_source_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        {
            std::lock_guard<std::mutex> lk(context->readyMtx_);
            context->enumeratingSources_ = false;
        }
        context->readyCv_.notify_one();
        return;
    }
#ifdef PA_LOG_SINK_SOURCES
    RING_DBG("Source %u\n"
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
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        {
            std::lock_guard<std::mutex> lk(context->readyMtx_);
            context->enumeratingSinks_ = false;
        }
        context->readyCv_.notify_one();
        return;
    }
#ifdef PA_LOG_SINK_SOURCES
    RING_DBG("Sink %u\n"
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
            RING_DBG("setting %s for playback", devName.c_str());
            preference.setPulseDevicePlayback(devName);
            break;

        case DeviceType::CAPTURE:
            RING_DBG("setting %s for capture", devName.c_str());
            preference.setPulseDeviceRecord(devName);
            break;

        case DeviceType::RINGTONE:
            RING_DBG("setting %s for ringer", devName.c_str());
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

} // namespace ring
