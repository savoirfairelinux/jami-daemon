/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "ringbuffer.h"
#include "dcblocker.h"
#include "noncopyable.h"
#include "audio_frame_resizer.h"
#include "audio-processing/audio_processor.h"

#include <chrono>
#include <mutex>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <array>

extern "C" {
struct SpeexEchoState_;
typedef struct SpeexEchoState_ SpeexEchoState;
}

/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware.
 */

// Define the audio api
#define OPENSL_API_STR     "opensl"
#define PULSEAUDIO_API_STR "pulseaudio"
#define ALSA_API_STR       "alsa"
#define JACK_API_STR       "jack"
#define COREAUDIO_API_STR  "coreaudio"
#define PORTAUDIO_API_STR  "portaudio"

#define PCM_DEFAULT     "default"     // Default ALSA plugin
#define PCM_DSNOOP      "plug:dsnoop" // Alsa plugin for microphone sharing
#define PCM_DMIX_DSNOOP "dmix/dsnoop" // Audio profile using Alsa dmix/dsnoop

namespace jami {

class AudioPreference;
class Resampler;

enum class AudioDeviceType { ALL = -1, PLAYBACK = 0, CAPTURE, RINGTONE };

class AudioLayer
{
private:
    NON_COPYABLE(AudioLayer);

protected:
    enum class Status { Idle, Starting, Started };

public:
    AudioLayer(const AudioPreference&);
    virtual ~AudioLayer();

    /**
     * Start the capture stream and prepare the playback stream.
     * The playback starts accordingly to its threshold
     */
    virtual void startStream(AudioDeviceType stream = AudioDeviceType::ALL) = 0;

    /**
     * Stop the playback and capture streams.
     * Drops the pending frames and put the capture and playback handles to PREPARED state
     */
    virtual void stopStream(AudioDeviceType stream = AudioDeviceType::ALL) = 0;

    virtual std::vector<std::string> getCaptureDeviceList() const = 0;
    virtual std::vector<std::string> getPlaybackDeviceList() const = 0;

    virtual int getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const = 0;
    virtual std::string getAudioDeviceName(int index, AudioDeviceType type) const = 0;
    virtual int getIndexCapture() const = 0;
    virtual int getIndexPlayback() const = 0;
    virtual int getIndexRingtone() const = 0;

    /**
     * Determine whether or not the audio layer is active (i.e. playback opened)
     */
    inline bool isStarted() const { return status_ == Status::Started; }

    template<class Rep, class Period>
    bool waitForStart(const std::chrono::duration<Rep, Period>& rel_time) const
    {
        std::unique_lock<std::mutex> lk(mutex_);
        startedCv_.wait_for(lk, rel_time, [this] { return isStarted(); });
        return isStarted();
    }

    /**
     * Send a chunk of data to the hardware buffer to start the playback
     * Copy data in the urgent buffer.
     * @param buffer The buffer containing the data to be played ( ringtones )
     */
    void putUrgent(AudioBuffer& buffer);

    /**
     * Start/Stop playing the incoming call notification sound (beep)
     * while playing back audio (typically during an ongoing call).
     */
    void playIncomingCallNotification(bool play) { playIncomingCallBeep_.exchange(play); }

    /**
     * Flush main buffer
     */
    void flushMain();

    /**
     * Flush urgent buffer
     */
    void flushUrgent();

    bool isCaptureMuted() const { return isCaptureMuted_; }

    /**
     * Mute capture (microphone)
     */
    void muteCapture(bool muted) { isCaptureMuted_ = muted; }

    bool isPlaybackMuted() const { return isPlaybackMuted_; }

    /**
     * Mute playback
     */
    void mutePlayback(bool muted) { isPlaybackMuted_ = muted; }

    bool isRingtoneMuted() const { return isRingtoneMuted_; }
    void muteRingtone(bool muted) { isRingtoneMuted_ = muted; }

    /**
     * Set capture stream gain (microphone)
     * Range should be [-1.0, 1.0]
     */
    void setCaptureGain(double gain) { captureGain_ = gain; }

    /**
     * Get capture stream gain (microphone)
     */
    double getCaptureGain() const { return captureGain_; }

    /**
     * Set playback stream gain (speaker)
     * Range should be [-1.0, 1.0]
     */
    void setPlaybackGain(double gain) { playbackGain_ = gain; }

    /**
     * Get playback stream gain (speaker)
     */
    double getPlaybackGain() const { return playbackGain_; }

    /**
     * Get the sample rate of the audio layer
     * @return unsigned int The sample rate
     *			    default: 44100 HZ
     */
    unsigned int getSampleRate() const { return audioFormat_.sample_rate; }

    /**
     * Get the audio format of the layer (sample rate & channel number).
     */
    AudioFormat getFormat() const { return audioFormat_; }

    /**
     * Emit an audio notification (beep) on incoming calls
     */
    void notifyIncomingCall();

    virtual void updatePreference(AudioPreference& pref, int index, AudioDeviceType type) = 0;

protected:
    /**
     * Callback to be called by derived classes when the audio output is opened.
     */
    void hardwareFormatAvailable(AudioFormat playback, size_t bufSize = 0);

    /**
     * Set the input format on necessary objects.
     */
    void hardwareInputFormatAvailable(AudioFormat capture);

    void devicesChanged();

    void playbackChanged(bool started);
    void recordChanged(bool started);
    void setHasNativeAEC(bool hasEAC);
    void setHasNativeNS(bool hasNS);

    std::shared_ptr<AudioFrame> getToPlay(AudioFormat format, size_t writableSamples);
    std::shared_ptr<AudioFrame> getToRing(AudioFormat format, size_t writableSamples);
    std::shared_ptr<AudioFrame> getPlayback(AudioFormat format, size_t samples)
    {
        const auto& ringBuff = getToRing(format, samples);
        const auto& playBuff = getToPlay(format, samples);
        return ringBuff ? ringBuff : playBuff;
    }

    void putRecorded(std::shared_ptr<AudioFrame>&& frame);

    void flush();

    /**
     * True if capture is not to be used
     */
    bool isCaptureMuted_;

    /**
     * True if playback is not to be used
     */
    bool isPlaybackMuted_;

    /**
     * True if ringtone should be muted
     */
    bool isRingtoneMuted_ {false};

    bool playbackStarted_ {false};
    bool recordStarted_ {false};
    bool hasNativeAEC_ {true};
    bool hasNativeNS_ {false};

    /**
     * Gain applied to mic signal
     */
    double captureGain_;

    /**
     * Gain applied to playback signal
     */
    double playbackGain_;

    // audio processor preferences
    const AudioPreference& pref_;

    /**
     * Buffers for audio processing
     */
    std::shared_ptr<RingBuffer> mainRingBuffer_;
    AudioBuffer ringtoneBuffer_;
    std::unique_ptr<AudioFrameResizer> playbackQueue_;

    /**
     * Whether or not the audio layer's playback stream is started
     */
    std::atomic<Status> status_ {Status::Idle};
    mutable std::condition_variable startedCv_;

    /**
     * Sample Rate that should be sent to the sound card
     */
    AudioFormat audioFormat_;

    /**
     * Sample Rate for input.
     */
    AudioFormat audioInputFormat_;

    size_t nativeFrameSize_ {0};

    /**
     * Urgent ring buffer used for ringtones
     */
    RingBuffer urgentRingBuffer_;

    /**
     * Lock for the entire audio layer
     */
    mutable std::mutex mutex_ {};

    /**
     * Remove audio offset that can be introduced by certain cheap audio device
     */
    DcBlocker dcblocker_ {};

    /**
     * Manage sampling rate conversion
     */
    std::unique_ptr<Resampler> resampler_;

private:
    std::mutex audioProcessorMutex {};
    std::unique_ptr<AudioProcessor> audioProcessor;

    void createAudioProcessor();
    void destroyAudioProcessor();

    // Set to "true" to play the incoming call notification (beep)
    // when the playback is on (typically when there is already an
    // active call).
    std::atomic_bool playIncomingCallBeep_ {false};
    /**
     * Time of the last incoming call notification
     */
    std::chrono::system_clock::time_point lastNotificationTime_ {
        std::chrono::system_clock::time_point::min()};
};

} // namespace jami
