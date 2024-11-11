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

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <vector>
#include <thread>

#include "audio/audiolayer.h"
#include "audio_player.h"
#include "audio_recorder.h"

class AudioPreference;

#include "noncopyable.h"

#include <memory>

namespace jami {

class RingBuffer;

#define ANDROID_BUFFER_QUEUE_LENGTH 2U
#define BUFFER_SIZE                 512U

#define MAX_NUMBER_INTERFACES    5
#define MAX_NUMBER_INPUT_DEVICES 3

/**
 * @file  OpenSLLayer.h
 * @brief Main sound class for android. Manages the data transfers between the application and the
 * hardware.
 */

class OpenSLLayer : public AudioLayer
{
public:
    /**
     * Constructor
     */
    OpenSLLayer(const AudioPreference& pref);

    /**
     * Destructor
     */
    ~OpenSLLayer();

    /**
     * Start the capture stream and prepare the playback stream.
     * The playback starts accordingly to its threshold
     */
    void startStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    /**
     * Stop the playback and capture streams.
     * Drops the pending frames and put the capture and playback handles to PREPARED state
     */
    void stopStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    /**
     * Scan the sound card available for capture on the system
     * @return std::vector<std::string> The vector containing the string description of the card
     */
    std::vector<std::string> getCaptureDeviceList() const override;

    /**
     * Scan the sound card available for capture on the system
     * @return std::vector<std::string> The vector containing the string description of the card
     */
    std::vector<std::string> getPlaybackDeviceList() const override;

    void init();

    void initAudioEngine();
    void shutdownAudioEngine();

    void startAudioCapture();
    void stopAudioCapture();

    virtual int getAudioDeviceIndex(const std::string&, AudioDeviceType) const override { return 0; }

    virtual std::string getAudioDeviceName(int, AudioDeviceType) const override { return ""; }

    void engineServicePlay();
    void engineServiceRing();
    void engineServiceRec();

private:
    /**
     * Get the index of the audio card for capture
     * @return int The index of the card used for capture
     *                     0 for the first available card on the system, 1 ...
     */
    virtual int getIndexCapture() const override { return 0; }

    /**
     * Get the index of the audio card for playback
     * @return int The index of the card used for playback
     *                     0 for the first available card on the system, 1 ...
     */
    virtual int getIndexPlayback() const override { return 0; }

    /**
     * Get the index of the audio card for ringtone (could be differnet from playback)
     * @return int The index of the card used for ringtone
     *                 0 for the first available card on the system, 1 ...
     */
    virtual int getIndexRingtone() const override { return 0; }

    uint32_t dbgEngineGetBufCount();

    void dumpAvailableEngineInterfaces();

    NON_COPYABLE(OpenSLLayer);

    virtual void updatePreference(AudioPreference& pref, int index, AudioDeviceType type) override;

    std::mutex recMtx {};
    std::condition_variable recCv {};

    /**
     * OpenSL standard object interface
     */
    SLObjectItf engineObject_ {nullptr};

    /**
     * OpenSL sound engine interface
     */
    SLEngineItf engineInterface_ {nullptr};

    AudioFormat hardwareFormat_ {AudioFormat::MONO()};
    size_t hardwareBuffSize_ {BUFFER_SIZE};

    std::vector<sample_buf> bufs_ {};

    AudioQueue freePlayBufQueue_ {BUF_COUNT};
    AudioQueue playBufQueue_ {BUF_COUNT};

    AudioQueue freeRingBufQueue_ {BUF_COUNT};
    AudioQueue ringBufQueue_ {BUF_COUNT};

    AudioQueue freeRecBufQueue_ {BUF_COUNT}; // Owner of the queue
    AudioQueue recBufQueue_ {BUF_COUNT};     // Owner of the queue

    std::unique_ptr<opensl::AudioPlayer> player_ {};
    std::unique_ptr<opensl::AudioPlayer> ringtone_ {};
    std::unique_ptr<opensl::AudioRecorder> recorder_ {};

    std::thread recThread {};
};

} // namespace jami
