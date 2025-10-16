/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#include "audio/pipewire/pipeloopbackcapture.h"

#include <list>
#include <string>
#include <memory>
#include <thread>
#include <sstream>

#include <spa/utils/hook.h>
#include <pipewire/core.h>
extern "C" {
struct pw_context;
struct pw_core;
struct pw_thread_loop;
struct pw_buffer;
struct pw_registry;
struct pw_client;
struct pw_registry_events;
struct spa_dict;
}

namespace jami {

class PipeWireStream;
struct PwDeviceInfo;

class PipeWireLayer : public AudioLayer 
{
public:
    PipeWireLayer(AudioPreference& pref);
    ~PipeWireLayer();

    // Implement AudioLayer virtual methods
    std::vector<std::string> getCaptureDeviceList() const override;
    std::vector<std::string> getPlaybackDeviceList() const override;
    void startStream(AudioDeviceType stream = AudioDeviceType::ALL) override;
    void startCaptureStream(const std::string& id) override;
    void stopCaptureStream(const std::string& id) override;
    void stopStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    // PipeWire specific methods
    void updateDeviceList();
    /*void readFromMic();
    void writeToSpeaker();
    void ringtoneToSpeaker();*/
    void updatePreference(AudioPreference& preference, int index, AudioDeviceType type) override;
    std::string getAudioDeviceName(int index, AudioDeviceType type) const override;

private:
    // PipeWire context and core
    pw_context* context_;
    pw_core* core_;
    pw_thread_loop* loop_;
    pw_registry* registry_;
    pw_client *client;
    spa_hook registry_listener;
    pw_registry_events registry_events;
    spa_hook client_listener;
    std::atomic_uint pendingStreams {0};
    spa_hook metadata_listener;

    // Streams
    std::unique_ptr<PipeWireStream> playback_;
    std::unique_ptr<PipeWireStream> record_;
    std::unique_ptr<PipeWireStream> ringtone_;

    // Device lists
    std::vector<PwDeviceInfo> sinkList_;
    std::vector<PwDeviceInfo> sourceList_;

    LoopbackCapture loopbackCapture_;

    // Helper methods
    void createStream(std::unique_ptr<PipeWireStream>& stream, AudioDeviceType type, const PwDeviceInfo& dev_info, std::function<void(pw_buffer* buf)>&& onData);
    void disconnectAudioStream();
    void onStreamReady();

    // Callback methods
    static void registryEventCallback(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props);
    static void registryEventCallbackRemove(void* data, uint32_t id);

    void handleSinkEvent(uint32_t id, const struct spa_dict* props);
    void handleSourceEvent(uint32_t id, const struct spa_dict* props);
    void queryDeviceProperties(PwDeviceInfo& deviceInfo, const struct spa_dict* props);
    /**
     * Write data from the ring buffer to the hardware and read data
     * from the hardware.
     */
    void readFromMic(pw_buffer* buf);
    void writeToSpeaker(pw_buffer* buf);
    void ringtoneToSpeaker(pw_buffer* buf);

    int getAudioDeviceIndex(const std::string& descr, AudioDeviceType type) const override;
    int getAudioDeviceIndexByName(const std::string& name, AudioDeviceType type) const;

    virtual int getIndexCapture() const override;
    virtual int getIndexPlayback() const override;
    virtual int getIndexRingtone() const override;

    //AudioPreference& preference_;
};

} // namespace jami
