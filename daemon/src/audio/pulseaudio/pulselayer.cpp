/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "audiostream.h"
#include "pulselayer.h"
#include "audio/samplerateconverter.h"
#include "audio/dcblocker.h"
#include "managerimpl.h"

namespace {

void playback_callback(pa_stream* s, size_t bytes, void* userdata)
{
    assert(s && bytes);
    assert(bytes > 0);
    static_cast<PulseLayer*>(userdata)->writeToSpeaker();
}

void capture_callback(pa_stream* s, size_t bytes, void* userdata)
{
    assert(s && bytes);
    assert(bytes > 0);
    static_cast<PulseLayer*>(userdata)->readFromMic();
}

void ringtone_callback(pa_stream* s, size_t bytes, void* userdata)
{
    assert(s && bytes);
    assert(bytes > 0);
    static_cast<PulseLayer*>(userdata)->ringtoneToSpeaker();
}

void stream_moved_callback(pa_stream *s, void *userdata UNUSED)
{
    DEBUG("stream_moved_callback: stream %d to %d", pa_stream_get_index(s), pa_stream_get_device_index(s));
}

} // end anonymous namespace

PulseLayer::PulseLayer()
    : playback_(0)
    , record_(0)
    , ringtone_(0)
    , sinkList_()
    , sourceList_()
    , mic_buffer_(0)
    , mic_buf_size_(0)
    , context_(0)
    , mainloop_(pa_threaded_mainloop_new())
{
    setenv("PULSE_PROP_media.role", "phone", 1);


    if (!mainloop_)
        throw std::runtime_error("Couldn't create pulseaudio mainloop");

    context_ = pa_context_new(pa_threaded_mainloop_get_api(mainloop_) , "SFLphone");

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
    disconnectAudioStream();

    if (mainloop_)
        pa_threaded_mainloop_stop(mainloop_);

    if (context_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
    }

    if (mainloop_)
        pa_threaded_mainloop_free(mainloop_);

    delete[] mic_buffer_;
}

void PulseLayer::context_state_callback(pa_context* c, void* user_data)
{
    PulseLayer* pulse = (PulseLayer*) user_data;
    assert(c && pulse->mainloop_);

    switch (pa_context_get_state(c)) {

        case PA_CONTEXT_CONNECTING:

        case PA_CONTEXT_AUTHORIZING:

        case PA_CONTEXT_SETTING_NAME:
            DEBUG("Audio: Waiting....");
            break;

        case PA_CONTEXT_READY:
            DEBUG("Audio: Connection to PulseAudio server established");
            pa_threaded_mainloop_signal(pulse->mainloop_, 0);
            pa_context_subscribe(c, (pa_subscription_mask_t)(PA_SUBSCRIPTION_MASK_SINK|
                                 PA_SUBSCRIPTION_MASK_SOURCE), NULL, pulse);
            pa_context_set_subscribe_callback(c, context_changed_callback, pulse);
            pulse->updateSinkList();
            pulse->updateSourceList();
            break;

        case PA_CONTEXT_TERMINATED:
            break;

        case PA_CONTEXT_FAILED:

        default:
            ERROR("Pulse: %s" , pa_strerror(pa_context_errno(c)));
            pa_threaded_mainloop_signal(pulse->mainloop_, 0);
            pulse->disconnectAudioStream();
            break;
    }
}


void PulseLayer::updateSinkList()
{
    sinkList_.clear();
    pa_context_get_sink_info_list(context_, sink_input_info_callback,  this);
}

void PulseLayer::updateSourceList()
{
    sourceList_.clear();
    pa_context_get_source_info_list(context_, source_input_info_callback, this);
}

bool PulseLayer::inSinkList(const std::string &deviceName) const
{
    return std::find(sinkList_.begin(), sinkList_.end(), deviceName) != sinkList_.end();
}


bool PulseLayer::inSourceList(const std::string &deviceName) const
{
    return std::find(sourceList_.begin(), sourceList_.end(), deviceName) != sourceList_.end();
}

std::vector<std::string> PulseLayer::getAudioDeviceList(AudioStreamDirection dir) const
{
    if(AUDIO_STREAM_CAPTURE == dir) {
        return sinkList_;
    }
    if(AUDIO_STREAM_PLAYBACK) {
        return sourceList_;
    }
}

void PulseLayer::createStreams(pa_context* c)
{
    std::string playbackDevice(audioPref.getDevicePlayback());
    std::string captureDevice(audioPref.getDeviceRecord());
    std::string ringtoneDevice(audioPref.getDeviceRingtone());
    std::string defaultDevice = "";

    DEBUG("PulseAudio: Devices:\n   playback: %s\n   record: %s\n   ringtone: %s",
           playbackDevice.c_str(), captureDevice.c_str(), ringtoneDevice.c_str());

    playback_ = new AudioStream(c, mainloop_, "SFLphone playback", PLAYBACK_STREAM, audioSampleRate_,
                                inSourceList(playbackDevice) ? playbackDevice : defaultDevice);

    pa_stream_set_write_callback(playback_->pulseStream(), playback_callback, this);
    pa_stream_set_moved_callback(playback_->pulseStream(), stream_moved_callback, this);

    record_ = new AudioStream(c, mainloop_, "SFLphone capture", CAPTURE_STREAM, audioSampleRate_,
                              inSinkList(captureDevice) ? captureDevice : defaultDevice);

    pa_stream_set_read_callback(record_->pulseStream() , capture_callback, this);
    pa_stream_set_moved_callback(record_->pulseStream(), stream_moved_callback, this);

    ringtone_ = new AudioStream(c, mainloop_, "SFLphone ringtone", RINGTONE_STREAM, audioSampleRate_,
                                inSourceList(ringtoneDevice) ? ringtoneDevice : defaultDevice);

    pa_stream_set_write_callback(ringtone_->pulseStream(), ringtone_callback, this);
    pa_stream_set_moved_callback(ringtone_->pulseStream(), stream_moved_callback, this);

    pa_threaded_mainloop_signal(mainloop_, 0);

    flushMain();
    flushUrgent();
}


void PulseLayer::disconnectAudioStream()
{
    if (playback_) {
        if (playback_->pulseStream()) {
            const char *name = pa_stream_get_device_name(playback_->pulseStream());

            if (name && *name)
                audioPref.setDevicePlayback(name);
        }

        delete playback_;
        playback_ = NULL;
    }

    if (ringtone_) {
        if (ringtone_->pulseStream()) {
            const char *name = pa_stream_get_device_name(ringtone_->pulseStream());

            if (name && *name)
                audioPref.setDeviceRingtone(name);
        }

        delete ringtone_;
        ringtone_ = NULL;
    }

    if (record_) {
        if (record_->pulseStream()) {
            const char *name = pa_stream_get_device_name(record_->pulseStream());

            if (name && *name)
                audioPref.setDeviceRecord(name);
        }

        delete record_;
        record_ = NULL;
    }
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
    int writable = pa_stream_writable_size(s);

    if (writable < 0)
        ERROR("Pulse: playback error : %s", pa_strerror(writable));

    if (writable <= 0)
        return;

    size_t bytes = writable;
    void *data;

    notifyincomingCall();

    size_t urgentBytes = urgentRingBuffer_.AvailForGet();

    if (urgentBytes > bytes)
        urgentBytes = bytes;

    if (urgentBytes) {
        pa_stream_begin_write(s, &data, &urgentBytes);
        urgentRingBuffer_.Get(data, urgentBytes);
        applyGain(static_cast<SFLDataFormat *>(data), urgentBytes / sizeof(SFLDataFormat), getPlaybackGain());  
        pa_stream_write(s, data, urgentBytes, NULL, 0, PA_SEEK_RELATIVE);
        // Consume the regular one as well (same amount of bytes)
        Manager::instance().getMainBuffer()->discard(urgentBytes);
        return;
    }

    AudioLoop *toneToPlay = Manager::instance().getTelephoneTone();

    if (toneToPlay) {
        if (playback_->isReady()) {
            pa_stream_begin_write(s, &data, &bytes);
            toneToPlay->getNext((SFLDataFormat*)data, bytes / sizeof(SFLDataFormat), 100);
            applyGain(static_cast<SFLDataFormat *>(data), bytes / sizeof(SFLDataFormat), getPlaybackGain());
            pa_stream_write(s, data, bytes, NULL, 0, PA_SEEK_RELATIVE);
        }

        return;
    }

    flushUrgent(); // flush remaining samples in _urgentRingBuffer

    size_t availSamples = Manager::instance().getMainBuffer()->availForGet() / sizeof(SFLDataFormat);

    if (availSamples == 0) {
        pa_stream_begin_write(s, &data, &bytes);
        memset(data, 0, bytes);
        pa_stream_write(s, data, bytes, NULL, 0, PA_SEEK_RELATIVE);
        return;
    }

    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();
    bool resample = audioSampleRate_ != mainBufferSampleRate;

    // how much samples we can write in the output
    size_t outSamples = bytes / sizeof(SFLDataFormat);

    // how much samples we want to read from the buffer
    size_t inSamples = outSamples;

    double resampleFactor = 1.;

    if (resample) {
        resampleFactor = (double) audioSampleRate_ / mainBufferSampleRate;
        inSamples = (double) inSamples / resampleFactor;
    }

    if (inSamples > availSamples)
        inSamples = availSamples;

    size_t outBytes = (double)inSamples * resampleFactor * sizeof(SFLDataFormat);

    size_t inBytes = inSamples * sizeof(SFLDataFormat);
    pa_stream_begin_write(s, &data, &inBytes);
    Manager::instance().getMainBuffer()->getData(data, inBytes);

    if (resample) {
        SFLDataFormat* rsmpl_out = (SFLDataFormat*) pa_xmalloc(outBytes);
        converter_->resample((SFLDataFormat*)data, rsmpl_out, mainBufferSampleRate, audioSampleRate_, inSamples);
        applyGain(rsmpl_out, outBytes / sizeof(SFLDataFormat), getPlaybackGain());
        pa_stream_write(s, rsmpl_out, outBytes, NULL, 0, PA_SEEK_RELATIVE);
        pa_xfree(rsmpl_out);
    } else {
        applyGain(static_cast<SFLDataFormat *>(data), inBytes / sizeof(SFLDataFormat), getPlaybackGain());
        pa_stream_write(s, data, inBytes, NULL, 0, PA_SEEK_RELATIVE);
    }
}

void PulseLayer::readFromMic()
{
    if (!record_ or !record_->isReady())
        return;

    const char *data = NULL;
    size_t bytes;

    if (pa_stream_peek(record_->pulseStream() , (const void**) &data , &bytes) < 0 or !data) {
        ERROR("Audio: Error capture stream peek failed: %s" , pa_strerror(pa_context_errno(context_)));
        return;
    }

    unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer()->getInternalSamplingRate();
    bool resample = audioSampleRate_ != mainBufferSampleRate;

    if (resample) {
        double resampleFactor = (double) audioSampleRate_ / mainBufferSampleRate;
        bytes = (double) bytes * resampleFactor;
    }

    size_t samples = bytes / sizeof(SFLDataFormat);

    if (bytes > mic_buf_size_) {
        mic_buf_size_ = bytes;
        delete[] mic_buffer_;
        mic_buffer_ = new SFLDataFormat[samples];
    }

    if (resample)
        converter_->resample((SFLDataFormat*)data, mic_buffer_, mainBufferSampleRate, audioSampleRate_, samples);

    dcblocker_.process(mic_buffer_, resample ? mic_buffer_ : (SFLDataFormat*)data, samples);
    applyGain(mic_buffer_, bytes / sizeof(SFLDataFormat), getCaptureGain());
    Manager::instance().getMainBuffer()->putData(mic_buffer_, bytes);

    if (pa_stream_drop(record_->pulseStream()) < 0)
        ERROR("Audio: Error: capture stream drop failed: %s" , pa_strerror(pa_context_errno(context_)));
}


void PulseLayer::ringtoneToSpeaker()
{
    if (!ringtone_ or !ringtone_->isReady())
        return;

    pa_stream *s = ringtone_->pulseStream();

    int writable = pa_stream_writable_size(s);

    if (writable < 0)
        ERROR("Pulse: ringtone error : %s", pa_strerror(writable));

    if (writable <= 0)
        return;

    size_t bytes = writable;
    void *data;

    pa_stream_begin_write(s, &data, &bytes);
    AudioLoop *fileToPlay = Manager::instance().getTelephoneFile();

    if (fileToPlay) {
        fileToPlay->getNext((SFLDataFormat *) data, bytes / sizeof(SFLDataFormat), 100);
        applyGain(static_cast<SFLDataFormat *>(data), bytes / sizeof(SFLDataFormat), getPlaybackGain());
    }
    else
        memset(data, 0, bytes);

    pa_stream_write(s, data, bytes, NULL, 0, PA_SEEK_RELATIVE);
}

void PulseLayer::context_changed_callback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx UNUSED, void* userdata)
{

    switch (t) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_SINK");
            ((PulseLayer *) userdata)->sinkList_.clear();
            pa_context_get_sink_info_list(c, sink_input_info_callback,  userdata);
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_SOURCE");
            ((PulseLayer *) userdata)->sourceList_.clear();
            pa_context_get_source_info_list(c, source_input_info_callback,  userdata);
            break;
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_SINK_INPUT");
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT");
            break;
        case PA_SUBSCRIPTION_EVENT_MODULE:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_MODULE");
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_CLIENT");
            break;
        case PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE");
            break;
        case PA_SUBSCRIPTION_EVENT_SERVER:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_SERVER");
            break;
        case PA_SUBSCRIPTION_EVENT_CARD:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_CARD");
            break;
        case PA_SUBSCRIPTION_EVENT_FACILITY_MASK:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_FACILITY_MASK");
            break;
        case PA_SUBSCRIPTION_EVENT_CHANGE:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_CHANGE");
            break;
        case PA_SUBSCRIPTION_EVENT_REMOVE:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_REMOVE");
            ((PulseLayer *) userdata)->sinkList_.clear();
            ((PulseLayer *) userdata)->sourceList_.clear();
            pa_context_get_sink_info_list(c, sink_input_info_callback,  userdata);
            pa_context_get_source_info_list(c, source_input_info_callback,  userdata);
            break;
        case PA_SUBSCRIPTION_EVENT_TYPE_MASK:
            DEBUG("Audio: PA_SUBSCRIPTION_EVENT_TYPE_MASK");
            break;
        default:
            DEBUG("Audio: Unknown event type %d", t);

    }
}

void PulseLayer::source_input_info_callback(pa_context *c UNUSED, const pa_source_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];

    if (eol)
        return;

    DEBUG("Sink %u\n"
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

    ((PulseLayer *)userdata)->sourceList_.push_back(i->name);
}

void PulseLayer::sink_input_info_callback(pa_context *c UNUSED, const pa_sink_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];

    if (eol)
        return;

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
           (double) i->latency,
           i->flags & PA_SINK_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
           i->flags & PA_SINK_LATENCY ? "LATENCY " : "",
           i->flags & PA_SINK_HARDWARE ? "HARDWARE" : "");

    ((PulseLayer *)userdata)->sinkList_.push_back(i->name);
}

