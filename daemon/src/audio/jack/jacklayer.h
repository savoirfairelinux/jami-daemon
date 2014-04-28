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

#ifndef JACK_LAYER_H_
#define JACK_LAYER_H_

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "noncopyable.h"
#include "audio/audiolayer.h"

class JackLayer : public AudioLayer {

    private:
        NON_COPYABLE(JackLayer);
        jack_client_t *captureClient_;
        jack_client_t *playbackClient_;
        std::vector<jack_port_t *> out_ports_;
        std::vector<jack_port_t *> in_ports_;
        std::vector<jack_ringbuffer_t *> out_ringbuffers_;
        std::vector<jack_ringbuffer_t *> in_ringbuffers_;
        std::thread ringbuffer_thread_;
        bool workerAlive_;
        std::mutex ringbuffer_thread_mutex_;
        std::condition_variable data_ready_;
        AudioBuffer playbackBuffer_;
        std::vector<float> playbackFloatBuffer_;
        AudioBuffer captureBuffer_;
        std::vector<float> captureFloatBuffer_;
        size_t hardwareBufferSize_;

        static int process_capture(jack_nframes_t frames, void *arg);
        static int process_playback(jack_nframes_t frames, void *arg);

        // separate thread
        void ringbuffer_worker();
        // called from ringbuffer_worker()
        void playback();
        void capture();
        void fillWithUrgent(AudioBuffer &buffer, size_t samplesToGet);
        void fillWithVoice(AudioBuffer &buffer, size_t samplesAvail);
        void fillWithToneOrRingtone(AudioBuffer &buffer);

        void read(AudioBuffer &buffer);
        void write(AudioBuffer &buffer, std::vector<float> &floatBuffer);


        std::vector<std::string> getCaptureDeviceList() const;
        std::vector<std::string> getPlaybackDeviceList() const;

        int getAudioDeviceIndex(const std::string& name, DeviceType type) const;
        std::string getAudioDeviceName(int index, DeviceType type) const;
        int getIndexCapture() const;
        int getIndexPlayback() const;
        int getIndexRingtone() const;
        void updatePreference(AudioPreference &pref, int index, DeviceType type);

        /**
         * Start the capture and playback.
         */
        void startStream();

        /**
         * Stop playback and capture.
         */
        void stopStream();

        static void onShutdown(void *data);

    public:

        JackLayer(const AudioPreference &);
        ~JackLayer();
};

#endif // JACK_LAYER_H_
