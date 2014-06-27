/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "jacklayer.h"
#include <cassert>
#include <climits>
#include "logger.h"
#include "audio/mainbuffer.h"
#include "manager.h"
#include "array_size.h"

#include <unistd.h>

#if 0
TODO
* implement shutdown callback
* auto connect optional
#endif

namespace
{
void connectPorts(jack_client_t *client, int portType, const std::vector<jack_port_t *> &ports)
{
    const char **physical_ports = jack_get_ports(client, NULL, NULL, portType | JackPortIsPhysical);
    for (unsigned i = 0; physical_ports[i]; ++i) {
        const char *port = jack_port_name(ports[i]);
        if (portType & JackPortIsInput) {
            if (jack_connect(client, port, physical_ports[i])) {
                ERROR("Can't connect %s to %s", port, physical_ports[i]);
                break;
            }
        } else {
            if (jack_connect(client, physical_ports[i], port)) {
                ERROR("Can't connect port %s to %s", physical_ports[i], port);
                break;
            }
        }
    }
    free(physical_ports);
}

bool ringbuffer_ready_for_read(const jack_ringbuffer_t *rb)
{
    // XXX 512 is arbitrary
    return jack_ringbuffer_read_space(rb) > 512;
}
}

void JackLayer::fillWithUrgent(AudioBuffer &buffer, size_t samplesToGet)
{
    // Urgent data (dtmf, incoming call signal) come first.
    samplesToGet = std::min(samplesToGet, hardwareBufferSize_);
    buffer.resize(samplesToGet);
    urgentRingBuffer_.get(buffer, MainBuffer::DEFAULT_ID);
    buffer.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

    // Consume the regular one as well (same amount of samples)
    Manager::instance().getMainBuffer().discard(samplesToGet, MainBuffer::DEFAULT_ID);
}

void JackLayer::fillWithVoice(AudioBuffer &buffer, size_t samplesAvail)
{
    MainBuffer &mainBuffer = Manager::instance().getMainBuffer();

    buffer.resize(samplesAvail);
    mainBuffer.getData(buffer, MainBuffer::DEFAULT_ID);
    buffer.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);

    if (audioFormat_.sample_rate != (unsigned) mainBuffer.getInternalSamplingRate()) {
        DEBUG("fillWithVoice sample_rate != mainBuffer.getInternalSamplingRate() \n");
        AudioBuffer out(buffer, false);
        out.setSampleRate(audioFormat_.sample_rate);
        resampler_.resample(buffer, out);
        buffer = out;
    }
}

void JackLayer::fillWithToneOrRingtone(AudioBuffer &buffer)
{
    buffer.resize(hardwareBufferSize_);
    AudioLoop *tone = Manager::instance().getTelephoneTone();
    AudioLoop *file_tone = Manager::instance().getTelephoneFile();

    // In case of a dtmf, the pointers will be set to nullptr once the dtmf length is
    // reached. For this reason we need to fill audio buffer with zeros if pointer is nullptr
    if (tone) {
        tone->getNext(buffer, playbackGain_);
    } else if (file_tone) {
        file_tone->getNext(buffer, playbackGain_);
    } else {
        buffer.reset();
    }
}

void
JackLayer::playback()
{
    notifyIncomingCall();

    const size_t samplesToGet = Manager::instance().getMainBuffer().availableForGet(MainBuffer::DEFAULT_ID);
    const size_t urgentSamplesToGet = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);

    if (urgentSamplesToGet > 0) {
        fillWithUrgent(playbackBuffer_, urgentSamplesToGet);
    } else {
		if (samplesToGet > 0) {
            fillWithVoice(playbackBuffer_, samplesToGet);
        } else {
            fillWithToneOrRingtone(playbackBuffer_);
        }
	}

    playbackFloatBuffer_.resize(playbackBuffer_.frames());
    write(playbackBuffer_, playbackFloatBuffer_);
}

void
JackLayer::capture()
{
    // get audio from jack ringbuffer
    read(captureBuffer_);

    MainBuffer &mbuffer = Manager::instance().getMainBuffer();
    const AudioFormat mainBufferFormat = mbuffer.getInternalAudioFormat();
    const bool resample = mainBufferFormat.sample_rate != audioFormat_.sample_rate;

    captureBuffer_.applyGain(isCaptureMuted_ ? 0.0 : captureGain_);

    if (resample) {
		int outSamples = captureBuffer_.frames() * (static_cast<double>(audioFormat_.sample_rate) / mainBufferFormat.sample_rate);
        AudioBuffer out(outSamples, mainBufferFormat);
        resampler_.resample(captureBuffer_, out);
        dcblocker_.process(out);
        mbuffer.putData(out, MainBuffer::DEFAULT_ID);
    } else {
        dcblocker_.process(captureBuffer_);
        mbuffer.putData(captureBuffer_, MainBuffer::DEFAULT_ID);
    }
}

static void
convertToFloat(const std::vector<SFLAudioSample> &src, std::vector<float> &dest)
{
    static const float INV_SHORT_MAX = 1 / (float) SHRT_MAX;
    if (dest.size() != src.size()) {
        ERROR("MISMATCH");
        return;
    }
    for (size_t i = 0; i < dest.size(); ++i)
        dest[i] = src[i] * INV_SHORT_MAX;
}

static void
convertFromFloat(std::vector<float> &src, std::vector<SFLAudioSample> &dest)
{
    if (dest.size() != src.size()) {
        ERROR("MISMATCH");
        return;
    }
    for (size_t i = 0; i < dest.size(); ++i)
        dest[i] = src[i] * SHRT_MAX;
}

void
JackLayer::write(AudioBuffer &buffer, std::vector<float> &floatBuffer)
{
    for (unsigned i = 0; i < out_ringbuffers_.size(); ++i) {
        const unsigned inChannel = std::min(i, buffer.channels() - 1);
        convertToFloat(*buffer.getChannel(inChannel), floatBuffer);

        // write to output
        const size_t to_ringbuffer = jack_ringbuffer_write_space(out_ringbuffers_[i]);
        const size_t write_bytes = std::min(buffer.frames() * sizeof(floatBuffer[0]), to_ringbuffer);
        // FIXME: while we have samples to write AND while we have space to write them
        const size_t written_bytes = jack_ringbuffer_write(out_ringbuffers_[i],
                (const char *) floatBuffer.data(), write_bytes);
        if (written_bytes < write_bytes)
            WARN("Dropped %zu bytes for channel %u", write_bytes - written_bytes, i);
    }
}

void
JackLayer::read(AudioBuffer &buffer)
{
    for (unsigned i = 0; i < in_ringbuffers_.size(); ++i) {

        const size_t incomingSamples = jack_ringbuffer_read_space(in_ringbuffers_[i]) / sizeof(captureFloatBuffer_[0]);
        if (!incomingSamples)
            continue;

        captureFloatBuffer_.resize(incomingSamples);
        buffer.resize(incomingSamples);

        // write to output
        const size_t from_ringbuffer = jack_ringbuffer_read_space(in_ringbuffers_[i]);
        const size_t expected_bytes = std::min(incomingSamples * sizeof(captureFloatBuffer_[0]), from_ringbuffer);
        // FIXME: while we have samples to write AND while we have space to write them
        const size_t read_bytes = jack_ringbuffer_read(in_ringbuffers_[i],
                (char *) captureFloatBuffer_.data(), expected_bytes);
        if (read_bytes < expected_bytes) {
            WARN("Dropped %zu bytes", expected_bytes - read_bytes);
            break;
        }

        /* Write the data one frame at a time.  This is
         * inefficient, but makes things simpler. */
        // FIXME: this is braindead, we should write blocks of samples at a time
        // convert a vector of samples from 1 channel to a float vector
        convertFromFloat(captureFloatBuffer_, *buffer.getChannel(i));
    }
}

/* This thread can lock, do whatever it wants, and read from/write to the jack
 * ring buffers
 * XXX: Access to shared state (i.e. member variables) should be synchronized if needed */
void
JackLayer::ringbuffer_worker()
{
    flushMain();
    flushUrgent();

	while (true) {

        std::unique_lock<std::mutex> lock(ringbuffer_thread_mutex_);

        // may have changed, we don't want to wait for a notification we won't get
        if (not workerAlive_)
            return;

        // FIXME this is all kinds of evil
        usleep(20000);

        capture();
        playback();

        // wait until process() signals more data
        // FIXME: this checks for spurious wakes, but the predicate
        // is rather arbitrary. We should wait until sflphone has/needs data
        // and jack has/needs data.
        data_ready_.wait(lock, [&] {
            // Note: lock is released while waiting, and held when woken
            // up, so this predicate is called while holding the lock
            return not workerAlive_
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
    free(physical_ports);
}


JackLayer::JackLayer(const AudioPreference &p) :
    AudioLayer(p),
    captureClient_(nullptr),
    playbackClient_(nullptr),
    out_ports_(),
    in_ports_(),
    out_ringbuffers_(),
    in_ringbuffers_(),
    ringbuffer_thread_(),
    workerAlive_(false),
    ringbuffer_thread_mutex_(),
    data_ready_(),
    playbackBuffer_(0, audioFormat_),
    playbackFloatBuffer_(),
    captureBuffer_(0, audioFormat_),
    captureFloatBuffer_(),
    hardwareBufferSize_(0)
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
    if (playRate != captureRate)
        ERROR("Mismatch between capture rate %u and playback rate %u", playRate, captureRate);

    hardwareBufferSize_ = jack_get_buffer_size(playbackClient_);

    auto update_buffer = [] (AudioBuffer &buf, size_t size, unsigned rate, unsigned nbChannels) {
        buf.setSampleRate(rate);
        buf.resize(size);
        buf.setChannelNum(nbChannels);
    };

    update_buffer(playbackBuffer_, hardwareBufferSize_, playRate, out_ports_.size());
    update_buffer(captureBuffer_, hardwareBufferSize_, captureRate, in_ports_.size());

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
        ERROR("JACK client could not close");
    if (jack_client_close(captureClient_))
        ERROR("JACK client could not close");

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
            WARN("Dropped %lu bytes", bytes_to_read - bytes_to_rb);
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
    if (isStarted_)
        return;

    dcblocker_.reset();
    const auto hardwareFormat = AudioFormat(playbackBuffer_.getSampleRate(), out_ports_.size());
    hardwareFormatAvailable(hardwareFormat);

    workerAlive_ = true;
    assert(not ringbuffer_thread_.joinable());
    ringbuffer_thread_ = std::thread(&JackLayer::ringbuffer_worker, this);

    if (jack_activate(playbackClient_) or jack_activate(captureClient_)) {
        ERROR("Could not activate JACK client");
        workerAlive_ = false;
        ringbuffer_thread_.join();
        isStarted_ = false;
        return;
    } else {
        isStarted_ = true;
    }

    connectPorts(playbackClient_, JackPortIsInput, out_ports_);
    connectPorts(captureClient_, JackPortIsOutput, in_ports_);
}

void
JackLayer::onShutdown(void * /* data */)
{
    WARN("JACK server shutdown");
    // FIXME: handle this safely
}

/**
 * Stop playback and capture.
 */
void
JackLayer::stopStream()
{
    {
        std::lock_guard<std::mutex> lock(ringbuffer_thread_mutex_);
        workerAlive_ = false;
        data_ready_.notify_one();
    }

    if (jack_deactivate(playbackClient_) or jack_deactivate(captureClient_)) {
        ERROR("JACK client could not deactivate");
    }

    isStarted_ = false;

    if (ringbuffer_thread_.joinable())
        ringbuffer_thread_.join();

    flushMain();
    flushUrgent();
}
