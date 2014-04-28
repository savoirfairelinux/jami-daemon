/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Jerome Oufella <jerome.oufella@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Authro: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef AUDIO_LAYER_H_
#define AUDIO_LAYER_H_


#include "ringbuffer.h"
#include "dcblocker.h"
#include "resampler.h"
#include "noncopyable.h"

#include <sys/time.h>
#include <mutex>
#include <vector>

/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware.
 */

class MainBuffer;
class AudioPreference;

namespace ost {
class Time;
}

enum class DeviceType {
    PLAYBACK,      /** To open playback device only */
    CAPTURE,       /** To open capture device only */
    RINGTONE       /** To open the ringtone device only */
};

class AudioLayer {

    private:
        NON_COPYABLE(AudioLayer);

    public:

        AudioLayer(const AudioPreference &);
        virtual ~AudioLayer();

        virtual std::vector<std::string> getCaptureDeviceList() const = 0;
        virtual std::vector<std::string> getPlaybackDeviceList() const = 0;

        virtual int getAudioDeviceIndex(const std::string& name, DeviceType type) const = 0;
        virtual std::string getAudioDeviceName(int index, DeviceType type) const = 0;
        virtual int getIndexCapture() const = 0;
        virtual int getIndexPlayback() const = 0;
        virtual int getIndexRingtone() const = 0;

        /**
         * Start the capture stream and prepare the playback stream.
         * The playback starts accordingly to its threshold
         * ALSA Library API
         */
        virtual void startStream() = 0;

        /**
         * Stop the playback and capture streams.
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         * ALSA Library API
         */
        virtual void stopStream() = 0;

        /**
         * Determine wether or not the audio layer is active (i.e. stream opened)
         */
        bool isStarted() const {
            return isStarted_;
        }

        /**
         * Send a chunk of data to the hardware buffer to start the playback
         * Copy data in the urgent buffer.
         * @param buffer The buffer containing the data to be played ( ringtones )
         */
        void putUrgent(AudioBuffer& buffer);

        /**
         * Flush main buffer
         */
        void flushMain();

        /**
         * Flush urgent buffer
         */
        void flushUrgent();

        bool isCaptureMuted() const {
            return isCaptureMuted_;
        }

        /**
         * Mute capture (microphone)
         */
        void muteCapture(bool muted) {
            isCaptureMuted_ = muted;
        }

        bool isPlaybackMuted() const {
            return isPlaybackMuted_;
        }

        /**
         * Mute playback
         */
        void mutePlayback(bool muted) {
            isPlaybackMuted_ = muted;
        }

        /**
         * Set capture stream gain (microphone)
         * Range should be [-1.0, 1.0]
         */
        void setCaptureGain(double gain) {
            captureGain_ = gain;
        }

        /**
         * Get capture stream gain (microphone)
         */
        double getCaptureGain() const {
            return captureGain_;
        }

        /**
         * Set playback stream gain (speaker)
         * Range should be [-1.0, 1.0]
         */
        void setPlaybackGain(double gain) {
            playbackGain_ = gain;
        }

        /**
         * Get playback stream gain (speaker)
         */
        double getPlaybackGain() const {
            return playbackGain_;
        }

        /**
         * Get the sample rate of the audio layer
         * @return unsigned int The sample rate
         *			    default: 44100 HZ
         */
        unsigned int getSampleRate() const {
            return audioFormat_.sample_rate;
        }

        /**
         * Get the audio format of the layer (sample rate & channel number).
         */
        AudioFormat getFormat() const {
            return audioFormat_;
        }

        /**
         * Emit an audio notification on incoming calls
         */
        void notifyIncomingCall();

        virtual void updatePreference(AudioPreference &pref, int index, DeviceType type) = 0;

    protected:
        /**
         * Callback to be called by derived classes when the audio output is opened.
         */
        void hardwareFormatAvailable(AudioFormat playback);

        /**
         * True if capture is not to be used
         */
        bool isCaptureMuted_;

        /**
         * True if playback is not to be used
         */
        bool isPlaybackMuted_;

        /**
         * Gain applied to mic signal
         */
        double captureGain_;

        /**
         * Gain applied to playback signal
         */
        double playbackGain_;

        /**
         * Whether or not the audio layer stream is started
         */
        bool isStarted_;

        /**
         * Sample Rate SFLphone should send sound data to the sound card
         */
        AudioFormat audioFormat_;

        /**
         * Urgent ring buffer used for ringtones
         */
        RingBuffer urgentRingBuffer_;

        /**
         * Lock for the entire audio layer
         */
        std::mutex mutex_;

        /**
         * Remove audio offset that can be introduced by certain cheap audio device
         */
        DcBlocker dcblocker_;

        /**
         * Manage sampling rate conversion
         */
        Resampler resampler_;

    private:

        /**
         * Time of the last incoming call notification
         */
        time_t lastNotificationTime_;
};

#endif // _AUDIO_LAYER_H_
