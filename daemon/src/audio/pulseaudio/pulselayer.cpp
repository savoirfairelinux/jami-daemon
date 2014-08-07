/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "audiostream.h"
#include "pulselayer.h"
#include "audio/resampler.h"
#include "audio/dcblocker.h"
#include "logger.h"
#include "manager.h"

#include <unistd.h>
#include <cstdlib>
#include <fstream>

static void
playback_callback(pa_stream * /*s*/, size_t /*bytes*/, void* userdata)
{
    static_cast<PulseLayer*>(userdata)->writeToSpeaker();
}

static void
capture_callback(pa_stream * /*s*/, size_t /*bytes*/, void* userdata)
{
    static_cast<PulseLayer*>(userdata)->readFromMic();
}

static void
ringtone_callback(pa_stream * /*s*/, size_t /*bytes*/, void* userdata)
{
    static_cast<PulseLayer*>(userdata)->ringtoneToSpeaker();
}

static void
stream_moved_callback(pa_stream *s, void *userdata UNUSED)
{
    DEBUG("stream %d to %d", pa_stream_get_index(s), pa_stream_get_device_index(s));
}

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
    , playback_(nullptr)
    , record_(nullptr)
    , ringtone_(nullptr)
    , sinkList_()
    , sourceList_()
    , micBuffer_(0, AudioFormat::MONO)
    , context_(nullptr)
    , mainloop_(pa_threaded_mainloop_new())
    , enumeratingSinks_(false)
    , enumeratingSources_(false)
    , preference_(pref)
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

    context_ = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mainloop_), "SFLphone", pl);

    if (pl)
        pa_proplist_free(pl);

#else
    setenv("PULSE_PROP_media.role", "phone", 1);
    context_ = pa_context_new(pa_threaded_mainloop_get_api(mainloop_), "SFLphone");
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
    }

    pa_threaded_mainloop_stop(mainloop_);
    pa_threaded_mainloop_free(mainloop_);
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
            DEBUG("Waiting....");
            break;

        case PA_CONTEXT_READY:
            DEBUG("Connection to PulseAudio server established");
            pa_threaded_mainloop_signal(pulse->mainloop_, 0);
            pa_context_subscribe(c, mask, nullptr, pulse);
            pa_context_set_subscribe_callback(c, context_changed_callback, pulse);
            pulse->updateSinkList();
            pulse->updateSourceList();
            break;

        case PA_CONTEXT_TERMINATED:
            break;

        case PA_CONTEXT_FAILED:
        default:
            ERROR("%s" , pa_strerror(pa_context_errno(c)));
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

bool PulseLayer::inSinkList(const std::string &deviceName)
{
    const bool found = std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::NameComparator(deviceName)) != sinkList_.end();

    DEBUG("seeking for %s in sinks. %s found", deviceName.c_str(), found ? "" : "NOT");
    return found;
}

bool PulseLayer::inSourceList(const std::string &deviceName)
{
    const bool found = std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::NameComparator(deviceName)) != sourceList_.end();

    DEBUG("seeking for %s in sources. %s found", deviceName.c_str(), found ? "" : "NOT");
    return found;
}

std::vector<std::string> PulseLayer::getCaptureDeviceList() const
{
    const unsigned n = sourceList_.size();
    std::vector<std::string> names(n);

    for (unsigned i = 0; i < n; i++)
        names[i] = sourceList_[i].description;

    return names;
}

std::vector<std::string> PulseLayer::getPlaybackDeviceList() const
{
    const unsigned n = sinkList_.size();
    std::vector<std::string> names(n);

    for (unsigned i = 0; i < n; i++)
        names[i] = sinkList_[i].description;

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
        ERROR("Unexpected device type");
        return 0;
    }
}

int PulseLayer::getAudioDeviceIndexByName(const std::string& name, DeviceType type) const
{
    switch (type) {
    case DeviceType::PLAYBACK:
    case DeviceType::RINGTONE:
        return std::distance(sinkList_.begin(), std::find_if(sinkList_.begin(), sinkList_.end(), PaDeviceInfos::NameComparator(name)));
    case DeviceType::CAPTURE:
        return std::distance(sourceList_.begin(), std::find_if(sourceList_.begin(), sourceList_.end(), PaDeviceInfos::NameComparator(name)));
    default:
        ERROR("Unexpected device type");
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
    switch (type) {
        case DeviceType::PLAYBACK:
        case DeviceType::RINGTONE:
            if (index < 0 or static_cast<size_t>(index) >= sinkList_.size()) {
                ERROR("Index %d out of range", index);
                return "";
            }

            return sinkList_[index].name;

        case DeviceType::CAPTURE:
            if (index < 0 or static_cast<size_t>(index) >= sourceList_.size()) {
                ERROR("Index %d out of range", index);
                return "";
            }

            return sourceList_[index].name;
        default:
            // Should never happen
            ERROR("Unexpected type");
            return "";
    }
}

void PulseLayer::createStreams(pa_context* c)
{
    while (enumeratingSinks_ or enumeratingSources_)
        usleep(20000); // 20 ms

    std::string playbackDevice(preference_.getPulseDevicePlayback());
    std::string captureDevice(preference_.getPulseDeviceRecord());
    std::string ringtoneDevice(preference_.getPulseDeviceRingtone());
    std::string defaultDevice = "";

    DEBUG("playback: %s record: %s ringtone: %s", playbackDevice.c_str(),
          captureDevice.c_str(), ringtoneDevice.c_str());

    // Create playback stream
    const PaDeviceInfos* dev_infos = getDeviceInfos(sinkList_, playbackDevice);

    if (dev_infos == nullptr) {
        dev_infos = &sinkList_[0];
        WARN("Prefered playback device %s not found in device list, selecting %s instead.",
             playbackDevice.c_str(), dev_infos->name.c_str());
    }

    playback_ = new AudioStream(c, mainloop_, "SFLphone playback", PLAYBACK_STREAM, audioFormat_.sample_rate, dev_infos);

    pa_stream_set_write_callback(playback_->pulseStream(), playback_callback, this);
    pa_stream_set_moved_callback(playback_->pulseStream(), stream_moved_callback, this);

    // Create capture stream
    dev_infos = getDeviceInfos(sourceList_, captureDevice);

    if (dev_infos == nullptr) {
        dev_infos = &sourceList_[0];
        WARN("Prefered capture device %s not found in device list, selecting %s instead.",
             captureDevice.c_str(), dev_infos->name.c_str());
    }

    record_ = new AudioStream(c, mainloop_, "SFLphone capture", CAPTURE_STREAM, audioFormat_.sample_rate, dev_infos);

    pa_stream_set_read_callback(record_->pulseStream() , capture_callback, this);
    pa_stream_set_moved_callback(record_->pulseStream(), stream_moved_callback, this);

    // Create ringtone stream
    dev_infos = getDeviceInfos(sinkList_, ringtoneDevice);

    if (dev_infos == nullptr) {
        dev_infos = &sinkList_[0];
        WARN("Prefered ringtone device %s not found in device list, selecting %s instead.",
             ringtoneDevice.c_str(), dev_infos->name.c_str());
    }

    ringtone_ = new AudioStream(c, mainloop_, "SFLphone ringtone", RINGTONE_STREAM, audioFormat_.sample_rate, dev_infos);

    hardwareFormatAvailable(playback_->getFormat());

    pa_stream_set_write_callback(ringtone_->pulseStream(), ringtone_callback, this);
    pa_stream_set_moved_callback(ringtone_->pulseStream(), stream_moved_callback, this);

    pa_threaded_mainloop_signal(mainloop_, 0);

    flushMain();
    flushUrgent();
}

// Delete stream and zero out its pointer
static void
cleanupStream(AudioStream *&stream)
{
    delete stream;
    stream = 0;
}

void PulseLayer::disconnectAudioStream()
{
    cleanupStream(playback_);
    cleanupStream(ringtone_);
    cleanupStream(record_);
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
            pa_stream_flush(playback_->pulseStream(), nullptr, nullptr);

        if (record_)
            pa_stream_flush(record_->pulseStream(), nullptr, nullptr);
    }

    disconnectAudioStream();
}

void PulseLayer::writeToSpeaker()
{
    if (!playback_ or !playback_->isReady())
        return;

    pa_stream *s = playback_->pulseStream();
    const pa_sample_spec* sample_spec = pa_stream_get_sample_spec(s);
    size_t sample_size = pa_frame_size(sample_spec);
    const AudioFormat format(playback_->getFormat());

    // available bytes to be written in pulseaudio internal buffer
    int ret = pa_stream_writable_size(s);

    if (ret < 0) {
        ERROR("Playback error : %s", pa_strerror(ret));
        return;
    } else if (ret == 0)
        return;

    size_t writableBytes = ret;
    const size_t writableSamples = writableBytes / sample_size;

    notifyIncomingCall();

    size_t urgentSamples = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);
    size_t urgentBytes = urgentSamples * sample_size;

    if (urgentSamples > writableSamples) {
        urgentSamples = writableSamples;
        urgentBytes = urgentSamples * sample_size;
    }

    SFLAudioSample *data = 0;

    if (urgentBytes) {
        AudioBuffer linearbuff(urgentSamples, format);
        pa_stream_begin_write(s, (void**)&data, &urgentBytes);
        urgentRingBuffer_.get(linearbuff, MainBuffer::DEFAULT_ID); // retrive only the first sample_spec->channels channels
        linearbuff.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        linearbuff.interleave(data);
        pa_stream_write(s, data, urgentBytes, nullptr, 0, PA_SEEK_RELATIVE);
        // Consume the regular one as well (same amount of samples)
        Manager::instance().getMainBuffer().discard(urgentSamples, MainBuffer::DEFAULT_ID);
        return;
    }

    // FIXME: not thread safe! we only lock the mutex when we get the
    // pointer, we have no guarantee that it will stay safe to use
    AudioLoop *toneToPlay = Manager::instance().getTelephoneTone();

    if (toneToPlay) {
        if (playback_->isReady()) {
            pa_stream_begin_write(s, (void**)&data, &writableBytes);
            AudioBuffer linearbuff(writableSamples, format);
            toneToPlay->getNext(linearbuff, playbackGain_); // retrive only n_channels
            linearbuff.interleave(data);
            pa_stream_write(s, data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
        }

        return;
    }

    flushUrgent(); // flush remaining samples in _urgentRingBuffer

    size_t availSamples = Manager::instance().getMainBuffer().availableForGet(MainBuffer::DEFAULT_ID);

    if (availSamples == 0) {
        pa_stream_begin_write(s, (void**)&data, &writableBytes);
        memset(data, 0, writableBytes);
        pa_stream_write(s, data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
        return;
    }

    // how many samples we want to read from the buffer
    size_t readableSamples = writableSamples;

    double resampleFactor = 1.;

    AudioFormat mainBufferAudioFormat = Manager::instance().getMainBuffer().getInternalAudioFormat();
    bool resample = audioFormat_.sample_rate != mainBufferAudioFormat.sample_rate;

    if (resample) {
        resampleFactor = (double) audioFormat_.sample_rate / mainBufferAudioFormat.sample_rate;
        readableSamples = (double) readableSamples / resampleFactor;
    }

    readableSamples = std::min(readableSamples, availSamples);
    size_t nResampled = (double) readableSamples * resampleFactor;
    size_t resampledBytes =  nResampled * sample_size;

    pa_stream_begin_write(s, (void**)&data, &resampledBytes);

    AudioBuffer linearbuff(readableSamples, format);
    Manager::instance().getMainBuffer().getData(linearbuff, MainBuffer::DEFAULT_ID);

    if (resample) {
        AudioBuffer rsmpl_out(nResampled, format);
        resampler_.resample(linearbuff, rsmpl_out);
        rsmpl_out.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        rsmpl_out.interleave(data);
        pa_stream_write(s, data, resampledBytes, nullptr, 0, PA_SEEK_RELATIVE);
    } else {
        linearbuff.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        linearbuff.interleave(data);
        pa_stream_write(s, data, resampledBytes, nullptr, 0, PA_SEEK_RELATIVE);
    }
}

void PulseLayer::readFromMic()
{
    if (!record_ or !record_->isReady())
        return;

    const char *data = nullptr;
    size_t bytes;

    const size_t sample_size = record_->sampleSize();
    const AudioFormat format(record_->getFormat());

    if (pa_stream_peek(record_->pulseStream() , (const void**) &data , &bytes) < 0 or !data)
        return;

    assert(format.nb_channels);
    assert(sample_size);
    const size_t samples = bytes / sample_size / format.nb_channels;

    AudioBuffer in(samples, format);
    in.deinterleave((SFLAudioSample*)data, samples, format.nb_channels);

    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();
    bool resample = audioFormat_.sample_rate != mainBufferSampleRate;

    in.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    AudioBuffer * out = &in;

    if (resample) {
        micBuffer_.setSampleRate(mainBufferSampleRate);
        resampler_.resample(in, micBuffer_);
        out = &micBuffer_;
    }

    dcblocker_.process(*out);
    out->applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
    Manager::instance().getMainBuffer().putData(*out, MainBuffer::DEFAULT_ID);

    if (pa_stream_drop(record_->pulseStream()) < 0)
        ERROR("Capture stream drop failed: %s" , pa_strerror(pa_context_errno(context_)));
}


void PulseLayer::ringtoneToSpeaker()
{
    if (!ringtone_ or !ringtone_->isReady())
        return;

    pa_stream *s = ringtone_->pulseStream();
    size_t sample_size = ringtone_->sampleSize();

    int writable = pa_stream_writable_size(s);

    if (writable < 0)
        ERROR("Ringtone error : %s", pa_strerror(writable));

    if (writable <= 0)
        return;

    size_t bytes = writable;
    void *data;

    pa_stream_begin_write(s, &data, &bytes);
    AudioLoop *fileToPlay = Manager::instance().getTelephoneFile();

    if (fileToPlay) {
        const unsigned samples = (bytes / sample_size) / ringtone_->channels();
        AudioBuffer tmp(samples, ringtone_->getFormat());
        fileToPlay->getNext(tmp, playbackGain_);
        tmp.interleave((SFLAudioSample*) data);
    } else {
        memset(data, 0, bytes);
    }

    pa_stream_write(s, data, bytes, nullptr, 0, PA_SEEK_RELATIVE);
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
                    DEBUG("Updating sink list");
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
                    DEBUG("Updating source list");
                    context->sourceList_.clear();
                    op = pa_context_get_source_info_list(c, source_input_info_callback, userdata);

                    if (op != nullptr)
                        pa_operation_unref(op);

                default:
                    break;
            }

            break;

        default:
            DEBUG("Unhandled event type 0x%x", type);
            break;
    }
}


void PulseLayer::source_input_info_callback(pa_context *c UNUSED, const pa_source_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        context->enumeratingSources_ = false;
        return;
    }

    DEBUG("Source %u\n"
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

    if (not context->inSourceList(i->name)) {
        context->sourceList_.push_back(*i);
    }
}

void PulseLayer::sink_input_info_callback(pa_context *c UNUSED, const pa_sink_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    PulseLayer *context = static_cast<PulseLayer*>(userdata);

    if (eol) {
        context->enumeratingSinks_ = false;
        return;
    }

    DEBUG("Sink %u\n"
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

    if (not context->inSinkList(i->name)) {
        context->sinkList_.push_back(*i);
    }
}

void PulseLayer::updatePreference(AudioPreference &preference, int index, DeviceType type)
{
    const std::string devName(getAudioDeviceName(index, type));

    switch (type) {
        case DeviceType::PLAYBACK:
            DEBUG("setting %s for playback", devName.c_str());
            preference.setPulseDevicePlayback(devName);
            break;

        case DeviceType::CAPTURE:
            DEBUG("setting %s for capture", devName.c_str());
            preference.setPulseDeviceRecord(devName);
            break;

        case DeviceType::RINGTONE:
            DEBUG("setting %s for ringer", devName.c_str());
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
