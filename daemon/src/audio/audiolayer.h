/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef __AUDIO_LAYER_H__
#define __AUDIO_LAYER_H__

#include <cc++/thread.h> // for ost::Mutex
#include <sys/time.h>

#include "ringbuffer.h"
#include "dcblocker.h"
#include "samplerateconverter.h"
#include "noncopyable.h"

/**
 * @file  audiolayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware.
 */

class MainBuffer;
class AudioPreference;

namespace ost {
class Time;
}

enum AudioStreamDirection { AUDIO_STREAM_CAPTURE, AUDIO_STREAM_PLAYBACK };

class AudioLayer {
    private:
        NON_COPYABLE(AudioLayer);

    public:
        AudioLayer();
        virtual ~AudioLayer();

        virtual std::vector<std::string> getAudioDeviceList(AudioStreamDirection dir) const = 0;

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
         * @param toCopy The size of the buffer
         */
        void putUrgent(void* buffer, int toCopy);

        /**
         * Flush main buffer
         */
        void flushMain();

        /**
         * Flush urgent buffer
         */
        void flushUrgent();

        /**
         * Apply gain to audio frame
         */
        static void applyGain(SFLDataFormat *src , int samples, int gain);

        /**
         * Convert audio amplitude value from linear value to dB
         */
        static double amplitudeLinearToDB(double value) {
            return 20.0 * log10(value);
        }

        /**
         * Convert audio amplitude from dB to Linear value
         */
        static double ampluitudeDBToLinear(double value) {
            return pow(10.0, value / 20.0);
        }
 
        /**
         * Set capture stream gain (microphone)
         */
        void setCaptureGain(unsigned int gain) {
            captureGain_ = gain;
        }

        /**
         * Set capture stream gain (microphone) 
         */
       unsigned int getCaptureGain(void) {
            return captureGain_;
        }

        /**
         * Set playback stream gain (speaker)
         */
        void setPlaybackGain(unsigned int gain) {
            playbackGain_ = gain;
        }

        /**
         * Get playback stream gain (speaker) 
         */
        unsigned int getPlaybackGain(void) {
            return playbackGain_;
        }

        /**
         * Get the sample rate of the audio layer
         * @return unsigned int The sample rate
         *			    default: 44100 HZ
         */
        unsigned int getSampleRate() const {
            return audioSampleRate_;
        }

        /**
         * Get the mutex lock for the entire audio layer
         */
        ost::Mutex* getMutexLock() {
            return &mutex_;
        }

	/**
         * Emit an audio notification on incoming calls
         */
        void notifyincomingCall();

        /**
         * Gain applied to mic signal
         */
        static unsigned int captureGain_;

        /**
         * Gain applied to playback signal
         */
        static unsigned int playbackGain_;

    protected:

        /**
         * Wether or not the audio layer stream is started
         */
        bool isStarted_;

        /**
         * Urgent ring buffer used for ringtones
         */
        RingBuffer urgentRingBuffer_;

        /**
         * Sample Rate SFLphone should send sound data to the sound card
         * The value can be set in the user config file- now: 44100HZ
         */
        unsigned int audioSampleRate_;

        /**
         * Lock for the entire audio layer
         */
        ost::Mutex mutex_;

        /**
         * Remove audio offset that can be introduced by certain cheap audio device
         */
        DcBlocker dcblocker_;

        /**
         * Configuration file for this 
         */
        AudioPreference &audioPref;

        /**
         * Manage sampling rate conversion
         */
        SamplerateConverter *converter_;

    private:
        /**
         * Time of the last incoming call notification
         */
        time_t lastNotificationTime_;
};

#endif // _AUDIO_LAYER_H_
