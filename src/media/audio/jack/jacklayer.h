/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *          Adrien Beraud <adrien.beraud@savoirfairelinux.com>
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

#pragma once

#include "noncopyable.h"
#include "audio/audiolayer.h"

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <memory>

namespace jami {

class RingBuffer;

class JackLayer : public AudioLayer
{
private:
    NON_COPYABLE(JackLayer);
    jack_client_t* captureClient_;
    jack_client_t* playbackClient_;
    std::vector<jack_port_t*> out_ports_;
    std::vector<jack_port_t*> in_ports_;
    std::vector<jack_ringbuffer_t*> out_ringbuffers_;
    std::vector<jack_ringbuffer_t*> in_ringbuffers_;
    std::thread ringbuffer_thread_;
    std::mutex ringbuffer_thread_mutex_;
    std::condition_variable data_ready_;

    static int process_capture(jack_nframes_t frames, void* arg);
    static int process_playback(jack_nframes_t frames, void* arg);

    void ringbuffer_worker();
    void playback();
    void capture();

    std::unique_ptr<AudioFrame> read();
    void write(const AudioFrame& buffer);
    size_t writeSpace();

    std::vector<std::string> getCaptureDeviceList() const;
    std::vector<std::string> getPlaybackDeviceList() const;

    int getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const;
    std::string getAudioDeviceName(int index, AudioDeviceType type) const;
    int getIndexCapture() const;
    int getIndexPlayback() const;
    int getIndexRingtone() const;
    void updatePreference(AudioPreference& pref, int index, AudioDeviceType type);

    /**
     * Start the capture and playback.
     */
    void startStream(AudioDeviceType stream = AudioDeviceType::ALL);

    /**
     * Stop playback and capture.
     */
    void stopStream(AudioDeviceType stream = AudioDeviceType::ALL);

    static void onShutdown(void* data);

public:
    JackLayer(const AudioPreference&);
    ~JackLayer();
};

} // namespace jami
