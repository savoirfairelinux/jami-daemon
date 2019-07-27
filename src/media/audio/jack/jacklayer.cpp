/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "jacklayer.h"

#include "logger.h"
#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"
#include "audio/audioloop.h"
#include "manager.h"
#include "array_size.h"

#include <unistd.h>

#include <cassert>
#include <climits>

/* TODO
 * implement shutdown callback
 * auto connect optional
 */

namespace jami {

namespace
{
void connectPorts(jack_client_t *client, int portType, const std::vector<jack_port_t *> &ports)
{
    const char **physical_ports = jack_get_ports(client, NULL, NULL, portType | JackPortIsPhysical);
    for (unsigned i = 0; physical_ports[i]; ++i) {
        if (i >= ports.size())
            break;
        const char *port = jack_port_name(ports[i]);
        if (portType & JackPortIsInput) {
            if (jack_connect(client, port, physical_ports[i])) {
                JAMI_ERR("Can't connect %s to %s", port, physical_ports[i]);
                break;
            }
        } else {
            if (jack_connect(client, physical_ports[i], port)) {
                JAMI_ERR("Can't connect port %s to %s", physical_ports[i], port);
                break;
            }
        }
    }
    jack_free(physical_ports);
}

bool ringbuffer_ready_for_read(const jack_ringbuffer_t *rb)
{
    // XXX 512 is arbitrary
    return jack_ringbuffer_read_space(rb) > 512;
}
}

void
JackLayer::playback()
{
    notifyIncomingCall();
    auto format = audioFormat_;
    format.sampleFormat = AV_SAMPLE_FMT_FLTP;
    if (auto toPlay = getPlayback(format, writeSpace())) {
        write(*toPlay);
    }
}

void
JackLayer::capture()
{
    if (auto buf = read())
        mainRingBuffer_->put(std::move(buf));
}

size_t
JackLayer::writeSpace()
{
    if (out_ringbuffers_.empty())
        return 0;
    size_t toWrite {std::numeric_limits<size_t>::max()};
    for (unsigned i = 0; i < out_ringbuffers_.size(); ++i) {
        toWrite = std::min(toWrite, jack_ringbuffer_write_space(out_ringbuffers_[i]));
    }
    return std::min<size_t>(toWrite / sizeof(float), audioFormat_.sample_rate / 25);
}

void
JackLayer::write(const AudioFrame& buffer)
{
    auto num_samples = buffer.pointer()->nb_samples;
    auto num_bytes = num_samples * sizeof(float);
    auto channels = std::min<size_t>(out_ringbuffers_.size(), buffer.pointer()->channels);
    for (size_t i = 0; i < channels; ++i) {
        jack_ringbuffer_write(out_ringbuffers_[i], (const char*)buffer.pointer()->extended_data[i], num_bytes);
    }
}

std::unique_ptr<AudioFrame>
JackLayer::read()
{
    if (in_ringbuffers_.empty())
        return {};

    size_t toRead {std::numeric_limits<size_t>::max()};
    for (unsigned i = 0; i < in_ringbuffers_.size(); ++i) {
        toRead = std::min(toRead, jack_ringbuffer_read_space(in_ringbuffers_[i]));
    }
    if (not toRead)
        return {};

    auto format = audioInputFormat_;
    format.sampleFormat = AV_SAMPLE_FMT_FLTP;
    auto buffer = std::make_unique<AudioFrame>(format, toRead / sizeof(jack_default_audio_sample_t));

    for (unsigned i = 0; i < in_ringbuffers_.size(); ++i) {
        jack_ringbuffer_read(in_ringbuffers_[i], (char *) buffer->pointer()->extended_data[i], toRead);
    }
    return buffer;
}

/* This thread can lock, do whatever it wants, and read from/write to the jack
 * ring buffers
 * XXX: Access to shared state (i.e. member variables) should be synchronized if needed */
void
JackLayer::ringbuffer_worker()
{
    flush();

    while (true) {

        std::unique_lock<std::mutex> lock(ringbuffer_thread_mutex_);

        // may have changed, we don't want to wait for a notification we won't get
        if (status_ != Status::Started)
            return;

        // FIXME this is all kinds of evil
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        capture();
        playback();

        // wait until process() signals more data
        // FIXME: this checks for spurious wakes, but the predicate
        // is rather arbitrary. We should wait until ring has/needs data
        // and jack has/needs data.
        data_ready_.wait(lock, [&] {
            return status_ != Status::Started
            or ringbuffer_ready_for_read(in_ringbuffers_[0]);
        });
    }
}

void
createPorts(jack_client_t *client, std::vector<jack_port_t *> &ports,
            bool playback, std::vector<jack_ringbuffer_t *> &ringbuffers)
{
    const char **physical_ports = jack_get_ports(client, NULL, NULL,
            playback ? JackPortIsInput : JackPortIsOutput | JackPortIsPhysical);
    for (unsigned i = 0; physical_ports[i]; ++i) {
        if (i == 2)
            break;
        char port_name[32] = {0};
        if (playback)
            snprintf(port_name, sizeof(port_name), "out_%d", i + 1);
        else
            snprintf(port_name, sizeof(port_name), "in_%d", i + 1);
        port_name[sizeof(port_name) - 1] = '\0';
        jack_port_t *port = jack_port_register(client,
                port_name, JACK_DEFAULT_AUDIO_TYPE, playback ? JackPortIsOutput : JackPortIsInput, 0);
        if (port == nullptr)
            throw std::runtime_error("Could not register JACK output port");
        ports.push_back(port);

        static const unsigned RB_SIZE = 16384;
        jack_ringbuffer_t *rb = jack_ringbuffer_create(RB_SIZE);
        if (rb == nullptr)
            throw std::runtime_error("Could not create JACK ringbuffer");
        if (jack_ringbuffer_mlock(rb))
            throw std::runtime_error("Could not lock JACK ringbuffer in memory");
        ringbuffers.push_back(rb);
    }
    jack_free(physical_ports);
}


JackLayer::JackLayer(const AudioPreference &p) :
    AudioLayer(p),
    captureClient_(nullptr),
    playbackClient_(nullptr),
    mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
{
    playbackClient_ = jack_client_open(PACKAGE_NAME,
            (jack_options_t) (JackNullOption | JackNoStartServer), NULL);
    if (!playbackClient_)
        throw std::runtime_error("Could not open JACK client");

    captureClient_ = jack_client_open(PACKAGE_NAME,
            (jack_options_t) (JackNullOption | JackNoStartServer), NULL);
    if (!captureClient_)
        throw std::runtime_error("Could not open JACK client");

    jack_set_process_callback(captureClient_, process_capture, this);
    jack_set_process_callback(playbackClient_, process_playback, this);

    createPorts(playbackClient_, out_ports_, true, out_ringbuffers_);
    createPorts(captureClient_, in_ports_, false, in_ringbuffers_);

    const auto playRate = jack_get_sample_rate(playbackClient_);
    const auto captureRate = jack_get_sample_rate(captureClient_);

    audioInputFormat_ = {captureRate, (unsigned)in_ringbuffers_.size()};
    hardwareFormatAvailable(AudioFormat(playRate, out_ringbuffers_.size()));
    hardwareInputFormatAvailable(audioInputFormat_);
    jack_on_shutdown(playbackClient_, onShutdown, this);
}

JackLayer::~JackLayer()
{
    stopStream();

    for (auto p : out_ports_)
        jack_port_unregister(playbackClient_, p);
    for (auto p : in_ports_)
        jack_port_unregister(captureClient_, p);

    if (jack_client_close(playbackClient_))
        JAMI_ERR("JACK client could not close");
    if (jack_client_close(captureClient_))
        JAMI_ERR("JACK client could not close");

    for (auto r : out_ringbuffers_)
        jack_ringbuffer_free(r);
    for (auto r : in_ringbuffers_)
        jack_ringbuffer_free(r);
}

void
JackLayer::updatePreference(AudioPreference & /*pref*/, int /*index*/, DeviceType /*type*/)
{}

std::vector<std::string>
JackLayer::getCaptureDeviceList() const
{
    return std::vector<std::string>();
}

std::vector<std::string>
JackLayer::getPlaybackDeviceList() const
{
    return std::vector<std::string>();
}

int
JackLayer::getAudioDeviceIndex(const std::string& /*name*/, DeviceType /*type*/) const { return 0; }

std::string
JackLayer::getAudioDeviceName(int /*index*/, DeviceType /*type*/) const { return ""; }

int
JackLayer::getIndexCapture() const { return 0; }

int
JackLayer::getIndexPlayback() const { return 0; }

int
JackLayer::getIndexRingtone() const { return 0; }

int
JackLayer::process_capture(jack_nframes_t frames, void *arg)
{
    JackLayer *context = static_cast<JackLayer*>(arg);

    for (unsigned i = 0; i < context->in_ringbuffers_.size(); ++i) {

        // read from input
        jack_default_audio_sample_t *in_buffers = static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(context->in_ports_[i], frames));

        const size_t bytes_to_read = frames * sizeof(*in_buffers);
        size_t bytes_to_rb = jack_ringbuffer_write(context->in_ringbuffers_[i], (char *) in_buffers, bytes_to_read);

        // fill the rest with silence
        if (bytes_to_rb < bytes_to_read) {
            // TODO: set some flag for underrun?
            JAMI_WARN("Dropped %lu bytes", bytes_to_read - bytes_to_rb);
        }
    }

    /* Tell the ringbuffer thread there is work to do.  If it is already
     * running, the lock will not be available.  We can't wait
     * here in the process() thread, but we don't need to signal
     * in that case, because the ringbuffer thread will read all the
     * data queued before waiting again. */
    if (context->ringbuffer_thread_mutex_.try_lock()) {
        context->data_ready_.notify_one();
        context->ringbuffer_thread_mutex_.unlock();
    }

    return 0;
}

int
JackLayer::process_playback(jack_nframes_t frames, void *arg)
{
    JackLayer *context = static_cast<JackLayer*>(arg);

    for (unsigned i = 0; i < context->out_ringbuffers_.size(); ++i) {
        // write to output
        jack_default_audio_sample_t *out_buffers = static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(context->out_ports_[i], frames));

        const size_t bytes_to_write = frames * sizeof(*out_buffers);
        size_t bytes_from_rb = jack_ringbuffer_read(context->out_ringbuffers_[i], (char *) out_buffers, bytes_to_write);

        // fill the rest with silence
        if (bytes_from_rb < bytes_to_write) {
            const size_t frames_read = bytes_from_rb / sizeof(*out_buffers);
            memset(out_buffers + frames_read, 0, bytes_to_write - bytes_from_rb);
        }
    }

    return 0;
}

/**
 * Start the capture and playback.
 */
void
JackLayer::startStream()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Idle)
            return;
        status_ = Status::Started;
    }

    dcblocker_.reset();
    if (jack_activate(playbackClient_) or jack_activate(captureClient_)) {
        JAMI_ERR("Could not activate JACK client");
        return;
    }
    ringbuffer_thread_ = std::thread(&JackLayer::ringbuffer_worker, this);
    connectPorts(playbackClient_, JackPortIsInput, out_ports_);
    connectPorts(captureClient_, JackPortIsOutput, in_ports_);
}

void
JackLayer::onShutdown(void * /* data */)
{
    JAMI_WARN("JACK server shutdown");
    // FIXME: handle this safely
}

/**
 * Stop playback and capture.
 */
void
JackLayer::stopStream()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != Status::Started)
            return;
        status_ = Status::Idle;
    }
    data_ready_.notify_one();

    if (jack_deactivate(playbackClient_) or jack_deactivate(captureClient_)) {
        JAMI_ERR("JACK client could not deactivate");
    }

    if (ringbuffer_thread_.joinable())
        ringbuffer_thread_.join();

    flush();
}

} // namespace jami
