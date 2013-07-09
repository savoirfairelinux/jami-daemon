/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Андрей Лухнов <aol.nnov@gmail.com>
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
#include "audio/samplerateconverter.h"
#include "audio/dcblocker.h"
#include "logger.h"
#include "manager.h"

#include <cstdlib>
#include <fstream>

namespace {

void playback_callback(pa_stream * /*s*/, size_t /*bytes*/, void* userdata)
{
    static_cast<PulseLayer*>(userdata)->writeToSpeaker();
}

void capture_callback(pa_stream * /*s*/, size_t /*bytes*/, void* userdata)
{
    static_cast<PulseLayer*>(userdata)->readFromMic();
}

void ringtone_callback(pa_stream * /*s*/, size_t /*bytes*/, void* userdata)
{
    static_cast<PulseLayer*>(userdata)->ringtoneToSpeaker();
}

void stream_moved_callback(pa_stream *s, void *userdata UNUSED)
{
    DEBUG("stream %d to %d", pa_stream_get_index(s), pa_stream_get_device_index(s));
}

} // end anonymous namespace

#ifdef RECTODISK
std::ofstream outfileResampled("testMicOuputResampled.raw", std::ifstream::binary);
std::ofstream outfile("testMicOuput.raw", std::ifstream::binary);
#endif

PulseLayer::PulseLayer(AudioPreference &pref)
    : playback_(0)
    , record_(0)
    , ringtone_(0)
    , sinkList_()
    , sourceList_()
    , mic_buffer_(0)
    , mic_buf_size_(0)
    , context_(0)
    , mainloop_(pa_threaded_mainloop_new())
    , enumeratingSinks_(false)
    , enumeratingSources_(false)
    , preference_(pref)
{
    if (!mainloop_)
        throw std::runtime_error("Couldn't create pulseaudio mainloop");

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

    if (!context_)
        throw std::runtime_error("Couldn't create pulseaudio context");

    pa_context_set_state_callback(context_, context_state_callback, this);

    if (pa_context_connect(context_, NULL , PA_CONTEXT_NOAUTOSPAWN , NULL) < 0)
        throw std::runtime_error("Could not connect pulseaudio context to the server");

    pa_threaded_mainloop_lock(mainloop_);

    if (pa_threaded_mainloop_start(mainloop_) < 0)
        throw std::runtime_error("Failed to start pulseaudio mainloop");

    pa_threaded_mainloop_wait(mainloop_);

    if (pa_context_get_state(context_) != PA_CONTEXT_READY)
        throw std::runtime_error("Couldn't connect to pulse audio server");

    pa_threaded_mainloop_unlock(mainloop_);

    isStarted_ = true;
}

PulseLayer::~PulseLayer()
{
#ifdef RECTODISK
    outfile.close();
    outfileResampled.close();
#endif

    disconnectAudioStream();

    if (mainloop_)
        pa_threaded_mainloop_stop(mainloop_);

    if (context_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
    }

    if (mainloop_)
        pa_threaded_mainloop_free(mainloop_);

    delete [] mic_buffer_;
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
            pa_context_subscribe(c, mask, NULL, pulse);
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
            pulse->disconnectAudioStream();
            break;
    }
}


void PulseLayer::updateSinkList()
{
    sinkList_.clear();
    enumeratingSinks_ = true;
    pa_operation *op = pa_context_get_sink_info_list(context_, sink_input_info_callback, this);

    if (op != NULL)
        pa_operation_unref(op);
}

void PulseLayer::updateSourceList()
{
    sourceList_.clear();
    enumeratingSources_ = true;
    pa_operation *op = pa_context_get_source_info_list(context_, source_input_info_callback, this);

    if (op != NULL)
        pa_operation_unref(op);
}

bool PulseLayer::inSinkList(const std::string &deviceName) const
{
    const bool found = std::find(sinkList_.begin(), sinkList_.end(), deviceName) != sinkList_.end();
    DEBUG("seeking for %s in sinks. %s found", deviceName.c_str(), found ? "" : "NOT");
    return found;
}


bool PulseLayer::inSourceList(const std::string &deviceName) const
{
    const bool found = std::find(sourceList_.begin(), sourceList_.end(), deviceName) != sourceList_.end();
    DEBUG("seeking for %s in sources. %s found", deviceName.c_str(), found ? "" : "NOT");
    return found;
}

std::vector<std::string> PulseLayer::getCaptureDeviceList() const
{
    return sourceList_;
}

std::vector<std::string> PulseLayer::getPlaybackDeviceList() const
{
    return sinkList_;
}

int PulseLayer::getAudioDeviceIndex(const std::string& name) const
{
    int index = std::distance(sourceList_.begin(), std::find(sourceList_.begin(), sourceList_.end(), name));

    if (index == std::distance(sourceList_.begin(), sourceList_.end())) {
        // not found in sources, search in sinks then
        index = std::distance(sinkList_.begin(), std::find(sinkList_.begin(), sinkList_.end(), name));
    }

    return index;
}

std::string PulseLayer::getAudioDeviceName(int index, PCMType type) const
{
    switch (type) {
        case SFL_PCM_PLAYBACK:
        case SFL_PCM_RINGTONE:
            if (index < 0 or static_cast<size_t>(index) >= sinkList_.size()) {
                ERROR("Index %d out of range", index);
                return "";
            }

            return sinkList_[index];

        case SFL_PCM_CAPTURE:
            if (index < 0 or static_cast<size_t>(index) >= sourceList_.size()) {
                ERROR("Index %d out of range", index);
                return "";
            }

            return sourceList_[index];

        default:
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

    DEBUG("Devices:\n   playback: %s\n   record: %s\n   ringtone: %s",
          playbackDevice.c_str(), captureDevice.c_str(), ringtoneDevice.c_str());

    playback_ = new AudioStream(c, mainloop_, "SFLphone playback", PLAYBACK_STREAM, sampleRate_,
                                inSinkList(playbackDevice) ? playbackDevice : defaultDevice);

    pa_stream_set_write_callback(playback_->pulseStream(), playback_callback, this);
    pa_stream_set_moved_callback(playback_->pulseStream(), stream_moved_callback, this);

    record_ = new AudioStream(c, mainloop_, "SFLphone capture", CAPTURE_STREAM, sampleRate_,
                              inSourceList(captureDevice) ? captureDevice : defaultDevice);

    pa_stream_set_read_callback(record_->pulseStream() , capture_callback, this);
    pa_stream_set_moved_callback(record_->pulseStream(), stream_moved_callback, this);

    ringtone_ = new AudioStream(c, mainloop_, "SFLphone ringtone", RINGTONE_STREAM, sampleRate_,
                                inSinkList(ringtoneDevice) ? ringtoneDevice : defaultDevice);

    pa_stream_set_write_callback(ringtone_->pulseStream(), ringtone_callback, this);
    pa_stream_set_moved_callback(ringtone_->pulseStream(), stream_moved_callback, this);

    pa_threaded_mainloop_signal(mainloop_, 0);

    flushMain();
    flushUrgent();
}

namespace {
// Delete stream and zero out its pointer
void
cleanupStream(AudioStream *&stream)
{
    delete stream;
    stream = 0;
}
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
    pa_threaded_mainloop_lock(mainloop_);

    if (playback_)
        pa_stream_flush(playback_->pulseStream(), NULL, NULL);

    if (record_)
        pa_stream_flush(record_->pulseStream(), NULL, NULL);

    pa_threaded_mainloop_unlock(mainloop_);

    disconnectAudioStream();
}

void PulseLayer::writeToSpeaker()
{
    if (!playback_ or !playback_->isReady())
        return;

    pa_stream *s = playback_->pulseStream();

    // available bytes to be written in pulseaudio internal buffer
    int ret = pa_stream_writable_size(s);

    if (ret < 0) {
        ERROR("Playback error : %s", pa_strerror(ret));
        return;
    } else if (ret == 0)
        return;

    size_t writableBytes = ret;

    notifyIncomingCall();

    size_t urgentBytes = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);

    if (urgentBytes > writableBytes)
        urgentBytes = writableBytes;

    void *data = 0;

    if (urgentBytes) {
        pa_stream_begin_write(s, &data, &urgentBytes);
        urgentRingBuffer_.get(data, urgentBytes, MainBuffer::DEFAULT_ID);
        applyGain(static_cast<SFLDataFormat *>(data), urgentBytes / sizeof(SFLDataFormat), getPlaybackGain());
        pa_stream_write(s, data, urgentBytes, NULL, 0, PA_SEEK_RELATIVE);
        // Consume the regular one as well (same amount of bytes)
        Manager::instance().getMainBuffer().discard(urgentBytes, MainBuffer::DEFAULT_ID);
        return;
    }

    // FIXME: not thread safe! we only lock the mutex when we get the
    // pointer, we have no guarantee that it will stay safe to use
    AudioLoop *toneToPlay = Manager::instance().getTelephoneTone();

    if (toneToPlay) {
        if (playback_->isReady()) {
            pa_stream_begin_write(s, &data, &writableBytes);
            toneToPlay->getNext((SFLDataFormat*)data, writableBytes / sizeof(SFLDataFormat), 100);
            applyGain(static_cast<SFLDataFormat *>(data), writableBytes / sizeof(SFLDataFormat), getPlaybackGain());
            pa_stream_write(s, data, writableBytes, NULL, 0, PA_SEEK_RELATIVE);
        }

        return;
    }

    flushUrgent(); // flush remaining samples in _urgentRingBuffer

    size_t availSamples = Manager::instance().getMainBuffer().availableForGet(MainBuffer::DEFAULT_ID) / sizeof(SFLDataFormat);

    if (availSamples == 0) {
        pa_stream_begin_write(s, &data, &writableBytes);
        memset(data, 0, writableBytes);
        pa_stream_write(s, data, writableBytes, NULL, 0, PA_SEEK_RELATIVE);
        return;
    }

    // how many samples we can write to the output
    size_t writableSamples = writableBytes / sizeof(SFLDataFormat);

    // how many samples we want to read from the buffer
    size_t readableSamples = writableSamples;

    double resampleFactor = 1.;

    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();
    bool resample = sampleRate_ != mainBufferSampleRate;

    if (resample) {
        resampleFactor = (double) sampleRate_ / mainBufferSampleRate;
        readableSamples = (double) readableSamples / resampleFactor;
    }

    if (readableSamples > availSamples)
        readableSamples = availSamples;

    size_t readableBytes = readableSamples * sizeof(SFLDataFormat);
    pa_stream_begin_write(s, &data, &readableBytes);
    Manager::instance().getMainBuffer().getData(data, readableBytes, MainBuffer::DEFAULT_ID);

    if (resample) {
        const size_t nResampled = (double) readableSamples * resampleFactor;
        size_t resampledBytes =  nResampled * sizeof(SFLDataFormat);
        SFLDataFormat* rsmpl_out = (SFLDataFormat*) pa_xmalloc(resampledBytes);
        converter_.resample((SFLDataFormat*)data, rsmpl_out, nResampled,
                            mainBufferSampleRate, sampleRate_, readableSamples);
        applyGain(rsmpl_out, nResampled, getPlaybackGain());
        pa_stream_write(s, rsmpl_out, resampledBytes, NULL, 0, PA_SEEK_RELATIVE);
        pa_xfree(rsmpl_out);
    } else {
        applyGain(static_cast<SFLDataFormat *>(data), readableSamples, getPlaybackGain());
        pa_stream_write(s, data, readableBytes, NULL, 0, PA_SEEK_RELATIVE);
    }
}

void PulseLayer::readFromMic()
{
    if (!record_ or !record_->isReady())
        return;

    const char *data = NULL;
    size_t bytes;

    if (pa_stream_peek(record_->pulseStream() , (const void**) &data , &bytes) < 0 or !data)
        return;

    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();
    bool resample = sampleRate_ != mainBufferSampleRate;

    if (resample) {
        double resampleFactor = (double) sampleRate_ / mainBufferSampleRate;
        bytes = (double) bytes * resampleFactor;
    }

    size_t samples = bytes / sizeof(SFLDataFormat);

    if (bytes > mic_buf_size_) {
        mic_buf_size_ = bytes;
        delete [] mic_buffer_;
        mic_buffer_ = new SFLDataFormat[samples];
    }

#ifdef RECTODISK
    outfile.write((const char *)data, bytes);
#endif

    if (resample) {
        converter_.resample((SFLDataFormat*)data, mic_buffer_, samples, mainBufferSampleRate, sampleRate_, samples);
    }

    dcblocker_.process(mic_buffer_, (SFLDataFormat*)data, samples);
    applyGain(mic_buffer_, bytes / sizeof(SFLDataFormat), getCaptureGain());
    Manager::instance().getMainBuffer().putData(mic_buffer_, bytes, MainBuffer::DEFAULT_ID);
#ifdef RECTODISK
    outfileResampled.write((const char *)mic_buffer_, bytes);
#endif

    if (pa_stream_drop(record_->pulseStream()) < 0)
        ERROR("Capture stream drop failed: %s" , pa_strerror(pa_context_errno(context_)));
}


void PulseLayer::ringtoneToSpeaker()
{
    if (!ringtone_ or !ringtone_->isReady())
        return;

    pa_stream *s = ringtone_->pulseStream();

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
        fileToPlay->getNext((SFLDataFormat *) data, bytes / sizeof(SFLDataFormat), 100);
        applyGain(static_cast<SFLDataFormat *>(data), bytes / sizeof(SFLDataFormat), getPlaybackGain());
    } else
        memset(data, 0, bytes);

    pa_stream_write(s, data, bytes, NULL, 0, PA_SEEK_RELATIVE);
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

                    if (op != NULL)
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

                    if (op != NULL)
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

    if (not context->inSourceList(i->name))
        context->sourceList_.push_back(i->name);
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

    if (not context->inSinkList(i->name))
        context->sinkList_.push_back(i->name);
}

void PulseLayer::updatePreference(AudioPreference &preference, int index, PCMType type)
{
    const std::string devName(getAudioDeviceName(index, type));

    switch (type) {
        case SFL_PCM_PLAYBACK:
            DEBUG("setting %s for playback", devName.c_str());
            preference.setPulseDevicePlayback(devName);
            break;

        case SFL_PCM_CAPTURE:
            DEBUG("setting %s for capture", devName.c_str());
            preference.setPulseDeviceRecord(devName);
            break;

        case SFL_PCM_RINGTONE:
            DEBUG("setting %s for ringer", devName.c_str());
            preference.setPulseDeviceRingtone(devName);
            break;

        default:
            break;
    }
}

int PulseLayer::getIndexCapture() const
{
    return getAudioDeviceIndex(preference_.getPulseDeviceRecord());
}

int PulseLayer::getIndexPlayback() const
{
    return getAudioDeviceIndex(preference_.getPulseDevicePlayback());
}

int PulseLayer::getIndexRingtone() const
{
    return getAudioDeviceIndex(preference_.getPulseDeviceRingtone());
}
