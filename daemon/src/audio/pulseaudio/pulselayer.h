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

#ifndef PULSE_LAYER_H_
#define PULSE_LAYER_H_

#include <list>
#include <string>
#include <pulse/pulseaudio.h>
#include <pulse/stream.h>
#include "audio/audiolayer.h"
#include "noncopyable.h"
#include "logger.h"

class AudioPreference;
class AudioStream;

/**
 * Convenience structure to hold PulseAudio device propreties such as supported channel number etc.
 */
typedef struct PaDeviceInfos {
        uint32_t index;
        std::string name;
        std::string description;
        pa_sample_spec sample_spec;
        pa_channel_map channel_map;

        PaDeviceInfos(const pa_source_info& source) :
            index(source.index),
            name(source.name),
            description(source.description),
            sample_spec(source.sample_spec),
            channel_map(source.channel_map) {}

        PaDeviceInfos(const pa_sink_info& source) :
            index(source.index),
            name(source.name),
            description(source.description),
            sample_spec(source.sample_spec),
            channel_map(source.channel_map) {}

        /**
         * Unary function to search for a device by name in a list using std functions.
         */
        class NameComparator {
            public:
                explicit NameComparator(const std::string &ref) : baseline(ref) {}
                bool operator()(const PaDeviceInfos &arg) {
                    return arg.name == baseline;
                }
            private:
                const std::string &baseline;
        };

        class DescriptionComparator {
            public:
                explicit DescriptionComparator(const std::string &ref) : baseline(ref) {}
                bool operator()(const PaDeviceInfos &arg) {
                    return arg.description == baseline;
                }
            private:
                const std::string &baseline;
        };
} PaDeviceInfos;

class PulseMainLoopLock {
    public:
        explicit PulseMainLoopLock(pa_threaded_mainloop *loop);
        void destroyLoop();
        ~PulseMainLoopLock();

    private:
        NON_COPYABLE(PulseMainLoopLock);
        pa_threaded_mainloop *loop_;
        bool destroyLoop_;
};

class PulseLayer : public AudioLayer {
    public:
        PulseLayer(AudioPreference &pref);
        ~PulseLayer();

        /**
         * Write data from the ring buffer to the harware and read data from the hardware
         */
        void readFromMic();
        void writeToSpeaker();
        void ringtoneToSpeaker();

        void updateSinkList();
        void updateSourceList();

        bool inSinkList(const std::string &deviceName);
        bool inSourceList(const std::string &deviceName);

        virtual std::vector<std::string> getCaptureDeviceList() const;
        virtual std::vector<std::string> getPlaybackDeviceList() const;
        int getAudioDeviceIndex(const std::string& descr, DeviceType type) const;
        int getAudioDeviceIndexByName(const std::string& name, DeviceType type) const;

        std::string getAudioDeviceName(int index, DeviceType type) const;

        virtual void startStream();
        virtual void stopStream();

    private:
        static void context_state_callback(pa_context* c, void* user_data);
        static void context_changed_callback(pa_context* c,
                                             pa_subscription_event_type_t t,
                                             uint32_t idx , void* userdata);
        static void source_input_info_callback(pa_context *c,
                                               const pa_source_info *i,
                                               int eol, void *userdata);
        static void sink_input_info_callback(pa_context *c,
                                             const pa_sink_info *i,
                                             int eol, void *userdata);

        virtual void updatePreference(AudioPreference &pref, int index, DeviceType type);

        virtual int getIndexCapture() const;
        virtual int getIndexPlayback() const;
        virtual int getIndexRingtone() const;

        NON_COPYABLE(PulseLayer);

        /**
         * Create the audio streams into the given context
         * @param c	The pulseaudio context
         */
        void createStreams(pa_context* c);

        /**
         * Close the connection with the local pulseaudio server
         */
        void disconnectAudioStream();

        /**
         * Returns a pointer to the PaEndpointInfos with the given name in sourceList_, or nullptr if not found.
         */
        const PaDeviceInfos* getDeviceInfos(const std::vector<PaDeviceInfos>&, const std::string& name) const;

        /**
         * A stream object to handle the pulseaudio playback stream
         */
        AudioStream* playback_;

        /**
         * A stream object to handle the pulseaudio capture stream
         */
        AudioStream* record_;

        /**
         * A special stream object to handle specific playback stream for ringtone
         */
        AudioStream* ringtone_;

        /**
         * Contains the list of playback devices
         */
        std::vector<PaDeviceInfos> sinkList_;

        /**
         * Contains the list of capture devices
         */
        std::vector<PaDeviceInfos> sourceList_;

        /*
         * Buffers used to avoid doing malloc/free in the audio thread
         */
        AudioBuffer micBuffer_;

        /** PulseAudio context and asynchronous loop */
        pa_context* context_;
        pa_threaded_mainloop* mainloop_;
        bool enumeratingSinks_;
        bool enumeratingSources_;
        AudioPreference &preference_;

        friend class AudioLayerTest;
};

#endif // PULSE_LAYER_H_

