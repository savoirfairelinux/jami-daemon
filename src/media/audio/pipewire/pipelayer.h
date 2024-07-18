#pragma once

#include "noncopyable.h"
#include "logger.h"
#include "audio/audiolayer.h"

#include <list>
#include <string>
#include <memory>
#include <thread>

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
    
    AudioPreference& preference_;
};