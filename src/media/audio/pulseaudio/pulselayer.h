/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "noncopyable.h"
#include "logger.h"
#include "audio/audiolayer.h"

#include <pulse/pulseaudio.h>
#include <pulse/stream.h>

#include <list>
#include <string>
#include <memory>
#include <thread>

namespace jami {

class AudioPreference;
class AudioStream;
class RingBuffer;

/**
 * Convenience structure to hold PulseAudio device propreties such as supported channel number etc.
 */
struct PaDeviceInfos
{
    uint32_t index {0};
    std::string name {};
    std::string description {"default"};
    pa_sample_spec sample_spec {};
    pa_channel_map channel_map {};
    uint32_t monitor_of {PA_INVALID_INDEX};

    PaDeviceInfos() {}

    PaDeviceInfos(const pa_source_info& source)
        : index(source.index)
        , name(source.name)
        , description(source.description)
        , sample_spec(source.sample_spec)
        , channel_map(source.channel_map)
        , monitor_of(source.monitor_of_sink)
    {}

    PaDeviceInfos(const pa_sink_info& source)
        : index(source.index)
        , name(source.name)
        , description(source.description)
        , sample_spec(source.sample_spec)
        , channel_map(source.channel_map)
    {}

    /**
     * Unary function to search for a device by name in a list using std functions.
     */
    class NameComparator
    {
    public:
        explicit NameComparator(const std::string& ref)
            : baseline(ref)
        {}
        bool operator()(const PaDeviceInfos& arg) { return arg.name == baseline; }

    private:
        const std::string& baseline;
    };

    class DescriptionComparator
    {
    public:
        explicit DescriptionComparator(const std::string& ref)
            : baseline(ref)
        {}
        bool operator()(const PaDeviceInfos& arg) { return arg.description == baseline; }

    private:
        const std::string& baseline;
    };
};

class PulseMainLoopLock
{
public:
    explicit PulseMainLoopLock(pa_threaded_mainloop* loop);
    ~PulseMainLoopLock();

private:
    NON_COPYABLE(PulseMainLoopLock);
    pa_threaded_mainloop* loop_;
};

class PulseLayer : public AudioLayer
{
public:
    PulseLayer(AudioPreference& pref);
    ~PulseLayer();

    /**
     * Write data from the ring buffer to the hardware and read data
     * from the hardware.
     */
    void readFromMic();
    void writeToSpeaker();
    void ringtoneToSpeaker();

    void updateSinkList();
    void updateSourceList();
    void updateServerInfo();

    bool inSinkList(const std::string& deviceName);
    bool inSourceList(const std::string& deviceName);

    virtual std::vector<std::string> getCaptureDeviceList() const;
    virtual std::vector<std::string> getPlaybackDeviceList() const;
    int getAudioDeviceIndex(const std::string& descr, AudioDeviceType type) const;
    int getAudioDeviceIndexByName(const std::string& name, AudioDeviceType type) const;

    std::string getAudioDeviceName(int index, AudioDeviceType type) const;

    virtual void startStream(AudioDeviceType stream = AudioDeviceType::ALL);
    virtual void stopStream(AudioDeviceType stream = AudioDeviceType::ALL);

private:
    static void context_state_callback(pa_context* c, void* user_data);
    static void context_changed_callback(pa_context* c,
                                         pa_subscription_event_type_t t,
                                         uint32_t idx,
                                         void* userdata);
    void contextStateChanged(pa_context* c);
    void contextChanged(pa_context*, pa_subscription_event_type_t, uint32_t idx);

    static void source_input_info_callback(pa_context* c,
                                           const pa_source_info* i,
                                           int eol,
                                           void* userdata);
    static void sink_input_info_callback(pa_context* c,
                                         const pa_sink_info* i,
                                         int eol,
                                         void* userdata);
    static void server_info_callback(pa_context*, const pa_server_info* i, void* userdata);

    virtual void updatePreference(AudioPreference& pref, int index, AudioDeviceType type);

    virtual int getIndexCapture() const;
    virtual int getIndexPlayback() const;
    virtual int getIndexRingtone() const;

    void waitForDevices();
    void waitForDeviceList();

    std::string getPreferredPlaybackDevice() const;
    std::string getPreferredRingtoneDevice() const;
    std::string getPreferredCaptureDevice() const;

    NON_COPYABLE(PulseLayer);

    /**
     * Create the audio stream
     */
    void createStream(std::unique_ptr<AudioStream>& stream, AudioDeviceType type, const PaDeviceInfos& dev_infos, bool ec, std::function<void(size_t)>&& onData);

    std::unique_ptr<AudioStream>& getStream(AudioDeviceType type)
    {
        if (type == AudioDeviceType::PLAYBACK)
            return playback_;
        else if (type == AudioDeviceType::RINGTONE)
            return ringtone_;
        else if (type == AudioDeviceType::CAPTURE)
            return record_;
        else
            return playback_;
    }

    /**
     * Close the connection with the local pulseaudio server
     */
    void disconnectAudioStream();
    void onStreamReady();

    /**
     * Returns a pointer to the PaEndpointInfos with the given name in sourceList_, or nullptr if
     * not found.
     */
    const PaDeviceInfos* getDeviceInfos(const std::vector<PaDeviceInfos>&,
                                        const std::string& name) const;

    std::atomic_uint pendingStreams {0};

    /**
     * A stream object to handle the pulseaudio playback stream
     */
    std::unique_ptr<AudioStream> playback_;

    /**
     * A stream object to handle the pulseaudio capture stream
     */
    std::unique_ptr<AudioStream> record_;

    /**
     * A special stream object to handle specific playback stream for ringtone
     */
    std::unique_ptr<AudioStream> ringtone_;

    /**
     * Contains the list of playback devices
     */
    std::vector<PaDeviceInfos> sinkList_ {};

    /**
     * Contains the list of capture devices
     */
    std::vector<PaDeviceInfos> sourceList_ {};

    /** PulseAudio server defaults */
    AudioFormat defaultAudioFormat_ {AudioFormat::MONO()};
    std::string defaultSink_ {};
    std::string defaultSource_ {};

    /** PulseAudio context and asynchronous loop */
    pa_context* context_ {nullptr};
    std::unique_ptr<pa_threaded_mainloop, decltype(pa_threaded_mainloop_free)&> mainloop_;
    bool enumeratingSinks_ {false};
    bool enumeratingSources_ {false};
    bool gettingServerInfo_ {false};
    std::atomic_bool waitingDeviceList_ {false};
    std::mutex readyMtx_ {};
    std::condition_variable readyCv_ {};
    std::thread streamStarter_ {};

    AudioPreference& preference_;

    pa_operation* subscribeOp_ {nullptr};
    friend class AudioLayerTest;
};

} // namespace jami
