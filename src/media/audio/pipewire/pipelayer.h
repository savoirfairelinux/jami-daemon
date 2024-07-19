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

#include <list>
#include <string>
#include <memory>
#include <thread>
#include <sstream>

extern "C" {
struct pw_context;
struct pw_core;
struct pw_thread_loop;
struct pw_buffer;
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
    void stopStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    // PipeWire specific methods
    void updateDeviceList();
    void readFromMic();
    void writeToSpeaker();
    void ringtoneToSpeaker();

private:
    // PipeWire context and core
    pw_context* context_;
    pw_core* core_;
    pw_thread_loop* loop_;

    // Streams
    std::unique_ptr<PipeWireStream> playback_;
    std::unique_ptr<PipeWireStream> record_;
    std::unique_ptr<PipeWireStream> ringtone_;

    // Device lists
    std::vector<PwDeviceInfo> sinkList_;
    std::vector<PwDeviceInfo> sourceList_;

    // Helper methods
    void createStream(std::unique_ptr<PipeWireStream>& stream, AudioDeviceType type, const PwDeviceInfo& dev_info);
    void disconnectAudioStream();

    // Callback methods
    static void registryEventCallback(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props);
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


    AudioPreference& preference_;
};

} // namespace jami
