/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "audio/audiolayer.h"

#include <aaudio/AAudio.h>

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>

namespace jami {

class AAudioLayer : public AudioLayer {
public:
    AAudioLayer(const AudioPreference& pref);
    ~AAudioLayer();

    void startStream(AudioDeviceType stream) override;
    void stopStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    std::vector<std::string> getCaptureDeviceList() const override;
    std::vector<std::string> getPlaybackDeviceList() const override;
    
    int getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const override;
    std::string getAudioDeviceName(int index, AudioDeviceType type) const override;

    int getIndexCapture() const override { return 0; }
    int getIndexPlayback() const override { return 0; }
    int getIndexRingtone() const override { return 0; }

    void updatePreference(AudioPreference& pref, int index, AudioDeviceType type) override;

private:
    struct AAudioStreamDeleter {
        void operator()(AAudioStream* stream) const {
            if (stream) {
                AAudioStream_close(stream);
            }
        }
    };
    using AAudioStreamPtr = std::unique_ptr<AAudioStream, AAudioStreamDeleter>;

    AAudioStreamPtr buildStream(AudioDeviceType type);

    // AAudio specific members
    AAudioStreamPtr playStream_ {nullptr};
    AAudioStreamPtr ringStream_ {nullptr};
    AAudioStreamPtr recStream_ {nullptr};
    int previousUnderrunCount_ {0};

    static aaudio_data_callback_result_t dataCallback(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames);

    static void errorCallback(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error);

    void stopPlayback();
    void stopCapture();
    
    void loop();
    void startStreamLocked(AudioDeviceType stream);
    void stopStreamLocked(AudioDeviceType stream);

    std::thread loopThread_;
    std::condition_variable loopCv_;
    std::set<AudioDeviceType> streamsToRestart_;
    bool isRunning_ {false};
};

}
